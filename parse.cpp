#include "parse.h"
#include "errors.h"
#include <cstdlib>

namespace basil {
  i64 parse_int(const string& s) {
    static buffer b;
    write(b, s);
    i64 i;
    read(b, i);
    return i;
  }

  Value flatten(Value term) {
    return term.is_list() && head(term).is_list() && &term.get_list().head() == &term.get_list().tail() ? head(term) : term;
  }

  Value parse_primary(TokenView& view, u32 indent);

  bool out_of_input(TokenView& view) { // returns true if out of input
    if (view.repl()) {
      view.expand();
      return false;
    }
    else {
      err(view.peek(), "Unexpected end of input.");
      return true;
    }
  }

  void parse_enclosed(TokenView& view, vector<Value>& terms, 
    TokenType terminator, u32 indent) {
    while (view.peek().type != terminator) {
      while (view.peek().type == T_NEWLINE) view.read();
      if (!view.peek() && out_of_input(view)) return;
      terms.push(parse(view, indent));
    }
    view.read();
  }

  void parse_block(TokenView& view, vector<Value>& terms, 
    u32 prev_indent, u32 indent) {
    while (view.peek().column > prev_indent) {
      terms.push(parse_line(view, indent, false));
      if (view.peek().type == T_NEWLINE 
        && view.peek().column > prev_indent) view.read();
      if (!view.peek() && out_of_input(view)) return;
    }
  }

  void parse_continuation(TokenView& view, vector<Value>& terms, 
    u32 prev_indent, u32 indent) {
    while (!view.peek() || view.peek().column > prev_indent) {
      while (view.peek().type != T_NEWLINE)
        terms.push(parse(view, indent));
      if (!view.peek() && out_of_input(view)) return;
    }
  }

  Value apply_op(TokenView& view, Value lhs, Value rhs, TokenType op) {
    switch (op) {
      case T_COLON: {
        Value v = list_of(string("annotate"), lhs, rhs);
        v.set_location(lhs.loc());
        return v;
      }
      case T_DOT: {
        Value v = list_of(lhs, rhs);
        v.set_location(lhs.loc());
        return v;
      }
      default:
        err(view.peek(), "Unexpected binary operator '", 
            view.peek().value, "'.");
        view.read();
        return Value(ERROR);
    }
  }

  Value parse_binary(TokenView& view, Value lhs, TokenType prev_op, 
    u32 indent) {
    Value rhs = parse_primary(view, indent);
    TokenType next_op = view.peek().type;
    if (next_op == T_COLON || next_op == T_DOT) {
      view.read();
      if (next_op == T_COLON && prev_op == T_DOT) // : has higher
                                                  // precedence
        return apply_op(view, lhs, 
          parse_binary(view, rhs, next_op, indent), prev_op);
      else
        return parse_binary(view, 
          apply_op(view, lhs, rhs, prev_op), next_op, indent);
    }
    return apply_op(view, lhs, rhs, prev_op);
  }

  Value parse_primary(TokenView& view, u32 indent) {
    SourceLocation first = view.peek();
    switch (view.peek().type) {
      case T_INT: {
        Value v(parse_int(view.read().value));
        v.set_location(first);
        return v;
      }
      case T_SYMBOL: {
        Value v(string(view.read().value));
        v.set_location(first);
        return v;
      }
      case T_PLUS: {
        view.read();
        Value v = list_of(Value("+"), 0, parse_primary(view, indent));
        v.set_location(first);
        return v;
      }
      case T_MINUS: {
        view.read();
        Value v = list_of(Value("-"), 0, parse_primary(view, indent));
        v.set_location(first);
        return v;
      }
      case T_QUOTE: {
        view.read();
        Value v = list_of(Value("quote"), parse_primary(view, indent));
        v.set_location(first);
        return v;
      }
      case T_LPAREN: {
        view.read();
        vector<Value> terms;
        parse_enclosed(view, terms, T_RPAREN, indent);
        Value v = list_of(terms);
        v.set_location(first);
        return v;
      }
      case T_LBRACK: {
        view.read();
        vector<Value> terms;
        terms.push(Value("list-of"));
        parse_enclosed(view, terms, T_RBRACK, indent);
        Value v = list_of(terms);
        v.set_location(first);
        return v;
      }
      case T_LBRACE: {
        view.read();
        vector<Value> terms;
        terms.push(Value("set-of"));
        parse_enclosed(view, terms, T_RBRACE, indent);
        Value v = list_of(terms);
        v.set_location(first);
        return v;
      }
      case T_PIPE: {
        view.read();
        vector<Value> terms;
        parse_enclosed(view, terms, T_PIPE, indent);
        Value v = terms.size() == 1 ? terms[0] : list_of(terms);
        v.set_location(first);
        return list_of(Value("splice"), v);
      }
      default:
        err(view.peek(), "Unexpected token '", 
          escape(view.peek().value), "'.");
        view.read();
        return error();
    }
  }

  Value parse(TokenView& view, u32 indent) {
    Value v = parse_primary(view, indent);
    if (v.is_error()) return v;
    if (view.peek().type == T_DOT)
      view.read(), v = parse_binary(view, v, T_DOT, indent);
    if (view.peek().type == T_COLON) {
      view.read();
      if (view.peek().type == T_NEWLINE) {
        view.read();
        vector<Value> terms;
        terms.push(v); // label;
        if (!view.peek() && out_of_input(view)) return error();
        if (view.peek().column > indent)
          parse_block(view, terms, indent, view.peek().column);
        return list_of(terms);
      }
      else v = parse_binary(view, v, T_COLON, indent);
    }
    return v;
  }

  Value parse_line(TokenView& view, u32 indent, bool consume_line) {
    SourceLocation first = view.peek();
    vector<Value> terms;
    while (view.peek() && view.peek().type != T_NEWLINE) {
      Value v = parse(view, indent);
      if (v.is_error()) return error();
      else terms.push(v);
    }
    if (consume_line) view.read();
    Value v = terms.size() == 1 ? terms[0] : list_of(terms);
    v.set_location(first);
    return v;
  }
}