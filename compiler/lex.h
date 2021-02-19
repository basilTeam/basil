#ifndef BASIL_LEX_H
#define BASIL_LEX_H

#include "source.h"
#include "util/defs.h"

namespace basil {
    enum TokenType : u8 {
        T_NONE,
        T_INT,
        T_SYMBOL,
        T_STRING,
        T_COEFF,
        T_FLOAT,
        T_LPAREN,
        T_RPAREN,
        T_ACCESS,
        T_LBRACK,
        T_RBRACK,
        T_LBRACE,
        T_RBRACE,
        T_SEMI,
        T_DOT,
        T_COMMA,
        T_COLON,
        T_PIPE,
        T_PLUS,
        T_MINUS,
        T_QUOTE,
        T_NEWLINE,
        NUM_TOKEN_TYPES
    };

    struct Token {
        const_slice<u8> value;
        TokenType type;
        u32 line, column;

        Token(TokenType type_in, const const_slice<u8>& value_in, u32 line_in, u32 column_in);
        operator bool() const;
    };

    class TokenView {
        vector<Token>* _tokens;
        u32 i;
        Source* _source;
        bool _repl;

      public:
        TokenView(vector<Token>& tokens, Source& source, bool repl = false);

        const Token& peek();
        const Token& read();
        void rewind();
        operator bool() const;
        bool repl() const;
        void expand();
    };

    Token scan(Source::View& view, bool follows_space = false);
} // namespace basil

void write(stream& io, const basil::Token& t);

#endif
