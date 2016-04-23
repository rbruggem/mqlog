#include "util.h"
#include <ftw.h>
#include <unistd.h>
#include <stdio.h>
#include <sched.h>
#include <sys/stat.h>
#include <string.h>

static int remove_callback(const char* file,
                           const struct stat* UNUSED(stat),
                           int UNUSED(typeflag),
                           struct FTW* UNUSED(ftwbuf)) {
    return remove(file);
}

int pow2(uint64_t n) {
    unsigned int one_bits = 0;
    while(n && one_bits <= 1) {
        if ((n & 1) == 1) {
            one_bits++;
        }
        n >>= 1;
    }
    return one_bits == 1;
}

size_t pagesize() {
    return sysconf(_SC_PAGESIZE);
}

int file_exists(const char* filename) {
    struct stat st = {0};
    int i = stat(filename, &st);
    if (i == 0) {
        return 1;
    }
    return 0;
}

size_t page_aligned_addr(size_t addr) {
    return (addr & ~(pagesize() - 1));
}

int ensure_directory(const char* dir) {
    if (file_exists(dir) == 0) {
        return mkdir(dir, 0700);
    }
    return 0;
}

int delete_directory(const char* dir) {
    const int max_fds = 64;
    return nftw(dir, remove_callback, max_fds, FTW_DEPTH | FTW_PHYS);
}

int append_file_to_dir(char* buf,
                       size_t len,
                       const char* dir,
                       const char* file) {
    int n = 0;
    if (dir[strlen(dir) - 1] == '/') {
        n  = snprintf(buf, len, "%s%s", dir, file);
    } else {
        n  = snprintf(buf, len, "%s/%s", dir, file);
    }

    if (n == -1) {
        return -1;
    }
    return n <= (int)len ? 0 : -1;
}


unsigned int crc32(unsigned char* message, size_t size) {
// http://www.hackersdelight.org/hdcodetxt/crc.c.txt (added boundary)
// ----------------------------- crc32b --------------------------------

/* This is the basic CRC-32 calculation with some optimization but no
table lookup. The the byte reversal is avoided by shifting the crc reg
right instead of left and by using a reversed 32-bit word to represent
the polynomial.
   When compiled to Cyclops with GCC, this function executes in 8 + 72n
instructions, where n is the number of bytes in the input message. It
should be doable in 4 + 61n instructions.
   If the inner loop is strung out (approx. 5*8 = 40 instructions),
it would take about 6 + 46n instructions. */
    int i, j;
    unsigned int byte, crc, mask;

    i = 0;
    crc = 0xFFFFFFFF;
    while(message[i] != 0 && i < (int)size) {
       byte = message[i];            // Get next byte.
       crc = crc ^ byte;
       for (j = 7; j >= 0; j--) {    // Do eight times.
          mask = -(crc & 1);
          crc = (crc >> 1) ^ (0xEDB88320 & mask);
       }
       i = i + 1;
    }
    return ~crc;
}
