#include "testfw.h"
#include "test_util.h"
#include <mqlog.h>
#include <util.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <assert.h>

enum { BUFFER_SIZE = 4096 };
enum { ARRAY_SIZE = 8192 };

struct producer_elem {
    int  idx;
    char buffer[BUFFER_SIZE];
};

struct consumer_elem {
    int  idx;
    int  size;
    const char* buffer;
};

struct producer_queues {
    int          len;
    int*         sizes;
    struct producer_elem* queue;
};

struct consumer_thread_args {
    const char* input_filename;
    char        output_filename[BUFFER_SIZE];
};

mqlog_t* lg;

static int compare_consumer_elems(const void* l, const void* r) {
    const struct consumer_elem* lhs = (const struct consumer_elem*)l;
    const struct consumer_elem* rhs = (const struct consumer_elem*)r;
    if (lhs->idx < rhs->idx) {
        return -1;
    } else if (lhs->idx == rhs->idx) {
        return 0;
    } else {
        return 1;
    }
}

static void producers_populate_from_file(const char* filename,
                                         struct producer_queues prod_queues[],
                                         int producers,
                                         int lower_bound) {
    FILE* file = fopen(filename, "r");
    if (file) {
        int producer = 0;
        int idx = 0;
        int n = 0;
        int bytes_to_read = max(rand() % BUFFER_SIZE, lower_bound);
        while (1) {
            struct producer_elem e;
            e.idx = idx;
            n = fread(e.buffer, 1, bytes_to_read, file);
            assert(n >= 0);
            e.buffer[n] = '\0';

            struct producer_queues* queues = &prod_queues[producer];
            int len = queues->len++;
            queues->queue[len] = e;
            queues->sizes[len] = n;

            bytes_to_read = rand() % BUFFER_SIZE;
            ++idx;
            producer = (producer + 1) % producers;

            if (feof(file) || ferror(file)) {
                break;
            }
        }

        fclose(file);
    }
}

static void consumer_save_to_file(const char* filename,
                                  struct consumer_elem consumer[],
                                  int len) {
    FILE* file = fopen(filename, "w");
    if (file) {
        for (int i = 0; i < len; ++i) {
            struct consumer_elem* celem = &consumer[i];
            ssize_t written = fwrite(
                (void*)celem->buffer,
                1,
                celem->size,
                file);

            assert(written == celem->size);
        }

        fclose(file);
    }
}

static int compare_input_output_files(const char* input_filename,
                                      const char* output_filename) {

    FILE* input = fopen(input_filename, "r");
    FILE* output = fopen(output_filename, "r");
    if (input && output) {
        int errors = 0;
        int i, o;
        do {
            i = getc(input);
            o = getc(output);

            if (i != o) {
                ++errors;
            }
        } while (i != EOF || o != EOF);

        fclose(input);
        fclose(output);

        return errors;
    }

    return -1;
}

static void* producer(void* arg) {
    struct producer_queues* queues = (struct producer_queues*)arg;

    for (int i = 0; i < queues->len; ++i) {
        size_t size = queues->sizes[i] + sizeof(int);
        ssize_t written = mqlog_write(lg, &queues->queue[i], size);
        if (written == ELLCKOP || written == ELLOCK) {
            --i;
            continue;
        }
        assert(written >= 0);
    }

    return NULL;
}

static void* consumer(void* arg) {
    const struct consumer_thread_args* args =
        (const struct consumer_thread_args*)arg;

    const char* input_filename = args->input_filename;
    const char* output_filename = args->output_filename;

    const ssize_t filesize = file_size(input_filename);
    assert(filesize > 0);

    uint64_t offset = 0;
    struct frame fr;
    ssize_t read = 0;

    int consumer_idx = 0;
    struct consumer_elem* consumer =
        (struct consumer_elem*)malloc(
            ARRAY_SIZE * sizeof(struct consumer_elem));

    size_t relevant_bytes_read = 0;
    while (relevant_bytes_read < (size_t)filesize) {
        read = mqlog_read(lg, offset, &fr);

        if (read > 0) {
            const struct producer_elem* pelem =
                (const struct producer_elem*)fr.buffer;

            const size_t relevant_size =
                frame_payload_size(&fr) - sizeof(int);

            struct consumer_elem celem = {
                .idx = pelem->idx,
                .size = relevant_size,
                .buffer = pelem->buffer
            };

            // double the size
            if (consumer_idx != 0 && consumer_idx % ARRAY_SIZE == 0) {
                consumer = (struct consumer_elem*)realloc(
                    consumer,
                    2 * consumer_idx * sizeof(struct consumer_elem));
            }

            consumer[consumer_idx++] = celem;
            ++offset;
            relevant_bytes_read += relevant_size;
        }
    }

    qsort(consumer,
          consumer_idx,
          sizeof(struct consumer_elem),
          compare_consumer_elems);

    consumer_save_to_file(output_filename, consumer, consumer_idx);

    int* errors = (int*)malloc(sizeof(int));
    *errors = compare_input_output_files(input_filename, output_filename);

    free(consumer);

    return errors;
}

TEST(big_test) {
    const size_t size = 100 * 1024; // 100kB
    const char* dir = "/tmp/big_test";
    const char* input_filename = "big_test_file.txt";
    const char* output_template = "/tmp/big_test/big_test_file_%d.txt";
    const int lower_bound = 1024;
    const int producers = 10;
    const int consumers = 10;

    delete_directory(dir);

    int rc = mqlog_open(&lg, dir, size, 0);
    ASSERT(rc == 0);
    ASSERT(lg);

    // initialise producer data structures
    const ssize_t filesize = file_size(input_filename);
    const int prod_queue_elems = filesize / producers / lower_bound;
    struct producer_queues prod_queues[producers];
    for (int i = 0; i < producers; ++i) {
        struct producer_queues queues = {
            .len = 0,
            .sizes = calloc(prod_queue_elems, sizeof(int)),
            .queue = calloc(prod_queue_elems, sizeof(struct producer_elem))
        };
        prod_queues[i] = queues;
    }

    producers_populate_from_file(
        input_filename,
        prod_queues,
        producers,
        lower_bound);

    pthread_t prod[producers];
    pthread_t cons[consumers];

    struct consumer_thread_args consumer_args[consumers];
    for (int i = 0; i < consumers; ++i) {
        struct consumer_thread_args args = {
            .input_filename = input_filename,
        };
        int n = snprintf(args.output_filename,
                         BUFFER_SIZE,
                         output_template,
                         i);
        ASSERT(n > 0);
        consumer_args[i] = args;
    }

    // produce in parallel
    for (int i = 0; i < producers; ++i) {
        rc = pthread_create(&prod[i], NULL, producer, (void*)&prod_queues[i]);
        ASSERT(rc == 0);
    }

    // consume in parallel
    for (int i = 0; i < consumers; ++i) {
        rc = pthread_create(&cons[i], NULL, consumer, (void*)&consumer_args[i]);
        ASSERT(rc == 0);
    }

    for (int i = 0; i < producers; ++i) {
        rc = pthread_join(prod[i], NULL);
        ASSERT(rc == 0);
    }

    int* errors[consumers];
    for (int i = 0; i < consumers; ++i) {
        rc = pthread_join(cons[i], (void*)&errors[i]);
        ASSERT(rc == 0);
    }

    for (int i = 0; i < consumers; ++i) {
        ASSERT(*errors[i] == 0);
        free(errors[i]);
    }

    for (int i = 0; i < producers; ++i) {
        free(prod_queues[i].sizes);
        free(prod_queues[i].queue);
    }

    ASSERT(mqlog_close(lg) == 0);
}
