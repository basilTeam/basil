/*
 * Copyright (c) 2021, the Basil authors
 * All rights reserved.
 *
 * This source code is licensed under the 3-Clause BSD License, the full text
 * of which can be found in the LICENSE file in the root directory
 * of this project.
 */

#include "forms.h"
#include "value.h"
#include "test.h"
#include "env.h"

using namespace basil;

SETUP {
    init_types_and_symbols();
}

TEST(term) {
    rc<Form> f = F_TERM;

    ASSERT_FALSE(f->is_invokable()); // terms should not be invokable
    ASSERT_EQUAL(f->kind, FK_TERM); // terms should have term formkind
}

TEST(prefix_callable) {
    rc<Form> f1 = f_callable(0, ASSOC_RIGHT, P_SELF, p_var("x")), 
        f2 = f_callable(10, ASSOC_RIGHT, P_SELF, p_var("x"), p_var("y"));

    ASSERT_TRUE(f1->is_invokable()); // all of these should be invokable
    ASSERT_TRUE(f2->is_invokable());

    ASSERT_EQUAL(f1->precedence, 0); // make sure all of these forms have the given precedence
    ASSERT_EQUAL(f2->precedence, 10);

    auto f1m = f1->start();
    ASSERT_FALSE(f1m->is_finished());
    f1m->advance(v_symbol({}, symbol_from("f")));
    ASSERT_FALSE(f1m->is_finished()); // f1 should still need one argument
    f1m->advance(v_int({}, 1));
    ASSERT_TRUE(f1m->is_finished()); // f1 should be done after name and one value
    ASSERT_TRUE((bool)f1m->match()); // f1 should match name and one value
    ASSERT_TRUE(&*f1m->match() == &*f1m); // f1 should result in itself

    f1m = f1->start(); // we should be able to restart f1 with the same results
    ASSERT_FALSE(f1m->is_finished());
    f1m->advance(v_symbol({}, symbol_from("f")));
    ASSERT_FALSE(f1m->is_finished());
    f1m->advance(v_int({}, 1));
    ASSERT_TRUE(f1m->is_finished());
    ASSERT_TRUE(f1m->match());
    ASSERT_TRUE(&*f1m->match() == &*f1m);

    auto f2m = f2->start();
    ASSERT_FALSE(f2m->is_finished());
    f2m->advance(v_symbol({}, symbol_from("f")));
    ASSERT_FALSE(f2m->is_finished());
    f2m->advance(v_int({}, 1));
    ASSERT_FALSE(f2m->is_finished());
    f2m->advance(v_int({}, 2));
    ASSERT_TRUE(f2m->is_finished()); // f2 should be done after name and one value
    ASSERT_TRUE(f2m->match()); // f2 should match name and two values
    ASSERT_TRUE(&*f2m->match() == &*f2m); // f2 should result in itself
}

TEST(infix_callable) {
    rc<Form> f1 = f_callable(40, ASSOC_LEFT, p_var("x"), P_SELF, p_var("y"));

    ASSERT_TRUE(f1->is_invokable());
    ASSERT_EQUAL(f1->precedence, 40);

    auto f1m = f1->start(); // try again!
    ASSERT_FALSE(f1m->is_finished());
    f1m->advance(v_int({}, 1));
    f1m->advance(v_symbol({}, symbol_from("foo")));
    ASSERT_FALSE(f1m->is_finished()); // still need one more argument
    f1m->advance(v_symbol({}, symbol_from("x")));
    ASSERT_TRUE(f1m->is_finished());
    ASSERT_TRUE(f1m->match()); // this time we should match
    ASSERT_TRUE(&*f1m->match() == &*f1m); // and we should return f1
}