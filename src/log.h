#ifndef LOG_H_
#define LOG_H_

#include <prot.h>
#include <sys/types.h>
#include <stdlib.h>
#include <stdint.h>

typedef struct log log_t;

/* non thread safe functions */
log_t*  log_init(const char*, uint64_t, size_t);
int     log_close(log_t*);

/* thread safe functions */
ssize_t log_write(log_t*, const void*, size_t);
int log_read(const log_t*, uint64_t, struct frame*);

int log_destroy(const char*);

#endif
