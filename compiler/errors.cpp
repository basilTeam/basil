#include "errors.h"
#include "lex.h"
#include "util/vec.h"

namespace basil {
    SourceLocation::SourceLocation() : line(0), column(0), length(0) {}

    SourceLocation::SourceLocation(u32 line_in, u16 column_in, u16 length_in)
        : line(line_in), column(column_in), length(length_in) {}

    SourceLocation::SourceLocation(const Token& token)
        : line(token.line), column(token.column), length(token.value.size()) {}

    const SourceLocation NO_LOCATION = {0, 0, 0};

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
            if (e.loc.length != 0) write(io, "[", e.loc.line + 1, ":", e.loc.column + 1, "] ");
            writeln(io, e.message);
        }
        clear_errors();
    }

    void print_errors(stream& io, const Source& src) {
        for (const Error& e : errors) {
            if (e.loc.length != 0) write(io, "[", e.loc.line + 1, ":", e.loc.column + 1, "] ");
            writeln(io, e.message);

            if (e.loc.length == 0) continue; // skip source printing if no location
            const auto& line = src.line(e.loc.line);
            if (line.back() == '\n') write(io, '\t', line);
            else
                writeln(io, '\t', line);
            u32 first = e.loc.column, last = e.loc.column + e.loc.length;
            u32 i = 0;
            write(io, '\t');
            for (; i < first; i++) write(io, ' ');
            for (; i < last; i++) write(io, '^');
            writeln(io, "");
        }
    }

    void silence_errors() {
        silenced++;
    }

    void unsilence_errors() {
        silenced--;
        if (silenced < 0) silenced = 0;
    }
} // namespace basil