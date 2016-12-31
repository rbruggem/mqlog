#include "btree.h"
#include "util.h"
#include <string.h>
#include <stdlib.h>
#include <assert.h>

struct btree_node {
    unsigned int           leaf;
    int                    size;
    struct btree_node*     parent;
    struct btree_node_data data[];
};

struct btree {
    const int               branch_factor;
    struct btree_node*      root;
    struct btree_node_data* max;
};

struct btree_iterator {
    struct btree_node* leaf;
    int                branch_factor;
    int                idx;
};

static int btree_midpoint(const struct btree*) __attribute__((const));
static size_t btree_node_size(const struct btree*) __attribute__((const));
static struct btree_node* btree_node_append(const struct btree*,
                                            int64_t,
                                            struct btree_node*);
static struct btree_node* btree_node_leaf_append(const struct btree*,
                                                 struct btree_node*);

static int btree_midpoint(const struct btree* tree) {
    // Branch factor:  capacity of the node (numbers of children)
    // Keys = branch factor - 1
    return (tree->branch_factor - 2) >> 1;
}

static size_t btree_node_size(const struct btree* tree) {
    return
        sizeof(struct btree_node) +
        sizeof(struct btree_node_data) * (tree->branch_factor);
}

static struct btree_node* btree_node_create(const struct btree* tree,
                                            struct btree_node* parent) {

    const size_t size = btree_node_size(tree);
    struct btree_node* node =
        (struct btree_node*)malloc(size);
    if (!node) {
        return 0;
    }

    memset(node, 0, size);
    node->leaf = 0;
    node->parent = parent;
    return node;
}

static struct btree_node* btree_node_leaf_create(const struct btree* tree,
                                                 struct btree_node* parent) {

    const size_t size = btree_node_size(tree);
    struct btree_node* node =
        (struct btree_node*)malloc(size);
    if (!node) {
        return 0;
    }

    memset(node, 0, size);
    node->leaf = 1;
    node->parent = parent;
    return node;
}

static int btree_node_free(struct btree_node* node) {
    // frees node and children
    if (!btree_node_leaf(node)) {
        const int len = node->size + 1;
        for (int i = 0; i < len; ++i) {
            if (btree_node_free(
                (struct btree_node*)node->data[i].value
            ) == -1) {
                return -1;
            }
        }
    }

    free(node);
    return 0;
}

static int btree_node_full(const struct btree* tree,
                           struct btree_node* node) {
    assert(node->size < tree->branch_factor);
    return node->size == tree->branch_factor - 1;
}

static int btree_node_empty(const struct btree_node* node) {
    return node->size == 0;
}

static int btree_is_root(struct btree_node* node) {
    return node->parent == 0;
}

static struct btree_node* btree_node_leaf_find(struct btree_node* node,
                                               int64_t key) {
    if (node->leaf) {
        return node;
    }

    int i = 0;
    for (i = 0; i < node->size; ++i) {
        if (key < node->data[i].key) {
            return btree_node_leaf_find(node->data[i].value, key);
        }
    }

    return btree_node_leaf_find(node->data[i].value, key);
}

static struct btree_node* btree_node_leaf_head(struct btree_node* node) {
    if (node->leaf) {
        return node;
    }

    return btree_node_leaf_head(node->data[0].value);
}

static struct btree_node* btree_node_leaf_shift(struct btree* tree,
                                                struct btree_node* node,
                                                int index) {
    assert(index >= 0);

    // shifts all values exclusive of next pointer
    for (int i = node->size; i >= index; --i) {
        node->data[i].key = node->data[i - 1].key;
        node->data[i].value = node->data[i - 1].value;

        if (tree->max == &node->data[i - 1]) {
            tree->max = &node->data[i];
        }
    }

    return node;
}

static struct btree_node* btree_node_shift(struct btree_node* node,
                                           int index) {
    assert(index >= 0);

    for (int i = node->size; i >= index; --i) {
        node->data[i].key = node->data[i - 1].key;
        node->data[i + 1].value = node->data[i].value;
    }

    return node;
}

