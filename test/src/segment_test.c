#include "testfw.h"
#include "test_util.h"
#include <util.h>
#include <segment.h>
#include <mqlogerrno.h>
#include <string.h>

TEST(segment_write_read) {
    const size_t size = 10485760; // 10 MB
    const char* dir = "/tmp/segment_write_read";

    ASSERT(delete_directory(dir) == 0);

    segment_t* sgm = NULL;
    int rc = segment_open(&sgm, dir, 0, size, 0);
    ASSERT(rc == 0);
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

    size_t payload_size = frame_payload_size(&fr);
    ASSERT(payload_size == str_size);
    ASSERT(strncmp((const char*)fr.buffer, str, payload_size) == 0);

    ++offset;
    read = segment_read(sgm, offset, &fr);
    ASSERT((size_t)read == str2_size);

    payload_size = frame_payload_size(&fr);
    ASSERT(payload_size == str2_size);
    ASSERT(strncmp((const char*)fr.buffer, str2, payload_size) == 0);

    ASSERT(segment_close(sgm) == 0);
}

TEST(segment_write_close_open_read) {
    const size_t size = 10485760; // 10 MB
    const char* dir = "/tmp/segment_write_close_open_read/";

    ASSERT(delete_directory(dir) == 0);

    segment_t* sgm = NULL;
    int rc = segment_open(&sgm, dir, 0, size, 0);
    ASSERT(rc == 0);
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

    sgm = NULL;
    rc = segment_open(&sgm, dir, 0, size, 0);
    ASSERT(rc == 0);
    ASSERT(sgm);

    uint64_t offset = 0;
    struct frame fr;

    ssize_t read = segment_read(sgm, offset, &fr);
    ASSERT((size_t)read == n_size);

    ++offset;
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

    ASSERT(segment_close(sgm) == 0);
}

TEST(segment_write_no_capacity) {
    const size_t size = 4096;
    const char* dir = "/tmp/segment_write_no_capacity";

    ASSERT(delete_directory(dir) == 0);

    segment_t* sgm = NULL;
    int rc =  segment_open(&sgm, dir, 0, size, 0);
    ASSERT(rc == 0);
    ASSERT(sgm);

    const size_t block0_size = 3000;
    unsigned char block0[block0_size];
    memset(block0, 0, block0_size);
    ssize_t written = segment_write(sgm, block0, block0_size);
    ASSERT((size_t)written == block0_size);

    const size_t block1_size = 1000;
    written = segment_write(sgm, block0, block1_size);
    ASSERT(written == 1000);

    const size_t block2_size = 1100;
    written = segment_write(sgm, block0, block2_size);
    ASSERT(written == ELEOS);

    const size_t block3_size = 5;
    written = segment_write(sgm, block0, block3_size);
    ASSERT(written == ELEOS);

    // segment full
    const size_t block4_size = 3000;
    written = segment_write(sgm, block0, block4_size);
    ASSERT(written == ELEOS);

    ASSERT(segment_close(sgm) == 0);
}
