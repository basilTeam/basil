#include "parse.h"
#include "errors.h"

namespace basil {
    i64 parse_int(const string& s) {
        static buffer b;
        write(b, s);
        i64 i;
        read(b, i);
        return i;
    }

    string parse_string(const string& s) {
        string t;
        const u8* sptr = s.raw();
        sptr++; // skip initial quote
        while (*sptr && *sptr != '\"') {
            if (*sptr == '\\') {
                sptr++;
                switch (*sptr) {
                    case 'a': t += '\a'; break;
                    case 'b': t += '\b'; break;
                    case 'f': t += '\f'; break;
                    case 'n': t += '\n'; break;
                    case 'r': t += '\r'; break;
                    case 't': t += '\t'; break;
                    case 'v': t += '\v'; break;
                    case '0': t += '\0'; break;
                    case '"': t += '"'; break;
                    case '\'': t += '\''; break;
                    case '\\': t += '\\'; break;
                    case '?': t += '\?'; break;
                    case '\0': break;
                }
                sptr++;
            }
            t += *(sptr++);
        }
        sptr++; // skip final quote
        return t;
    }

    Value parse_primary(TokenView& view, u32 indent);
    void parse_line(TokenView& view, u32 indent, bool consume_line, vector<Value>& terms);

    bool out_of_input(TokenView& view) { // returns true if out of input
        if (view.repl()) {
            view.expand();
            return error_count() != 0; // if there are errors, act like out of input
        } else {
            err(view.peek(), "Unexpected end of input.");
            return true;
        }
    }

    SourceLocation parse_enclosed(TokenView& view, vector<Value>& terms, TokenType terminator, u32 indent) {
        vector<Value> tuple_elements;
        bool errored = false;
        while (view.peek().type != terminator) {
            while (view.peek().type == T_NEWLINE) view.read();
            if (!view.peek() && out_of_input(view)) return NO_LOCATION;
            if (view.peek().type == terminator) break;
            terms.push(parse(view, indent));
            if (view.peek().type == T_COMMA) {
                if (terminator != T_RPAREN && !errored) { // not enclosed by parens
                    err(view.peek(), "Tuple literals require parentheses."), errored = true;
                    tuple_elements.push(error());
                } else if (!errored) { // correct usage
                    tuple_elements.push(list_of(terms));
                    terms.clear();
                }
                view.read();
            }
        }
        SourceLocation result = view.peek();
        view.read();
        if (tuple_elements.size() > 0) {
            tuple_elements.push(list_of(terms));
            terms.clear();
            terms.push(Value("tuple-of"));
            for (const Value& v : tuple_elements) terms.push(v);
        }
        return result;
    }

    void parse_block(TokenView& view, vector<Value>& terms, u32 prev_indent, u32 indent) {
        while (view.peek().column > prev_indent) {
            if (view.peek().type != T_NEWLINE) parse_line(terms, view, indent, false);
            if (view.peek().type == T_NEWLINE) {
                if (view.peek().column <= prev_indent && view.repl()) {
                    view.read();
                    return;
                } else
                    view.read();
            }
            if (!view.peek() && (!view.repl() || out_of_input(view))) return;
        }
    }

    Value apply_op(TokenView& view, Value lhs, Value rhs, TokenType op) {
        switch (op) {
            case T_COLON: {
                Value v = list_of(Value("annotate"), lhs, rhs);
                v.set_location(span(lhs.loc(), rhs.loc()));
                return v;
            }
            case T_DOT: {
                Value v = list_of(Value("at", SYMBOL), lhs, list_of(Value("quote", SYMBOL), rhs));
                v.set_location(span(lhs.loc(), rhs.loc()));
                return v;
            }
            default:
                err(view.peek(), "Unexpected binary operator '", view.peek().value, "'.");
                view.read();
                return Value(ERROR);
        }
    }

    Value parse_binary(TokenView& view, Value lhs, TokenType prev_op, u32 indent) {
        Value rhs = parse_primary(view, indent);
        TokenType next_op = view.peek().type;
        if (next_op == T_COLON || next_op == T_DOT) {
            view.read();
            if (next_op == T_COLON && prev_op == T_DOT) // : has higher
                                                        // precedence
                return apply_op(view, lhs, parse_binary(view, rhs, next_op, indent), prev_op);
            else
                return parse_binary(view, apply_op(view, lhs, rhs, prev_op), next_op, indent);
        }
        return apply_op(view, lhs, rhs, prev_op);
    }

