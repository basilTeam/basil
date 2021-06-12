#include "token.h"
#include "test.h"
#include "errors.h"

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

TEST(ints) {
    Source src = create_source("1 21 003 4647");
    Source::View view(src);
    optional<Token> a = lex(view), b = lex(view), c = lex(view), d = lex(view);
    ASSERT_EQUAL(error_count(), 0);

    ASSERT_EQUAL(a->kind, TK_INT);
    ASSERT_EQUAL(a->contents, symbol_from("1"));

    ASSERT_EQUAL(b->kind, TK_INT);
    ASSERT_EQUAL(b->contents, symbol_from("21"));
    
    ASSERT_EQUAL(c->kind, TK_INT);
    ASSERT_EQUAL(c->contents, symbol_from("003"));

    ASSERT_EQUAL(d->kind, TK_INT);
    ASSERT_EQUAL(d->contents, symbol_from("4647"));
}

TEST(floats) {
    Source src = create_source("1.0 2.22 31.13 00.000");
    Source::View view(src);
    optional<Token> a = lex(view), b = lex(view), c = lex(view), d = lex(view);
    ASSERT_EQUAL(error_count(), 0);

    ASSERT_EQUAL(a->kind, TK_FLOAT);
    ASSERT_EQUAL(a->contents, symbol_from("1.0"));

    ASSERT_EQUAL(b->kind, TK_FLOAT);
    ASSERT_EQUAL(b->contents, symbol_from("2.22"));

    ASSERT_EQUAL(c->kind, TK_FLOAT);
    ASSERT_EQUAL(c->contents, symbol_from("31.13"));

    ASSERT_EQUAL(d->kind, TK_FLOAT);
    ASSERT_EQUAL(d->contents, symbol_from("00.000"));
}

TEST(int_coeffs) {
    Source src = create_source("1x 02y 3(4z)");
    Source::View view(src);
    optional<Token> a = lex(view), b = lex(view), c = lex(view), d = lex(view), e = lex(view),
        f = lex(view), g = lex(view), h = lex(view), i = lex(view);
    ASSERT_EQUAL(error_count(), 0);

    ASSERT_EQUAL(a->kind, TK_INTCOEFF);
    ASSERT_EQUAL(a->contents, symbol_from("1"));

    ASSERT_EQUAL(b->kind, TK_SYMBOL);
    ASSERT_EQUAL(b->contents, symbol_from("x"));

    ASSERT_EQUAL(c->kind, TK_INTCOEFF);
    ASSERT_EQUAL(c->contents, symbol_from("02"));

    ASSERT_EQUAL(d->kind, TK_SYMBOL);
    ASSERT_EQUAL(d->contents, symbol_from("y"));

    ASSERT_EQUAL(e->kind, TK_INTCOEFF);
    ASSERT_EQUAL(e->contents, symbol_from("3"));

    ASSERT_EQUAL(f->kind, TK_LPAREN);

    ASSERT_EQUAL(g->kind, TK_INTCOEFF);
    ASSERT_EQUAL(g->contents, symbol_from("4"));

    ASSERT_EQUAL(h->kind, TK_SYMBOL);
    ASSERT_EQUAL(h->contents, symbol_from("z"));

    ASSERT_EQUAL(i->kind, TK_RPAREN);
}

TEST(float_coeffs) {
    Source src = create_source("0.1x 2.004(4) 00100.11011[y]");
    Source::View view(src);

    optional<Token> a = lex(view), b = lex(view), c = lex(view), d = lex(view), e = lex(view),
        f = lex(view), g = lex(view), h = lex(view), i = lex(view), j = lex(view);
    ASSERT_EQUAL(error_count(), 0);

    ASSERT_EQUAL(a->kind, TK_FLOATCOEFF);
    ASSERT_EQUAL(a->contents, symbol_from("0.1"));

    ASSERT_EQUAL(b->kind, TK_SYMBOL);
    ASSERT_EQUAL(b->contents, symbol_from("x"));

    ASSERT_EQUAL(c->kind, TK_FLOATCOEFF);
    ASSERT_EQUAL(c->contents, symbol_from("2.004"));

    ASSERT_EQUAL(d->kind, TK_LPAREN);

    ASSERT_EQUAL(e->kind, TK_INT);
    ASSERT_EQUAL(e->contents, symbol_from("4"));

    ASSERT_EQUAL(f->kind, TK_RPAREN);

    ASSERT_EQUAL(g->kind, TK_FLOATCOEFF);
    ASSERT_EQUAL(g->contents, symbol_from("00100.11011"));

    ASSERT_EQUAL(h->kind, TK_LSQUARE);

    ASSERT_EQUAL(i->kind, TK_SYMBOL);
    ASSERT_EQUAL(i->contents, symbol_from("y"));

    ASSERT_EQUAL(j->kind, TK_RSQUARE);
}

