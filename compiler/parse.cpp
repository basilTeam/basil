/*
 * Copyright (c) 2021, the Basil authors
 * All rights reserved.
 *
 * This source code is licensed under the 3-Clause BSD License, the full text
 * of which can be found in the LICENSE file in the root directory
 * of this project.
 */

#include "parse.h"
#include "driver.h"

namespace basil {
    struct ParseContext {
        u32 prev_indent, indent;
        optional<TokenKind> enclosing;
    };

    // converts a symbol to an integer constant
    i64 to_int(Source::Pos pos, Symbol s) {
        const ustring& u = string_from(s);
        i64 acc = 0;
        for (rune r : u) {
            i32 val = utf8_digit_value(r);
            if (unicode_error()) {
                panic("Incorrectly-formatted integer constant!");
            }
            if (acc > 9223372036854775807l / 10) {
                err(pos, "Integer constant is too big! Must be less than 9,223,372,036,854,775,807.");
                return 0;
            }
            acc *= 10;
            acc += val;
        }
        return acc;
    }

    // converts a symbol to a floating-point constant
    double to_float(Source::Pos pos, Symbol s) {
        const ustring& u = string_from(s);
        double acc = 0, i = 0;
        for (rune r : u) {
            if (r == '.') {
                i = acc, acc = 0; // store accumulator in i, reset for fractional part
            }
            else {
                i32 val = utf8_digit_value(r);
                if (unicode_error()) {
                    panic("Incorrectly-formatted floating-point constant!");
                }
                acc *= 10;
                acc += val;
            }
        }
        double denom = 1;
        while (denom <= acc) denom *= 10; // find correct power of 10 for fractional part
        return i + (acc / denom); 
    }

    Value parse_expr(TokenView& view, ParseContext ctx);
    Value parse_suffix(TokenView& view, ParseContext ctx);

    Value parse_enclosed(Source::Pos begin, TokenKind closer, TokenView& view, ParseContext ctx) {
        vector<Value> values;
        while (!out_of_input(view) && view.peek().kind != closer) {
            values.push(parse_expr(view, ctx));
            while (!out_of_input(view) && view.peek().kind == TK_NEWLINE) view.read(); // consume blank lines
        }
        if (view.peek().kind != closer) {
            err(view.peek().pos, "Missing closing punctuation - expected '", closer, "'.");
            return v_error(view.peek().pos);
        }
        Source::Pos end = view.peek().pos;
        view.read(); // consume terminator
        if (values.size() == 0) return v_void(begin);
        return v_list(span(begin, end), t_list(T_ANY), move(values));
    }  

    Value parse_indented(optional<Value> opener, TokenView& view, ParseContext ctx) {
        vector<Value> values;
        if (opener) values.push(*opener);
        while (!out_of_input(view) 
            && ((view.peek().kind == TK_NEWLINE && !is_repl()) 
                || view.peek().pos.col_start > ctx.prev_indent)) {
            if (ctx.enclosing && view.peek().kind == *ctx.enclosing) break; // we've hit the end of some outer block
            if (view.peek().kind != TK_NEWLINE) values.push(parse_expr(view, ctx));
            else {
                view.read();
                continue;
            }
        }
        return v_list(span(values.front().pos, values.back().pos), t_list(T_ANY), move(values));
    }

    // Parses an indented or inline block after we've seen that a block should start.
    Value parse_block(TokenView& view, ParseContext ctx, optional<Value> opener) {
        Source::Pos initial = view.peek().pos;
        if (view && view.peek().kind == TK_NEWLINE) { // consider indented block
            while (view.peek().kind == TK_NEWLINE) view.read(); // consume all leading newlines
            if (!out_of_input(view) && view.peek().pos.col_start > ctx.indent)
                return parse_indented(opener, view, ParseContext{ctx.indent, u16(view.peek().pos.col_start), ctx.enclosing});
            else return v_error(initial);
        }
        else { // inline block
            vector<Value> values;
            if (opener) values.push(*opener);
            while (view && view.peek().kind != TK_NEWLINE) {
                if (ctx.enclosing && view.peek().kind == *ctx.enclosing) break; // we've hit the end of some outer block
                Value v = parse_expr(view, ctx);
                if (v.pos.line_start > initial.line_end) break; // stop if we're on a different line
                values.push(v);
            }
            return v_list(span(values.front().pos, values.back().pos), t_list(T_ANY), move(values));
        }
    }

