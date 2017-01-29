#include "testfw.h"
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

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

int testfw_run(const char* test_name) {
    int total_errors = 0;
    for (struct node* elem = head; elem != 0; elem = elem->next) {
        int errors = 0;

        // run test if test_name is NULL or test_name is the name of the test
        if ((test_name &&
            strncmp(elem->test_name, test_name, strlen(elem->test_name)) == 0) ||
            test_name == NULL) {

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

    char* test_name = NULL;
    if (argc == 2) {
        test_name = argv[1];
    }

    const int errors = testfw_run(test_name);

    testfw_free();

    return errors;
}
