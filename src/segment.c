#include "segment.h"
#include "prot.h"
#include "util.h"
#include "logerrno.h"
#include "cassert.h"
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <stdio.h>
#include <errno.h>
#include <assert.h>

#define DATA_SUFFIX  "log"
#define INDEX_SUFFIX "idx"

#define LATEST_SEGMENT_VERSION 0

struct index_entry {
    volatile size_t physical_offset;
};

struct offset_pair {
    uint32_t index;
    uint32_t data;
}; // strictly 8 bytes

// Helper for compare and swap operation.
// `value` needs to fit within `cas_helper`.
// See https://gcc.gnu.org/ml/gcc-help/2008-10/msg00217.html
union cas_offset_pair {
    struct offset_pair value;
    uint64_t cas_helper;
};

CASSERT(sizeof(uint64_t) >= sizeof(struct offset_pair), segement_c)

struct segment {
    // struct segment version.
    // It is required to be the first 4 bytes of the struct.
    unsigned int                   version;
    unsigned int                   flags;
    int                            index_fd;      // index file descriptor
    int                            data_fd;       // segment file descriptor
    uint32_t                       size;          // size of the segment in bytes
    uint64_t                       base_offset;   // base offset of the segment
    volatile unsigned char*        buffer;
    volatile struct index_entry*   index;
    volatile struct offset_pair    s_offset_pair; // sync (to disk) offset
    volatile union cas_offset_pair w_offset_pair;
};

static int index_filename(char filename[], size_t len, uint64_t offset) {
    int n = snprintf(filename, len, "%jd.%s", offset, INDEX_SUFFIX);
    return n <= (int)len ? 0 : -1;
}

static int data_filename(char filename[], size_t len, uint64_t offset) {
    int n = snprintf(filename, len, "%jd.%s", offset, DATA_SUFFIX);
    return n <= (int)len ? 0 : -1;
}

