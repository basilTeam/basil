#include "source.h"
#include "util/io.h"
#include "errors.h"
#include "util/utf8.h"

namespace basil {
    void Source::check_limits(optional<const char*> filename) const {
        if (lines.size() > 1000000) 
            err(full_span(), "Source file ", filename ? *filename : "", " with ", lines.size(),
                " lines exceeds maximum length of 1000000 lines.");
        else for (u32 i = 0; i < lines.size(); i ++) {
            if (lines[i]->size() > 4000)
                err(line_span(i), "Line ", i, " of source file ", filename ? *filename : "", 
                    " with length ", lines[i]->size(), " exceeds maximum line length of 4000 characters.");
        }
    }

    ustring* process_line(const string& s) {
        ustring line(s);
        ustring* u = new ustring();
        for (rune r : line) {
            if (r == '\t') *u += "    ";
            else *u += r;
        }
        return u;
    }

    Source::Source() {}

    Source::Source(const char* filename) {
        file f(filename, "r");
        if (!f) {
            err({}, "Could not open file '", filename, "'.");
            return;
        }

        string s; // we're using this as a byte buffer
        while (f.peek()) {
            s += f.peek();
            if (f.read() == '\n')
                lines.push(process_line(s)), s = ""; // convert to ustring, clear buffer 
        }
        if (s.size() > 0) lines.push(process_line(s));
        if (lines.size() > 0 && lines.back()->back() != '\n') *lines.back() += '\n';
    }

    Source::Source(stream& io) {
        string s; // we're using this as a byte buffer
        while (io.peek()) {
            s += io.peek();
            if (io.read() == '\n')
                lines.push(process_line(s)), s = ""; // convert to ustring, clear buffer 
        }
        if (s.size() > 0) lines.push(process_line(s));
        if (lines.size() > 0 && lines.back()->back() != '\n') *lines.back() += '\n';
    }

    Source::~Source() {
        for (ustring* line : lines) delete line;
    }

    Source::Source(const Source& other) {
        for (ustring* line : other.lines) lines.push(new ustring(*line));
    }

    Source& Source::operator=(const Source& other) {
        if (this != &other) {
            for (ustring* line : lines) delete line;
            lines.clear();
            for (ustring* line : other.lines) lines.push(new ustring(*line));
        }
        return *this;
    }

    Source::View::View(const Source& src_in): 
        line(0), column(0), iter(src_in[0].begin()), src(&src_in) {}

    Source::View::View(const Source& src_in, u32 line):
        line(line), column(0), iter(src_in[line].begin()), src(&src_in) {}

    Source::Pos Source::View::pos() const {
        return Source::Pos{line, column, line, column + 1};
    }

    rune Source::View::last() const {
        if (line == 0 && column == 0) return 0;
        auto iter_copy = iter;
        -- iter_copy;
        return *iter_copy;
    }

    rune Source::View::peek() const {
        if (line == src->lines.size()) return 0;
        return *iter;
    }
    
    rune Source::View::peek(u32 n) const {
        Source::View copy = *this;
        while (n --) copy.read();
        return copy.peek();
    }

    rune Source::View::read() {
        rune r = peek();
        if (!r) return r;
        column ++;
        iter ++;
        if (iter == (*src)[line].end()) {
            column = 0, line ++;
            if (line < src->size()) iter = (*src)[line].begin();
        }
        return r;
    }

    const ustring& Source::operator[](u32 i) const {
        return *lines[i];
    }

    u32 Source::size() const {
        return lines.size();
    }

    Source::Pos::Pos():
        line_start(0), col_start(0), line_end(0), col_end(0) {}

    Source::Pos::Pos(u32 line_start_in, u32 col_start_in, u32 line_end_in, u32 col_end_in):
        line_start(line_start_in), col_start(col_start_in),
        line_end(line_end_in), col_end(col_end_in) {}
    
    bool Source::Pos::operator==(const Source::Pos& other) const {
        return line_start == other.line_start
            && line_end == other.line_end
            && col_start == other.col_start
            && col_end == other.col_end;
    }
    
    bool Source::Pos::operator!=(const Source::Pos& other) const {
        return !operator==(other);
    }

    Source::Pos Source::line_span(u32 i) const {
        return { i, 0, i, lines[i]->size() };
    }

    Source::Pos Source::full_span() const {
        if (lines.size() == 0) return { 0, 0, 0, 0 };
        return { 0, 0, lines.size(), 0 };
    }
    
    Source::View Source::expand_line(stream& io) {
        string s; // we're using this as a byte buffer
        while (io.peek()) {
            s += io.peek();
            if (io.read() == '\n') {
                lines.push(process_line(s)); // convert to ustring and push 
                break; // stop after reading one line
            }
        }
        return Source::View(*this, lines.size() - 1);
    }

    Source::Pos span(Source::Pos a, Source::Pos b) { 
        if (a.line_start * 4096 + a.col_start < b.line_start * 4096 + b.col_start)
            return { a.line_start, a.col_start, b.line_end, b.col_end };
        else
            return { b.line_start, b.col_start, a.line_end, a.col_end };
    }
}

// Lines and columns are 0-indexed, but we print them starting from 1.
void write(stream& io, const basil::Source::Pos& pos) {
    write(io, "[", pos.line_start + 1, ":", pos.col_start + 1, "]");
}