#include "testfw.h"
#include <segment.h>
#include <log.h>
#include <util.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>

TEST(test_segment_write_read) {
    const size_t size = 10485760; // 10 MB
    const const char* dir = "/tmp/test_segment_write_read";

    segment_t* sgm = segment_open(dir, 0, size);
    ASSERT(sgm);

    const char* str = "Lorem ipsum dolor sit amet, etc ...";
    const size_t str_size = strlen(str);
    ssize_t written = segment_write(sgm, str, str_size);
    ASSERT(str_size == (size_t)written);

    const char* str2 = "what's up?";
    const size_t str2_size = strlen(str2);
    written = segment_write(sgm, str2, str2_size);
    ASSERT(str2_size == (size_t)written);

    uint64_t offset = 0;
    struct frame fr;

    ssize_t read = segment_read(sgm, offset, &fr);
    ASSERT((size_t)read == str_size);

    offset += fr.hdr->size;
    size_t payload_size = frame_payload_size(&fr);
    ASSERT(payload_size == str_size);
    ASSERT(strncmp((const char*)fr.buffer, str, payload_size) == 0);

    read = segment_read(sgm, offset, &fr);
    ASSERT((size_t)read == str2_size);

    offset += fr.hdr->size;
    payload_size = frame_payload_size(&fr);
    ASSERT(payload_size == str2_size);
    ASSERT(strncmp((const char*)fr.buffer, str2, payload_size) == 0);

    ASSERT(segment_close(sgm) == 0);

    ASSERT(delete_directory(dir) == 0);
}

TEST(test_segment_write_close_open_read) {
    const size_t size = 10485760; // 10 MB
    const const char* dir = "/tmp/test_segment_write_close_open_read/";

    segment_t* sgm = segment_open(dir, 0, size);
    ASSERT(sgm);

    const int n = 14434;
    const size_t n_size = sizeof(n);
    ssize_t written = segment_write(sgm, &n, n_size);
    ASSERT(n_size == (size_t)written);

    const double d = 45435.2445;
    const size_t d_size = sizeof(d);
    written = segment_write(sgm, &d, d_size);
    ASSERT(d_size == (size_t)written);

    ASSERT(segment_close(sgm) == 0);

    sgm = segment_open(dir, 0, size);
    ASSERT(sgm);

    uint64_t offset = 0;
    struct frame fr;

    ssize_t read = segment_read(sgm, offset, &fr);
    ASSERT((size_t)read == n_size);

    offset += fr.hdr->size;
    size_t payload_size = frame_payload_size(&fr);
    ASSERT(payload_size == n_size);
    int n_read = *(int*)fr.buffer;
    ASSERT(n == n_read);

    read = segment_read(sgm, offset, &fr);
    ASSERT((size_t)read == d_size);
    payload_size = frame_payload_size(&fr);
    ASSERT(payload_size == d_size);
    double d_read = *(double*)fr.buffer;
    ASSERT(d == d_read);

    offset += fr.hdr->size;

    ASSERT(segment_close(sgm) == 0);

    ASSERT(delete_directory(dir) == 0);
}

TEST(test_segment_write_no_capacity) {
    const size_t size = 4096;
    const char* dir = "/tmp/test_segment_write_no_capacity";

    segment_t* sgm = segment_open(dir, 0, size);
    ASSERT(sgm);

    const size_t block0_size = 3000;
    unsigned char block0[block0_size];
    ssize_t written = segment_write(sgm, block0, block0_size);
    ASSERT((size_t)written == block0_size);

    const size_t block1_size = 2000;
    written = segment_write(sgm, block0, block1_size);
    ASSERT(written == -1);

    const size_t block2_size = 1055;
    written = segment_write(sgm, block0, block2_size);
    ASSERT(written == 1055);

    const size_t block3_size = 5;
    written = segment_write(sgm, block0, block3_size);
    ASSERT(written == 5);

    // segment full
    const size_t block4_size = 1;
    written = segment_write(sgm, block0, block4_size);
    ASSERT(written == -1);

    ASSERT(segment_close(sgm) == 0);

    ASSERT(delete_directory(dir) == 0);
}

