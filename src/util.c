#include "util.h"
#include <unistd.h>
#include <stdio.h>
#include <sched.h>
#include <sys/stat.h>
#include <string.h>

size_t pagesize() {
    return sysconf(_SC_PAGESIZE);
}

int file_exists(const char* filename) {
    struct stat st;
    int i = stat(filename, &st);
    if (i == 0) {
        return 1;
    }
    return 0;
}

ssize_t file_size(const char* filename) {
    struct stat st;

    if (stat(filename, &st) == 0) {
        return st.st_size;
    }

    return -1;
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

int has_suffix(const char* str, const char* suffix) {
    if (!str || !suffix) {
        return 0;
    }
    size_t lenstr = strlen(str);
    size_t lensuffix = strlen(suffix);
    if (lensuffix >  lenstr) {
        return 0;
    }

    return strncmp(str + lenstr - lensuffix, suffix, lensuffix) == 0;
}
