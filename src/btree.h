#ifndef MQLOG_BTREE_H_
#define MQLOG_BTREE_H_

/*
 * Implements a B+tree
 */

#include <inttypes.h>

struct btree_node_data {
    int64_t                key;
    void*                  value;
};

typedef struct btree btree_t;
typedef struct btree_node btree_node_t;
typedef struct btree_iterator btree_iterator_t;

btree_t* btree_init(int);
int      btree_free(btree_t*);

int   btree_insert(btree_t*, int64_t, void*);
void* btree_find(btree_t*, int64_t);
void* btree_find_le(btree_t*, int64_t);

int   btree_empty(const btree_t*);
const btree_node_t* btree_root(const btree_t*);

const struct btree_node_data* btree_max(const btree_t*);
int          btree_node_leaf(const btree_node_t*);
const struct btree_node_data* btree_node_data(const btree_node_t*, int);
int          btree_node_length(const btree_node_t*);

btree_iterator_t* btree_iterator_head(btree_t*);
btree_iterator_t* btree_iterator_find(btree_t*, int64_t);
btree_iterator_t* btree_iterator_find_le(btree_t*, int64_t);
int               btree_iterator_valid(const btree_iterator_t*);
btree_iterator_t* btree_iterator_next(btree_iterator_t*);
const struct btree_node_data* btree_iterator_data(const btree_iterator_t*);

#endif
