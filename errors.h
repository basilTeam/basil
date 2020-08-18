#ifndef BASIL_ERRORS_H
#define BASIL_ERRORS_H

#include "defs.h"
#include "str.h"
#include "io.h"
#include "slice.h"

namespace basil {
  class Token;
  class Source;

  struct SourceLocation {
    u32 line;
    u16 column, length;

    SourceLocation();
    SourceLocation(u32 line_in, u16 column_in, u16 length_in = 1);
    SourceLocation(const Token& token);
    // expand later with other types of objects
  };

  // has length of zero, indicates position not in source
  extern const SourceLocation NO_LOCATION; 

  void err(SourceLocation loc, const string& message);
  u32 error_count();
  void clear_errors();
  void print_errors(stream& io);
  void print_errors(stream& io, const Source& src);
  
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