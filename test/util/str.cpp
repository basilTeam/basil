/*
 * Copyright (c) 2021, the Basil authors
 * All rights reserved.
 *
 * This source code is licensed under the 3-Clause BSD License, the full text
 * of which can be found in the LICENSE file in the root directory
 * of this project.
 */

#include "util/str.h"
#include "test.h"

TEST(from_literal) {
    string a = "hello";
    string b = "hello";
    ASSERT_EQUAL(a, b);
}

TEST(add_char) {
    string a = "h";
    ASSERT_EQUAL(a.size(), 1);
    a += 'e';
    a += 'l';
    ASSERT_EQUAL(a, "hel");
    ASSERT_EQUAL(a.size(), 3);
    a[0] = 'y';
    a += 'l';
    a += 'o';
    ASSERT_EQUAL(a, "yello");
    ASSERT_EQUAL(a.size(), 5);
}

TEST(add_string) {
    string a, b;
    ASSERT_EQUAL(a.size(), 0);
    a += "abc";
    b += "def";
    ASSERT_EQUAL(a.size(), 3);
    ASSERT_EQUAL(b.size(), 3);
    string t = a;
    a = b;
    b = t;
    ASSERT_EQUAL(a, "def");
    ASSERT_EQUAL(b, "abc");
    ASSERT_EQUAL(b + a, "abcdef");
    ASSERT_EQUAL(a + b, "defabc");
    ASSERT_EQUAL((a + b).size(), 6);
}

TEST(deep_copy) {
    string a = "hello";
    string b = a;
    a[0] = 'y';
    ASSERT_EQUAL(b, "hello");
    b += "!!!";
    ASSERT_EQUAL(a.size(), 5);
    ASSERT_EQUAL(b.size(), 8);
    ASSERT_EQUAL(a, "yello");
    ASSERT_EQUAL(b, "hello!!!");
}

TEST(deep_assign) {
    string a = "hello";
    string b = "world";
    ASSERT_EQUAL(a, "hello");
    ASSERT_EQUAL(b, "world");
    b = a;
    ASSERT_EQUAL(b, "hello");
    ASSERT_EQUAL(a, b);
    a[0] = 'y';
    ASSERT_EQUAL(a, "yello");
    ASSERT_EQUAL(b, "hello");
    ASSERT_NOT_EQUAL(a, b);
}

TEST(compare_literal) {
    string a = "cat";
    ASSERT_LESS(a, "dog");
    ASSERT_LESS(a, "category");
    ASSERT_GREATER(a, "ca");
    ASSERT_EQUAL(a, "cat");
    ASSERT_NOT_EQUAL(a, "apple");
    ASSERT_LESS_OR_EQUAL(a, "cat");
    ASSERT_GREATER_OR_EQUAL(a, "ball");
}

TEST(compare_string) {
    string a = "cat", b = "dog", c = "ball", d = "apple";
    ASSERT_LESS(d, c);
    ASSERT_LESS(d, a);
    ASSERT_GREATER(a, c);
    ASSERT_GREATER_OR_EQUAL(b, c);
    ASSERT_EQUAL(a, a);
    ASSERT_NOT_EQUAL(a, b);
}

TEST(random_access) {
    string a = "ampersand";
    ASSERT_EQUAL(a[0], 'a');
    ASSERT_EQUAL(a[8], 'd');
    ASSERT_EQUAL(a[4], 'r');
    a[0] = 'y';
    a[2] = 'm';
    ASSERT_EQUAL(a[0], 'y');
    ASSERT_EQUAL(a[2], 'm');
    a += 't';
    ASSERT_EQUAL(a[9], 't');
}

TEST(endswith) {
    string a = "a";
    ASSERT_TRUE(a.endswith('a'));
    a[0] = 'b';
    ASSERT_TRUE(a.endswith('b'));
    a += 'c';
    ASSERT_TRUE(a.endswith('c'));
    ASSERT_FALSE(a.endswith('a'));
    a = "kitkat";
    ASSERT_TRUE(a.endswith('t'));
}

TEST(slice) {
    string a = "abcdef";
    string b{a[{0, 3}]}, c{a[{3, 6}]};
    ASSERT_EQUAL(b, "abc");
    ASSERT_EQUAL(c, "def");
    ASSERT_EQUAL(b.size(), 3);
    ASSERT_EQUAL(c.size(), 3);
    string t = b;
    b = c;
    c = t;
    ASSERT_EQUAL(b, "def");
    ASSERT_EQUAL(c, "abc");
    ASSERT_EQUAL(c + b, "abcdef");
}