static int open_index(const char* dir, uint64_t offset, size_t size) {
    const size_t len0 = 64;
    char filename[len0];
    if (index_filename(filename, len0, offset) == -1) {
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

static int mmap_helper(void** ptr, size_t size, int fd) {
    // TODO: maybe mapping the whole file is not necessary.
    void* ptr0 = mmap(0,
                      size,
                      PROT_READ | PROT_WRITE,
                      MAP_SHARED,
                      fd,
                      0);
    if (!ptr0) {
        return ELMMAP;
    }

    // Hint: the segment file will mostly be accessed sequentially.
    if (madvise(ptr0, size, MADV_SEQUENTIAL) != 0) {
        munmap(ptr0, size);
        return ELMADV;
    }

    *ptr = ptr0;
    return 0;
}

static int find_w_offset_pair(struct offset_pair* w_offset_pair,
                              volatile const unsigned char* buffer,
                              volatile const struct index_entry* index,
                              size_t size) {

    // size if index buffer size, not the number of index entries
    const size_t max_index_entries = size / sizeof(struct index_entry);

    size_t prev_physical_offset = 0;
    const struct header* prev_hdr = NULL;
    size_t i = 0;
    for (; i < max_index_entries; ++i) {
        if (index[i].physical_offset == 0) {
            if (!(i == 0 && prot_is_header((void*)buffer))) {
                size_t offset = 0;
                if (i != 0) {
                    prev_hdr =
                        (const struct header*)&buffer[prev_physical_offset];
                    offset = prev_hdr->size;
                }
                w_offset_pair->index = i;
                w_offset_pair->data = prev_physical_offset + offset;
                return 0;
            }
        }
        prev_physical_offset = index[i].physical_offset;
    }

    return ELWOFFS;
}

static size_t calculate_index_size(size_t data_size) {
    // implements `ceil` function
    return (data_size + 1) / sizeof(struct header);
}

static int claim(segment_t* sgm,
                 struct offset_pair old,
                 struct offset_pair new) {

    // See https://gcc.gnu.org/ml/gcc-help/2008-10/msg00217.html
    const union cas_offset_pair cas_old = {
        .value = old
    };

    const union cas_offset_pair cas_new = {
        .value = new
    };

    // Atomicly reserve index and data area.
    if (!__sync_bool_compare_and_swap(
        &sgm->w_offset_pair.cas_helper,
        cas_old.cas_helper,
        cas_new.cas_helper)) {

        return ELLOCK;
    }

    return 0;
}

static int mark_eos(segment_t* sgm, struct offset_pair curr) {
    const size_t header_size = sizeof(struct header);

    // marking EOS does not increase index
    const struct offset_pair new = {
        .index = curr.index,
        .data = curr.data + header_size
    };

    int rc = claim(sgm, curr, new);
    if (rc != 0) {
        return rc;
    }

    const size_t w_offset = curr.data;

    struct header* hdr = (struct header*)(sgm->buffer + w_offset);
    header_init(hdr);
    hdr->size = header_size;

    // Mark segment as complete for writes.
    hdr->flags = HEADER_FLAGS_EOS;

    return ELEOS;
}

static int marked_eos(const segment_t* sgm) {
    const size_t header_size = sizeof(struct header);
    const size_t w_offset = sgm->w_offset_pair.value.data;
    if (w_offset < header_size) {
        return 0;
    }

    const uint64_t last_header_offset = w_offset - header_size;

    const struct header* hdr =
       (const struct header*)(sgm->buffer + last_header_offset);

    return hdr->flags == HEADER_FLAGS_EOS;
}

static int sync_data(segment_t* sgm) {
    const void* addr = (void*)&sgm->buffer[sgm->s_offset_pair.data];
    const size_t w_offset = sgm->w_offset_pair.value.data;
    const size_t size = w_offset - sgm->s_offset_pair.data;

    // addr needs to be a multiple of pagesize for msync to work.
    void* sync_addr = (void*)page_aligned_addr((size_t)addr);
    const size_t diff = (size_t)addr - (size_t)sync_addr;
    const size_t sync_size = size + diff;

    if (msync(sync_addr, sync_size, MS_SYNC) != 0) {
        return ELDTSYN;
    }

    sgm->s_offset_pair.data = w_offset;

    // returns size in bytes of the synced area
    return sync_size;
}

static int sync_index(segment_t* sgm) {
    const void* addr = (const void*)&sgm->index[sgm->s_offset_pair.index];
    const size_t w_index = sgm->w_offset_pair.value.index;
    const size_t length = w_index - sgm->s_offset_pair.index;
    const size_t size = sizeof(struct index_entry) * length;

    // addr needs to be a multiple of pagesize for msync to work.
    void* sync_addr = (void*)page_aligned_addr((size_t)addr);
    const size_t diff = (size_t)addr - (size_t)sync_addr;
    const size_t sync_size = size + diff;

    if (msync(sync_addr, sync_size, MS_SYNC) != 0) {
        return ELDTSYN;
    }
    sgm->s_offset_pair.index = w_index;

    // returns number of index entries synced
    return length;
}

int segment_open(segment_t** sgm_ptr,
                 const char* dir,
                 uint64_t base_offset,
                 uint32_t size,  // segment max size: 4GB
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
    memset(sgm, 0, sizeof(struct segment));
    sgm->base_offset = base_offset;

    // The index will contain one entry for each entry in the segment
    // TODO: use sparse index
    const size_t index_size = calculate_index_size(size);

    // The file will be created if it does
    // not exist.
    int index_fd = open_index(dir, base_offset, index_size);
    if (index_fd < 0) {
        free(sgm);
        return index_fd;
    }

    sgm->index_fd = index_fd;

    int rc = mmap_helper((void**)&sgm->index, index_size, sgm->index_fd);
    if (rc != 0) {
        close(sgm->index_fd);
        free(sgm);
        return rc;
    }

    // The data file is the actual segment file.
    int data_fd = open_data(dir, base_offset, size);
    if (data_fd < 0) {
        close(sgm->index_fd);
        munmap((void*)sgm->index, index_size);
        free(sgm);
        return data_fd;
    }

    sgm->data_fd = data_fd;

    // Map the segment file into memory.
    rc = mmap_helper((void**)&sgm->buffer, size, sgm->data_fd);
    if (rc != 0) {
        close(sgm->data_fd);
        close(sgm->index_fd);
        munmap((void*)sgm->index, index_size);
        free(sgm);
        return rc;
    }

    sgm->size = size;
    sgm->flags = flags;
    sgm->version = LATEST_SEGMENT_VERSION;

    struct offset_pair w_offset_pair;
    rc = find_w_offset_pair(&w_offset_pair,
                            sgm->buffer,
                            sgm->index,
                            index_size);
    if (rc != 0) {
        close(sgm->data_fd);
        close(sgm->index_fd);
        munmap((void*)sgm->index, index_size);
        munmap((void*)sgm->buffer, size);
        free(sgm);
        return rc;
    }

    union cas_offset_pair cas_w_offset_pair = {
        .value = w_offset_pair
    };

    sgm->w_offset_pair = cas_w_offset_pair;
    sgm->s_offset_pair = cas_w_offset_pair.value;

    *sgm_ptr = sgm;

    return 0;
}

int segment_close(segment_t* sgm) {
    int rc = segment_sync(sgm);
    if (rc < 0) {
        return rc;
    }

    // Reclaim all resources.
    const size_t index_size = calculate_index_size(sgm->size);
    munmap((void*)sgm->buffer, sgm->size);
    munmap((void*)sgm->index, index_size);
    close(sgm->data_fd);
    close(sgm->index_fd);
    free(sgm);

    // TODO: the directory may require fsyncing too.

    return 0;
}

// TODO: should be called `segment_base_offset`
uint64_t segment_base_offset(const segment_t* sgm) {
    return sgm->base_offset;
}

uint64_t segment_write_offset(const segment_t* sgm) {
    return sgm->base_offset + sgm->w_offset_pair.value.index;
}

uint64_t segment_read_offset(const segment_t* sgm) {
    // Only what has been written can be read.
    return sgm->base_offset + sgm->w_offset_pair.value.index;
}

ssize_t segment_write(segment_t* sgm, const void* buf, size_t size) {
    // First of all check if the segment is writable.
    if (marked_eos(sgm)) {
        return ELEOS;
    }

    const struct offset_pair curr_w_offset_pair = sgm->w_offset_pair.value;

    // The data inserted into the segment
    // has size: header size + buf size.
    const size_t header_size = sizeof(struct header);
    const size_t frame_size = header_size + size;

    // w_offset marks the begging of the area in the log,
    // where the frame can be written.
    const size_t w_offset = curr_w_offset_pair.data;

    // Make sure there's always available space to include
    // and End Of Segment (EOS) frame.
    // To enforce this, a payload can only be inserted if:
    // sizeof(payload) + 2 * sizeof(header) <= space left in segment.
    if (header_size + frame_size > sgm->size - w_offset) {
        // No more entries in this segment: add EOS frame.
        return mark_eos(sgm, curr_w_offset_pair);
    }

    const struct offset_pair new_w_offset_pair = {
        .index = curr_w_offset_pair.index + 1,
        .data = curr_w_offset_pair.data + frame_size
    };

    int rc = claim(sgm, curr_w_offset_pair, new_w_offset_pair);
    if (rc != 0) {
        return rc;
    }

    LOG_PRINT("segment_write %p, "
              "curr offset pair: (%"PRIu32", %"PRIu32"), "
              "new offset pair (%"PRIu32", %"PRIu32")\n",
              (void*)sgm,
              curr_w_offset_pair.index,
              curr_w_offset_pair.data,
              new_w_offset_pair.index,
              new_w_offset_pair.data);

    // Calculate the offset where to insert the payload.
    const size_t payload_offset = w_offset + header_size;

    // The payload gets inserted before the header.
    // This is because the header contains a flags that
    // indicates that the entire frame has been segmentged,
    // therefore it needs to be inserted last.
    //
    // TODO: Double check this, it may not be true.
    // See http://0b4af6cdc2f0c5998459-c0245c5c937c5dedcca3f1764ecc9b2f.r43.cf2.rackcdn.com/17780-osdi14-paper-pillai.pdf
    memcpy((unsigned char*)sgm->buffer + payload_offset, buf, size);

    struct header* hdr = (struct header*)(sgm->buffer + w_offset);
    header_init(hdr);

    // Useful to check a segment's file data integrity.
    // TODO: is this really needed? Does a filesystem do this?.
    // TODO: should this be calculated before reserving an area in
    // the segment?
    hdr->crc32 = crc32((unsigned char*)buf, size);
    hdr->size = frame_size;

    // Marks content as ready to be consumed.
    // This flag is needed because w_offset is incremented before
    // the new playload is inserted.
    hdr->flags = HEADER_FLAGS_READY;

    // Update the index, after inserting data.
    // In case of a crash, the index can be rebuilt by scanning
    // the data.
    // TODO: add functionality to rebuild the index.
    const size_t i_offset = curr_w_offset_pair.index;
    const struct index_entry entry = {
        .physical_offset = w_offset,
    };
    sgm->index[i_offset] = entry;

    // Returns the number of bytes of the initial buffer that
    // have been inserted into the segment.
    // The header is transparent to clients.
    return size;
}

ssize_t segment_read(const segment_t* sgm,
                     uint64_t relative_offset,
                     struct frame* fr) {
    size_t boundary = sgm->w_offset_pair.value.data;
    if ((sgm->flags & SGM_RDCMT) == SGM_RDCMT) {
        boundary = sgm->s_offset_pair.data;
    }

    const size_t i_offset = sgm->w_offset_pair.value.index;

    // Check that the physical offset is within the right boundary.
    if (relative_offset >= i_offset) {
        return ELNORD;
    }

    // Index lookup O(1)
    volatile const struct index_entry* entry = &sgm->index[relative_offset];
    const size_t physical_offset = entry->physical_offset;

    if (relative_offset != 0 && physical_offset == 0) {
        // physical_offset can be zero only if relative_offset is zero
        return ELNORD;
    }

    // Check that the physical offset is within the right boundary.
    if (physical_offset >= boundary) {
        return ELNORD;
    }

    // Assume there's a header.
    struct header* hdr = (struct header*)(sgm->buffer + physical_offset);

    // Verify `hdr` is a valid header.
    switch (hdr->flags) {
        case HEADER_FLAGS_READY:
            break;

        case HEADER_FLAGS_EOS:
            fr->hdr = hdr;
            return ELEOS;

        case HEADER_FLAGS_EMPTY:
            // This case is usually hit when a new frame is about to being
            // written to the segment but only the write offset has been
            // incremented.
            return ELINVHD;

        default:
            return ELINVHD;

    }

    fr->hdr = hdr;

    // No copy.
    const size_t header_size = sizeof(struct header);
    fr->buffer = (unsigned char*)sgm->buffer + physical_offset + header_size;

    return fr->hdr->size - header_size;
}

ssize_t segment_sync(segment_t* sgm) {
    ssize_t size = sync_data(sgm);
    if (size <= 0) {
        return size;
    }

    return sync_index(sgm);
}
