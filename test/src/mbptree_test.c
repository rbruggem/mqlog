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
            if (*(arr + i * w + j + 1) != key) {
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
           {0, 15, 0},
             {0, 2, 0},
             {0, 6, 0},
             {0, 12, 0},
             {0, 20, 0},
               {1, 1, 0},
               {1, 2, 0},
               {1, 5, 0},
               {1, 6, 0},
               {1, 10, 0},
               {1, 12, 0},
               {1, 15, 0},
               {1, 20, 22}};

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
           {0, 33, 0},
             {0, 5, 0},
             {0, 15, 0},
             {0, 27, 0},
             {0, 35, 37},
               {0, 2, 0},
               {0, 6, 0},
               {0, 12, 0},
               {0, 20, 0},
               {0, 25, 0},
               {0, 30, 0},
               {0, 34, 0},
               {0, 36, 0},
               {0, 49, 55},
                 {1, 1, 0},
                 {1, 2, 0},
                 {1, 5, 0},
                 {1, 6, 0},
                 {1, 10, 0},
                 {1, 12, 0},
                 {1, 15, 0},
                 {1, 20, 0},
                 {1, 22, 0},
                 {1, 25, 0},
                 {1, 27, 0},
                 {1, 30, 0},
                 {1, 33, 0},
                 {1, 34, 0},
                 {1, 35, 0},
                 {1, 36, 0},
                 {1, 37, 0},
                 {1, 49, 0},
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
        {{0, 34, 81, 136, 180, 245},
           {0, 6, 15, 25, 0, 0},
           {0, 55, 67, 77, 0, 0},
           {0, 85, 100, 126, 0, 0},
           {0, 140, 167, 177, 0, 0},
           {0, 190, 196, 211, 0, 0},
           {0, 260, 264, 0, 0, 0},
             {1, 1, 2, 5, 0, 0},
             {1, 6, 10, 12, 0, 0},
             {1, 15, 20, 22, 0, 0},
             {1, 25, 27, 29, 0, 0},
             {1, 34, 45, 47, 0, 0},
             {1, 55, 58, 62, 0, 0},
             {1, 67, 69, 74, 0, 0},
             {1, 77, 79, 80, 0, 0},
             {1, 81, 82, 83, 0, 0},
             {1, 85, 88, 89, 0, 0},
             {1, 100, 111, 116, 0, 0},
             {1, 126, 129, 130, 0, 0},
             {1, 136, 138, 139, 0, 0},
             {1, 140, 144, 145, 0, 0},
             {1, 167, 170, 176, 0, 0},
             {1, 177, 178, 179, 0, 0},
             {1, 180, 182, 189, 0, 0},
             {1, 190, 191, 194, 0, 0},
             {1, 196, 197, 210, 0, 0},
             {1, 211, 223, 243, 0, 0},
             {1, 245, 250, 251, 0, 0},
             {1, 260, 261, 263, 0, 0},
             {1, 264, 265, 266, 278, 0}};

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
        {{0, 67, 136, 0, 0},
          {0, 15, 34, 0, 0},
          {0, 81, 100, 0, 0},
          {0, 167, 180, 196, 245},
            {0, 5, 10, 0, 0},
            {0, 22, 27, 0, 0},
            {0, 47, 58, 0, 0},
            {0, 74, 79, 0, 0},
            {0, 83, 88, 0, 0},
            {0, 116, 129, 0, 0},
            {0, 139, 144, 0, 0},
            {0, 176, 178, 0, 0},
            {0, 189, 191, 0, 0},
            {0, 210, 223, 0, 0},
            {0, 251, 261, 264, 0},
              {1, 1, 2, 0, 0},
              {1, 5, 6, 0, 0},
              {1, 10, 12, 0, 0},
              {1, 15, 20, 0, 0},
              {1, 22, 25, 0, 0},
              {1, 27, 29, 0, 0},
              {1, 34, 45, 0, 0},
              {1, 47, 55, 0, 0},
              {1, 58, 62, 0, 0},
              {1, 67, 69, 0, 0},
              {1, 74, 77, 0, 0},
              {1, 79, 80, 0, 0},
              {1, 81, 82, 0, 0},
              {1, 83, 85, 0, 0},
              {1, 88, 89, 0, 0},
              {1, 100, 111, 0, 0},
              {1, 116, 126, 0, 0},
              {1, 129, 130, 0, 0},
              {1, 136, 138, 0, 0},
              {1, 139, 140, 0, 0},
              {1, 144, 145, 0, 0},
              {1, 167, 170, 0, 0},
              {1, 176, 177, 0, 0},
              {1, 178, 179, 0, 0},
              {1, 180, 182, 0, 0},
              {1, 189, 190, 0, 0},
              {1, 191, 194, 0, 0},
              {1, 196, 197, 0, 0},
              {1, 210, 211, 0, 0},
              {1, 223, 243, 0, 0},
              {1, 245, 250, 0, 0},
              {1, 251, 260, 0, 0},
              {1, 261, 263, 0, 0},
              {1, 264, 265, 266, 278}};

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
