/*
 * Copyright (c) 2021, the Basil authors
 * All rights reserved.
 *
 * This source code is licensed under the 3-Clause BSD License, the full text
 * of which can be found in the LICENSE file in the root directory
 * of this project.
 */

#include "source.h"
#include "test.h"

using namespace basil;

TEST(empty_source) {
    Source source;
    ASSERT_EQUAL(source.size(), 0);
}

TEST(from_stream) {
    buffer b;
    writeln(b, "abcdef");
    writeln(b, "ghi");
    Source source(b);
    ASSERT_EQUAL(source.size(), 2);
    ASSERT_EQUAL(source[0].size(), 7);
    ASSERT_EQUAL(source[1].size(), 4);
    ASSERT_EQUAL(source[0], "abcdef\n");
    ASSERT_EQUAL(source[1], "ghi\n");
    Source::Pos line1 = source.line_span(0), line2 = source.line_span(1);
    Source::Pos corr1 = { 0, 0, 0, 7 }; // [line 1 col 1, line 1 col 8) - includes newline
    Source::Pos corr2 = { 1, 0, 1, 4 }; // [line 2 col 1, line 2 col 5) - includes newline
    ASSERT_EQUAL(line1, corr1);
    ASSERT_EQUAL(line2, corr2);
}

TEST(from_file) {
    Source source("test/compiler/source-example");
    ASSERT_EQUAL(source.size(), 7);
    ASSERT_EQUAL(source[0], "abc def\n");
    ASSERT_EQUAL(source[1], "foo bar baz quux\n");
    ASSERT_EQUAL(source[2], "\n");
    ASSERT_EQUAL(source[3], "ghi\n");
    ASSERT_EQUAL(source[4], "\n");
    ASSERT_EQUAL(source[5], "\n");
    ASSERT_EQUAL(source[6], "fromage\n");
}

TEST(pos_printing) {
    buffer b;
    write(b, Source::Pos{ 0, 0, 0, 0 }); // line 1 col 1
    ASSERT_EQUAL(string(b), "[1:1]");
}

TEST(traverse_line) {
    buffer b;
    write(b, "abc");
    Source source(b);
    Source::View view(source); // create view at start of file
    ASSERT_EQUAL(view.line, 0);
    ASSERT_EQUAL(view.column, 0);
    ASSERT_EQUAL(view.peek(), 'a');
    ASSERT_EQUAL(view.read(), 'a');
    ASSERT_EQUAL(view.line, 0);
    ASSERT_EQUAL(view.column, 1);
    ASSERT_EQUAL(view.read(), 'b');
    ASSERT_EQUAL(view.read(), 'c');
    ASSERT_EQUAL(view.line, 0);
    ASSERT_EQUAL(view.column, 3);
    ASSERT_EQUAL(view.read(), '\n');
    ASSERT_EQUAL(view.line, 1);
    ASSERT_EQUAL(view.column, 0);
    ASSERT_EQUAL(view.read(), '\0');
    ASSERT_EQUAL(view.line, 1);
    ASSERT_EQUAL(view.column, 0);
    ASSERT_EQUAL(view.read(), '\0');
}

TEST(view_last) {
    buffer b;
    write(b, "abc");
    Source source(b);
    Source::View view(source);
    ASSERT_EQUAL(view.peek(), 'a');
    ASSERT_EQUAL(view.last(), '\0');
    view.read();
    ASSERT_EQUAL(view.peek(), 'b');
    ASSERT_EQUAL(view.last(), 'a');
    view.read();
    view.read();
    view.read();
    ASSERT_EQUAL(view.peek(), '\0');
    ASSERT_EQUAL(view.last(), '\n');
}