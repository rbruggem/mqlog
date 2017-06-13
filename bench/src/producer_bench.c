#include "bench_util.h"
#include <stdlib.h>
#include <mqlog.h>
#include <util.h>

int producer_bench(size_t segment_size, size_t num, size_t size) {
    unsigned char* block = random_block(size);
    if (!block) {
        return -1;
    }

    const char* dir = "/tmp/producer_bench";
    delete_directory(dir);


    mqlog_t* lg = NULL;
    int rc = mqlog_open(&lg, dir, segment_size, 0);
    if (rc != 0) {
        return -1;
    }

    struct timespec tsr, tsb, tse;
    if (clock_gettime(CLOCK_REALTIME, &tsb) < 0) {
        return -1;
    }

    for (size_t i = 0; i < num; ++i) {
        const ssize_t written = mqlog_write(lg, block, size);
        if ((size_t)written != size) {
            return -1;
        }
    }

    if (clock_gettime(CLOCK_REALTIME, &tse) < 0) {
        return -1;
    }

    tsr.tv_sec = tse.tv_sec - tsb.tv_sec;
    tsr.tv_nsec = tse.tv_nsec - tsb.tv_nsec;

    print_report(__func__, tsr, num, num * size);

    mqlog_close(lg);
    free(block);

    return 0;
}
