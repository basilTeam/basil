/*
 * Copyright (c) 2021, the Basil authors
 * All rights reserved.
 *
 * This source code is licensed under the 3-Clause BSD License, the full text
 * of which can be found in the LICENSE file in the root directory
 * of this project.
 */

#ifndef BASIL_TEST_H
#define BASIL_TEST_H

#include "util/vec.h"
#include "util/hash.h"
#include "util/io.h"
#include "util/ustr.h"
#include "jasmine/target.h"

bool __internal_require(jasmine::Architecture arch);

#define onlyin(arch) if (__internal_require((arch))) return;

struct exc_message {
    ustring msg;
};

struct test_node {
    ustring name;
    void(*callback)();
    test_node* next;
};

extern test_node* test_list;
extern void (*setup_fn)();

int main(int argc, char** argv);

#define TEST(name) void __test_##name(); \
    auto __dummy_##name = (test_list = new test_node{#name, __test_##name, test_list}); \
    void __test_##name()

#define LET1(a) auto _a = (a);
#define LET2(a, b) auto _a = (a); auto _b = (b);
#define ASSERT_EQUAL(a, b) {LET2(a, b); if (_a != _b) throw exc_message{format<ustring>("line ", __LINE__, ": ", _a, " != ", _b)};}
#define ASSERT_NOT_EQUAL(a, b) {LET2(a, b); if (_a == _b) throw exc_message{format<ustring>("line ", __LINE__, ": ", _a, " == ", _b)};}
#define ASSERT_LESS(a, b) {LET2(a, b); if (_a >= _b) throw exc_message{format<ustring>("line ", __LINE__, ": ", _a, " >= ", _b)};}
#define ASSERT_GREATER(a, b) {LET2(a, b); if (_a <= _b) throw exc_message{format<ustring>("line ", __LINE__, ": ", _a, " <= ", _b)};}
#define ASSERT_LESS_OR_EQUAL(a, b) {LET2(a, b); if (_a > _b) throw exc_message{format<ustring>("line ", __LINE__, ": ", _a, " > ", _b)};}
#define ASSERT_GREATER_OR_EQUAL(a, b) {LET2(a, b); if (_a < _b) throw exc_message{format<ustring>("line ", __LINE__, ": ", _a, " < ", _b)};}
#define ASSERT_TRUE(a) {LET1(a); if (!_a) throw exc_message{format<ustring>("line ", __LINE__, ": ", (bool)_a, " was not true")};}
#define ASSERT_FALSE(a) {LET1(a); if (_a) throw exc_message{format<ustring>("line ", __LINE__, ": ", (bool)_a, " was not false")};}
#define ASSERT_NO_ERRORS(src) { if (error_count()) { buffer b; print_errors(b, (src)); discard_errors(); throw exc_message{format<ustring>("line ", __LINE__, ": errors were reported:\n", b)}; } }

#define SETUP void __setup_fn(); auto __setup_dummy = (setup_fn = __setup_fn); void __setup_fn() 

#endif