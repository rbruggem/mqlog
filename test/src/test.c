#include <log.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>

int test_write() {
    const size_t size = 10485760; // 10 MB
    log_t* lg = log_init("/tmp/test", 0, size);
    if (!lg) {
        printf("Failed to create log\n");
        return -1;
    }

    const char* str = "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Morbi euismod purus ante, eget vestibulum est semper a. Mauris quis leo in elit tristique ultrices vel ut nisl. Quisque eget ante fermentum, tristique ex id, gravida mauris. Vestibulum non nisl sagittis, pharetra lacus ut, viverra neque. Pellentesque ut porttitor enim. Nulla ut tempus risus, et suscipit nunc. Proin est odio, mattis nec faucibus sed, suscipit non nulla. Quisque vel gravida enim. Fusce vel quam id lorem lobortis condimentum. Vestibulum a metus eros. Pellentesque iaculis feugiat lacus eu condimentum. Curabitur nec enim placerat, sodales sem non, suscipit nulla. Nunc viverra vehicula odio, ac tempus turpis vulputate consectetur. Mauris rhoncus dolor vitae elit tincidunt pulvinar. Integer egestas pulvinar metus sit amet tincidunt.";
    ssize_t w = log_write(lg, str, strlen(str));
    printf("written %zu bytes\n", w);

    const char* str2 = "what's up?";
    w = log_write(lg, str2, strlen(str2));
    printf("written %zu bytes\n", w);

    log_close(lg);
    return 0;
}

int test_read() {
    const size_t size = 10485760; // 10 MB
    log_t* lg = log_init("/tmp/test", 0, size);
    if (!lg) {
        printf("Failed to create log\n");
        return -1;
    }

    uint64_t offset = 0;
    struct frame fr;
    while (log_read(lg, offset, &fr) == 0) {
        offset += fr.hdr->size;
        size_t payload_size = frame_payload_size(&fr);
        printf("> %"PRIu64": %.*s\n", offset, (int)payload_size, fr.buffer);
    }

    log_close(lg);
    return 0;
}

int main() {
    test_write();
    test_read();

    return 0;
}