TEST(separators) {
    Source src = create_source("([x)]y{}z\\w\\\n");
    Source::View view(src);

    optional<Token> a = lex(view), b = lex(view), c = lex(view), d = lex(view), e = lex(view),
        f = lex(view), g = lex(view), h = lex(view), i = lex(view), j = lex(view), k = lex(view),
        l = lex(view), m = lex(view);
    ASSERT_EQUAL(error_count(), 0);

    ASSERT_EQUAL(a->kind, TK_LPAREN);
    ASSERT_EQUAL(b->kind, TK_LSQUARE);
    ASSERT_EQUAL(c->kind, TK_SYMBOL);
    ASSERT_EQUAL(d->kind, TK_RPAREN);
    ASSERT_EQUAL(e->kind, TK_RSQUARE);
    ASSERT_EQUAL(f->kind, TK_SYMBOL);
    ASSERT_EQUAL(g->kind, TK_LBRACE);
    ASSERT_EQUAL(h->kind, TK_RBRACE);
    ASSERT_EQUAL(i->kind, TK_SYMBOL);
    ASSERT_EQUAL(j->kind, TK_SPLICE);
    ASSERT_EQUAL(k->kind, TK_SYMBOL);
    ASSERT_EQUAL(l->kind, TK_SPLICE);
    ASSERT_EQUAL(m->kind, TK_NEWLINE);
}

TEST(symbols) {
    Source src = create_source("abc AbC x2 y_3 $something + -- : . ... :: := %2");
    Source::View view(src);

    optional<Token> a = lex(view), b = lex(view), c = lex(view), d = lex(view), e = lex(view), f = lex(view),
        g = lex(view), h = lex(view), i = lex(view), j = lex(view), k = lex(view), l = lex(view), m = lex(view);
    
    ASSERT_EQUAL(error_count(), 0);
    
    ASSERT_EQUAL(a->kind, TK_SYMBOL);
    ASSERT_EQUAL(a->contents, symbol_from("abc"));

    ASSERT_EQUAL(b->kind, TK_SYMBOL);
    ASSERT_EQUAL(b->contents, symbol_from("AbC"));

    ASSERT_EQUAL(c->kind, TK_SYMBOL);
    ASSERT_EQUAL(c->contents, symbol_from("x2"));

    ASSERT_EQUAL(d->kind, TK_SYMBOL);
    ASSERT_EQUAL(d->contents, symbol_from("y_3"));

    ASSERT_EQUAL(e->kind, TK_SYMBOL);
    ASSERT_EQUAL(e->contents, symbol_from("$something"));

    ASSERT_EQUAL(f->kind, TK_SYMBOL);
    ASSERT_EQUAL(f->contents, symbol_from("+"));

    ASSERT_EQUAL(g->kind, TK_SYMBOL);
    ASSERT_EQUAL(g->contents, symbol_from("--"));

    ASSERT_EQUAL(h->kind, TK_SYMBOL);
    ASSERT_EQUAL(h->contents, symbol_from(":"));

    ASSERT_EQUAL(i->kind, TK_SYMBOL);
    ASSERT_EQUAL(i->contents, symbol_from("."));

    ASSERT_EQUAL(j->kind, TK_SYMBOL);
    ASSERT_EQUAL(j->contents, symbol_from("..."));

    ASSERT_EQUAL(k->kind, TK_SYMBOL);
    ASSERT_EQUAL(k->contents, symbol_from("::"));

    ASSERT_EQUAL(l->kind, TK_SYMBOL);
    ASSERT_EQUAL(l->contents, symbol_from(":="));

    ASSERT_EQUAL(m->kind, TK_SYMBOL);
    ASSERT_EQUAL(m->contents, symbol_from("%2"));
}

TEST(eof) {
    Source src = create_source("abc");
    Source::View view(src);

    optional<Token> a = lex(view), b = lex(view), c = lex(view), d = lex(view);
    ASSERT_EQUAL(error_count(), 0);

    ASSERT_EQUAL(a->kind, TK_SYMBOL);
    ASSERT_EQUAL(a->contents, symbol_from("abc"));

    ASSERT_EQUAL(b->kind, TK_NEWLINE);

    ASSERT_FALSE(c);
    ASSERT_FALSE(d);
}

TEST(block_colon) {
    Source src = create_source("do: :: 2.1: (:3): 4");
    Source::View view(src);

    optional<Token> a = lex(view), b = lex(view), c = lex(view), d = lex(view),
        e = lex(view), f = lex(view), 
        g = lex(view), 
        h = lex(view), i = lex(view), 
        j = lex(view), k = lex(view);
    ASSERT_EQUAL(error_count(), 0);

    ASSERT_EQUAL(a->kind, TK_SYMBOL);
    ASSERT_EQUAL(a->contents, symbol_from("do"));

    ASSERT_EQUAL(b->kind, TK_BLOCK);

    ASSERT_EQUAL(c->kind, TK_SYMBOL);
    ASSERT_EQUAL(c->contents, symbol_from("::"));

    ASSERT_EQUAL(d->kind, TK_FLOAT);
    ASSERT_EQUAL(d->contents, symbol_from("2.1"));

    ASSERT_EQUAL(e->kind, TK_BLOCK);

    ASSERT_EQUAL(f->kind, TK_LPAREN);

    println(g->contents);
    ASSERT_EQUAL(g->kind, TK_QUOTE);

    ASSERT_EQUAL(h->kind, TK_INT);
    ASSERT_EQUAL(h->contents, symbol_from("3"));

    ASSERT_EQUAL(i->kind, TK_RPAREN);

    ASSERT_EQUAL(j->kind, TK_BLOCK);

    ASSERT_EQUAL(k->kind, TK_INT);
    ASSERT_EQUAL(k->contents, symbol_from("4"));
}