static int btree_node_leaf_insert(struct btree* tree,
                                  struct btree_node* node,
                                  int64_t key,
                                  void* value) {
    assert(!btree_node_full(tree, node));
    assert(node->leaf == 1);

    struct btree_node* next = node->data[node->size].value;

    int i = 0;
    for (; i < node->size; ++i) {
        if (key < node->data[i].key) {
            node = btree_node_leaf_shift(tree, node, i);
            break;
        }
    }
    node->data[i].key = key;
    node->data[i].value = value;
    ++node->size;

    if (!tree->max ||
        node->data[node->size - 1].key >= tree->max->key) {
        tree->max = &node->data[node->size - 1];
    }

    node->data[node->size].value = next;

    return 0;
}

static int btree_node_insert(const struct btree* tree,
                             int64_t key,
                             struct btree_node* child) {
    assert(!btree_is_root(child));
    assert(!btree_node_full(tree, child->parent));
    assert(child->size > 0);

    struct btree_node* parent = child->parent;

    int i = 0;
    for (; i < parent->size; ++i) {
        if (key < parent->data[i].key) {
            parent = btree_node_shift(parent, i);
            break;
        }
    }

    parent->data[i].key = key;
    parent->data[i + 1].value = child;
    ++parent->size;

    return 0;
}

static struct btree_node* btree_root_split(const struct btree* tree,
                                           int64_t child_key,
                                           struct btree_node* child) {
    assert(btree_is_root(child->parent));
    assert(btree_node_full(tree, child->parent));

    struct btree_node* root = child->parent;
    struct btree_node* new_root = btree_node_create(tree, 0);
    if (!new_root) {
        return 0;
    }

    struct btree_node* new_node = btree_node_create(tree, new_root);
    if (!new_node) {
        return 0;
    }

    int mid = btree_midpoint(tree);
    int64_t new_root_key = root->data[mid].key;


    if (child_key >= new_root_key) {
        // child_key belongs to the right node, increment mid
        // and update new_root_key
        ++mid;
        assert(mid < root->size);
        new_root_key = min(root->data[mid].key, child_key);

        const int len = root->size;
        // populate first slot
        int child_added = 0;
        if (child_key < root->data[mid].key) {
            child_added = 1;
            child->parent = new_node;
            new_node->data[0].value = child;
        } else {
            ++mid;
            struct btree_node* child_node = root->data[mid].value;
            child_node->parent = new_node;
            new_node->data[0].value = child_node;
            --root->size;
        }

        int j = 0;
        for (int i = mid; i < len; ++i) {
            int64_t key = root->data[i].key;
            if (child_added == 0 && child_key < key) {
                new_node->data[j].key = child_key;
                child->parent = new_node;
                new_node->data[j + 1].value = child;
                child_added = 1;
                --i;
            } else {
                new_node->data[j].key = key;
                struct btree_node* child_node = root->data[i + 1].value;
                child_node->parent = new_node;
                new_node->data[j + 1].value = child_node;
                --root->size;
            }
            ++new_node->size;
            ++j;
        }

        if (child_added == 0) {
            new_node->data[j].key = child_key;
            child->parent = new_node;
            new_node->data[j + 1].value = child;
            ++new_node->size;
            child_added = 1;
        }

        assert(child_added == 1);

    } else {
        // child_key belongs to the left node
        ++mid;
        struct btree_node* child_node = root->data[mid].value;
        child_node->parent = new_node;
        new_node->data[0].value = child_node;

        int j = 0;
        const int len = root->size;
        --root->size; // because of new_root_key
        for (int i = mid; i < len; ++i) {
            new_node->data[j].key = root->data[i].key;
            struct btree_node* child_node = root->data[i + 1].value;
            child_node->parent = new_node;
            new_node->data[j + 1].value = child_node;
            --root->size;
            ++new_node->size;
            ++j;
        }

        int i = 0;
        for (; i < mid; ++i) {
            if (child_key < root->data[i].key) {
                root = btree_node_shift(root, i);
                break;
            }
        }

        root->data[i].key = child_key;
        child->parent = root;
        root->data[i + 1].value = child;
        ++root->size;
    }

    new_root->data[0].key = new_root_key;
    ++new_root->size;

    new_root->data[0].value = root;
    root->parent = new_root;

    new_root->data[1].value = new_node;
    new_node->parent = new_root;

    return new_root;
}

