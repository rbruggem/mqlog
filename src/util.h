#ifndef MQLOG_UTIL_H_
#define MQLOG_UTIL_H_

#include <inttypes.h>
#include <stdlib.h>
#include <sys/types.h>

#define max(x, y) ((x) > (y) ? (x) : (y))
#define min(x, y) ((x) < (y) ? (x) : (y))

int          pow2(uint64_t);
size_t       pagesize() __attribute__((const));
int          file_exists(const char*);
ssize_t      file_size(const char*);
size_t       page_aligned_addr(size_t);
int          ensure_directory(const char*);
int          append_file_to_dir(char*, size_t, const char*, const char*);
int          has_suffix(const char*, const char*);

#ifdef LOG_DEBUG
#include <stdio.h>
#define LOG_PRINT(...) printf(__VA_ARGS__)
#else
#define LOG_PRINT(...) do {} while (0);
#endif

#ifdef UNUSED
#elif defined(__GNUC__)
#define UNUSED(x) UNUSED_ ## x __attribute__((unused))
#elif defined(__LCLINT__)
#define UNUSED(x) /*@unused@*/ x
#else
#define UNUSED(x) x
#endif

#endif
