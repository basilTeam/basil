/*
 * Copyright (c) 2021, the Basil authors
 * All rights reserved.
 *
 * This source code is licensed under the 3-Clause BSD License, the full text
 * of which can be found in the LICENSE file in the root directory
 * of this project.
 */

#include "test.h"
#include "ssa.h"
#include "driver.h"

using namespace basil;

SETUP {
    init();
    get_perf_info().set_max_count(3); // compile everything
}

TEST(simple_increment) {
    rc<IRFunction> inc = compile(R"(
do:
    def inc x? = x + 1
    inc 1
)", load_step, lex_step, parse_step, eval_step, ast_step, ssa_step)[symbol_from("inc")];

    ASSERT_EQUAL(inc->blocks.size(), 2); // should have entry + exit
}

TEST(multiple_assignments) {
    rc<IRFunction> main = compile(R"(
do:
    def x = 0
    x = 1
    x = 2
    x = x + 1 + x
)", load_step, lex_step, parse_step, eval_step, ast_step, ssa_step)[symbol_from("#main")];
    auto i1 = main->entry->insns[0], i2 = main->entry->insns[1];

    // the first and second assignments to x should be identical
    ASSERT_TRUE(i1->dest);
    ASSERT_TRUE(i2->dest);
    ASSERT_TRUE(i1->dest->kind == IK_VAR);
    ASSERT_TRUE(i2->dest->kind == IK_VAR);
    ASSERT_EQUAL(i1->dest->data.var, i2->dest->data.var);

    enforce_ssa(main);
    // println("");
    // println(main);

    // after enforcing ssa, the two assignments to x should have different ids
    ASSERT_NOT_EQUAL(i1->dest->data.var, i2->dest->data.var);
}

TEST(simple_phi) {
    rc<IRFunction> main = compile(R"(
do:
    def x = 0
    def y = 0
    x = 1
    if x == 1 then
        x = 2
    else
        x = 3
    y = x + 1
)", load_step, lex_step, parse_step, eval_step, ast_step, ssa_step)[symbol_from("#main")];

    enforce_ssa(main);
}

TEST(simple_loop) {
    rc<IRFunction> main = compile(R"(
do:
    def x = 0
    x = 1
    while x < 10 do:
        x = x + 1
    def y = x
)", load_step, lex_step, parse_step, eval_step, ast_step, ssa_step)[symbol_from("#main")];

    enforce_ssa(main);
    println(main);
}