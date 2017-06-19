#ifndef MQLOG_MBPTREE_H_
#define MQLOG_MBPTREE_H_

#include <inttypes.h>

/*
 * Implements an immutable monotonic thread safe B+tree.
 * Properties:
 * * All keys must be inserted maintaining a monotonically increases order.
 * * deletions and updations are disallowed
 * * keys are of type `uint64_t`
 *
 * Unlike a standard B+tree, when a new element is inserted into a full leaf
 * and a new leaf is created, the full leaf is not split and elements do not get
 * moved over to the new leaf.
 * This allows to use space more efficiently and reduce allocations of new
 * nodes. It has been made possible by the data structures' monitonic property.
 * Non-leaf nodes split as standard B+tree non-leaf nodes.
 */

union mbptree_value {
    uint32_t u32;
    uint64_t u64;
    void*    addr;
};

typedef union  mbptree_value mbptree_value_t;
typedef struct mbptree mbptree_t;
typedef struct mbptree_leaf_iterator mbptree_leaf_iterator_t;
typedef struct mbptree_bfs_iterator mbptree_bfs_iterator_t;

#define u32(x)  (mbptree_value_t){.ddr = x}
#define u64(x)  (mbptree_value_t){.u64 = x}
#define addr(x) (mbptree_value_t){.addr = x}

/* non-threadsafe functions */
mbptree_t* mbptree_init(int);
int mbptree_free(mbptree_t*);

/* thread-safe */
int mbptree_append(mbptree_t*, uint64_t, mbptree_value_t);

int mbptree_last_value(const mbptree_t*, mbptree_value_t*);

/* leaf iterator */
int mbptree_leaf_first(mbptree_t*, mbptree_leaf_iterator_t**);
int mbptree_leaf_floor(mbptree_t*, uint64_t, mbptree_leaf_iterator_t**);
int mbptree_leaf_iterator_valid(const mbptree_leaf_iterator_t*);
uint64_t mbptree_leaf_iterator_key(const mbptree_leaf_iterator_t*);
mbptree_value_t mbptree_leaf_iterator_value(const mbptree_leaf_iterator_t*);
mbptree_leaf_iterator_t* mbptree_leaf_iterator_next(mbptree_leaf_iterator_t*);

/* breadth first search iterator (not thread-safe) */
mbptree_bfs_iterator_t* mbptree_bfs_first(const mbptree_t*);
int mbptree_bfs_iterator_key(const mbptree_bfs_iterator_t*, int idx, uint64_t*);
mbptree_bfs_iterator_t* mbptree_bfs_iterator_next(mbptree_bfs_iterator_t*);
int mbptree_bfs_iterator_valid(const mbptree_bfs_iterator_t*);
int mbptree_bfs_iterator_leaf(const mbptree_bfs_iterator_t*);

/* prints the b+tree, the function works on trees with less than 1024 nodes */
void mbptree_print(const mbptree_t*);

#endif
