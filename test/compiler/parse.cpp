#include "test.h"
#include "parse.h"

using namespace basil;

SETUP {
    init_types_and_symbols();
}

template<typename ...Args>
Source create_source(const Args&... args) {
    buffer b;
    write(b, args...);
    return Source(b);
}

TEST(constants) {
    Source src = create_source("1 2.0 \'a\' \"abc\" foo");
    Source::View view(src);
    auto tokens = lex_all(view);
    TokenView tview(tokens);

    Value a = parse(tview);
    ASSERT_EQUAL(error_count(), 0);

    ASSERT_EQUAL(a, v_int(a.pos, 1));
}

TEST(variables) {
    Source src = create_source("x :: y = z_w");
    Source::View view(src);
    auto tokens = lex_all(view);
    TokenView tview(tokens);

    Value a = parse(tview), 
        b = parse(tview), 
        c = parse(tview),
        d = parse(tview), 
        e = parse(tview);
    ASSERT_EQUAL(error_count(), 0);

    ASSERT_EQUAL(a, v_symbol(a.pos, symbol_from("x")));
    ASSERT_EQUAL(b, v_symbol(a.pos, symbol_from("::")));
    ASSERT_EQUAL(c, v_symbol(a.pos, symbol_from("y")));
    ASSERT_EQUAL(d, v_symbol(a.pos, symbol_from("=")));
    ASSERT_EQUAL(e, v_symbol(a.pos, symbol_from("z_w")));
}

TEST(enclosing) {
    Source src = create_source("() (1) (2 \n(3)\n)");
    Source::View view(src);
    auto tokens = lex_all(view);
    TokenView tview(tokens);

    Value a = parse(tview), b = parse(tview), c = parse(tview);
    ASSERT_EQUAL(error_count(), 0);

    ASSERT_EQUAL(a, v_void(a.pos)); // ()
    ASSERT_EQUAL(b, v_list(b.pos, t_list(T_ANY), v_int(b.pos, 1))); // (1)
    ASSERT_EQUAL(c, v_list(c.pos, t_list(T_ANY), v_int(c.pos, 2),
        v_list(c.pos, t_list(T_ANY), v_int(c.pos, 3)))); // (2 (3))
}

TEST(array) {
    Source src = create_source("[] [ 1] [\"a\" b c ]");
    Source::View view(src);
    auto tokens = lex_all(view);
    TokenView tview(tokens);

    Value a = parse(tview), b = parse(tview), c = parse(tview);
    ASSERT_EQUAL(error_count(), 0);

    ASSERT_EQUAL(a, v_list(a.pos, t_list(T_ANY), v_symbol(a.pos, symbol_from("array"))));
    ASSERT_EQUAL(b, v_list(b.pos, t_list(T_ANY), v_symbol(b.pos, symbol_from("array")), v_int(b.pos, 1)));
    ASSERT_EQUAL(c, v_list(c.pos, t_list(T_ANY), v_symbol(c.pos, symbol_from("array")),
        v_string(c.pos, "a"), v_symbol(c.pos, symbol_from("b")), v_symbol(c.pos, symbol_from("c"))));
}