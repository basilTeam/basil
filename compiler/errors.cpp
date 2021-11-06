/*
 * Copyright (c) 2021, the Basil authors
 * All rights reserved.
 *
 * This source code is licensed under the 3-Clause BSD License, the full text
 * of which can be found in the LICENSE file in the root directory
 * of this project.
 */

#include "errors.h"
#include "eval.h"
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
        vector<Value> trace;
    };

    static vector<Error> errors;

    vector<Value> get_stacktrace() {
        vector<Value> trace;
        const auto& info = get_perf_info();
        for (i64 i = i64(info.counts.size()) - 1; i >= 0; i --) {
            trace.push(info.counts[i].call_term);
        }
        return trace;
    }

    void err(Source::Pos pos, const ustring& msg) {
        errors.push({ pos, msg, {}, get_stacktrace() });
    }

    void note(Source::Pos pos, const ustring& msg) {
        if (errors.size() == 0) panic("Tried to add note to error, but no errors have been reported!");
        errors.back().notes.push({ pos, msg });
    }

    u32 error_count() {
        return errors.size();
    } 
      
    void print_source(stream& io, const char* color, Source::Pos pos, const Source& src) {
        if (pos == Source::Pos()) return;
        if (pos.line_start >= src.size()) return;
        
        const auto& line = src[pos.line_start];
        u32 first = pos.col_start, last = pos.line_end == pos.line_start ? pos.col_end : line.size();
        write(io, color, "│", RESET, "       ");
        auto it = line.begin();
        for (u32 i = 0; i < line.size(); i ++) {
            if (i == first) write(io, BOLD, ITALIC, color);
            if (i == last) write(io, RESET);
            write(io, *it ++);
        }
        if (line.back() != '\n') write(io, '\n');
        u32 i = 0;
        write(io, color, "└───────");
        for (; i < first; i ++) write(io, "─");
        write(io, BOLD, color);
        for (; i < last; i ++) write(io, '^');
        write(io, RESET);
        writeln(io, "");
    }

    void print_error(stream& io, const Error& e, rc<Source> src) {
        writeln(io, e.pos, BOLDRED, " Error", RESET, ": ", e.msg);
        if (src) print_source(io, RED, e.pos, *src);
        for (const auto& pos : e.trace) {
            writeln(io, pos.pos, "\t", "in call to '", BOLDYELLOW, v_head(pos), RESET, "'");
            if (src) print_source(io, YELLOW, pos.pos, *src);
        }
        for (const Note& n : e.notes) {
            writeln(io, ITALICRED, "Note", RESET, ": ", n.msg);
            if (src) print_source(io, RED, n.pos, *src);
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