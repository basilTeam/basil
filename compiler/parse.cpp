#include "parse.h"

namespace basil {
    struct ParseContext {
        u32 prev_indent, indent;
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
        while (denom < acc) denom *= 10; // find correct power of 10 for fractional part
        return i + (acc / denom); 
    }

    Value parse_expr(TokenView& view, ParseContext ctx);
    Value parse_suffix(TokenView& view, ParseContext ctx);

    Value parse_enclosed(Source::Pos begin, TokenKind closer, TokenView& view, ParseContext ctx) {
        vector<Value> values;
        while (view && view.peek().kind != closer) {
            values.push(parse_suffix(view, ctx));
            while (view.peek().kind == TK_NEWLINE) view.read(); // consume blank lines
        }
        if (view.peek().kind != closer) {
            err(view.peek().pos, "Missing closing punctuation - expected '", closer, "'.");
            return v_error(view.peek().pos);
        }
        Source::Pos end = view.peek().pos;
        view.read(); // consume terminator
        if (values.size() == 0) return v_void(begin);
        return v_list(span(begin, end), t_list(T_ANY), values);
    }  

    Value parse_indented(Value opener, TokenView& view, ParseContext ctx) {
        vector<Value> values;
        values.push(opener);
        while (view.peek().pos.col_start > ctx.prev_indent) {
            if (view.peek().kind != TK_NEWLINE) values.push(parse_expr(view, ctx));
            if (view.peek().kind == TK_NEWLINE) {
                view.read();
            }
            if (!view) break;
        }
        return v_list(span(values.front().pos, values.back().pos), t_list(T_ANY), values);
    }

    // Pulls a simple expression from the token stream.
    Value parse_primary(TokenView& view, ParseContext ctx) {
        if (!view) 
            panic("Attempted to get tokens from exhausted token view!");
        
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
            case TK_LPAREN: return parse_enclosed(pos, TK_RPAREN, view, ctx);
            case TK_LSQUARE: {
                Value enclosed = parse_enclosed(pos, TK_RSQUARE, view, ctx);
                return v_cons(enclosed.pos, t_list(T_ANY),  // [x y z] => (array x y z)
                    v_symbol(pos, S_ARRAY),
                    enclosed
                );
            }
            case TK_LBRACE: {
                Value enclosed = parse_enclosed(pos, TK_RBRACE, view, ctx); // {x y z} => (dict x y z)
                return v_cons(enclosed.pos, t_list(T_ANY),
                    v_symbol(pos, S_DICT),
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
            Value indices = parse_enclosed(pos, TK_RSQUARE, view, ctx);
            primary = v_list(span(primary.pos, indices.pos), t_list(T_ANY), // foo[bar] = (at foo (array bar))
                v_symbol(indices.pos, S_AT),
                primary,
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
        while (view.peek().kind == TK_NEWLINE) view.read(); // consume leading newlines
        Value suffixed = parse_suffix(view, ctx);
        if (view && view.peek().kind == TK_BLOCK) { // consider block
            view.read(); // consume block
            if (view && view.peek().kind == TK_NEWLINE) { // consider indented block
                while (view.peek().kind == TK_NEWLINE) view.read(); // consume all leading newlines
                if (view && view.peek().pos.col_start > ctx.indent)
                    return parse_indented(suffixed, view, ParseContext{ctx.indent, u16(view.peek().pos.col_start)});
                else {
                    err(view.peek().pos, 
                        "Expected indented block, but line isn't indented past the previous non-empty line.");
                    return v_error(view.peek().pos);
                }
            }
            else { // inline block
                vector<Value> values;
                values.push(suffixed);
                while (view && view.peek().kind != TK_NEWLINE) {
                    Value v = parse_expr(view, ctx);
                    if (v.pos.line_start > suffixed.pos.line_end) break; // stop if we're on a different line
                    values.push(v);
                }
                return v_list(span(values.front().pos, values.back().pos), t_list(T_ANY), values);
            }
        }
        return suffixed;
    }

    optional<Value> parse(TokenView& view) {
        while (view.peek().kind == TK_NEWLINE) view.read(); // consume leading newlines
        if (!view) return none<Value>(); // return void for empty source
        return some<Value>(parse_expr(view, ParseContext{0, u16(view.peek().pos.col_start)})); // start with indentation of first token
    }
}