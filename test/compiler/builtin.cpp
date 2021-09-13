#include "driver.h"
#include "eval.h"
#include "test.h"

using namespace basil;

SETUP {
    init();
    get_perf_info().set_max_count(99999); // try and do everything comptime
}

TEST(arithmetic) {
    ASSERT_EQUAL(compile("1 + 2 * 3", load_step, lex_step, parse_step, eval_step), v_int({}, 7));
    ASSERT_EQUAL(compile("(1 + 2) * 3", load_step, lex_step, parse_step, eval_step), v_int({}, 9));
}

TEST(def_vars) {
    compile("def x = 1", load_step, lex_step, parse_step, eval_step);
    ASSERT_EQUAL(compile("x", load_step, lex_step, parse_step, eval_step), v_int({}, 1));
}

TEST(def_functions) {
    compile("def id x? = x", load_step, lex_step, parse_step, eval_step);
    ASSERT_EQUAL(compile("id 1", load_step, lex_step, parse_step, eval_step), v_int({}, 1));

    compile("def x? add y? = x + y", load_step, lex_step, parse_step, eval_step);
    ASSERT_EQUAL(compile("1 add 2", load_step, lex_step, parse_step, eval_step), v_int({}, 3));

    compile("def apply f? x? y? = x f y", load_step, lex_step, parse_step, eval_step);
    ASSERT_EQUAL(compile("apply add 1 2", load_step, lex_step, parse_step, eval_step), v_int({}, 3));

    ASSERT_EQUAL(compile(R"(
do:
    def inc x? = 
        def y = x
        y + 1
    def x = inc 1
    x
)", load_step, lex_step, parse_step, eval_step), v_int({}, 2));
}

TEST(def_variadic) {
    compile("def begin exprs...? end = exprs head", load_step, lex_step, parse_step, eval_step);
    ASSERT_EQUAL(compile("1 + begin 1 2 3 end + 4", load_step, lex_step, parse_step, eval_step), v_int({}, 6));

    compile("def sym-list :syms...? = syms", load_step, lex_step, parse_step, eval_step);
    ASSERT_EQUAL(compile("sym-list x y z", load_step, lex_step, parse_step, eval_step), v_list({}, t_list(T_SYMBOL),
        v_symbol({}, symbol_from("x")), v_symbol({}, symbol_from("y")), v_symbol({}, symbol_from("z"))
    ));
}

TEST(do) {
    ASSERT_EQUAL(compile("(do 1 2 3)", load_step, lex_step, parse_step, eval_step), v_int({}, 3));
    ASSERT_EQUAL(compile("do 1 2 3", load_step, lex_step, parse_step, eval_step), v_int({}, 3));
}

TEST(conditional_logic) {
    ASSERT_EQUAL(compile("if false and false or not false then 1 else 2", load_step, lex_step, parse_step, eval_step), v_int({}, 1));
    ASSERT_EQUAL(compile("if false then 1 else if true then 2 else 3", load_step, lex_step, parse_step, eval_step), v_int({}, 2));
}

TEST(string_manip) {
    ASSERT_EQUAL(compile("\"hello world\" length", load_step, lex_step, parse_step, eval_step), v_int({}, 11));
    ASSERT_EQUAL(compile("find 'o' \"hello world\"", load_step, lex_step, parse_step, eval_step), v_int({}, 4));
}

TEST(factorial) {
    ASSERT_EQUAL(compile(R"(
do:
    def x? factorial =
        if x == 0 then
            1
        else
            x - 1 factorial * x

    10 factorial

    )", load_step, lex_step, parse_step, eval_step), v_int({}, 3628800));
}

TEST(tuples) {
    // 1, 2, 3 should be a tuple of three ints
    ASSERT_EQUAL(compile("1, 2, 3", load_step, lex_step, parse_step, eval_step), 
        v_tuple({}, t_tuple(T_INT, T_INT, T_INT), 
            v_int({}, 1), v_int({}, 2), v_int({}, 3)
        ));

    // (1, 2), (3, 4) should be a tuple of two tuples, each having two ints
    ASSERT_EQUAL(compile("(1, 2), (3, 4)", load_step, lex_step, parse_step, eval_step), 
        v_tuple({}, t_tuple(t_tuple(T_INT, T_INT), t_tuple(T_INT, T_INT)), 
            v_tuple({}, t_tuple(T_INT, T_INT), v_int({}, 1), v_int({}, 2)), 
            v_tuple({}, t_tuple(T_INT, T_INT), v_int({}, 3), v_int({}, 4))
        ));
}

TEST(nonterminating) {
    get_perf_info().set_max_count(50);
    Value collatz = compile(R"(
do:
    def collatz n? =
        if n % 2 == 0 then
            collatz n / 2
        else
            collatz 3n + 1

    collatz 100
)", load_step, lex_step, parse_step, eval_step);
    ASSERT_TRUE(collatz.type.of(K_RUNTIME));
    ASSERT_EQUAL(t_runtime_base(collatz.type), T_UNDEFINED);
    get_perf_info().set_max_count(99999);
}