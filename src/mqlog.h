#ifndef MQLOG_MQLOG_H_
#define MQLOG_MQLOG_H_

#include <sys/types.h>
#include <prot.h>
#include <mqlogerrno.h>

#define MQLOG_RDDRT 0x0
#define MQLOG_RDCMT 0x1

typedef struct mqlog mqlog_t;

/* non thread safe functions */
int     mqlog_open(mqlog_t**, const char*, size_t, unsigned int);
int     mqlog_close(mqlog_t*);

/* thread safe functions */
ssize_t mqlog_write(mqlog_t*, const void*, size_t);
ssize_t mqlog_read(const mqlog_t*, uint64_t, struct frame*);
ssize_t mqlog_sync(const mqlog_t*);

#endif
