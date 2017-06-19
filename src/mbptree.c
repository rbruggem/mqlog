#include "mbptree.h"
#include "mqlogerrno.h"
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>

struct mbptree_data {
    uint64_t key;
    union mbptree_value value;
};

struct mbptree_node {
    int                  leaf;
    int                  size;
    struct mbptree_node* parent;
    struct mbptree_data  data[];
};

struct mbptree_lock {  // two phase locking
    uint32_t exclusive_lock;
    uint32_t shared_lock;
};

union cas_mbptree_lock {
    struct mbptree_lock value;
    uint64_t            cas_helper;
};

struct mbptree {
    int                  branch_factor;
    volatile union cas_mbptree_lock lock;
    struct mbptree_node* root;
    struct mbptree_node* last_leaf;
};

struct mbptree_leaf_iterator {
    int                  branch_factor;
    int                  idx;
    const struct mbptree_node* leaf;
};

enum { QUEUE_SIZE = 1024 };

struct mbptree_bfs_iterator {
    int                  head;
    int                  tail;
    struct mbptree_node* queue[];
};

static void mbptree_print_node(const struct mbptree_node*);

static int mbptree_node_full(const mbptree_t* tree,
                             struct mbptree_node* node) {
    assert(node->size < tree->branch_factor);
    return node->size >= tree->branch_factor - 1;
}

static struct mbptree_node* mbptree_create_node(
    int branch_factor,
    struct mbptree_node* parent) {

    const size_t size = sizeof(struct mbptree_node) +
        branch_factor * sizeof(struct mbptree_data);

    struct mbptree_node* node = (struct mbptree_node*)calloc(1, size);
    if (!node) {
        return NULL;
    }

    node->size = 0;
    node->parent = parent;

    return node;
}

static struct mbptree_node* mbptree_create_leaf(
    int branch_factor,
    struct mbptree_node* parent) {

    struct mbptree_node* node = mbptree_create_node(branch_factor, parent);
    if (!node) {
        return NULL;
    }

    node->leaf = 1;
    return node;
}

static int mbptree_is_leaf(struct mbptree_node* node) {
    return node->leaf == 1;
}

static int mbptree_free_node(struct mbptree_node* node) {
    // frees node and children
    if (!mbptree_is_leaf(node)) {
        for (int i = 0; i <= node->size; ++i) {
            struct mbptree_node* child = node->data[i].value.addr;
            if (child) {
                if (mbptree_free_node(child) == -1) {
                    return -1;
                }
            }
        }
    }

    free(node);
    return 0;
}

static int mbptree_midpoint(const mbptree_t* tree) {
    // returns index in `data` that represents the mid
    return tree->branch_factor >> 1;
}

static int mbptree_is_root(const struct mbptree_node* node) {
    return node->parent == NULL;
}

static uint64_t mbptree_move_half_node(const mbptree_t* tree,
                                       struct mbptree_node* lhs,
                                       struct mbptree_node* rhs) {
    assert(lhs->parent == rhs->parent);
    assert(lhs->leaf == 0);
    assert(rhs->leaf == 0);

    const int mid = mbptree_midpoint(tree);
    const uint64_t mid_key = lhs->data[mid].key;

    for (int key_idx = mid + 1; key_idx < lhs->size; ++key_idx) {
        rhs->data[rhs->size++].key = lhs->data[key_idx].key;
        lhs->data[key_idx].key = 0;
    }

    for (int value_idx = mid + 1, j = 0;
         value_idx <= lhs->size;
         ++value_idx, ++j) {
        rhs->data[j].value = lhs->data[value_idx].value;
        lhs->data[value_idx].value.u64 = 0;

        struct mbptree_node* moved_node = rhs->data[j].value.addr;
        moved_node->parent = rhs;
    }

    lhs->size = mid;

    return mid_key;
}

static struct mbptree_node* mbptree_split_root(mbptree_t* tree,
                                               struct mbptree_node* root,
                                               uint64_t key,
                                               struct mbptree_node* child) {
    assert(tree->root == root);

    struct mbptree_node* new_root =
        mbptree_create_node(tree->branch_factor, NULL);
    if (!new_root) {
        return NULL;
    }

    struct mbptree_node* new_node =
        mbptree_create_node(tree->branch_factor, new_root);
    if (!new_node) {
        return NULL;
    }

    root->parent = new_root;

    const uint64_t mid_key = mbptree_move_half_node(tree, root, new_node);

    new_node->data[new_node->size++].key = key;
    new_node->data[new_node->size].value.addr = child;
    child->parent = new_node;

    // fix the nodes
    new_root->data[new_root->size].value.addr = root;
    new_root->data[new_root->size++].key = mid_key;
    new_root->data[new_root->size].value.addr = new_node;

    tree->root = new_root;

    return new_root;
}