static struct btree_node* btree_parent_split(const struct btree* tree,
                                             struct btree_node* child) {
    assert(!btree_is_root(child));
    assert(child->size > 0);

    // child's parent needs to be split!
    struct btree_node* parent = child->parent;
    assert(parent->parent);
    struct btree_node* new_parent = btree_node_create(tree, parent->parent);
    if (!new_parent) {
        return 0;
    }

    // divide keys in the two nodes
    int mid = btree_midpoint(tree);
    const int64_t child_key = child->data[0].key;
    int64_t node_key = parent->data[mid].key;

    if (child_key >= node_key) {
        ++mid;
        node_key = min(parent->data[mid].key, child_key);

        int child_added = 0;
        const int len = parent->size;
        if (child_key < parent->data[mid].key) {
            child_added = 1;
            child->parent = new_parent;
            new_parent->data[0].value = child;
        } else {
            ++mid;
            struct btree_node* child_node = parent->data[mid].value;
            child_node->parent = new_parent;
            new_parent->data[0].value = child_node;
            --parent->size;
        }

        int j = 0;
        for (int i = mid; i < len; ++i) {
            int64_t key = parent->data[i].key;
            if (child_added == 0 && child_key < key) {
                new_parent->data[j].key = child_key;
                child->parent = new_parent;
                new_parent->data[j + 1].value = child;
                child_added = 1;
                --i;
            } else {
                new_parent->data[j].key = key;
                struct btree_node* child_node = parent->data[i + 1].value;
                child_node->parent = new_parent;
                new_parent->data[j + 1].value = child_node;
                --parent->size;
            }
            ++new_parent->size;
            ++j;
        }

        if (child_added == 0) {
            new_parent->data[j].key = child_key;
            child->parent = new_parent;
            new_parent->data[j + 1].value = child;
            ++new_parent->size;
            child_added = 1;
        }

        assert(child_added == 1);

    } else {
        // child_key belongs to the left node
        ++mid;
        struct btree_node* child_node = parent->data[mid].value;
        child_node->parent = new_parent;
        new_parent->data[0].value = child_node;

        int j = 0;
        const int len = parent->size;
        --parent->size; // because of new_parent_key
        for (int i = mid; i < len; ++i) {
            new_parent->data[j].key = parent->data[i].key;
            struct btree_node* child_node = parent->data[i + 1].value;
            child_node->parent = new_parent;
            new_parent->data[j + 1].value = child_node;
            --parent->size;
            ++new_parent->size;
            ++j;
        }

        int i = 0;
        for (; i < mid; ++i) {
            if (child_key < parent->data[i].key) {
                parent = btree_node_shift(parent, i);
                break;
            }
        }

        parent->data[i].key = child_key;
        child->parent = parent;
        parent->data[i + 1].value = child;
        ++parent->size;
    }

    return btree_node_append(tree, node_key, new_parent);
}

static struct btree_node* btree_node_leaf_split(struct btree* tree,
                                                struct btree_node* leaf,
                                                int64_t key,
                                                void* value) {
    struct btree_node* new_leaf = btree_node_leaf_create(tree, leaf->parent);
    if (!new_leaf) {
        return 0;
    }

    struct btree_node* new_root = 0;
    if (btree_is_root(leaf)) {
        new_root = btree_node_create(tree, 0);
        if (!new_root) {
            return 0;
        }
    }

    // divide keys in the two nodes
    int mid = btree_midpoint(tree);

    if (key >= leaf->data[mid].key) {
        ++mid;

        struct btree_node* next = leaf->data[leaf->size].value;

        int value_added = 0;
        const int len = leaf->size;
        int j = 0;
        for (int i = mid; i < len; ++i) {
            if (key < leaf->data[i].key && value_added == 0) {
                new_leaf->data[j].key = key;
                new_leaf->data[j].value = value;
                value_added = 1;
                --i;
            } else {
                new_leaf->data[j].key = leaf->data[i].key;
                new_leaf->data[j].value = leaf->data[i].value;
                --leaf->size;
            }
            ++new_leaf->size;
            ++j;
        }

        if (value_added == 0) {
            new_leaf->data[j].key = key;
            new_leaf->data[j].value = value;
            ++new_leaf->size;
        }

        if (!tree->max ||
            new_leaf->data[leaf->size - 1].key >= tree->max->key) {
            tree->max = &new_leaf->data[new_leaf->size - 1];
        }

        new_leaf->data[new_leaf->size].value = next;

    } else {
        const int moved_max = tree->max == &leaf->data[leaf->size - 1];

        // move values from leaf to new_leaf,
        // from mid to the end of array
        const int len = leaf->size;
        int j = 0, i = mid;
        for (i = mid; i < len; ++i) {
            new_leaf->data[j].key = leaf->data[i].key;
            new_leaf->data[j].value = leaf->data[i].value;
            --leaf->size;
            ++new_leaf->size;
            ++j;
        }

        // move pointer to next node from leaf to new_leaf
        assert(len == i);
        new_leaf->data[j].key = leaf->data[i].key;
        new_leaf->data[j].value = leaf->data[i].value;

        // move `i` to the position where the new key
        // will be inserted
        const int len2 = leaf->size;
        i = 0;
        for (i = 0; i < len2; ++i) {
            if (key < leaf->data[i].key) {
                break;
            }
        }
        // shift all items greater or equal to `i`
        // to the right
        leaf = btree_node_leaf_shift(tree, leaf, i);

        // add new key / value,
        // space has been made in the previous statement
        leaf->data[i].key = key;
        leaf->data[i].value = value;
        ++leaf->size;

        // if max has been moved, update it
        if (moved_max) {
            tree->max = &new_leaf->data[new_leaf->size - 1];
        }
    }

    // since new_leaf comes after leaf,
    // leaf needs to point to new_leaf
    leaf->data[leaf->size].value = new_leaf;

    // handle root being a leaf
    if (btree_is_root(leaf)) {
        new_root->data[0].key = new_leaf->data[0].key;
        new_root->data[0].value = leaf;
        new_root->data[1].value = new_leaf;
        ++new_root->size;

        leaf->parent = new_root;
        new_leaf->parent = new_root;

        return new_root;
    }

    return btree_node_leaf_append(tree, new_leaf);
}

