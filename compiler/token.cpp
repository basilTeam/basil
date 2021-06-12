#include "token.h"
#include "utf8.h"
#include "errors.h"

namespace basil {
    const char* TOKEN_NAMES[NUM_TOKEN_KINDS] = {
        "(", ")", "[", "[", "]", "{", "}", "<newline>", "\\",
        "int-coeff", "float-coeff", ":", "plus", "minus", "quote",
        "int", "float", "symbol", "string", "char",
        "none"
    };

    TokenView::TokenView(const vector<Token>& tokens_in):
        tokens(tokens_in), eof(Token{tokens.back().pos, S_NONE, TK_NONE}), i(0) {}

    TokenView::operator bool() const {
        return i < tokens.size();
    }

    const Token& TokenView::peek() const {
        if (i >= tokens.size()) {
            return eof;
        }
        return tokens[i];
    }

    const Token& TokenView::read() {
        return tokens[i ++];
    }

    bool is_space(rune r) {
        return utf8_is_separator(r) || r == '\t' || r == '\n' || r == ' ' || r == '\v';
    }

    bool is_separator(rune r) {
        return is_space(r) || utf8_is_punctuation_open(r) || utf8_is_punctuation_close(r) || utf8_is_initial_quote(r) || utf8_is_final_quote(r)
            || r == '"' || r == '\'' || r == ',' || r == '.' || r == ';' || r == '\\' || r == '\0';
    }

    bool is_opener(rune r) {
        return utf8_is_punctuation_open(r) || utf8_is_initial_quote(r) || r == '"' || r == '\'' || r == '\\';
    }

    bool is_digit(rune r) {
        return utf8_is_digit(r); // Nd character class
    }

    bool is_letter(rune r) {
        return utf8_is_mark(r) || utf8_is_letter(r);
    }

    bool is_sigil(rune r) {
        return utf8_is_connector(r) || utf8_is_dash(r) || utf8_is_other_punctuation(r) || utf8_is_symbol(r);
    }

    TokenKind SINGLETON_KINDS[128] = {
        TK_NONE, TK_NONE, TK_NONE, TK_NONE, TK_NONE, TK_NONE, TK_NONE, TK_NONE, // 00 - 07
        TK_NONE, TK_NONE, TK_NEWLINE, TK_NONE, TK_NONE, TK_NONE, TK_NONE, TK_NONE, // 08 - 0f
        TK_NONE, TK_NONE, TK_NONE, TK_NONE, TK_NONE, TK_NONE, TK_NONE, TK_NONE, // 10 - 17
        TK_NONE, TK_NONE, TK_NONE, TK_NONE, TK_NONE, TK_NONE, TK_NONE, TK_NONE, // 18 - 1f
        TK_NONE, TK_NONE, TK_NONE, TK_NONE, TK_NONE, TK_NONE, TK_NONE, TK_NONE, // 20 - 27
        TK_LPAREN, TK_RPAREN, TK_NONE, TK_NONE, TK_NONE, TK_NONE, TK_NONE, TK_NONE, // 28 - 2f
        TK_NONE, TK_NONE, TK_NONE, TK_NONE, TK_NONE, TK_NONE, TK_NONE, TK_NONE, // 30 - 37
        TK_NONE, TK_NONE, TK_NONE, TK_NONE, TK_NONE, TK_NONE, TK_NONE, TK_NONE, // 38 - 3f
        TK_NONE, TK_NONE, TK_NONE, TK_NONE, TK_NONE, TK_NONE, TK_NONE, TK_NONE, // 40 - 47
        TK_NONE, TK_NONE, TK_NONE, TK_NONE, TK_NONE, TK_NONE, TK_NONE, TK_NONE, // 48 - 4f
        TK_NONE, TK_NONE, TK_NONE, TK_NONE, TK_NONE, TK_NONE, TK_NONE, TK_NONE, // 50 - 57
        TK_NONE, TK_NONE, TK_NONE, TK_LSQUARE, TK_SPLICE, TK_RSQUARE, TK_NONE, TK_NONE, // 58 - 5f
        TK_NONE, TK_NONE, TK_NONE, TK_NONE, TK_NONE, TK_NONE, TK_NONE, TK_NONE, // 60 - 67
        TK_NONE, TK_NONE, TK_NONE, TK_NONE, TK_NONE, TK_NONE, TK_NONE, TK_NONE, // 68 - 6f
        TK_NONE, TK_NONE, TK_NONE, TK_NONE, TK_NONE, TK_NONE, TK_NONE, TK_NONE, // 70 - 77
        TK_NONE, TK_NONE, TK_NONE, TK_LBRACE, TK_NONE, TK_RBRACE, TK_NONE, TK_NONE // 78 - 7f
    };

