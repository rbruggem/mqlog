#include "testfw.h"
#include "test_util.h"
#include <mqlogerrno.h>
#include <mbptree.h>
#include <stdlib.h>

static int mbptree_compare(const mbptree_t* tree, const uint64_t* arr, int w) {
    mbptree_bfs_iterator_t* iterator = mbptree_bfs_first(tree);
    for (int i = 0; mbptree_bfs_iterator_valid(iterator);
           iterator = mbptree_bfs_iterator_next(iterator), ++i) {

        for (int j = 0;; ++j) {
            if ((int)*(arr + i * w) != mbptree_bfs_iterator_leaf(iterator)) {
                return 0;
            }
            uint64_t key;
            if (mbptree_bfs_iterator_key(iterator, j, &key) != 0) {
                break;
            }
            const uint64_t expected = *(arr + i * w + j + 1);
            if (expected != key) {
                fprintf(stderr,
                        "Expected %"PRIu64", got %"PRIu64"\n",
                        expected,
                        key);
                return 0;
            }
        }
    }
    free(iterator);
    return 1;
}

TEST(mbptree1) {
    const int branch_factor = 3;
    mbptree_t* tree = mbptree_init(branch_factor);
    ASSERT(tree != 0);

    const uint64_t result[][3] =
        {{0, 10, 0},
           {0, 5, 0},
           {0, 15, 22},
             {1, 1, 2},
             {1, 5, 6},
             {1, 10, 12},
             {1, 15, 20},
             {1, 22, 0}};

    const uint64_t arr[] = {1, 2, 5, 6, 10, 12, 15, 20, 22, 0};
    for (int i = 0; arr[i] != 0; ++i) {
        int rc = mbptree_append(tree, arr[i], u64(arr[i]));
        ASSERT(rc == 0);
    }

    ASSERT(mbptree_compare(tree, (const uint64_t*)result, branch_factor));

    mbptree_leaf_iterator_t* iterator;
    ASSERT(mbptree_leaf_floor(tree, 16, &iterator) == 0);
    ASSERT(iterator);
    ASSERT(mbptree_leaf_iterator_valid(iterator) == 1);
    ASSERT(mbptree_leaf_iterator_key(iterator) == 15);
    free(iterator);

    const uint64_t arr2[] = {25, 27, 30, 33, 34, 35, 36, 37, 49, 55, 100, 0};
    for (int i = 0; arr2[i] != 0; ++i) {
        int rc = mbptree_append(tree, arr2[i], u64(arr2[i]));
        ASSERT(rc == 0);
    }

    const uint64_t result2[][3] =
        {{0, 22, 0},
           {0, 10, 0},
           {0, 33, 37},
             {0, 5, 15},
             {0, 15, 0},
             {0, 27, 0},
             {0, 35, 0},
             {0, 55, 0},
               {1, 1, 2},
               {1, 5, 6},
               {1, 10, 12},
               {1, 15, 20},
               {1, 22, 25},
               {1, 27, 30},
               {1, 33, 34},
               {1, 35, 36},
               {1, 37, 49},
               {1, 55, 100}};

    ASSERT(mbptree_compare(tree, (const uint64_t*)result2,  branch_factor));

    int rc = mbptree_append(tree, 8, u64(8));
    ASSERT(rc == ELIDXNM);

    mbptree_free(tree);
}

TEST(mbptree2) {
    const int branch_factor = 6;
    mbptree_t* tree = mbptree_init(branch_factor);
    ASSERT(tree != 0);

    const uint64_t result[][6] =
        {{0, 74, 144, 0, 0, 0},
           {0, 12, 27, 55, 0, 0},
           {0, 82, 100, 130, 0, 0},
           {0, 177, 189, 197, 245, 263},
             {1, 1, 2, 5, 6, 10},
             {1, 12, 15, 20, 22, 25},
             {1, 27, 29, 34, 45, 47},
             {1, 55, 58, 62, 67, 69},
             {1, 74, 77, 79, 80, 81},
             {1, 82, 83, 85, 88, 89},
             {1, 100, 111, 116, 126, 129},
             {1, 130, 136, 138, 139, 140},
             {1, 144, 145, 167, 170, 176},
             {1, 177, 178, 179, 180, 182},
             {1, 189, 190, 191, 194, 196},
             {1, 197, 210, 211, 223, 243},
             {1, 245, 250, 251, 260, 261},
             {1, 263, 264, 265, 266, 278}};

    const uint64_t arr[] = {
        1, 2, 5, 6, 10, 12, 15, 20, 22,
        25, 27, 29, 34, 45, 47, 55, 58, 62,
        67, 69, 74, 77, 79, 80, 81, 82, 83,
        85, 88, 89, 100, 111, 116, 126, 129,
        130, 136, 138, 139, 140, 144, 145,
        167, 170, 176, 177, 178, 179, 180,
        182, 189, 190, 191, 194, 196, 197,
        210, 211, 223, 243, 245, 250, 251,
        260, 261, 263, 264, 265, 266, 278,
        0};

    for (int i = 0; arr[i] != 0; ++i) {
        int rc = mbptree_append(tree, arr[i], u64(arr[i]));
        ASSERT(rc == 0);
    }

    ASSERT(mbptree_compare(tree, (const uint64_t*)result, branch_factor));
    mbptree_free(tree);
}

TEST(mbptree3) {
    const int branch_factor = 5;
    mbptree_t* tree = mbptree_init(branch_factor);
    ASSERT(tree != 0);

    const uint64_t result[][5] =
        {{0, 136, 0, 0, 0},
          {0, 34, 81, 0, 0},
          {0, 180, 245, 0, 0},
            {0, 10, 22, 0, 0},
            {0, 58, 74, 0, 0},
            {0, 88, 116, 0, 0},
            {0, 144, 176, 0, 0},
            {0, 191, 210, 0, 0},
            {0, 261, 266, 0, 0},
              {1, 1, 2, 5, 6},
              {1, 10, 12, 15, 20},
              {1, 22, 25, 27, 29},
              {1, 34, 45, 47, 55},
              {1, 58, 62, 67, 69},
              {1, 74, 77, 79, 80},
              {1, 81, 82, 83, 85},
              {1, 88, 89, 100, 111},
              {1, 116, 126, 129, 130},
              {1, 136, 138, 139, 140},
              {1, 144, 145, 167, 170},
              {1, 176, 177, 178, 179},
              {1, 180, 182, 189, 190},
              {1, 191, 194, 196, 197},
              {1, 210, 211, 223, 243},
              {1, 245, 250, 251, 260},
              {1, 261, 263, 264, 265},
              {1, 266, 278}};

    const uint64_t arr[] = {
        1, 2, 5, 6, 10, 12, 15, 20, 22,
        25, 27, 29, 34, 45, 47, 55, 58, 62,
        67, 69, 74, 77, 79, 80, 81, 82, 83,
        85, 88, 89, 100, 111, 116, 126, 129,
        130, 136, 138, 139, 140, 144, 145,
        167, 170, 176, 177, 178, 179, 180,
        182, 189, 190, 191, 194, 196, 197,
        210, 211, 223, 243, 245, 250, 251,
        260, 261, 263, 264, 265, 266, 278,
        0};

    for (int i = 0; arr[i] != 0; ++i) {
        int rc = mbptree_append(tree, arr[i], u64(arr[i]));
        ASSERT(rc == 0);
    }

    ASSERT(mbptree_compare(tree, (const uint64_t*)result, branch_factor));
    mbptree_free(tree);
}
