#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <util.h>

int producer_bench(size_t, size_t, size_t);

void err(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, fmt, args);
    va_end(args);
    exit(1);
}

int main(int argc, char* argv[]) {
    srand((unsigned)time(0));

    int lim = file_limit();
    if (lim < 4096) {
        fprintf(stderr, "Recommended file ulimit >= 4096, current %d\n", lim);
    }

    size_t segment_size = 524288000;  // default to 512MB
    size_t num = 50000000;  // default to 50M
    size_t size = 100;  // default to 100bytes

    char c;
    char* benchmark = NULL;
    while ((c = getopt(argc, argv, "b:n:s:")) != -1) {
        switch (c) {
            case 'b':
                benchmark = optarg;
                break;
            case 'n':
                num = atoi(optarg);
                break;
            case 's':
                size = atoi(optarg);
                break;
            case '?':
                err("Unknown option character `\\x%x'.\n", optopt);
        }
    }

    if (!benchmark) {
        err("Benchmark type option not specified -- 'b'\n");
    }

    if (strncmp(benchmark, "producer_bench", strlen("producer_bench")) == 0) {
        if (producer_bench(segment_size, num, size) != 0) {
            err("producer_bench test failed\n");
        }
    }

    return 0;
}