    Value parse_primary(TokenView& view, u32 indent) {
        SourceLocation first = view.peek();
        Value v;
        switch (view.peek().type) {
            case T_INT: {
                v = parse_int(view.read().value);
                v.set_location(first);
                break;
            }
            case T_SYMBOL: {
                v = Value(string(view.read().value), SYMBOL);
                v.set_location(first);
                break;
            }
            case T_STRING: {
                v = Value(parse_string(view.read().value), STRING);
                v.set_location(first);
                break;
            }
            case T_PLUS: {
                view.read();
                Value operand = parse_primary(view, indent);
                v = list_of(Value("+"), 0, operand);
                v.set_location(span(first, operand.loc()));
                break;
            }
            case T_MINUS: {
                view.read();
                Value operand = parse_primary(view, indent);
                v = list_of(Value("-"), 0, operand);
                v.set_location(span(first, operand.loc()));
                break;
            }
            case T_QUOTE: {
                view.read();
                Value operand = parse_primary(view, indent);
                v = list_of(Value("quote"), operand);
                v.set_location(span(first, operand.loc()));
                break;
            }
            case T_COEFF: {
                i64 i = parse_int(view.read().value);
                Value operand = parse_primary(view, indent);
                v = list_of(Value("*"), i, operand);
                v.set_location(span(first, operand.loc()));
                break;
            }
            case T_LPAREN: {
                view.read();
                vector<Value> terms;
                auto end = parse_enclosed(view, terms, T_RPAREN, indent);
                for (const Value& v : terms)
                    if (v.is_error()) return error();
                v = list_of(terms);
                v.set_location(span(first, end));
                break;
            }
            case T_LBRACK: {
                view.read();
                vector<Value> terms;
                terms.push(Value("list-of"));
                auto end = parse_enclosed(view, terms, T_RBRACK, indent);
                for (const Value& v : terms)
                    if (v.is_error()) return error();
                v = list_of(terms);
                v.set_location(span(first, end));
                break;
            }
            case T_LBRACE: {
                view.read();
                vector<Value> terms;
                terms.push(Value("set-of"));
                auto end = parse_enclosed(view, terms, T_RBRACE, indent);
                for (const Value& v : terms)
                    if (v.is_error()) return error();
                v = list_of(terms);
                v.set_location(span(first, end));
                break;
            }
            case T_PIPE: {
                view.read();
                vector<Value> terms;
                auto end = parse_enclosed(view, terms, T_PIPE, indent);
                for (const Value& v : terms)
                    if (v.is_error()) return error();
                v = cons(Value("splice"), list_of(terms));
                v.set_location(span(first, end));
                break;
            }
            default:
                err(view.peek(), "Unexpected token '", escape(view.peek().value), "'.");
                view.read();
                return error();
        }
        while (view.peek().type == T_ACCESS) {
            view.read();
            vector<Value> terms;
            terms.push(Value("list-of"));
            auto end = parse_enclosed(view, terms, T_RBRACK, indent);
            for (const Value& v : terms)
                if (v.is_error()) return error();
            Value indices = terms.size() == 2 ? terms[1] : list_of(terms);
            v = list_of(Value("at", SYMBOL), v, indices);
            v.set_location(span(first, end));
        }
        return v;
    }

    Value parse(TokenView& view, u32 indent) {
        Value v = parse_primary(view, indent);
        if (v.is_error()) return v;
        if (view.peek().type == T_DOT) view.read(), v = parse_binary(view, v, T_DOT, indent);
        if (view.peek().type == T_COLON) {
            view.read();
            // parse a labeled block
            v = parse_binary(view, v, T_COLON, indent);
        }
        return v;
    }

    void parse_line(TokenView& view, u32 indent, bool consume_line, vector<Value>& terms) {
        SourceLocation first = view.peek();
        while (view.peek()) {
            if (view.peek().type == T_NEWLINE) {
                view.read();
                if (!view.peek() && (!view.repl() || out_of_input(view))) return;
                if (view.peek().column > indent) {
                    vector<Value> body;
                    parse_block(view, body, indent, view.peek().column);
                    if (terms.size() > 0) terms.back() = cons(terms.back(), list_of(body));
                }
                else if (!consume_line)
                    view.rewind();

                return;
            }
            Value v = parse(view, indent);
            if (!v.is_error()) terms.push(v);
        }
    }

    void parse_line(vector<Value>& outer_terms, TokenView& view, u32 indent, bool consume_line) {
        SourceLocation first = view.peek();
        vector<Value> terms;
        if (view.peek().type == T_NEWLINE) {
            if (consume_line) view.read();
        }
        parse_line(view, indent, consume_line, terms);
        // Value line = terms.size() == 1 ? terms[0] : list_of(terms);
        // if (!line.is_void()) {
        //     line.set_location(span(terms.front().loc(), terms.back().loc()));
        //     outer_terms.push(line);
        // }
        for (const Value& v : terms) outer_terms.push(v);
    }
} // namespace basil