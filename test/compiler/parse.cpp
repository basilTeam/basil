/*
 * Copyright (c) 2021, the Basil authors
 * All rights reserved.
 *
 * This source code is licensed under the 3-Clause BSD License, the full text
 * of which can be found in the LICENSE file in the root directory
 * of this project.
 */

#include "test.h"
#include "driver.h"
#include "parse.h"

using namespace basil;

SETUP {
    init_types_and_symbols();
}

template<typename ...Args>
rc<Source> create_source(const Args&... args) {
    buffer b;
    write(b, args...);
    return ref<Source>(b);
}

TEST(constants) {
    rc<Source> src = create_source("1 2.0 \'a\' \"abc\" foo");
    Source::View view(*src);
    auto tokens = lex_all(view);
    TokenView tview(src, tokens);

    Value a = *parse(tview);
    ASSERT_EQUAL(error_count(), 0);

    ASSERT_EQUAL(a, v_int(a.pos, 1));
}

TEST(variables) {
    rc<Source> src = create_source("x :: y = z_w");
    Source::View view(*src);
    auto tokens = lex_all(view);
    TokenView tview(src, tokens);

    Value a = *parse(tview), 
        b = *parse(tview), 
        c = *parse(tview),
        d = *parse(tview), 
        e = *parse(tview);
    ASSERT_EQUAL(error_count(), 0);

    ASSERT_EQUAL(a, v_symbol(a.pos, symbol_from("x")));
    ASSERT_EQUAL(b, v_symbol(a.pos, symbol_from("::")));
    ASSERT_EQUAL(c, v_symbol(a.pos, symbol_from("y")));
    ASSERT_EQUAL(d, v_symbol(a.pos, symbol_from("=")));
    ASSERT_EQUAL(e, v_list(a.pos, t_list(T_ANY), v_symbol(a.pos, symbol_from("z_w"))));
}

TEST(enclosing) {
    rc<Source> src = create_source("() (1) (2 \n(3)\n)");
    Source::View view(*src);
    auto tokens = lex_all(view);
    TokenView tview(src, tokens);

    Value a = *parse(tview), b = *parse(tview), c = *parse(tview);
    ASSERT_EQUAL(error_count(), 0);

    ASSERT_EQUAL(a, v_void(a.pos)); // ()
    ASSERT_EQUAL(b, v_list(b.pos, t_list(T_ANY), v_int(b.pos, 1))); // (1)
    ASSERT_EQUAL(c, v_list(c.pos, t_list(T_ANY), v_int(c.pos, 2),
        v_list(c.pos, t_list(T_ANY), v_int(c.pos, 3)))); // (2 (3))
}

TEST(array) {
    rc<Source> src = create_source("[] [ 1] [\"a\" b c ]");
    Source::View view(*src);
    auto tokens = lex_all(view);
    TokenView tview(src, tokens);

    Value a = *parse(tview), b = *parse(tview), c = *parse(tview);
    ASSERT_EQUAL(error_count(), 0);

    ASSERT_EQUAL(a, v_list(a.pos, t_list(T_ANY), v_symbol(a.pos, symbol_from("array"))));
    ASSERT_EQUAL(b, v_list(b.pos, t_list(T_ANY), v_symbol(b.pos, symbol_from("array")), v_int(b.pos, 1)));
    ASSERT_EQUAL(c, v_list(c.pos, t_list(T_ANY), v_symbol(c.pos, symbol_from("array")),
        v_string(c.pos, "a"), v_symbol(c.pos, symbol_from("b")), v_symbol(c.pos, symbol_from("c"))));
}

TEST(indent) {
    rc<Source> src = create_source(R"(
    a:
        b c:
          d
        e f g:
            h i
            
    j)");
    Source::View view(*src);
    auto tokens = lex_all(view);
    TokenView tview(src, tokens);

    Value a = *parse(tview), b = *parse(tview);
    ASSERT_NO_ERRORS(src);

    ASSERT_EQUAL(a, compile("(a b (c d) e f (g h i))", load_step, lex_step, parse_step));
    ASSERT_EQUAL(b, v_symbol(b.pos, symbol_from("j")));
}

TEST(trailing_paren) {
    rc<Source> src = create_source("a (");
    Source::View view(*src);
    auto tokens = lex_all(view);
    TokenView tview(src, tokens);

    Value a = *parse(tview);
    ASSERT_NO_ERRORS(src);

    auto b = parse(tview);
    ASSERT_FALSE(b);
    ASSERT_TRUE(error_count());
    discard_errors();
}