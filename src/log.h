#ifndef LOG_H_
#define LOG_H_

#include <sys/types.h>
#include <prot.h>
#include <logerrno.h>

#define LOG_RDDRT 0x0
#define LOG_RDCMT 0x1

typedef struct log log_t;

/* non thread safe functions */
int     log_open(log_t**, const char*, size_t, unsigned int);
int     log_close(log_t*);
int     log_destroy(log_t*);

/* thread safe functions */
ssize_t log_write(log_t*, const void*, size_t);
ssize_t log_read(const log_t*, uint64_t*, struct frame*);
ssize_t log_sync(const log_t*);

#endif
