#ifndef MQLOG_MBPTREE_H_
#define MQLOG_MBPTREE_H_

#include <inttypes.h>

/*
 * Implements an immutable monotonic thread safe B+tree.
 * All keys must be monotonically increasing!
 * Deleting/updating is not supported because not needed.
 */

union mbptree_value {
    uint32_t u32;
    uint64_t u64;
    void*    addr;
};

typedef union mbptree_value mbptree_value_t;
typedef struct mbptree mbptree_t;

#define u32(x)  (mbptree_value_t){.ddr = x}
#define u64(x)  (mbptree_value_t){.u64 = x}
#define addr(x) (mbptree_value_t){.addr = x}

/* non-threadsafe functions */
mbptree_t* mbptree_init(int);
void mbptree_free(mbptree_t*);

/* threadsafe functions */
int mbptree_append(mbptree_t*, uint64_t, mbptree_value_t);

/* prints the b+tree, the function works in trees with less than 1024 nodes */
void mbptree_print(const mbptree_t*);

#endif