static struct mbptree_node* mbptree_split_node(mbptree_t*,
                                               struct mbptree_node*,
                                               uint64_t,
                                               struct mbptree_node*);
static struct mbptree_node* mbptree_append_node(mbptree_t* tree,
                                                uint64_t key,
                                                struct mbptree_node* node) {

    assert(mbptree_is_root(node) == 0);

    struct mbptree_node* parent = node->parent;

    if (!mbptree_node_full(tree, parent)) {
        parent->data[parent->size++].key = key;
        parent->data[parent->size].value.addr = node;

        return tree->root;
    }

    // parent node is full
    if (mbptree_is_root(parent)) {
        return mbptree_split_root(tree, parent, key, node);
    }

    return mbptree_split_node(tree, parent, key, node);
}

static struct mbptree_node* mbptree_split_node(mbptree_t* tree,
                                               struct mbptree_node* parent,
                                               uint64_t key,
                                               struct mbptree_node* child) {
    assert(mbptree_node_full(tree, parent) == 1);
    assert(mbptree_is_root(parent) == 0);

    struct mbptree_node* new_parent =
        mbptree_create_node(tree->branch_factor, parent->parent);
    if (!new_parent) {
        return NULL;
    }

    const uint64_t mid_key = mbptree_move_half_node(tree, parent, new_parent);

    new_parent->data[new_parent->size++].key = key;
    new_parent->data[new_parent->size].value.addr = child;
    child->parent = new_parent;

    return mbptree_append_node(tree, mid_key, new_parent);
}

static struct mbptree_node* mbptree_append_leaf(mbptree_t* tree,
                                                struct mbptree_node* new_leaf) {
    // assumptions:
    // * new_leaf is populated
    // * last_leaf points to the `old` last_leaf
    // * last_leaf is consistently populated with new_leaf
    // * leaf points to new_leaf
    // * leaf and new_leaf point to the same parent
    assert(tree->last_leaf != new_leaf);
    assert(tree->last_leaf->size == tree->branch_factor - 1);
    assert(tree->last_leaf->data[tree->last_leaf->size].value.addr == new_leaf);
    assert(tree->last_leaf->parent == new_leaf->parent);

    struct mbptree_node* root = tree->root;

    struct mbptree_node* parent = tree->last_leaf->parent;
    if (!mbptree_node_full(tree, parent)) {
        const int idx = parent->size++;
        parent->data[idx].key = new_leaf->data[0].key;
        parent->data[idx + 1].value.addr = new_leaf;
    } else {
        // parent is full: split the node
        if (mbptree_is_root(parent)) {
            root = mbptree_split_root(
                tree,
                parent,
                new_leaf->data[0].key,
                new_leaf);
        } else {
            root = mbptree_split_node(
                tree,
                parent,
                new_leaf->data[0].key,
                new_leaf);
        }
    }

    tree->last_leaf = new_leaf;
    return root;
}

static struct mbptree_node* mbptree_new_leaf(const mbptree_t* tree,
                                             struct mbptree_node* leaf) {

    assert(leaf->leaf == 1);

    struct mbptree_node* parent = leaf->parent;
    struct mbptree_node* new_leaf =
        mbptree_create_leaf(tree->branch_factor, parent);
    if (!new_leaf) {
        return NULL;
    }

    // make leaf point to next leaf
    leaf->data[leaf->size].value.addr = new_leaf;
    return new_leaf;
}

static const struct mbptree_node* mbptree_find_leaf(
    const struct mbptree_node* node,
    uint64_t key) {

    if (node->leaf) {
        return node;
    }

    int i = 0;
    for (i = 0; i < node->size; ++i) {
        if (key < node->data[i].key) {
            return mbptree_find_leaf(
                (const struct mbptree_node*)node->data[i].value.addr, key);
        }
    }

    assert(i == node->size);

    return mbptree_find_leaf(
        (const struct mbptree_node*)node->data[i].value.addr, key);
}

