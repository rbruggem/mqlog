#include "bench_util.h"
#include <string.h>
#include <stdio.h>
#include <util.h>

unsigned char* random_block(size_t size) {
    // assume `rand()` has been seeded
    unsigned char* block = calloc(1, size);
    if (!block) {
        return NULL;
    }

    for (size_t i = 0; i < size; ++i) {
        block[i] = rand();
    }

    return block;
}

void print_report(const char* bench, struct timespec ts, size_t ops, size_t size) {
    const long double den = (long double)ts.tv_sec +
                            (long double)ts.tv_nsec / 1000000000.0L;
    const long double ops_throughput = (long double)ops / den;

    // `size` is in bytes, but will be reported in MB
    const long double disk_throughput = (long double)size / den / (1 << 20);

    printf("%-25s %4Leops/sec, %4LeMB/sec\n",
           bench, ops_throughput,
           disk_throughput);
}