    Symbol SINGLETON_SYMS[128] = {
        S_NONE, S_NONE, S_NONE, S_NONE, S_NONE, S_NONE, S_NONE, S_NONE, // 00 - 07
        S_NONE, S_NONE, S_NEWLINE, S_NONE, S_NONE, S_NONE, S_NONE, S_NONE, // 08 - 0f
        S_NONE, S_NONE, S_NONE, S_NONE, S_NONE, S_NONE, S_NONE, S_NONE, // 10 - 17
        S_NONE, S_NONE, S_NONE, S_NONE, S_NONE, S_NONE, S_NONE, S_NONE, // 18 - 1f
        S_NONE, S_NONE, S_NONE, S_NONE, S_NONE, S_NONE, S_NONE, S_NONE, // 20 - 27
        S_LPAREN, S_RPAREN, S_NONE, S_NONE, S_NONE, S_NONE, S_NONE, S_NONE, // 28 - 2f
        S_NONE, S_NONE, S_NONE, S_NONE, S_NONE, S_NONE, S_NONE, S_NONE, // 30 - 37
        S_NONE, S_NONE, S_NONE, S_NONE, S_NONE, S_NONE, S_NONE, S_NONE, // 38 - 3f
        S_NONE, S_NONE, S_NONE, S_NONE, S_NONE, S_NONE, S_NONE, S_NONE, // 40 - 47
        S_NONE, S_NONE, S_NONE, S_NONE, S_NONE, S_NONE, S_NONE, S_NONE, // 48 - 4f
        S_NONE, S_NONE, S_NONE, S_NONE, S_NONE, S_NONE, S_NONE, S_NONE, // 50 - 57
        S_NONE, S_NONE, S_NONE, S_LSQUARE, S_BACKSLASH, S_RSQUARE, S_NONE, S_NONE, // 58 - 5f
        S_NONE, S_NONE, S_NONE, S_NONE, S_NONE, S_NONE, S_NONE, S_NONE, // 60 - 67
        S_NONE, S_NONE, S_NONE, S_NONE, S_NONE, S_NONE, S_NONE, S_NONE, // 68 - 6f
        S_NONE, S_NONE, S_NONE, S_NONE, S_NONE, S_NONE, S_NONE, S_NONE, // 70 - 77
        S_NONE, S_NONE, S_NONE, S_LBRACE, S_NONE, S_RBRACE, S_NONE, S_NONE // 78 - 7f
    };

    // Returns the hex digit value of the provided rune, or -1 if the rune is not a hex digit.
    i32 hex_value(rune r) {
        if (r >= '0' && r <= '9') return r - '0';
        if (r >= 'A' && r <= 'F') return 10 + (r - 'A');
        if (r >= 'a' && r <= 'a') return 10 + (r - 'a');
        return -1;
    }

    // Returns the actual rune corresponding to the escape sequence starting at the view.
    // The backslash is already read when we enter this function.
    rune escape_seq(Source::View& view) {
        Source::Pos pos = view.pos();
        rune ch = view.read();
        switch (ch) {
            case 't': return '\t';
            case 'n': return '\n';
            case 'v': return '\v';
            case 'r': return '\r';
            case '\\': return '\\';
            case '"': return '"';
            case '\'': return '\'';
            case 'u': { // unicode escape sequence - \uXXXX
                rune acc = 0;
                for (int i = 0; i < 4; i ++) {
                    i32 hex = hex_value(view.peek());
                    if (hex == -1) {
                        err(view.pos(), "Expected hexadecimal digit in unicode escape sequence, ",
                            "found non-digit character '", view.peek(), "'.");
                        view.read();
                        return 0;
                    }
                    else {
                        acc *= 16;
                        acc += hex;
                        view.read();
                    }
                }
                return acc;
            }
            default:
                err(pos, "Invalid escape sequence '", ch, "'.");
                return 0;
        }
    }

    bool is_block_colon(Source::View& view) {
        return is_space(view.peek(1)) || is_opener(view.peek(1));
    }

