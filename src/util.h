#ifndef UTIL_H_
#define UTIL_H_

#include <inttypes.h>
#include <stdlib.h>

int          pow2(uint64_t);
size_t       pagesize() __attribute__((const));
int          file_exists(const char*);
size_t       page_aligned_addr(size_t);
int          ensure_directory(const char*);
int          append_file_to_dir(char*, size_t, const char*, const char*);
unsigned int crc32(unsigned char*, size_t);

#ifdef LOG_DEBUG
#include <stdio.h>
#define PRINT(...) printf(__VA_ARGS__)
#else
#define PRINT(x) do {} while (0);
#endif

#endif
