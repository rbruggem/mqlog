#include "mqlog.h"
#include "segment.h"
#include "util.h"
#include "btree.h"
#include <string.h>
#include <dirent.h>
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>

enum { BRANCH_FACTOR = 7 };
enum { MAX_DIR_SIZE = 1024 };

struct mqlog {
    size_t          size;
    unsigned int    flags;
    char            dir[MAX_DIR_SIZE];
    btree_t*        index;
    pthread_mutex_t lock;
};

static int create_segment(segment_t** sgm, uint64_t base_offset, mqlog_t* lg) {
    // TODO: this is not thread safe.

    unsigned int flags = SGM_RDDRT;
    if ((lg->flags & MQLOG_RDCMT) == MQLOG_RDCMT) {
        flags = SGM_RDCMT;
    }

    int rc = segment_open(sgm, lg->dir, base_offset, lg->size, flags);
    if (rc != 0) {
        return rc;
    }

    return 0;
}

static ssize_t mqlog_write_try(mqlog_t* lg, const void* buf, size_t size) {
    // TODO: this is not thread safe.
    const struct btree_node_data* node_data = btree_max(lg->index);
    segment_t* sgm = NULL;
    unsigned int new_segment = 0;
    if (node_data) {
        sgm = node_data->value;
    } else {
        // This is the first segment
        new_segment = 1;
        int rc = create_segment(&sgm, 0, lg);
        if (rc != 0) {
            return rc;
        }
    }

    // TODO Handle ELLOCK
    ssize_t written = segment_write(sgm, buf, size);
    if (written == ELEOS && new_segment) {
        // this means the the new frame is greater than the
        // entire segment
        segment_close(sgm);
        return ELNOWCP;
    }

    if (written == ELEOS) {
        // segment has no capacity left
        assert(new_segment == 0);
        new_segment = 1;
        const uint64_t new_base_offset = segment_write_offset(sgm);
        int rc = create_segment(&sgm, new_base_offset, lg);
        if (rc != 0) {
            // TODO add return code
            return rc;
        }

        // TODO Handle ELLOCK
        written = segment_write(sgm, buf, size);
        if (written == ELEOS) {
            // Only attempt to write twice.
            // This point is reached if a payload greater than
            // the segment size is being inserted.
            // Handling payloads greater than the segment size
            // is currently not supported.
            segment_close(sgm);
            return ELNOWCP;
        }
    } else {

    }

    if (new_segment) {
        const uint64_t base_offset = segment_base_offset(sgm);
        if (btree_insert(lg->index, base_offset, sgm) != 0) {
            segment_close(sgm);
            return ELIDXOP;
        }
    }

    return written;
}

static ssize_t mqlog_read_try(const mqlog_t* lg,
                            uint64_t offset,
                            struct frame* fr) {
    // Find the segment the offset is located.
    // This can return `prev` or `curr` segment.
    btree_iterator_t* iter = btree_iterator_find_le(lg->index, offset);
    if (!btree_iterator_valid(iter)) {
        free(iter);
        return ELNORD;
    }

    const struct btree_node_data* node_data = btree_iterator_data(iter);
    const segment_t* sgm = (const segment_t*)node_data->value;

    free(iter);

    uint64_t base_offset = segment_base_offset(sgm);
    uint64_t relative_offset = offset - base_offset;

    return segment_read(sgm, relative_offset, fr);
}

static int load_segments(mqlog_t* lg) {
    DIR* d = opendir(lg->dir);
    if (d) {
        struct dirent *dir;
        while ((dir = readdir(d)) != NULL) {
            // TODO: don't hardcode `.log`
            if (has_suffix(dir->d_name, ".log")) {
                char str[MAX_DIR_SIZE];
                snprintf(str, MAX_DIR_SIZE, "%s/%s", lg->dir, dir->d_name);

                ssize_t size = file_size(str);
                if (size == -1) {
                    return ELLDSGM;
                }

                snprintf(str, MAX_DIR_SIZE, "%s", dir->d_name);

                uint64_t offset = strtoll(str, NULL, 10);

                segment_t* sgm = 0;
                int rc = segment_open(&sgm, lg->dir, offset, size, lg->flags);
                if (rc != 0) {
                    return ELLDSGM;
                }

                if (btree_insert(lg->index, offset, sgm) != 0) {
                    segment_close(sgm);
                    return ELLDSGM;
                }
            }
        }
    }
    closedir(d);

    return 0;
}

int mqlog_open(mqlog_t** lg_ptr,
             const char* dir,
             size_t size,
             unsigned int flags) {

    // Size has to be a multiple of page size.
    if (size % pagesize() != 0) {
        return ELNOPGM;
    }

    if (ensure_directory(dir) != 0) {
        return ELLGDIR;
    }

    struct mqlog* lg = (struct mqlog*)malloc(sizeof(struct mqlog));
    if (!lg) {
        return ELALLC;
    }

    // Initialize segment struct.
    memset(lg, 0, sizeof(struct mqlog));
    lg->size = size;
    snprintf(lg->dir, MAX_DIR_SIZE, "%s", dir);

    lg->flags = flags;

    lg->index = btree_init(BRANCH_FACTOR);
    if (!lg->index) {
        mqlog_close(lg);
        return ELIDXCR;
    }

    if (pthread_mutex_init(&lg->lock, NULL)) {
        mqlog_close(lg);
        return ELLCKOP;
    }

    load_segments(lg);

    *lg_ptr = lg;

    return 0;
}

int mqlog_close(mqlog_t* lg) {
    int errors = 0;

    if (lg->index) {
        btree_iterator_t* iter = btree_iterator_head(lg->index);
        for (; btree_iterator_valid(iter); iter = btree_iterator_next(iter)) {

            const struct btree_node_data* node_data = btree_iterator_data(iter);
            segment_t* sgm = (segment_t*)node_data->value;
            if (segment_close(sgm) != 0) {
                ++errors;
            }
        }

        free(iter);

        if (btree_free(lg->index) != 0) {
            ++errors;
        }
    }

    pthread_mutex_destroy(&lg->lock);

    free(lg);
    return errors == 0 ? 0 : ELLGCLS;
}

ssize_t mqlog_write(mqlog_t* lg, const void* buf, size_t size) {
    if (size == 0) {
        return 0;
    }

    if (pthread_mutex_trylock(&lg->lock) != 0) {
        return errno == EBUSY ? ELLOCK : ELLCKOP;
    }

    ssize_t written = mqlog_write_try(lg, buf, size);

    if (pthread_mutex_unlock(&lg->lock) != 0) {
        return ELLCKOP;
    }

    return written;
}

ssize_t mqlog_read(const mqlog_t* lg, uint64_t offset, struct frame* fr) {
    ssize_t read = mqlog_read_try(lg, offset, fr);
    return read;
}

ssize_t mqlog_sync(const mqlog_t* lg) {
    // TODO this only syncs the last segment
    const struct btree_node_data* node_data = btree_max(lg->index);
    if (node_data) {
        return segment_sync((segment_t*)node_data->value);
    }

    return 0;
}
