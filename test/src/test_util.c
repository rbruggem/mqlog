#include "test_util.h"
#include <util.h>
#include <ftw.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

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

// credit to
// https://www.quora.com/How-do-you-write-a-C-program-to-split-a-string-by-a-delimiter
char** strsplit(int* numtokens, const char* str, const char* delim) {
    // copy the original string so that we don't overwrite parts of it
    // (don't do this if you don't need to keep the old line,
    // as this is less efficient)
    char *s = strdup(str);
    // these three variables are part of a very common idiom to
    // implement a dynamically-growing array
    size_t tokens_alloc = 1;
    size_t tokens_used = 0;
    char **tokens = calloc(tokens_alloc, sizeof(char*));
    char *token, *strtok_ctx;
    for (token = strtok_r(s, delim, &strtok_ctx);
            token != NULL;
            token = strtok_r(NULL, delim, &strtok_ctx)) {
        // check if we need to allocate more space for tokens
        if (tokens_used == tokens_alloc) {
            tokens_alloc *= 2;
            tokens = realloc(tokens, tokens_alloc * sizeof(char*));
        }
        tokens[tokens_used++] = strdup(token);
    }
    // cleanup
    if (tokens_used == 0) {
        free(tokens);
        tokens = NULL;
    } else {
        tokens = realloc(tokens, tokens_used * sizeof(char*));
    }
    *numtokens = tokens_used;
    free(s);
    return tokens;
}

int str_in_array(const char* arr[], int len, const char* str) {
    for (int i = 0; i < len; ++i) {
        if (strncmp(arr[i], str, strlen(str)) == 0) {
            return 1;
        }
    }
    return 0;
}
