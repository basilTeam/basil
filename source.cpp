#include "source.h"
#include "io.h"

namespace basil {
  void Source::add_lines(string* s) {
    const u8* p = s->raw();
    const u8* start = p;
    u32 size = 0;
    while (*p) {
      size ++; // we're not at a null char, so this char should be included
      if (*p == '\n') {
        _lines.push({ size, start });
        start = p + 1; // next line starts after the \n
        size = 0;
      }
      p ++;
    }
    if (size > 0) _lines.push({ size, start }); // in case file doesn't end with a newline
  }

  void Source::calc_lines() {
    _lines.clear();

    for (string* s : _sections) {
      add_lines(s);
    }
  }

  Source::Source() {}

  Source::Source(const char* filename) {
    file f(filename, "r");
    if (!f) return;

    _sections.push(new string());
    while (f) {
      if (f.peek() == '\t') *_sections.back() += "    ";
      else *_sections.back() += f.read();
    }

    calc_lines();
  }

  Source::~Source() {
    for (string * s : _sections) delete s;
  }

  Source::Source(const Source& other) {
    for (string *s : _sections) delete s;
    _sections.clear();
    for (string *s : other._sections) {
      _sections.push(new string(*s));
    }
    calc_lines();
  }

  Source& Source::operator=(const Source& other) {
    if (this == &other) return *this;
    for (string *s : _sections) delete s;
    _sections.clear();
    for (string *s : other._sections) {
      _sections.push(new string(*s));
    }
    calc_lines();
    return *this;
  }

  const_slice<u8> Source::line(u32 i) const {
    return _lines[i];
  }
  
  const vector<const_slice<u8>>& Source::lines() const {
    return _lines;
  }

  Source::View::View(const Source* const src, u32 line, u32 column):
    _src(src), _line(line), _column(column) {}

  u8 Source::View::peek() const {
    if (_line >= _src->lines().size()) return '\0';
    return _src->line(_line)[_column];
  }

  u8 Source::View::read() {
    u8 ch = peek();
    if (!ch) return ch; // exit early on null char

    _column ++;
    if (_column >= _src->line(_line).size())
      _column = 0, _line ++;
    
    return ch;
  }

  const u8* Source::View::pos() const {
    return &_src->line(_line)[_column];
  }

  u32 Source::View::col() const {
    return _column;
  }

  u32 Source::View::line() const {
    return _line;
  }

  Source::View Source::begin() const {
    return Source::View(this, 0, 0);
  }

  Source::View Source::expand(stream& io, u8 last) {
    Source::View view = Source::View(this, _lines.size(), 0);

    string* line = new string();
    while (io.peek() != last) {
      if (io.peek() == '\t') *line += "    ", io.read();
      else *line += io.read();
    }
    *line += io.read();

    add_lines(line);
    _sections.push(line);

    return view;
  }
}