    // Pulls a simple expression from the token stream.
    Value parse_primary(TokenView& view, ParseContext ctx) {
        if (!view) {
            err(view.tokens.back().pos, "Unexpected end of file.");
            return v_error(view.peek().pos);
        }
        
        Source::Pos pos = view.peek().pos;
        Symbol contents = view.peek().contents;
        switch (view.read().kind) {
            case TK_INT:
                return v_int(pos, to_int(pos, contents));
            case TK_FLOAT:
                return v_double(pos, to_float(pos, contents));
            case TK_SYMBOL:
                return v_symbol(pos, contents);
            case TK_STRING:
                return v_string(pos, string_from(contents));
            case TK_CHAR:
                return v_char(pos, string_from(contents).front());
            case TK_INTCOEFF: {
                Value next = parse_expr(view, ctx);
                return v_list(span(pos, next.pos), t_list(T_ANY),  // intcoeff term => (* intcoeff term)
                    v_symbol(pos, S_TIMES),
                    v_int(pos, to_int(pos, contents)),
                    next
                );
            }
            case TK_FLOATCOEFF: {
                Value next = parse_expr(view, ctx);
                return v_list(span(pos, next.pos), t_list(T_ANY),  // floatcoeff term => (* floatcoeff term)
                    v_symbol(pos, S_TIMES),
                    v_double(pos, to_float(pos, contents)),
                    next
                );
            }
            case TK_PLUS: {
                Value next = parse_expr(view, ctx);
                return v_list(span(pos, next.pos), t_list(T_ANY),  // + term => (+ 0 term)
                    v_symbol(pos, S_PLUS),
                    v_int(pos, 0),
                    next
                );
            }
            case TK_MINUS: {
                Value next = parse_expr(view, ctx);
                return v_list(span(pos, next.pos), t_list(T_ANY),  // - term => (- 0 term)
                    v_symbol(pos, S_MINUS),
                    v_int(pos, 0),
                    next
                );
            }
            case TK_QUOTE: {
                Value next = parse_expr(view, ctx);
                return v_list(span(pos, next.pos), t_list(T_ANY),  // : term => (quote term)
                    v_symbol(pos, S_QUOTE),
                    next
                );
            }
            case TK_LPAREN: 
                return parse_enclosed(pos, TK_RPAREN, view, 
                    ParseContext{ctx.prev_indent, ctx.indent, some<TokenKind>(TK_RPAREN)}
                );
            case TK_LSQUARE: {
                Value enclosed = parse_enclosed(pos, TK_RSQUARE, view, 
                    ParseContext{ctx.prev_indent, ctx.indent, some<TokenKind>(TK_RSQUARE)}
                );
                return v_cons(enclosed.pos, t_list(T_ANY),  // [x y z] => (array x y z)
                    v_symbol(pos, S_LIST),
                    enclosed
                );
            }
            case TK_LBRACE: {
                Value enclosed = parse_enclosed(pos, TK_RBRACE, view, 
                    ParseContext{ctx.prev_indent, ctx.indent, some<TokenKind>(TK_RBRACE)}
                ); // {x y z} => (set x y z)
                return v_cons(enclosed.pos, t_list(T_ANY),
                    v_symbol(pos, S_ARRAY),
                    enclosed
                );
            }
            case TK_SPLICE: {
                Value enclosed = parse_enclosed(pos, TK_SPLICE, view, ctx); // \x y z\ => (splice x y z)
                return v_cons(enclosed.pos, t_list(T_ANY),
                    v_symbol(pos, S_SPLICE),
                    enclosed
                );
            }
            case TK_BLOCK: {
                return parse_block(view, ctx, none<Value>());
            }
            default:
                err(pos, "Unexpected token '", escape(string_from(contents)), "'.");
                return v_error(pos);
        }
    }

    // Pulls an expression from the token stream, handling any attached access brackets.
    Value parse_suffix(TokenView& view, ParseContext ctx) {
        Value primary = parse_primary(view, ctx);
        while (view && view.peek().kind == TK_ACCESS) {
            Source::Pos pos = view.peek().pos;
            view.read(); // consume access bracket
            Value indices = parse_enclosed(pos, TK_RSQUARE, view, 
                ParseContext{ctx.prev_indent, ctx.indent, some<TokenKind>(TK_RSQUARE)}
            );
            primary = v_list(span(primary.pos, indices.pos), t_list(T_ANY), // foo[bar] = (at foo (array bar))
                primary,
                v_symbol(indices.pos, S_AT),
                v_cons(indices.pos, t_list(T_ANY),
                    v_symbol(indices.pos, S_ARRAY),
                    indices
                )
            );
        }
        return primary;
    }

    // Pulls a full expression from the token stream, handling any indented blocks.
    Value parse_expr(TokenView& view, ParseContext ctx) {
        while (!out_of_input(view) && view.peek().kind == TK_NEWLINE) view.read(); // consume leading newlines
        Value suffixed = parse_suffix(view, ctx);
        if (view && view.peek().kind == TK_BLOCK && !suffixed.type.of(K_LIST)) { // consider block
            if (suffixed.type == T_SYMBOL && 
                (suffixed.data.sym == S_ASSIGN || suffixed.data.sym == S_OF || suffixed.data.sym == S_CASE_ARROW)) {
                return suffixed;
            }
            view.read();
            suffixed = parse_block(view, ctx, some<Value>(suffixed));
        }
        return suffixed;
    }

    optional<Value> parse(TokenView& view) {
        while (view.peek().kind == TK_NEWLINE) view.read(); // consume leading newlines
        if (out_of_input(view)) return none<Value>(); // return void for empty source

        // bit of a workaround to find indent
        u32 indent = 0;
        for (i64 i = i64(view.i); i < view.tokens.size() && i >= 0; i --) {
            if (view.tokens[i].pos.line_start == view.peek().pos.line_start) {
                indent = view.tokens[i].pos.col_start; // earlier token on same line
            }
            else break;
        }

        Value v = parse_expr(view, ParseContext{0, indent, none<TokenKind>()});
        return v.type == T_ERROR ? none<Value>() : some<Value>(v);
    }
}