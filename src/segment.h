#ifndef MQLOG_SEGMENT_H_
#define MQLOG_SEGMENT_H_

#include <prot.h>
#include <sys/types.h>
#include <stdlib.h>
#include <stdint.h>

typedef struct segment segment_t;


#define SGM_RDDRT 0x0
#define SGM_RDCMT 0x1

/* non thread safe functions */
int         segment_open(segment_t**,
                         const char*,
                         uint64_t,
                         uint32_t,
                         unsigned int);
int         segment_close(segment_t*);

uint64_t    segment_base_offset(const segment_t*);
uint64_t    segment_write_offset(const segment_t*);
uint64_t    segment_read_offset(const segment_t*);

/* thread safe functions */
ssize_t     segment_write(segment_t*, const void*, size_t);
ssize_t     segment_read(const segment_t*, uint64_t, struct frame*);
ssize_t     segment_sync(segment_t*);

#endif
