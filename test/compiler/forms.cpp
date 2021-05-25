#include "forms.h"
#include "value.h"
#include "test.h"

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
    rc<Form> f1 = f_callable(0, p_keyword(symbol_from("f")), P_VAR), 
        f2 = f_callable(10, p_keyword(symbol_from("f")), P_VAR, P_VAR);

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
    ASSERT_TRUE(&*f1m->match() == &*f1->invokable); // f1 should result in itself

    f1m = f1->start(); // we should be able to restart f1 with the same results
    ASSERT_FALSE(f1m->is_finished());
    f1m->advance(v_symbol({}, symbol_from("f")));
    ASSERT_FALSE(f1m->is_finished());
    f1m->advance(v_int({}, 1));
    ASSERT_TRUE(f1m->is_finished());
    ASSERT_TRUE(f1m->match());
    ASSERT_TRUE(&*f1m->match() == &*f1->invokable);

    auto f2m = f2->start();
    ASSERT_FALSE(f2m->is_finished());
    f2m->advance(v_symbol({}, symbol_from("f")));
    ASSERT_FALSE(f2m->is_finished());
    f2m->advance(v_int({}, 1));
    ASSERT_FALSE(f2m->is_finished());
    f2m->advance(v_int({}, 2));
    ASSERT_TRUE(f2m->is_finished()); // f2 should be done after name and one value
    ASSERT_TRUE(f2m->match()); // f2 should match name and two values
    ASSERT_TRUE(&*f2m->match() == &*f2->invokable); // f2 should result in itself
}

TEST(infix_callable) {
    rc<Form> f1 = f_callable(40, P_VAR, p_keyword(symbol_from("foo")), P_VAR);

    ASSERT_TRUE(f1->is_invokable());
    ASSERT_EQUAL(f1->precedence, 40);

    auto f1m = f1->start();
    ASSERT_FALSE(f1m->is_finished());
    f1m->advance(v_int({}, 1));
    ASSERT_FALSE(f1m->is_finished());
    f1m->advance(v_int({}, 2));
    ASSERT_TRUE(f1m->is_finished()); // should stop, expected "foo", got 2
    ASSERT_FALSE(f1m->match()); // shouldn't match

    f1m = f1->start(); // try again!
    ASSERT_FALSE(f1m->is_finished());
    f1m->advance(v_int({}, 1));
    f1m->advance(v_symbol({}, symbol_from("foo")));
    ASSERT_FALSE(f1m->is_finished()); // still need one more argument
    f1m->advance(v_symbol({}, symbol_from("x")));
    ASSERT_TRUE(f1m->is_finished());
    ASSERT_TRUE(f1m->match()); // this time we should match
    ASSERT_TRUE(&*f1m->match() == &*f1->invokable); // and we should return f1
}