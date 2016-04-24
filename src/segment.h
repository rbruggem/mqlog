#ifndef SEGMENT_H_
#define SEGMENT_H_

#include <prot.h>
#include <sys/types.h>
#include <stdlib.h>
#include <stdint.h>

typedef struct segment segment_t;

/* non thread safe functions */
segment_t*  segment_open(const char*, uint64_t, size_t);
int         segment_close(segment_t*);

/* thread safe functions */
ssize_t     segment_write(segment_t*, const void*, size_t);
int         segment_read(const segment_t*, uint64_t, struct frame*);

int         segment_destroy(const char*);

#endif
