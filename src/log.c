#include "log.h"
#include "segment.h"
#include "util.h"
#include <string.h>

enum { MAX_DIR_SIZE = 1024 };

struct log {
    size_t     size;
    uint64_t   offset;
    char       dir[MAX_DIR_SIZE];
    segment_t* prev;
    segment_t* curr;
    segment_t* empty;
};

static segment_t* next_segment(log_t* lg) {
    segment_t* sgm = segment_open(lg->dir, lg->offset, lg->size);
    if (sgm == NULL) {
        return NULL;
    }
    lg->offset += lg->size;
    return sgm;
}

static segment_t* find_segment(const log_t* lg, uint64_t* offset) {
    // `offset` will be modified in case of overflow to next segment.

    const uint64_t curr_offset = segment_offset(lg->curr);

    if (lg->prev) {
        const uint64_t prev_offset = segment_offset(lg->prev);
        const uint64_t prev_roffset = segment_roffset(lg->prev);

        if (prev_offset <=  *offset && *offset < curr_offset) {
            if (*offset < prev_roffset) {
                return lg->prev;

            } else {
                *offset = curr_offset;
            }
        }
    }

    if (curr_offset <= *offset) {
        return lg->curr;
    }

    return NULL;
}

log_t* log_open(const char* dir, size_t size) {
    // Size has to be a multiple of page size.
    if (size % pagesize() != 0) {
        return NULL;
    }

    if (ensure_directory(dir) != 0) {
        return NULL;
    }

    struct log* lg = (struct log*)malloc(sizeof(struct log));
    if (!lg) {
        return NULL;
    }

    // Initialize segment struct.
    bzero(lg, sizeof(struct log));
    lg->size = size;
    lg->offset = 0;
    strncpy(lg->dir, dir, MAX_DIR_SIZE);

    // prev will stay NULL for now.
    lg->curr = next_segment(lg);
    if (lg->curr == NULL) {
        goto error;
    }

    lg->empty = next_segment(lg);
    if (lg->empty == NULL) {
        goto error;
    }

    return lg;

error:
    log_close(lg);

    return NULL;
}

int log_close(log_t* lg) {
    int errors = 0;
    if (lg->prev) {
        if (segment_close(lg->prev) != 0) {
            ++errors;
        }
    }
    if (lg->curr) {
        if (segment_close(lg->curr) != 0) {
            ++errors;
        }
    }
    if (lg->empty) {
        if (segment_close(lg->empty) != 0) {
            ++errors;
        }
    }

    free(lg);
    return errors == 0 ? 0 : -1;
}

int log_destroy(log_t* lg) {
    if (delete_directory(lg->dir) != 0) {
        return -1;
    }

    return log_close(lg);
}

ssize_t log_write(log_t* lg, const void* buf, size_t size) {
    ssize_t written = segment_write(lg->curr, buf, size);
    if (written == -1) {
        // segment has no capacity left
        // TODO: use error code.
        lg->prev = lg->curr;
        lg->curr = lg->empty;
        lg->empty = next_segment(lg);

        return segment_write(lg->curr, buf, size);
    }

    return written;
}

ssize_t log_read(const log_t* lg, uint64_t offset, struct frame* fr) {
    const segment_t* sgm = find_segment(lg, &offset);
    if (sgm == NULL) {
        return -1;
    }

    // TODO, if size is poweroff two, a bitmask can be used.
    uint64_t relative_offset = offset % lg->size;

    return segment_read(sgm, relative_offset, fr);
}
