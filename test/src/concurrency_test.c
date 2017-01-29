#include "testfw.h"
#include "test_util.h"
#include <pthread.h>
#include <mqlog.h>
#include <util.h>
#include <string.h>
#include <assert.h>

struct string {
    size_t len;
    char   str[128];
};

static char* rand_string(char* str, size_t size) {
    const char charset[] = "abcdefghijklmnopqrstuvwxyz";
    if (size) {
        --size;
        for (size_t n = 0; n < size; n++) {
            int key = rand() % (int)(sizeof charset - 1);
            str[n] = charset[key];
        }
        str[size] = '\0';
    }
    return str;
}

struct thread_args {
    mqlog_t*        lg; struct string data[128];
};

static void* producer(void* arg) {
    struct thread_args* args = (struct thread_args*)arg;
    mqlog_t* lg = (mqlog_t*)args->lg;

    srand(time(NULL));

    for (size_t i = 0; i < 128; ++i) {
        struct string* sstr = &args->data[i];
        sstr->len = rand() % 128 + 1;
        rand_string(sstr->str, sstr->len);

        if (mqlog_write(lg, sstr->str, sstr->len) < 0) {
            --i;
        }
    }

    return NULL;
}

static void* consumer(void* arg) {
    struct thread_args* args = (struct thread_args*)arg;
    mqlog_t* lg = (mqlog_t*)args->lg;

    uint64_t offset = 0;
    struct frame fr;

    for (size_t i = 0; i < 128; ++i) {
        ssize_t read = mqlog_read(lg, offset, &fr);

        if (read == ELNORD || read == ELINVHD || read == ELLOCK) {
            sched_yield();
            --i;
            continue;
        }

        assert(read > 0);

        struct string* sstr = &args->data[i];
        sstr->len = frame_payload_size(&fr);
        memcpy(sstr->str, fr.buffer, sstr->len);
        ++offset;
    }

    return NULL;
}

TEST(simple_concurrency_test) {
    const size_t size = 4096;
    const char* dir = "/tmp/simple_concurrency_test";

    delete_directory(dir);

    mqlog_t* lg = NULL;
    int rc = mqlog_open(&lg, dir, size, 0);
    ASSERT(rc == 0);
    ASSERT(lg);

    struct thread_args prod_args = {
        .lg = lg
    };

    struct thread_args cons_args = {
        .lg = lg
    };

    pthread_t prod0, cons0;

    rc = pthread_create(&prod0, NULL, producer, (void*)&prod_args);
    ASSERT(rc == 0);

    rc = pthread_create(&cons0, NULL, consumer, (void*)&cons_args);
    ASSERT(rc == 0);

    rc = pthread_join(prod0, NULL);
    ASSERT(rc == 0);

    rc = pthread_join(cons0, NULL);
    ASSERT(rc == 0);

    for (int i = 0; i < 128; ++i) {
        ASSERT(prod_args.data[i].len == cons_args.data[i].len);
        ASSERT(strncmp(prod_args.data[i].str,
               cons_args.data[i].str,
               prod_args.data[i].len) == 0);
    }

    ASSERT(mqlog_close(lg) == 0);
}
