#ifndef BASIL_SOURCE_H
#define BASIL_SOURCE_H

#include "util/defs.h"
#include "util/str.h"
#include "util/vec.h"

namespace basil {
    class Source {
        vector<string*> _sections;
        vector<const_slice<u8>> _lines;

        void add_lines(string* s);
        void calc_lines();

      public:
        Source();
        Source(const char* filename);
        ~Source();
        Source(const Source& other);
        Source& operator=(const Source& other);

        void add_line(const string& text);
        const_slice<u8> line(u32 i) const;
        const vector<const_slice<u8>>& lines() const;

        class View {
            const Source* const _src;
            u32 _line, _column;

            View(const Source* const src, u32 line, u32 column);
            friend class Source;

          public:
            u8 peek() const;
            u8 read();
            u32 line() const;
            u32 col() const;
            const u8* pos() const;
        };

        View begin() const;
        View expand(stream& io, u8 last = '\n');
    };
} // namespace basil

#endif