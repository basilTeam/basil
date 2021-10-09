/*
 * Copyright (c) 2021, the Basil authors
 * All rights reserved.
 *
 * This source code is licensed under the 3-Clause BSD License, the full text
 * of which can be found in the LICENSE file in the root directory
 * of this project.
 */

#include "util/ustr.h"
#include "test.h"

TEST(size) {
    ustring a = "abc";
    ASSERT_EQUAL(a.size(), 3);
    ustring b = "λλ";
    ASSERT_EQUAL(b.size(), 2);
    a += "λ";
    ASSERT_EQUAL(a.size(), 4);
    ustring c = "👽👽";
    ASSERT_EQUAL(c.size(), 2);
    ustring d = b + c;
    ASSERT_EQUAL(d, "λλ👽👽");
    ASSERT_EQUAL(d.size(), 4);
}

// 00000000000000000000001110110011

TEST(add_char) {
    ustring a;
    ASSERT_EQUAL(a.size(), 0);
    a += 'a';
    ASSERT_EQUAL(a.size(), 1);
    a += 'b';
    ASSERT_EQUAL(a.size(), 2);
    a += rune(0x3B3);
    ASSERT_EQUAL(a, "abγ");
    ASSERT_EQUAL(a.size(), 3);
    a += 'd';
    ASSERT_EQUAL(a, "abγd");
    ASSERT_EQUAL(a.size(), 4);
}

TEST(add_literal) {
    ustring a = "abc";
    a += "def";
    ASSERT_EQUAL(a.size(), 6);
    ASSERT_EQUAL(a, "abcdef");
    a += "γδε";
    ASSERT_EQUAL(a, "abcdefγδε");
    ASSERT_EQUAL(a.size(), 9);
    ustring b = "zyx";
    ustring t = a;
    a = b;
    b = t;
    ASSERT_EQUAL(a, "zyx");
    ASSERT_EQUAL(a.size(), 3);
    ASSERT_EQUAL(b, "abcdefγδε");
    ASSERT_EQUAL(b.size(), 9);
    ASSERT_EQUAL((a + b).size(), 12);
}

TEST(iterate) {
    ustring a = "abc😋的了和def";
    rune runes[10];
    u32 i = 0;
    for (rune r : a) runes[i ++] = r;
    ASSERT_EQUAL(a.size(), 10);
    ASSERT_EQUAL(runes[0], 0x0061);
    ASSERT_EQUAL(runes[1], 0x0062);
    ASSERT_EQUAL(runes[2], 0x0063);
    ASSERT_EQUAL(runes[3], 0x1F60B);
    ASSERT_EQUAL(runes[4], 0x7684);
    ASSERT_EQUAL(runes[5], 0x4E86);
    ASSERT_EQUAL(runes[6], 0x548C);
    ASSERT_EQUAL(runes[7], 0x0064);
    ASSERT_EQUAL(runes[8], 0x0065);
    ASSERT_EQUAL(runes[9], 0x0066);
}