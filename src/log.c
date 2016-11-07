#include "log.h"
#include "segment.h"
#include "util.h"
#include <string.h>

enum { MAX_DIR_SIZE = 1024 };

struct log {
    size_t       size;
    unsigned int flags;
    uint64_t     offset;
    char         dir[MAX_DIR_SIZE];
    segment_t*   prev;
    segment_t*   curr;
    segment_t*   empty;
};

static int next_segment(segment_t** sgm, log_t* lg) {
    unsigned int flags = SGM_RDDRT;
    if ((lg->flags & LOG_RDCMT) == LOG_RDCMT) {
        flags = SGM_RDCMT;
    }

    int rc = segment_open(sgm, lg->dir, lg->offset, lg->size, flags);
    if (rc != 0) {
        return rc;
    }
    lg->offset += lg->size;
    return 0;
}

static segment_t* find_segment(const log_t* lg, uint64_t* offset) {
    // `offset` will be modified in case of overflow to next segment.

    const uint64_t absolute_curr_offset = segment_offset(lg->curr);

    // check prev segment first.
    if (lg->prev) {
        const uint64_t absolute_prev_offset = segment_offset(lg->prev);
        const uint64_t prev_roffset = segment_roffset(lg->prev);

        if (absolute_prev_offset <=  *offset &&
            *offset < absolute_curr_offset) {
            // `offset` is in the previous segment.

            if (*offset < prev_roffset) {
                return lg->prev;
            } else {
                // `offset` is within the prev segment,
                // but it is lowers then the prev segment read offset.
                // This means the prev segment has been market EOS and contains
                // padding because, when writing, the payload did not fit into
                // the available space.
                // `offset` needs to be moved to the next segment.
                *offset = absolute_curr_offset;
            }
        }
    }

    // check curr segment
    if (absolute_curr_offset <= *offset) {
        return lg->curr;
    }

    return NULL;
}

int log_open(log_t** lg_ptr, const char* dir, size_t size, unsigned int flags) {
    // Size has to be a multiple of page size.
    if (size % pagesize() != 0) {
        return ELNOPGM;
    }

    if (ensure_directory(dir) != 0) {
        return ELLGDIR;
    }

    struct log* lg = (struct log*)malloc(sizeof(struct log));
    if (!lg) {
        return ELALLC;
    }

    // Initialize segment struct.
    bzero(lg, sizeof(struct log));
    lg->size = size;
    lg->offset = 0;
    strncpy(lg->dir, dir, MAX_DIR_SIZE);

    lg->flags = flags;

    // prev will stay NULL for now.
    int rc = next_segment(&lg->curr, lg);
    if (rc != 0) {
        log_close(lg);
        return rc;
    }

    rc = next_segment(&lg->empty, lg);
    if (rc != 0) {
        log_close(lg);
        return rc;
    }

    *lg_ptr = lg;

    return 0;
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
    return errors == 0 ? 0 : ELLGCLS;
}

int log_destroy(log_t* lg) {
    if (delete_directory(lg->dir) != 0) {
        return ELLGDTR;
    }

    return log_close(lg);
}

ssize_t log_write(log_t* lg, const void* buf, size_t size) {
    ssize_t written = segment_write(lg->curr, buf, size);
    // TODO Handle ELLOCK
    if (written == ELEOS) {

        // segment has no capacity left
        // TODO: this is not thread safe.
        lg->prev = lg->curr;
        lg->curr = lg->empty;
        int rc = next_segment(&lg->empty, lg);
        if (rc != 0) {
            // TODO add return code
            return rc;
        }

        written = segment_write(lg->curr, buf, size);
        if (written == ELEOS) {
            // Only attempt to write twice.
            // This point is reached if a payload greater than
            // the segment size is being inserted.
            // Handling payloads greater than the segment size
            // is currently not supported.
            return ELNOWCP;
        }
    }

    return written;
}

ssize_t log_read(const log_t* lg, uint64_t* offset, struct frame* fr) {
    const uint64_t orig_offset = *offset;
    ssize_t read = 0;
    while (1) {
        // Find the segment the offset is located.
        // This can return `prev` or `curr` segment.
        const segment_t* sgm = find_segment(lg, offset);

        // sgm being NULL means that the segment was not found.
        // This happens when produces are quicker than consumers
        // and that the given offset is lower than lowest offset
        // the log is able to serve.
        if (sgm == NULL) {
            return ELOSLOW;
        }

        // TODO, if size is power of two, a bitmask can be used.
        uint64_t relative_offset = *offset % lg->size;

        read = segment_read(sgm, relative_offset, fr);

        // TODO: use a function
        if (read == ELEOS && fr->hdr->flags == HEADER_FLAGS_EOS) {
            *offset += segment_unused(sgm);
        } else {
            break;
        }
    }

    return read;
}

ssize_t log_sync(const log_t* lg) {
    return segment_sync(lg->curr);
}