static int mbptree_tryappend(mbptree_t* tree,
                             uint64_t key,
                             mbptree_value_t value) {
    struct mbptree_node* leaf = tree->last_leaf;

    if (leaf->size > 0 && key <= leaf->data[leaf->size - 1].key) {
        return ELIDXNM;
    }

    if (!mbptree_node_full(tree, leaf)) {
        struct mbptree_data* data = &leaf->data[leaf->size++];
        data->key = key;
        data->value = value;
        return 0;
    }

    // create a new root if the leaf is full and it is the root
    struct mbptree_node* new_root = NULL;
    if (mbptree_is_root(leaf)) {
        new_root = mbptree_create_node(tree->branch_factor, NULL);
        if (!new_root) {
            return ELALLC;
        }
        leaf->parent = new_root;
    }

    // leaf is full, create a new one
    struct mbptree_node* new_leaf = mbptree_new_leaf(tree, leaf);
    if (!new_leaf) {
        leaf->parent = NULL;
        mbptree_free_node(new_root);
        return ELALLC;
    }

    // add data to new_leaf
    struct mbptree_data* data = &new_leaf->data[new_leaf->size++];
    data->key = key;
    data->value = value;


    // add leafs to new_root
    if (new_root) {
        struct mbptree_data* data = &new_root->data[new_root->size++];
        data->key = new_leaf->data[0].key;
        data->value.addr = leaf;
        data = &new_root->data[new_root->size];
        data->value.addr = new_leaf;

        tree->last_leaf = new_leaf;
        tree->root = new_root;

        return 0;
    }

    // leaf was split and leaf was not the root
    new_root = mbptree_append_leaf(tree, new_leaf);
    if (!new_root) {
        // PANIC! the tree might be in an unconsistent state.
        // this can happen if a few nodes are successfully allocated before
        // a subsequent allocation fails.
        return ELIDXPC;
    }

    assert(new_root != NULL);
    tree->root = new_root;

    return 0;
}

mbptree_t* mbptree_init(int branch_factor) {
    mbptree_t* tree = (mbptree_t*)calloc(1, sizeof(struct mbptree));
    if (!tree) {
        return NULL;
    }

    tree->branch_factor = branch_factor;

    struct mbptree_node* node = mbptree_create_leaf(branch_factor, NULL);
    if (!node) {
        free(tree);
        return NULL;
    }

    const struct mbptree_lock lock = {.exclusive_lock = 0, .shared_lock = 0};

    tree->lock.value = lock;
    tree->root = node;
    tree->last_leaf = node;

    return tree;
}

int mbptree_free(mbptree_t* tree) {
    mbptree_free_node(tree->root);
    free(tree);
    return 0;
}

int mbptree_append(mbptree_t* tree, uint64_t key, mbptree_value_t value) {
    const union cas_mbptree_lock old_val = {
        .value = {.exclusive_lock = 0, .shared_lock = 0}
    };
    const union cas_mbptree_lock new_val = {
        .value = {.exclusive_lock = 1, .shared_lock = 0}
    };

    if (!__sync_bool_compare_and_swap(
        &tree->lock.cas_helper,
        old_val.cas_helper,
        new_val.cas_helper)) {
         return ELIDXLK;
    }

    const int rc = mbptree_tryappend(tree, key, value);

    tree->lock = old_val;

    return rc;
}

int mbptree_last_value(const mbptree_t* tree, mbptree_value_t* value) {
    const struct mbptree_node* last_leaf = tree->last_leaf;
    if (last_leaf->size == 0) {
        return -1;
    }

    *value = last_leaf->data[last_leaf->size - 1].value;
    return 0;
}

int mbptree_leaf_first(mbptree_t* tree, mbptree_leaf_iterator_t** iterator_ptr) {
    return mbptree_leaf_floor(tree, 0, iterator_ptr);
}

int mbptree_leaf_floor(mbptree_t* tree,
                       uint64_t key,
                       mbptree_leaf_iterator_t** iterator_ptr) {

    struct mbptree_leaf_iterator* iterator =
        (struct mbptree_leaf_iterator*)
            calloc(1, sizeof(struct mbptree_leaf_iterator));
    if (!iterator) {
        return ELALLC;
    }

    iterator->branch_factor = tree->branch_factor;
    iterator->leaf = NULL;

    // lock
    const union cas_mbptree_lock old_val = tree->lock;
    union cas_mbptree_lock expected_val = old_val;
    expected_val.value.exclusive_lock = 0;
    union cas_mbptree_lock new_val = old_val;
    ++new_val.value.shared_lock;

    if (!__sync_bool_compare_and_swap(
        &tree->lock.cas_helper,
        expected_val.cas_helper,
        new_val.cas_helper)) {
         return ELIDXLK;
    }

    const struct mbptree_node* leaf = mbptree_find_leaf(tree->root, key);
    --tree->lock.value.shared_lock;

    assert(leaf);

    int idx = -1;
    for (int i = 0; i < leaf->size; ++i) {
        if (leaf->data[i].key <= key) {
            idx = i;
        } else {
            break;
        }
    }

    if (idx != -1) {
        iterator->idx = idx;
        iterator->leaf = leaf;
    }

    *iterator_ptr = iterator;

    return 0;
}

