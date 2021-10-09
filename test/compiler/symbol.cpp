/*
 * Copyright (c) 2021, the Basil authors
 * All rights reserved.
 *
 * This source code is licensed under the 3-Clause BSD License, the full text
 * of which can be found in the LICENSE file in the root directory
 * of this project.
 */

#include "type.h"
#include "test.h"

using namespace basil;

SETUP {
    init_types_and_symbols();
}

TEST(none_equality) {
    ASSERT_EQUAL(S_NONE, S_NONE);
}

TEST(no_duplicates) {
    Symbol a = symbol_from("hello"), b = symbol_from("hello"), c = symbol_from("world"), d = symbol_from("world");
    ASSERT_EQUAL(a, b);
    ASSERT_EQUAL(c, d);
    ASSERT_NOT_EQUAL(b, c);
}

TEST(symbol_to_string) {
    Symbol a = symbol_from("hello");
    ASSERT_EQUAL(string_from(a), "hello");
}