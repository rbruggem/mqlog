#include "segment.h"
#include "prot.h"
#include "util.h"
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

struct segment {
    int                meta_fd;
    int                data_fd;
    size_t             size;
    uint64_t           offset;
    unsigned char*     buffer;
    volatile uint64_t  w_offset; // write offset
    volatile uint64_t  s_offset; // sync (to disk) offset
};

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
        return -1;
    }

    if (ensure_directory(dir) != 0) {
        return -1;
    }

    const size_t len1 = 256;
    char file[len1];
    if (append_file_to_dir(file, len1, dir, filename) == -1) {
        return -1;
    }

    // File contains the fullpath of the meta file.
    int fd = open(file,
                  O_RDWR | O_CREAT,
                  S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (fd < 0) {
        return -1;
    }

    return fd;
}

static int open_data(const char* dir, uint64_t offset, size_t size) {
    const size_t len0 = 64;
    char filename[len0];
    if (data_filename(filename, len0, offset) == -1) {
        return -1;
    }

    if (ensure_directory(dir) != 0) {
        return -1;
    }

    const size_t len1 = 256;
    char file[len1];
    if (append_file_to_dir(file, len1, dir, filename) == -1) {
        return -1;
    }

    // file contains the fullpath of the segment file
    int fd = open(file,
                  O_RDWR | O_CREAT,
                  S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (fd < 0) {
        return -1;
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

    return -1;
}

static int load_meta(segment_t* sgm, int meta_fd) {
    // TODO: this is very primitive.
    const size_t size = sizeof(segment_t);
    ssize_t n = pread(meta_fd, sgm, sizeof(*sgm), 0);
    if (n == 0) {
        // meta file is new and empty
        sgm->meta_fd = meta_fd;
        return 0;
    }

    if ((size_t)n == size) {
        sgm->meta_fd = meta_fd;
        return 0;
    }
    return -1;
}

static int sync_meta(const segment_t* sgm) {
    // TODO: this is very primitive.
    const size_t size = sizeof(segment_t);
    ssize_t n = pwrite(sgm->meta_fd, sgm, size, 0);
    if (n < 0) {
        return -1;
    }
    return (size_t)n == size ? 0 : -1;
}

static uint64_t claim_woffset(segment_t* sgm, size_t size) {
    // TODO: this seems to generate a `lock cmpxchg` instruction.
    // Compare with `lock xadd`.
    return __sync_fetch_and_add(&sgm->w_offset, size);
}

segment_t* segment_open(const char* dir, uint64_t offset, size_t size) {
    // Size has to be a multiple of page size.
    if (size % pagesize() != 0) {
        return NULL;
    }

    struct segment* sgm = (struct segment*)malloc(sizeof(struct segment));
    if (!sgm) {
        return NULL;
    }

    // Initialize segment struct
    bzero(sgm, sizeof(struct segment));

    // The meta file contains the contents
    // of struct segment for this particular segment file.
    // The file fill be created if it does
    // not exist.
    int meta_fd = open_meta(dir, offset);
    if (meta_fd < 0) {
        free(sgm);
        return NULL;
    }

    // Load the contents of the meta file into sgm.
    // This will only occur if the meta file is not new.
    if (load_meta(sgm, meta_fd) != 0) {
        close(meta_fd);
        free(sgm);
        return NULL;
    }

    // The data file is the actual segment file.
    int data_fd = open_data(dir, offset, size);
    if (data_fd < 0) {
        close(meta_fd);
        free(sgm);
        return NULL;
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
        return NULL;
    }

    sgm->buffer = buffer;
    sgm->size = size;

    // Hint: the segment file will mostly be accessed sequentially.
    if (madvise(sgm->buffer, size, MADV_SEQUENTIAL) != 0) {
        munmap(sgm->buffer, sgm->size);
        close(sgm->data_fd);
        close(sgm->meta_fd);
        free(sgm);
        return NULL;
    }

    return sgm;
}

int segment_close(segment_t* sgm) {
    // TODO: sync.
    sync_meta(sgm);

    // Reclaim all resources.
    munmap(sgm->buffer, sgm->size);
    close(sgm->data_fd);
    close(sgm->meta_fd);
    free(sgm);

    // TODO: the directory may require fsyncing too.

    return 0;
}

ssize_t segment_write(segment_t* sgm, const void* buf, size_t size) {
    // `buf` will be frame with a header,
    // therefore the actually data inserted into the segment
    // has size: header size + buf size

    const size_t header_size = sizeof(struct header);
    const size_t frame_size = size + header_size;

    // This function will claim a slot in the segment enough
    // to store the contents of `buf` + the header.
    // This call is thread safe.
    //
    // TODO: alignment
    const uint64_t w_offset = claim_woffset(sgm, frame_size);

    // Calculate the offset where to insert the payload
    const uint64_t payload_offset = w_offset + header_size;

    // The payload gets inserted before the header.
    // This is because the header contains a flags that
    // indicates that the entire frame has been segmentged,
    // therefore it needs to be inserted last.
    //
    // TODO: Double check this, it may not be true.
    // See http://0b4af6cdc2f0c5998459-c0245c5c937c5dedcca3f1764ecc9b2f.r43.cf2.rackcdn.com/17780-osdi14-paper-pillai.pdf

    memcpy(sgm->buffer + payload_offset, buf, size);

    struct header hdr;
    header_init(&hdr);

    // Useful to check a segment's file data integrity.
    //
    // TODO: is this really needed? Does a filesystem do this?
    hdr.crc32 = crc32((unsigned char*)buf, size);
    hdr.size = frame_size;

    // Marks content as ready to be consumed.
    // This flag needed because w_offset is increment before
    // the new playload is inserted.
    hdr.flags = HEADER_FLAGS_READY;

    // The header gets copied into the buffer.
    memcpy(sgm->buffer + w_offset, &hdr, header_size);

    // Returns the number of byte of the initial buffer that
    // have been inserted into the segment.
    // The header is transparent to clients.
    return size;
}

int segment_read(const segment_t* sgm, uint64_t offset, struct frame* fr) {
    // Check that the offset is within the write boundary.
    if (offset > sgm->w_offset) {
        return -1;
    }

    // Assume there's a header.
    struct header* hdr = (struct header*)(sgm->buffer + offset);

    // Verify `hdr` is a valid header
    if (hdr->flags != HEADER_FLAGS_READY) {
        return -1;
    }

    fr->hdr = hdr;

    // No copy.
    const size_t header_size = sizeof(struct header);
    fr->buffer = sgm->buffer + offset + header_size;

    return 0;
}

int segment_destroy(const char* dir) {
    return delete_directory(dir);
}
