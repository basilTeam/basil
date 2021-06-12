#ifndef BASIL_TOKEN_H
#define BASIL_TOKEN_H

#include "source.h"
#include "type.h"

namespace basil {
    // Represents a distinct kind of token.
    enum TokenKind {
        TK_LPAREN,      // A left parenthese.
        TK_RPAREN,      // A right parenthese.
        TK_ACCESS,      // An access bracket.
        TK_LSQUARE,     // A left square bracket.
        TK_RSQUARE,     // A right square bracket.
        TK_LBRACE,      // A left curly brace.
        TK_RBRACE,      // A right curly brace.
        TK_NEWLINE,     // A newline or line break.
        TK_SPLICE,      // A splice character, aka '\'
        TK_INTCOEFF,    // An integer coefficient.
        TK_FLOATCOEFF,  // A rational/floating-point coefficient.
        TK_BLOCK,       // A postfix colon indicating the start of a block.
        TK_PLUS,        // A unary prefix plus sign.
        TK_MINUS,       // A unary prefix minus sign.
        TK_QUOTE,       // A unary prefix quote.
        TK_INT,         // An integer constant.
        TK_FLOAT,       // A rational/floating-point constant.
        TK_SYMBOL,      // A name or identifier.
        TK_STRING,      // A string constant.
        TK_CHAR,        // A character constant.
        TK_NONE,        // Marks a token as invalid.
        NUM_TOKEN_KINDS
    };

    // A single token of Basil source code.
    struct Token {
        Source::Pos pos;
        Symbol contents; // The text contents of this token.
        TokenKind kind; // What kind of token this is.
    };

    // Movable view over a vector of tokens. Provides a peek/read interface
    // for tokens to be used in the context-free parser.
    struct TokenView {
        const vector<Token>& tokens;
        Token eof;
        u32 i;

        // Constructs a TokenView at the beginning of the provided token vector.
        TokenView(const vector<Token>& tokens);

        // Returns whether or not there are still tokens to be read from this view.
        operator bool() const;

        // Returns the next token to be read by this view.
        const Token& peek() const;

        // Returns and moves past the next token in the token sequence.
        const Token& read();
    };

    // Consumes the next token available from the provided view, moving
    // it forward.
    // Returns a none optional if no token can be read.
    optional<Token> lex(Source::View& view);

    // Reads all available tokens from the provided view and returns them
    // in a vector.
    vector<Token> lex_all(Source::View& view);
}

void write(stream& io, basil::TokenKind tk);
void write(stream& io, const basil::Token& t);

#endif