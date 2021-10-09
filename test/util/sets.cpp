/*
 * Copyright (c) 2021, the Basil authors
 * All rights reserved.
 *
 * This source code is licensed under the 3-Clause BSD License, the full text
 * of which can be found in the LICENSE file in the root directory
 * of this project.
 */

#include "test.h"
#include "util/sets.h"
#include "util/vec.h"

TEST(empty_set) {
    bitset b;
    ASSERT_FALSE(b.contains(0));
    ASSERT_FALSE(b.contains(1));
    ASSERT_FALSE(b.contains(4000));
}

TEST(one_element) {
    bitset b;
    b.insert(3);
    ASSERT_TRUE(b.contains(3));
    ASSERT_FALSE(b.contains(2));
    ASSERT_FALSE(b.contains(7));
    
    b.erase(3);
    ASSERT_FALSE(b.contains(3));
}

TEST(hundred_elements) {
    bitset b;
    for (u32 i = 0; i < 100; i ++)
        b.insert(i);

    for (u32 i = 0; i < 100; i ++)
        ASSERT_TRUE(b.contains(i));

    for (u32 i = 0; i < 100; i += 2)
        b.erase(i);
    
    for (u32 i = 0; i < 100; i += 2) {
        ASSERT_FALSE(b.contains(i));
        ASSERT_TRUE(b.contains(i + 1));
    }
}

TEST(copy_ctor) {
    bitset a;
    a.insert(10);
    a.insert(40);

    bitset b = a;
    a.erase(40);
    ASSERT_FALSE(a.contains(40));
    ASSERT_TRUE(b.contains(40));

    a.insert(70);
    ASSERT_TRUE(a.contains(70));
    ASSERT_FALSE(b.contains(70));

    b.erase(10);
    ASSERT_TRUE(a.contains(10));
    ASSERT_FALSE(b.contains(10));
}

TEST(iteration) {
    vector<u32> results;

    bitset a;
    for (u32 i = 0; i < 10; i ++) if (i % 3 == 0) a.insert(i);

    for (u32 i : a) results.push(i);
    ASSERT_EQUAL(results.size(), 4);
    ASSERT_EQUAL(results[0], 0);
    ASSERT_EQUAL(results[1], 3);
    ASSERT_EQUAL(results[2], 6);
    ASSERT_EQUAL(results[3], 9);

    a.erase(0);
    a.erase(3);
    a.insert(45);
    results.clear();
    for (u32 i : a) results.push(i);
    ASSERT_EQUAL(results.size(), 3);
    ASSERT_EQUAL(results[0], 6);
    ASSERT_EQUAL(results[1], 9);
    ASSERT_EQUAL(results[2], 45);
}

TEST(duplicates) {
    bitset a;
    ASSERT_TRUE(a.insert(4));
    ASSERT_TRUE(a.contains(4));
    ASSERT_FALSE(a.insert(4));

    ASSERT_TRUE(a.erase(4));
    ASSERT_FALSE(a.contains(4));
    ASSERT_FALSE(a.erase(4));
}

TEST(clear) {
    bitset b;
    b.insert(1);
    b.insert(2);
    b.insert(3);

    ASSERT_TRUE(b.contains(1));
    ASSERT_TRUE(b.contains(2));
    ASSERT_TRUE(b.contains(3));

    b.clear();
    ASSERT_FALSE(b.contains(1));
    ASSERT_FALSE(b.contains(2));
    ASSERT_FALSE(b.contains(3));
}