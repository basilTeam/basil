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
		sptr ++; // skip initial quote
		while (*sptr && *sptr != '\"') {
			if (*sptr == '\\') {
				sptr ++;
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
				sptr ++;
			}
			t += *(sptr ++);
		}
		sptr ++; // skip final quote
		return t;
	}

  Value parse_primary(TokenView& view, u32 indent);	
	void parse_line(TokenView& view, u32 indent, bool consume_line,
		vector<Value>& terms);

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
			if (view.peek().type != T_NEWLINE)
      	terms.push(parse_line(view, indent, false));
      if (view.peek().type == T_NEWLINE) {
				if (view.peek().column <= prev_indent && view.repl()) {
					view.read();
					return;
				}
				else view.read();
			}
      if (!view.peek() && (!view.repl() || out_of_input(view))) return;
    }
		if (view.peek().type == T_QUOTE 
			&& view.peek().column == prev_indent) { // continuation
			u32 new_indent = view.peek().column;
			parse_line(view, new_indent, true, terms);
		}
  }

  Value apply_op(TokenView& view, Value lhs, Value rhs, TokenType op) {
    switch (op) {
      case T_COLON: {
        Value v = list_of(Value("annotate"), lhs, rhs);
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
			case T_STRING: {
				Value v(parse_string(view.read().value), STRING);
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
      case T_COEFF: {
        i64 i = parse_int(view.read().value);
        Value v = list_of(Value("*"), i, parse_primary(view, indent));
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
        Value v = cons(Value("splice"), list_of(terms));
        v.set_location(first);
        return v;
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
      // parse a labeled block
      if (v.is_symbol() && view.peek().type == T_NEWLINE) {
        view.read();
        vector<Value> terms;
        terms.push(v); // label;
        if (!view.peek() && out_of_input(view)) return error();
        if (view.peek().column > indent)
          parse_block(view, terms, indent, view.peek().column);
        return list_of(terms);
      }
      else if (view.peek().type == T_NEWLINE) {
        view.rewind(); // unget :
        return v; // unlabeled block, handled in parse_line
      }
      else v = parse_binary(view, v, T_COLON, indent);
    }
    return v;
  }

	void parse_line(TokenView& view, u32 indent, bool consume_line,
		vector<Value>& terms) {
		SourceLocation first = view.peek();
    while (view.peek()) {
      if (view.peek().type == T_NEWLINE) {
        view.read();
        if (!view.peek() && (!view.repl() || out_of_input(view))) return;
        if (view.peek().column > indent) 
          parse_block(view, terms, indent, view.peek().column);
				else if (view.peek().type == T_QUOTE 
					&& view.peek().column == indent) { // continuation
					u32 new_indent = view.peek().column;
					parse_line(view, new_indent, true, terms);
				}
        else if (!consume_line) view.rewind();
				
				return;
      }
      Value v = parse(view, indent);
      if (!v.is_error()) terms.push(v);
    }
	}

  Value parse_line(TokenView& view, u32 indent, bool consume_line) {
		SourceLocation first = view.peek();
    vector<Value> terms;
		if (view.peek().type == T_NEWLINE) {
			if (consume_line) view.read();
			return empty();
		}
		parse_line(view, indent, consume_line, terms);

    Value v = list_of(terms);
    v.set_location(first);
    return v;
  }
}