#include "test_util.h"
#include <util.h>
#include <ftw.h>
#include <stdio.h>

static int remove_callback(const char* file,
                           const struct stat* UNUSED(stat),
                           int UNUSED(typeflag),
                           struct FTW* UNUSED(ftwbuf)) {
    return remove(file);
}

int delete_directory(const char* dir) {
    struct stat st;
    const int max_fds = 64;
    int i = stat(dir, &st);
    if (i == 0) {
        return nftw(dir, remove_callback, max_fds, FTW_DEPTH | FTW_PHYS);
    }
    return 0;
}