int mbptree_leaf_iterator_valid(const mbptree_leaf_iterator_t* iterator) {
    if (iterator->leaf == NULL) {
        return 0;
    }
    return iterator->idx < iterator->leaf->size;
}

uint64_t mbptree_leaf_iterator_key(const mbptree_leaf_iterator_t* iterator) {
    return iterator->leaf->data[iterator->idx].key;
}

mbptree_value_t mbptree_leaf_iterator_value(
    const mbptree_leaf_iterator_t* iterator) {
    return iterator->leaf->data[iterator->idx].value;
}

mbptree_leaf_iterator_t* mbptree_leaf_iterator_next(
    mbptree_leaf_iterator_t* iterator) {

    assert(iterator->leaf);

    const int next_idx = iterator->idx + 1;
    assert(next_idx <= iterator->leaf->size);

    if (next_idx == iterator->leaf->size) {
        const int next_node_idx = iterator->leaf->size;
        iterator->leaf =
            (struct mbptree_node*)
                iterator->leaf->data[next_node_idx].value.addr;

        iterator->idx = 0;
    } else {
        iterator->idx = next_idx;
    }

    return iterator;
}

mbptree_bfs_iterator_t* mbptree_bfs_first(const mbptree_t* tree) {
    mbptree_bfs_iterator_t* iterator =
        (mbptree_bfs_iterator_t*)calloc(1, sizeof(mbptree_bfs_iterator_t) +
        QUEUE_SIZE * sizeof(struct mbptree_node));
    if (!iterator) {
        return NULL;
    }

    iterator->head = 0;
    iterator->tail = 0;

    iterator->queue[iterator->tail++] = tree->root
;
    const struct mbptree_node* node = iterator->queue[iterator->head];
    if (!node->leaf) {
        for (int i = 0; i <= node->size; ++i) {
            const struct mbptree_data* data = &node->data[i];
            iterator->queue[iterator->tail++] = data->value.addr;
        }
    }

    return iterator;
}

int mbptree_bfs_iterator_key(const mbptree_bfs_iterator_t* iterator,
                             int idx,
                             uint64_t* key) {
    const struct mbptree_node* node = iterator->queue[iterator->head];
    if (idx < node->size) {
        *key = node->data[idx].key;
        return 0;
    }

    return -1;
}

mbptree_bfs_iterator_t* mbptree_bfs_iterator_next(
    mbptree_bfs_iterator_t* iterator) {
    ++iterator->head;

    const struct mbptree_node* node = iterator->queue[iterator->head];
    if (node && !node->leaf) {
        for (int i = 0; i <= node->size; ++i) {
            const struct mbptree_data* data = &node->data[i];
            iterator->queue[iterator->tail++] = data->value.addr;
        }
    }
    return iterator;
}

int mbptree_bfs_iterator_valid(const mbptree_bfs_iterator_t* iterator) {
    return iterator->head < iterator->tail;
}

int mbptree_bfs_iterator_leaf(const mbptree_bfs_iterator_t* iterator) {
    return iterator->queue[iterator->head]->leaf;
}

// LCOV_EXCL_START
static void mbptree_print_node(const struct mbptree_node* node) {
    printf("{ %p ", (void*)node);
    if (node->leaf) {
        printf("L |");
    } else {
        printf("N |");
    }

    int j = 0;
    for (j = 0; j < node->size; ++j) {
        if (j != 0) {
            printf(",");
        }
        const struct mbptree_data* data = &node->data[j];
        if (node->leaf) {
            printf(" %"PRIu64":%"PRIu64, data->key, data->value.u64);
        } else {
            printf(" %"PRIu64":%p", data->key, (void*)data->value.addr);
        }
    }

    if (node->leaf) {
        const struct mbptree_data* data = &node->data[j];
        printf(" | count %d next: %"PRIu64, j, data->value.u64);
    } else {
        const struct mbptree_data* data = &node->data[j];
        printf(" _:%p |", data->value.addr);
    }
    printf(" parent: %p", (void*)node->parent);
    printf(" }\n");
}

void mbptree_print(const mbptree_t* tree) {
    mbptree_bfs_iterator_t* iterator = mbptree_bfs_first(tree);
    for (; mbptree_bfs_iterator_valid(iterator);
           iterator = mbptree_bfs_iterator_next(iterator)) {
        const struct mbptree_node* node = iterator->queue[iterator->head];
        mbptree_print_node(node);
    }
    free(iterator);
}
// LCOV_EXCL_STOP
