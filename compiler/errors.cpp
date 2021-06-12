#include "errors.h"
#include "util/vec.h"
#include "util/utf8.h"

namespace basil {
    struct Error {
        Source::Pos pos;
        ustring msg;
    };

    static vector<Error> errors;

    void err(Source::Pos pos, const ustring& msg) {
        errors.push({ pos, msg });
    }

    u32 error_count() {
        return errors.size();
    } 
      
    void print_source(stream& io, Source::Pos pos, const Source& src) {
        const auto& line = src[pos.line_start];
        if (line.back() == '\n') write(io, '\t', line);
        else writeln(io, '\t', line);
        u32 first = pos.col_start, last = pos.line_end == pos.line_start ? pos.col_end : line.size();
        u32 i = 0;
        write(io, '\t');
        for (; i < first; i ++) write(io, ' ');
        for (; i < last; i ++) write(io, '^');
        writeln(io, "");
    }

    void print_error(stream& io, const Error& e, rc<Source> src) {
        writeln(io, e.pos, " ", e.msg);
        if (src) print_source(io, e.pos, *src);
    }

    void print_errors(stream& io, rc<Source> src) {
        for (const Error& e : errors) print_error(io, e, src);
    }

    void discard_errors() {
        errors.clear();
    }
}