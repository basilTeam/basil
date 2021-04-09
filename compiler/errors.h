#ifndef BASIL_ERRORS_H
#define BASIL_ERRORS_H

#include "util/defs.h"
#include "util/io.h"
#include "util/slice.h"
#include "util/str.h"

namespace basil {
  class Token;
  class Source;
  class Value;

  struct SourceLocation {
    u32 line_start, line_end;
    u16 column_start, column_end;

    SourceLocation();
    SourceLocation(u32 line, u16 col);
    SourceLocation(u32 line_start_in, u16 column_start_in, u32 line_end_in, u16 column_end_in);
    SourceLocation(const Token& token);

    // expand later with other types of objects
    bool empty() const;
  };

  SourceLocation span(SourceLocation a, SourceLocation b);

  // has length of zero, indicates position not in source
  extern const SourceLocation NO_LOCATION; 

  void err(SourceLocation loc, const string& message);
  u32 error_count();
  void clear_errors();
  void print_errors(stream& io);
  void print_errors(stream& io, const Source& src);
	void silence_errors();
	void unsilence_errors();

  string commalist(const Value& value, bool quote);
  
  template<typename ...Args>
  void err(SourceLocation loc, Args... args) {
    buffer b;
    write(b, args...);
    string message;
    while (b.peek()) message += b.read();
    basil::err(loc, message);
  }
}

#endif