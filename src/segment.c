#include "segment.h"
#include "prot.h"
#include "util.h"
#include "logerrno.h"
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <stdio.h>
#include <errno.h>

#define DATA_SUFFIX  "log"
#define META_SUFFIX  "meta"

#define LATEST_SEGMENT_VERSION 0

struct segment {
    // struct segment version.
    // It is required to be the first 4 bytes of the struct.
    unsigned int       version;

    unsigned int       flags;

    int                meta_fd;
    int                data_fd;
    size_t             size;
    uint64_t           offset;
    unsigned char*     buffer;
    volatile uint64_t  w_offset; // write offset
    volatile uint64_t  s_offset; // sync (to disk) offset
}; // 48 bytes

static int meta_filename(char filename[], size_t len, uint64_t offset) {
    int n = snprintf(filename, len, "%jd.%s", offset, META_SUFFIX);
    return n <= (int)len ? 0 : -1;
}

static int data_filename(char filename[], size_t len, uint64_t offset) {
    int n = snprintf(filename, len, "%jd.%s", offset, DATA_SUFFIX);
    return n <= (int)len ? 0 : -1;
}

static int open_meta(const char* dir, uint64_t offset) {
    const size_t len0 = 64;
    char filename[len0];
    if (meta_filename(filename, len0, offset) == -1) {
        return ELSOFLW;
    }

    if (ensure_directory(dir) != 0) {
        return ELLGDIR;
    }

    const size_t len1 = 256;
    char file[len1];
    if (append_file_to_dir(file, len1, dir, filename) == -1) {
        return ELSOFLW;
    }

    // File contains the fullpath of the meta file.
    int fd = open(file,
                  O_RDWR | O_CREAT,
                  S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (fd < 0) {
        return ELFLEOP;
    }

    return fd;
}

static int open_data(const char* dir, uint64_t offset, size_t size) {
    const size_t len0 = 64;
    char filename[len0];
    if (data_filename(filename, len0, offset) == -1) {
        return ELSOFLW;
    }

    if (ensure_directory(dir) != 0) {
        return ELFLEOP;
    }

    const size_t len1 = 256;
    char file[len1];
    if (append_file_to_dir(file, len1, dir, filename) == -1) {
        return ELSOFLW;
    }

    // file contains the fullpath of the segment file
    int fd = open(file,
                  O_RDWR | O_CREAT,
                  S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (fd < 0) {
        return ELFLEOP;
    }

    struct stat file_stat;
    if (fstat(fd, &file_stat) < 0) {
        goto error;
    }

    size_t file_size = file_stat.st_size;
    if (file_size > 0 && file_size != size) {
        goto error;
    }

    if (file_size == 0) {
        if (ftruncate(fd, size) < 0) {
            goto error;
        }
    }

    return fd;

error:
    if (fd > 0) {
        close(fd);
    }

    return ELFLEOP;
}

static int load_meta_v0(segment_t* sgm,
                      int meta_fd,
                      unsigned char* buffer,
                      size_t size) {

    const size_t segment_size = sizeof(segment_t);
    if (segment_size > size) {
        return -1;
    }

    memcpy(sgm, buffer, segment_size);
    sgm->meta_fd = meta_fd;
    sgm->version = LATEST_SEGMENT_VERSION;

    return 0;
}

static int load_meta(segment_t* sgm, int meta_fd) {
    const size_t size = 64; // must be enough to hold struct segment
    unsigned char buffer[size];

    const ssize_t n = pread(meta_fd, buffer, size, 0);
    if (n == -1) {
        return -1;
    }

    // meta file is new and empty.
    if (n == 0) {
        sgm->meta_fd = meta_fd;
        sgm->version = LATEST_SEGMENT_VERSION;
        return 0;
    }

    // First four bytes contain the version.
    const unsigned int* segment_version = (unsigned int*)buffer;

    switch (*segment_version) {
        case 0:
            return load_meta_v0(sgm, meta_fd, buffer, min((size_t)n, size));

        default:
            return ELSGRMT;
    };

    return ELSGRMT;
}

static int sync_meta(const segment_t* sgm) {
    const size_t size = sizeof(segment_t);
    ssize_t n = pwrite(sgm->meta_fd, sgm, size, 0);
    if (n < 0) {
        return ELSGSMT;
    }

    int rc = fdatasync(sgm->meta_fd);
    if (rc != 0) {
        return ELMTSYN;
    }

    return (size_t)n == size ? 0 : ELSGSMT;
}

static int marked_eos(const segment_t* sgm) {
    const size_t header_size = sizeof(struct header);
    if (sgm->w_offset < header_size) {
        return 0;
    }

    const uint64_t last_header_offset = sgm->w_offset - header_size;

    const struct header* hdr =
        (const struct header*)(sgm->buffer + last_header_offset);

    return hdr->flags == HEADER_FLAGS_EOS;
}

int segment_open(segment_t** sgm_ptr,
                 const char* dir,
                 uint64_t offset,
                 size_t size,
                 unsigned int flags) {
    // Size has to be a multiple of page size.
    if (size % pagesize() != 0) {
        return ELNOPGM;
    }

    struct segment* sgm = (struct segment*)malloc(sizeof(struct segment));
    if (!sgm) {
        return ELALLC;
    }

    // Initialize segment struct
    bzero(sgm, sizeof(struct segment));
    sgm->offset = offset;

    // The meta file contains the contents
    // of struct segment for this particular segment file.
    // The file will be created if it does
    // not exist.
    int meta_fd = open_meta(dir, offset);
    if (meta_fd < 0) {
        free(sgm);
        return meta_fd;
    }

    // flags are overwritten.
    sgm->flags = flags;

    // Load the contents of the meta file into sgm.
    // This will only occur if the meta file is not new.
    int rc = load_meta(sgm, meta_fd);
    if (rc != 0) {
        close(meta_fd);
        free(sgm);
        return rc;
    }

    // The data file is the actual segment file.
    int data_fd = open_data(dir, offset, size);
    if (data_fd < 0) {
        close(meta_fd);
        free(sgm);
        return data_fd;
    }

    sgm->data_fd = data_fd;

    // Map the segment file into memory.
    //
    // TODO: maybe mapping the whole file is not necessary.
    void* buffer = mmap(0,
                        size,
                        PROT_READ | PROT_WRITE,
                        MAP_SHARED,
                        data_fd,
                        0);
    if (!buffer) {
        close(sgm->data_fd);
        close(sgm->meta_fd);
        free(sgm);
        return ELMMAP;
    }

    sgm->buffer = buffer;
    sgm->size = size;

    // Hint: the segment file will mostly be accessed sequentially.
    if (madvise(sgm->buffer, size, MADV_SEQUENTIAL) != 0) {
        munmap(sgm->buffer, sgm->size);
        close(sgm->data_fd);
        close(sgm->meta_fd);
        free(sgm);
        return ELMADV;
    }

    *sgm_ptr = sgm;

    return 0;
}

int segment_close(segment_t* sgm) {
    int rc = segment_sync(sgm);
    if (rc < 0) {
        return rc;
    }

    rc = sync_meta(sgm);
    if (rc != 0) {
        return rc;
    }

    // Reclaim all resources.
    munmap(sgm->buffer, sgm->size);
    close(sgm->data_fd);
    close(sgm->meta_fd);
    free(sgm);

    // TODO: the directory may require fsyncing too.

    return 0;
}

uint64_t segment_offset(const segment_t* sgm) {
    return sgm->offset;
}

uint64_t segment_roffset(const segment_t* sgm) {
    // Only what has been written can be read.
    return sgm->w_offset + sgm->offset;
}

ssize_t segment_write(segment_t* sgm, const void* buf, size_t size) {
    // First of all check is the segment is writable.
    if (marked_eos(sgm)) {
        return ELEOS;
    }

    // `buf` will be frame with a header,
    // therefore the actually data inserted into the segment
    // has size: header size + buf size.
    const size_t header_size = sizeof(struct header);
    const size_t frame_size = size + header_size;

    const uint64_t w_offset = sgm->w_offset;

    // Make sure there's always available space in to include
    // and End Of Segment frame.
    // To enforce this, a payload can only be inserted if:
    // sizeof(payload) + 2 * sizeof(header) <= space left in segment.
    if (header_size + frame_size > sgm->size - w_offset) {

        // No more entries in this segment: add EOS frame.
        if (!__sync_bool_compare_and_swap(
            &sgm->w_offset,
            w_offset,
            w_offset + header_size)) {

            return ELLOCK;
        }

        struct header* hdr = (struct header*)(sgm->buffer + w_offset);
        header_init(hdr);
        hdr->size = header_size;

        // Mark segment as complete for writes.
        hdr->flags = HEADER_FLAGS_EOS;

        return ELEOS;
    }

    if (!__sync_bool_compare_and_swap(
        &sgm->w_offset,
        w_offset,
        w_offset + frame_size)) {

        return ELLOCK;
    }

    // Calculate the offset where to insert the payload.
    const uint64_t payload_offset = w_offset + header_size;

    // The payload gets inserted before the header.
    // This is because the header contains a flags that
    // indicates that the entire frame has been segmentged,
    // therefore it needs to be inserted last.
    //
    // TODO: Double check this, it may not be true.
    // See http://0b4af6cdc2f0c5998459-c0245c5c937c5dedcca3f1764ecc9b2f.r43.cf2.rackcdn.com/17780-osdi14-paper-pillai.pdf
    memcpy(sgm->buffer + payload_offset, buf, size);

    struct header* hdr = (struct header*)(sgm->buffer + w_offset);
    header_init(hdr);

    // Useful to check a segment's file data integrity.
    //
    // TODO: is this really needed? Does a filesystem do this?
    hdr->crc32 = crc32((unsigned char*)buf, size);
    hdr->size = frame_size;

    // Marks content as ready to be consumed.
    // This flag is needed because w_offset is incremented before
    // the new playload is inserted.
    hdr->flags = HEADER_FLAGS_READY;

    // Returns the number of byte of the initial buffer that
    // have been inserted into the segment.
    // The header is transparent to clients.
    return size;
}

ssize_t segment_read(const segment_t* sgm, uint64_t offset, struct frame* fr) {
    uint64_t boundary = sgm->w_offset;
    if ((sgm->flags & SGM_RDCMT) == SGM_RDCMT) {
        boundary = sgm->s_offset;
    }

    // Check that the offset is within the right boundary.
    if (offset >= boundary) {
        return ELNORD;
    }

    // Assume there's a header.
    struct header* hdr = (struct header*)(sgm->buffer + offset);

    // Verify `hdr` is a valid header.
    switch (hdr->flags) {
        case HEADER_FLAGS_READY:
            break;

        case HEADER_FLAGS_EOS:
            fr->hdr = hdr;
            return ELEOS;

        case HEADER_FLAGS_EMPTY:
            return ELINVHD;

        default:
            return ELINVHD;

    }

    fr->hdr = hdr;

    // No copy.
    const size_t header_size = sizeof(struct header);
    fr->buffer = sgm->buffer + offset + header_size;

    return fr->hdr->size - header_size;
}

ssize_t segment_sync(segment_t* sgm) {
    const void* addr = &sgm->buffer[sgm->s_offset];
    const size_t size = sgm->w_offset - sgm->s_offset;

    // addr needs to be a multiple of pagesize for msync to work.
    void* sync_addr = (void*)page_aligned_addr((size_t)addr);
    const size_t diff = (size_t)addr - (size_t)sync_addr;
    const size_t sync_size = size + diff;

    if (msync(sync_addr, sync_size, MS_SYNC) != 0) {
        return ELDTSYN;
    }

    sgm->s_offset = sgm->w_offset;

    return sync_size;
}

size_t segment_unused(const segment_t* sgm) {
    return sgm->size - sgm->w_offset;
}

size_t segment_next_segment_delta(const segment_t* sgm) {
    // return: one EOS frame (consists only of the header) +
    // any unused space.
    return sgm->size - sgm->w_offset + sizeof(struct header);
}
