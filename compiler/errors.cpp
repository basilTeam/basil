#include "errors.h"
#include "values.h"
#include "util/vec.h"
#include "lex.h"
#include "util/vec.h"

namespace basil {
  SourceLocation::SourceLocation():
    line_start(0), line_end(0), column_start(0), column_end(0) {}

  SourceLocation::SourceLocation(u32 line, u16 col):
    line_start(line), column_start(col), line_end(line), column_end(col + 1) {}

  SourceLocation::SourceLocation(u32 line_start_in, u16 column_start_in, u32 line_end_in, u16 column_end_in):
    line_start(line_start_in), column_start(column_start_in), line_end(line_end_in), column_end(column_end_in) {} 

  SourceLocation::SourceLocation(const Token& token):
    line_start(token.line), column_start(token.column), line_end(token.line), column_end(token.column + token.value.size()) {}

  bool SourceLocation::empty() const {
    return line_start == line_end && column_start == column_end;
  }

  SourceLocation span(SourceLocation a, SourceLocation b) {
    if (a.line_start * 65536 + a.column_start < b.line_start * 65536 + b.column_start)
      return { a.line_start, a.column_start, b.line_end, b.column_end };
    else 
      return { b.line_start, b.column_start, a.line_end, a.column_end };
  }

  const SourceLocation NO_LOCATION;

  string commalist(const Value& value, bool quote) {
    buffer b;
    vector<Value> values;
    if (value.is_list()) values = to_vector(value);
    else if (value.is_array()) values = value.get_array().values();
    else if (value.is_product()) values = value.get_product().values();
    else if (value.is_type() && value.get_type()->kind() == KIND_PRODUCT) {
      const ProductType* p = (const ProductType*)value.get_type();
      for (u32 i = 0; i < p->count(); i ++) values.push(Value(p->member(i), TYPE));
    }
    for (u32 i = 0; i < values.size(); i ++) {
      if (i > 0) {
        if (i == values.size() - 1 && i > 0) write(b, " and ");
        else write(b, ", ");
      }
      write(b, quote ? "'" : "", values[i], quote ? "'" : "");
    }
    string s;
    while (b) s += b.read();
    return s;
  }

  struct Error {
      SourceLocation loc;
      string message;
  };

  static vector<Error> errors;
  static i64 silenced = 0;

  void err(SourceLocation loc, const string& message) {
    if (!silenced) errors.push({loc, message});
  }

  u32 error_count() {
    return errors.size();
  }

  void clear_errors() {
      errors.clear();
  }

  void print_errors(stream& io) {
    for (const Error& e : errors) {
      if (!e.loc.empty()) 
        write(io, "[", e.loc.line_start + 1, ":", e.loc.column_start + 1, "] ");
      writeln(io, e.message);
    }
  }

  void print_errors(stream& io, const Source& src) {
    for (const Error& e : errors) {
      if (!e.loc.empty()) 
        write(io, "[", e.loc.line_start + 1, ":", e.loc.column_start + 1, "] ");
      writeln(io, e.message);

      if (e.loc.empty()) continue; // skip source printing if no location
      const auto& line = src.line(e.loc.line_start);
      if (line.back() == '\n') write(io, '\t', line);
      else writeln(io, '\t', line);
      u32 first = e.loc.column_start, last = e.loc.line_end == e.loc.line_start ? e.loc.column_end : line.size();
      u32 i = 0;
      write(io, '\t');
      for (; i < first; i ++) write(io, ' ');
      for (; i < last; i ++) write(io, '^');
      writeln(io, "");
    }
  }

  void unsilence_errors() {
      silenced--;
      if (silenced < 0) silenced = 0;
  }
} // namespace basil