static struct btree_node* btree_node_leaf_append(const struct btree* tree,
                                            struct btree_node* node) {
    // check if node is the root node
    if (btree_is_root(node)) {
        return node;
    }

    // node has a parent
    struct btree_node* parent = node->parent;

    if (!btree_node_full(tree, parent)) {
        // parent is not full
        btree_node_insert(tree, node->data[0].key, node);
        return tree->root;
    }

    if (btree_is_root(parent)) {
        return btree_root_split(tree, node->data[0].key, node);
    }

    return btree_parent_split(tree, node);
}

static struct btree_node* btree_node_append(const struct btree* tree,
                                            int64_t key,
                                            struct btree_node* node) {
    // check if node is the root node
    if (btree_is_root(node)) {
        return node;
    }

    // node has a parent
    struct btree_node* parent = node->parent;

    if (!btree_node_full(tree, parent)) {
        // parent is not full
        btree_node_insert(tree, key, node);
        return tree->root;
    }

    if (btree_is_root(parent)) {
        return btree_root_split(tree, key, node);
    }

    return btree_parent_split(tree, node);
}

static int btree_node_leaf_find_value(struct btree_node* leaf,
                                      int64_t key,
                                      void** value) {
    assert(btree_node_leaf(leaf));

    for (int i = 0; i < leaf->size; ++i) {
        if (key == leaf->data[i].key) {
            *value = leaf->data[i].value;
            return i;
        } else if (key < leaf->data[i].key) {
            return -1;
        }
    }
    return -1;
}

static int btree_node_leaf_find_value_le(struct btree_node* leaf,
                                         int64_t key,
                                         void** value) {
    assert(btree_node_leaf(leaf));

    int prev_idx = -1;
    void* prev_value = NULL;
    for (int i = 0; i < leaf->size; ++i) {
        if (leaf->data[i].key <= key) {
            prev_value = leaf->data[i].value;
            prev_idx = i;
        } else {
            break;
        }
    }

    if (prev_idx != -1) {
        *value = prev_value;
    }
    return prev_idx;
}

struct btree* btree_init(int branch_factor) {
    if (branch_factor <= 1) {
        return 0;
    }

    struct btree* tree  = (struct btree*)malloc(sizeof(struct btree));
    if (!tree) {
        return 0;
    }
    struct btree init = {.branch_factor = branch_factor, .root = 0};
    memcpy(tree, &init, sizeof(init));

    struct btree_node* root = btree_node_leaf_create(tree, 0);
    if (!root) {
        free(tree);
        return 0;
    }

    tree->root = root;

    return tree;
}

int btree_free(btree_t* tree) {
    if (btree_node_free(tree->root) != 0) {
        return -1;
    }
    free(tree);
    return 0;
}

