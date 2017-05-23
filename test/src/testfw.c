#include "testfw.h"
#include "test_util.h"
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

enum {ALL, INCLUDE, EXCLUDE};

struct node {
    void         (*test_fn)(int*);
    const char*  test_name;
    struct node* next;
};

static struct node* head = NULL;
static struct node* tail = NULL;

void testfw_add(void (*test_fn)(int*), const char* test_name) {
    struct node* elem = (struct node*)malloc(sizeof(struct node));
    memset(elem, 0, sizeof(struct node));
    elem->test_fn = test_fn;
    elem->test_name = test_name;

    if (tail) {
        tail->next = elem;
    } else {
        head = elem;
    }
    tail = elem;
}

void testfw_free() {
    struct node* node = head;
    while (node) {
        struct node* node_to_delete = node;
        node = node->next;
        free(node_to_delete);
    }
}

int testfw_run(int mode, const char** test_names, int len) {
    int total_errors = 0;
    for (struct node* elem = head; elem != 0; elem = elem->next) {
        int errors = 0;

        if (
            mode == ALL ||
            (mode == INCLUDE &&
                str_in_array(test_names, len, elem->test_name)) ||
            (mode == EXCLUDE && !str_in_array(test_names, len, elem->test_name))
        ) {

            struct timespec tstart={0,0}, tend={0,0};
            clock_gettime(CLOCK_MONOTONIC, &tstart);

            elem->test_fn(&errors);

            clock_gettime(CLOCK_MONOTONIC, &tend);

            const double sec = ((double)tend.tv_sec + 1.0e-9*tend.tv_nsec) -
                               ((double)tstart.tv_sec + 1.0e-9*tstart.tv_nsec);

            if (errors == 0) {
                PASSED(elem->test_name, sec);
            }
        }

        total_errors += errors;
    }

    return total_errors;
}

int main(int argc, char* argv[]) {
    srand((unsigned)time(0));

    char** test_names = NULL;
    char* value = NULL;
    char c;
    int mode = ALL;
    while ((c = getopt (argc, argv, "i:e:")) != -1) {
        switch (c) {
            case 'i':
                value = optarg;
                mode = INCLUDE;
                break;
            case 'e':
                // only one between 'e' and 'i' applies
                value = optarg;
                mode = EXCLUDE;
                break;
            case '?':
                if (optopt == 'i' || optopt == 'e') {
                    fprintf(stderr,
                            "Option -%c requires an argument.\n",
                            optopt);
                } else {
                    fprintf(stderr,
                            "Unknown option character `\\x%x'.\n",
                            optopt);
                }
                return 1;
        }
    }

    int len = 0;
    if (value) {
        test_names = strsplit(&len, value, ",");
    }

    const int errors = testfw_run(mode, (const char**)test_names, len);

    if (test_names) {
        for (int i = 0; i < len; ++i) {
            free(test_names[i]);
        }
        free(test_names);
    }

    testfw_free();

    return errors;
}
