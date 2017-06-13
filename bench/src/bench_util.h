#ifndef MQLOG_BENCH_UTIL_H
#define MQLOG_BENCH_UTIL_H

#include <stddef.h>
#include <time.h>

unsigned char* random_block(size_t);
void print_report(const char*, struct timespec, size_t, size_t);

#endif