int btree_insert(btree_t* tree,
                 int64_t key,
                 void* value) {

    struct btree_node* leaf = btree_node_leaf_find(tree->root, key);

    void* old_value = 0;
    int i = btree_node_leaf_find_value(leaf, key, &old_value);
    if (i != -1) {
        leaf->data[i].value = value;
        return 0;
    }

    if (!btree_node_full(tree, leaf)) {
        btree_node_leaf_insert(tree, leaf, key, value);
        return 0;
    }

    // the leaf was full, split the leaf
    struct btree_node* new_root = btree_node_leaf_split(tree, leaf, key, value);
    if (!new_root) {
        return -1;
    }

    tree->root = new_root;

    return 0;
}

void* btree_find(btree_t* tree, int64_t key) {
    struct btree_node* leaf = btree_node_leaf_find(tree->root, key);
    void* value = 0;
    if (btree_node_leaf_find_value(leaf, key, &value) != -1) {
        assert(value != 0);
        return value;
    }
    return NULL;
}

void* btree_find_le(btree_t* tree, int64_t key) {
    struct btree_node* leaf = btree_node_leaf_find(tree->root, key);
    void* value = 0;
    if (btree_node_leaf_find_value_le(leaf, key, &value) != -1) {
        assert(value != 0);
        return value;
    }
    return NULL;
}

int btree_empty(const btree_t* tree) {
    return btree_is_root(tree->root) && btree_node_empty(tree->root);
}

const btree_node_t* btree_root(const btree_t* tree) {
    return tree->root;
}

const struct btree_node_data* btree_max(const btree_t* tree) {
    return tree->max;
}

int btree_node_leaf(const btree_node_t* node) {
    return node->leaf == 1;
}

const struct btree_node_data* btree_node_data(const btree_node_t* node,
                                              int idx) {
    assert(idx <= node->size);
    return &node->data[idx];
}

int btree_node_length(const btree_node_t* node) {
    return node->size;
}

btree_iterator_t* btree_iterator_head(btree_t* tree) {
    struct btree_iterator* iter =
        (struct btree_iterator*)malloc(sizeof(struct btree_iterator));
    if (!iter) {
        return NULL;
    }
    iter->branch_factor = tree->branch_factor;
    iter->leaf = NULL;

    struct btree_node* leaf = btree_node_leaf_head(tree->root);
    iter->leaf = leaf;
    iter->idx = 0;

    return iter;
}

btree_iterator_t* btree_iterator_find(btree_t* tree, int64_t key) {
    struct btree_iterator* iter =
        (struct btree_iterator*)malloc(sizeof(struct btree_iterator));
    if (!iter) {
        return NULL;
    }
    iter->branch_factor = tree->branch_factor;
    iter->leaf = NULL;

    struct btree_node* leaf = btree_node_leaf_find(tree->root, key);
    void* value = 0;
    int idx = btree_node_leaf_find_value(leaf, key, &value);
    if (idx != -1) {
        assert(value != 0);
        iter->leaf = leaf;
        iter->idx = idx;
    }

    return iter;
}

btree_iterator_t* btree_iterator_find_le(btree_t* tree, int64_t key) {
    struct btree_iterator* iter =
        (struct btree_iterator*)malloc(sizeof(struct btree_iterator));
    if (!iter) {
        return NULL;
    }
    iter->branch_factor = tree->branch_factor;
    iter->leaf = NULL;

    struct btree_node* leaf = btree_node_leaf_find(tree->root, key);
    void* value = 0;
    int idx = btree_node_leaf_find_value_le(leaf, key, &value);
    if (idx != -1) {
        assert(value != 0);
        iter->leaf = leaf;
        iter->idx = idx;
    }

    return iter;
}

int btree_iterator_valid(const btree_iterator_t* iter) {
    if (iter->leaf == NULL) {
        return 0;
    }

    return iter->idx < iter->leaf->size;
}

btree_iterator_t* btree_iterator_next(btree_iterator_t* iter) {
    assert(iter->leaf);

    const int next_idx = iter->idx + 1;
    assert(next_idx <= iter->leaf->size);

    if (next_idx == iter->leaf->size) {
        const int next_node_idx = iter->leaf->size;
        iter->leaf = (struct btree_node*)iter->leaf->data[next_node_idx].value;

        iter->idx = 0;
    } else {
        iter->idx = next_idx;
    }

    return iter;
}

const struct btree_node_data* btree_iterator_data(
    const btree_iterator_t* iter) {
    return &iter->leaf->data[iter->idx];
}
