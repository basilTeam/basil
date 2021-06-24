#include "errors.h"
#include "util/vec.h"
#include "util/utf8.h"

namespace basil {
    struct Note {
        Source::Pos pos;
        ustring msg;
    };
    
    struct Error {
        Source::Pos pos;
        ustring msg;
        vector<Note> notes;
    };

    static vector<Error> errors;

    void err(Source::Pos pos, const ustring& msg) {
        errors.push({ pos, msg });
    }

    void note(Source::Pos pos, const ustring& msg) {
        if (errors.size() == 0) panic("Tried to add note to error, but no errors have been reported!");
        errors.back().notes.push({ pos, msg });
    }

    u32 error_count() {
        return errors.size();
    } 
      
    void print_source(stream& io, Source::Pos pos, const Source& src) {
        if (pos == Source::Pos()) return;
        const auto& line = src[pos.line_start];
        write(io, RED, "│", RESET, "       ");
        if (line.back() == '\n') write(io, line);
        else writeln(io, line);
        u32 first = pos.col_start, last = pos.line_end == pos.line_start ? pos.col_end : line.size();
        u32 i = 0;
        write(io, RED, "└───────");
        for (; i < first; i ++) write(io, "─");
        write(io, BOLDRED);
        for (; i < last; i ++) write(io, '^');
        write(io, RESET);
        writeln(io, "");
    }

    void print_error(stream& io, const Error& e, rc<Source> src) {
        writeln(io, e.pos, BOLDRED, " Error", RESET, ": ", e.msg);
        if (src) print_source(io, e.pos, *src);
        for (const Note& n : e.notes) {
            writeln(io, ITALICRED, "Note", RESET, ": ", n.msg);
            if (src) print_source(io, n.pos, *src);
        }
        writeln(io, "");
    }

    void print_errors(stream& io, rc<Source> src) {
        for (const Error& e : errors) print_error(io, e, src);
    }

    void discard_errors() {
        errors.clear();
    }
}