    optional<Token> lex(Source::View& view) {
        Token result{view.pos(), S_NONE, TK_NONE};
        rune ch = view.peek();
        while (ch == '#') { // comments
            while (ch && ch != '\n') view.read(), ch = view.peek();
        }
        bool has_leading_space = false;
        while (ch != '\n' && is_space(ch)) view.read(), ch = view.peek(); // consume leading spaces (but not newlines)
        
        if (!ch) {
            return none<Token>();
        }

        Source::Pos begin = view.pos(), end = view.pos();

        if (ch < 128 && SINGLETON_KINDS[ch] != TK_NONE) // singleton characters like parens and brackets
            if (ch == '[' && view.last() != '\0' && !is_space(view.last()) && !is_opener(view.last())
                && !is_digit(view.last()) && view.last() != '+' && view.last() != '-' && view.last() != ':') // access bracket
                result = Token{view.pos(), S_LSQUARE, TK_ACCESS}, view.read();
            else 
                result = Token{view.pos(), SINGLETON_SYMS[ch], SINGLETON_KINDS[ch]}, view.read();
        else if (ch == '\'') {
            ustring acc;
            view.read(); // consume leading quote
            switch (view.peek()) {
                case '\'': err(view.pos(), "Character literal must contain at least one character."); break;
                case '\n': err(view.pos(), "Character literal may not contain a line break."); break;
                case '\0': err(view.pos(), "Unexpected end of input."); break;
                case '\\': view.read(), acc += escape_seq(view); break; // escape sequence break;
                default: acc += view.read(); // normal char
            }
            if (view.peek() && view.peek() != '\'') {
                err(view.pos(), "Expected closing quote in character literal, found '", view.peek(), "'.");
                return some<Token>(result);
            } 
            else {
                end = view.pos();
                view.read(); // consume trailing quote
                result = Token{span(begin, end), symbol_from(acc), TK_CHAR};
            }
        }
        else if (ch == '\"') {
            ustring acc;
            view.read(); // consume leading quote
            while (view.peek() && view.peek() != '\"') {
                if (view.peek() == '\n') err(view.pos(), "String literal may not contain a line break.");
                else if (view.peek() == '\\') view.read(), acc += escape_seq(view); // escape sequence
                else acc += view.read(); // normal char
            }
            end = view.pos();
            if (view.peek() == '\0') err(view.pos(), "Unexpected end of input.");
            else if (view.peek() != '\"') 
                err(view.pos(), "Expected closing quote in string literal, found '", view.peek(), "'.");
            else view.read(); // consume trailing quote
            result = Token{span(begin, view.pos()), symbol_from(acc), TK_STRING};
        }
        else if (is_digit(ch)) {
            ustring acc;
            bool floating = false;
            while (is_digit(ch)) end = view.pos(), acc += view.read(), ch = view.peek();
            if (ch == '.') {
                floating = true;
                end = view.pos(), acc += view.read(), ch = view.peek(); // consume dot
                if (!is_digit(ch)) {
                    err(view.pos(), "Expected at least one digit after decimal point.");
                    return none<Token>();
                }
                while (is_digit(ch)) end = view.pos(), acc += view.read(), ch = view.peek();
            }
            // whether it's an integer or float constant, we should be done reading the numeric portion
            if (is_opener(ch) || is_letter(ch))
                result = Token{span(begin, end), symbol_from(acc), floating ? TK_FLOATCOEFF : TK_INTCOEFF};
            else if (is_separator(ch) 
                || (ch == ':' && is_block_colon(view))) // block colons are special, since : is not a separator but it can terminate numbers
                result = Token{span(begin, end), symbol_from(acc), floating ? TK_FLOAT : TK_INT};
            else {
                err(view.pos(), "Unexpected character in numeric literal: '", ch, "'.");
                return none<Token>();
            }
        }
        else if (is_letter(ch) || is_sigil(ch)) {
            ustring acc;
            bool skip_symbol = false;
            switch (ch) {
                case '_':
                    err(view.pos(), "Symbols may not begin with underscores.");
                    view.read();
                    return none<Token>();
                case '+': // prefix plus
                    end = view.pos(), acc += view.read(), ch = view.peek();
                    if (is_letter(ch) || is_opener(ch)) 
                        skip_symbol = true, result = Token{span(begin, end), S_PLUS, TK_PLUS};
                    break;
                case '-': // prefix minus
                    end = view.pos(), acc += view.read(), ch = view.peek();
                    if (is_letter(ch) || is_opener(ch)) 
                        skip_symbol = true, result = Token{span(begin, end), S_MINUS, TK_MINUS};
                    break;
                case ':': // prefix quote or block colon  
                    if (view.last() != '\0' && !is_space(view.last()) 
                        && !is_opener(view.last()) && !is_sigil(view.last()) 
                        && is_block_colon(view)) { // block colon
                        skip_symbol = true, result = Token{view.pos(), S_COLON, TK_BLOCK};
                        view.read();
                    }
                    else { // prefix quote or normal ident
                        end = view.pos(), acc += view.read(), ch = view.peek();
                        if (is_letter(ch) || is_digit(ch) || is_opener(ch)) 
                            skip_symbol = true, result = Token{span(begin, end), S_COLON, TK_QUOTE};
                    }
                    break;
                default:
                    if (is_separator(ch)) {
                        rune repeated = ch; // identifiers that start with a separator are legal if
                                            // they only consist of that separator
                        skip_symbol = true; // we want to skip normal symbol stuff
                        while (ch == repeated) 
                            end = view.pos(), acc += view.read(), ch = view.peek();
                        result = Token{span(begin, end), symbol_from(acc), TK_SYMBOL};
                    }
            }
            if (!skip_symbol) { // do normal symbol tokenization
                while ((is_letter(ch) || is_sigil(ch) || is_digit(ch)) && !is_separator(ch)) {
                    if (ch == ':' && acc.back() != ':') break; // block colon ends identifiers that don't end with colon
                    end = view.pos(), acc += view.read(), ch = view.peek();
                }
                result = Token{span(begin, end), symbol_from(acc), TK_SYMBOL};
            }
        }

        return some<Token>(result);
    }

    vector<Token> lex_all(Source::View& view) {
        vector<Token> tokens;

        while (view.peek()) {
            auto t = lex(view);
            if (t) tokens.push(*t);
        }

        return tokens;
    }
}

void write(stream& io, basil::TokenKind tk) {
    write(io, basil::TOKEN_NAMES[tk]);
}

void write(stream& io, const basil::Token& t) {
    write(io, t.pos, " \"", escape(string_from(t.contents)), "\" : ", t.kind);
}