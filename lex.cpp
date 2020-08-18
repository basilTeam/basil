#include "lex.h"
#include "io.h"
#include "errors.h"
#include <cctype>
#include <cstdlib>

namespace basil {
  Token::Token(TokenType type_in, const const_slice<u8>& value_in, u32 line_in, u32 column_in):
    value(value_in), type(type_in), line(line_in), column(column_in) {}
  
  Token::operator bool() const {
    return type != T_NONE;
  }

  static const char* TOKEN_NAMES[NUM_TOKEN_TYPES] = {
    "none", "int", "symbol", "coeff", "left paren", "right paren",
    "left bracket", "right bracket", "left brace", "right brace",
    "semicolon", "dot", "colon", "pipe", "plus", "minus", "quote",
    "newline"
  };

  static Token NONE = Token(T_NONE, const_slice<u8>{0, nullptr}, 0, 0);

  static TokenType DELIMITERS[128] = {
    T_NONE, T_NONE, T_NONE, T_NONE, T_NONE, T_NONE, T_NONE, T_NONE, // 00
    T_NONE, T_NONE, T_NEWLINE, T_NONE, T_NONE, T_NONE, T_NONE, T_NONE, // 08
    T_NONE, T_NONE, T_NONE, T_NONE, T_NONE, T_NONE, T_NONE, T_NONE, // 10
    T_NONE, T_NONE, T_NONE, T_NONE, T_NONE, T_NONE, T_NONE, T_NONE, // 18
    T_NONE, T_NONE, T_NONE, T_NONE, T_NONE, T_NONE, T_NONE, T_NONE, // 20
    T_LPAREN, T_RPAREN, T_NONE, T_NONE, T_NONE, T_NONE, T_DOT, T_NONE, // 28
    T_NONE, T_NONE, T_NONE, T_NONE, T_NONE, T_NONE, T_NONE, T_NONE, // 30
    T_NONE, T_NONE, T_COLON, T_SEMI, T_NONE, T_NONE, T_NONE, T_NONE, // 38
    T_NONE, T_NONE, T_NONE, T_NONE, T_NONE, T_NONE, T_NONE, T_NONE, // 40
    T_NONE, T_NONE, T_NONE, T_NONE, T_NONE, T_NONE, T_NONE, T_NONE, // 48
    T_NONE, T_NONE, T_NONE, T_NONE, T_NONE, T_NONE, T_NONE, T_NONE, // 50
    T_NONE, T_NONE, T_NONE, T_LBRACK, T_NONE, T_RBRACK, T_NONE, T_NONE, // 58
    T_NONE, T_NONE, T_NONE, T_NONE, T_NONE, T_NONE, T_NONE, T_NONE, // 60
    T_NONE, T_NONE, T_NONE, T_NONE, T_NONE, T_NONE, T_NONE, T_NONE, // 68
    T_NONE, T_NONE, T_NONE, T_NONE, T_NONE, T_NONE, T_NONE, T_NONE, // 70
    T_NONE, T_NONE, T_NONE, T_LBRACE, T_PIPE, T_RBRACE, T_NONE, T_NONE  // 78
  };

  bool isdelimiter(u8 ch) {
    return DELIMITERS[ch];
  }

  // is valid char in symbol
  bool issymbol(u8 ch) {
    return isprint(ch) && !isdelimiter(ch) && !isspace(ch);
  }

  // is valid first char in symbol
  bool issymbolstart(u8 ch) {
    return issymbol(ch) && !isdigit(ch) && ch != '_';
  }

  // is symbolic char (e.g. $, +, @), not a letter or digit
  bool issymbolic(u8 ch) {
    return issymbolstart(ch) && !isalpha(ch);
  }

  Token scan(Source::View& view) {
    const u8* start = view.pos();
    u32 start_col = view.col(), line = view.line();
    u8 ch = view.peek();

    if (!ch) {
      return NONE;
    }
    else if (ch == '#') {
      while (view.peek() && view.peek() != '\n') view.read();
      return scan(view);
    }
    else if (ch == '.') {
      while (view.peek() == '.') view.read();
      u32 len = view.pos() - start;
      return Token(len > 1 ? T_SYMBOL : T_DOT, { len, start }, line, start_col);
    }
    else if (ch == ':') {
      while (view.peek() == ':') view.read();
      u32 len = view.pos() - start;
      if (len > 1) return Token(T_SYMBOL, { len, start }, line, start_col);
      if (isspace(view.peek())) return Token(T_COLON, { 1, start }, line, start_col);
      else return Token(T_QUOTE, { 1, start }, line, start_col);
    }
    else if (isdelimiter(ch)) {
      view.read();
      return Token(DELIMITERS[ch], { 1, start }, line, start_col);
    }
    else if (issymbolstart(ch)) {
      view.read();
      if (ch == '+' && !isspace(view.peek()) 
          && !issymbolic(view.peek()))
        return Token(T_PLUS, { 1, start }, line, start_col);
      else if (ch == '-' && !isspace(view.peek()) 
          && !issymbolic(view.peek()))
        return Token(T_MINUS, { 1, start }, line, start_col);

      while (issymbol(view.peek())) view.read();
      return Token(T_SYMBOL, { u32(view.pos() - start), start }, line, start_col); 
    }
    else if (isdigit(ch)) {
      while (isdigit(view.peek())) view.read();
      if (isalpha(view.peek())) 
        return Token(T_COEFF, { u32(view.pos() - start), start }, line, start_col); 
      else if (isdelimiter(view.peek()) || isspace(view.peek()))
        return Token(T_INT, { u32(view.pos() - start), start }, line, start_col);
      else
        err({ line, u16(view.col()) }, "Unexpected character in integer '", view.peek(), "'.");
    }
    else if (isspace(ch)) {
      view.read();
      return scan(view);
    }
    else err({ line, u16(view.col()) }, "Unexpected character in input '", view.peek(), "'.");
    return NONE;
  }    
  
  TokenView::TokenView(vector<Token>& tokens, 
    Source& source, bool repl):
    _tokens(&tokens), i(0), _source(&source), _repl(repl) {}

  const Token& TokenView::peek() {
    return i < _tokens->size() ? (*_tokens)[i] : NONE;
  }

  const Token& TokenView::read() {
    const Token& t = peek();
    i ++;
    return t;
  }

  TokenView::operator bool() const {
    return i < _tokens->size();
  }

  bool TokenView::repl() const {
    return _repl;
  }

  void TokenView::expand() {
    print(". ");
    Source::View view = _source->expand(_stdin);

    while (view.peek())
      _tokens->push(scan(view));
    if (error_count())
      print_errors(_stdout, *_source), clear_errors();
  }
}

void write(stream& io, const basil::Token& t) {
  string s = escape(t.value);
  write(io, "[", basil::TOKEN_NAMES[t.type], ": '", s, "' : ", t.line, " ", t.column, "]");
}
