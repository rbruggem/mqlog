#ifndef LOG_H_
#define LOG_H_

#include <sys/types.h>
#include <prot.h>

typedef struct log log_t;

/* non thread safe functions */
log_t*  log_open(const char*, size_t);
int     log_close(log_t*);
int     log_destroy(log_t*);

/* thread safe functions */
ssize_t log_write(log_t*, const void*, size_t);
ssize_t log_read(const log_t*, uint64_t, struct frame*);

#endif