TEST(test_log_write_read) {
    const size_t size = 1048576; // 1 MB
    const const char* dir = "/tmp/test_log_write_read";

    log_t* lg = log_open(dir, size);
    ASSERT(lg);

    const char* str = "d dfmfo}ädaq 2";
    const size_t str_size = strlen(str);
    ssize_t written = log_write(lg, str, str_size);
    ASSERT(str_size == (size_t)written);

    const char* str2 = "asdasd d33?";
    const size_t str2_size = strlen(str2);
    written = log_write(lg, str2, str2_size);
    ASSERT(str2_size == (size_t)written);

    uint64_t offset = 0;
    struct frame fr;

    ssize_t read = log_read(lg, offset, &fr);
    ASSERT((size_t)read == str_size);

    offset += fr.hdr->size;
    size_t payload_size = frame_payload_size(&fr);
    ASSERT(payload_size == str_size);
    ASSERT(strncmp((const char*)fr.buffer, str, payload_size) == 0);

    read = log_read(lg, offset, &fr);
    ASSERT((size_t)read == str2_size);

    offset += fr.hdr->size;
    payload_size = frame_payload_size(&fr);
    ASSERT(payload_size == str2_size);
    ASSERT(strncmp((const char*)fr.buffer, str2, payload_size) == 0);

    ASSERT(log_destroy(lg) == 0);
 }

TEST(test_log_write_close_open_read) {
    const size_t size = 10485760; // 10 MB
    const const char* dir = "/tmp/test_log_write_close_open_read";

    log_t* lg = log_open(dir, size);
    ASSERT(lg);

    const int n = 14434;
    const size_t n_size = sizeof(n);
    ssize_t written = log_write(lg, &n, n_size);
    ASSERT(n_size == (size_t)written);

    const double d = 45435.2445;
    const size_t d_size = sizeof(d);
    written = log_write(lg, &d, d_size);
    ASSERT(d_size == (size_t)written);

    ASSERT(log_close(lg) == 0);

    lg = log_open(dir, size);
    ASSERT(lg);

    uint64_t offset = 0;
    struct frame fr;

    ssize_t read = log_read(lg, offset, &fr);
    ASSERT((size_t)read == n_size);

    offset += fr.hdr->size;
    size_t payload_size = frame_payload_size(&fr);
    ASSERT(payload_size == n_size);
    int n_read = *(int*)fr.buffer;
    ASSERT(n == n_read);

    read = log_read(lg, offset, &fr);
    ASSERT((size_t)read == d_size);
    payload_size = frame_payload_size(&fr);
    ASSERT(payload_size == d_size);
    double d_read = *(double*)fr.buffer;
    ASSERT(d == d_read);

    offset += fr.hdr->size;

    ASSERT(log_destroy(lg) == 0);
}

TEST(test_log_write_overflow_read) {
    const size_t size = 8192;
    const const char* dir = "/tmp/test_log_write_overflow_read";

    log_t* lg = log_open(dir, size);
    ASSERT(lg);

    const char* str = "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";
    const size_t str_size = strlen(str);

    ASSERT(str_size == 3397);

    // write first time, no overflow
    ssize_t written = log_write(lg, str, str_size);
    ASSERT(str_size == (size_t)written);

    // write again, no overflow
    written = log_write(lg, str, str_size);
    ASSERT(str_size == (size_t)written);

    // write one more time, should overflow
    written = log_write(lg, str, str_size);
    ASSERT(str_size == (size_t)written);

    uint64_t offset = 0;
    struct frame fr;

    ssize_t read = log_read(lg, offset, &fr);
    ASSERT((size_t)read == str_size);

    offset += fr.hdr->size;
    size_t payload_size = frame_payload_size(&fr);
    ASSERT(payload_size == str_size);
    ASSERT(strncmp((const char*)fr.buffer, str, payload_size) == 0);

    read = log_read(lg, offset, &fr);
    ASSERT((size_t)read == str_size);

    offset += fr.hdr->size;
    payload_size = frame_payload_size(&fr);
    ASSERT(payload_size == str_size);
    ASSERT(strncmp((const char*)fr.buffer, str, payload_size) == 0);

    read = log_read(lg, offset, &fr);
    ASSERT((size_t)read == str_size);

    offset += fr.hdr->size;
    payload_size = frame_payload_size(&fr);
    ASSERT(payload_size == str_size);
    ASSERT(strncmp((const char*)fr.buffer, str, payload_size) == 0);

    ASSERT(log_destroy(lg) == 0);
 }
