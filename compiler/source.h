/*
 * Copyright (c) 2021, the Basil authors
 * All rights reserved.
 *
 * This source code is licensed under the 3-Clause BSD License, the full text
 * of which can be found in the LICENSE file in the root directory
 * of this project.
 */

#ifndef BASIL_SOURCE_H
#define BASIL_SOURCE_H

#include "util/vec.h"
#include "util/str.h"
#include "util/ustr.h"
#include "util/option.h"

namespace basil {
    // Represents a Basil source file. The contents of a Source object are just text,
    // but we use the class to provide a nice abstract interface for exploring and
    // revisiting source information without passing it around everywhere as strings.
    class Source {
        optional<ustring> filepath = none<ustring>();
        vector<ustring*> lines;

        // Checks that the Source file is within the size limitations enforced by the
        // Basil language. No source can be more than a million lines long, and no
        // individual line can be more than four thousand characters long. We use this 
        // to enforce styling, but also to guarantee that all source locations can be 
        // represented by a 64-bit Source::Pos.
        void check_limits() const;
    public:
        // Constructs an empty Source object.
        Source();

        // Loads the entirety of the file at the provided path into the constructed
        // Source object.
        Source(const ustring& path);

        // Reads all characters from the provided stream into the constructed
        // Source object.
        Source(stream& io);

        ~Source();
        Source(const Source& other);
        Source& operator=(const Source& other);

        // Represents an exclusive range of characters within a source file.
        struct Pos {
            u32 line_start : 20;
            u32 col_start : 12;
            u32 line_end : 20;
            u32 col_end : 12;

            Pos();
            Pos(u32 line_start, u32 col_start, u32 line_end, u32 col_end);
            bool operator==(const Pos& other) const;
            bool operator!=(const Pos& other) const;
        };

        // Represents a position in a source file, and provides a peek/read interface
        // to aid in parsing.
        struct View {
            u32 line, column;
            ustring::iterator iter;
            const Source* src;

            // Constructs a View at the start of the provided Source object.
            View(const Source& src);

            // Constructs a View at the start of the line at index i in the provided Source object.
            View(const Source& src, u32 line);

            // Returns the current source position of the character pointed to by this view.
            Pos pos() const;

            // Returns the UTF-8 character immediately before this View.
            // Returns the null character if this View is pointing to the first character of the
            // Source.
            rune last() const;

            // Returns the UTF-8 character currently pointed to by this View.
            // Returns the null character (U+0000) if pointing off the end of the referenced Source.
            rune peek() const;

            // Returns the UTF-8 character n spaces ahead of this View. peek(0) is the same as peek().
            rune peek(u32 n) const;

            // Returns the UTF-8 character currently pointed to by this View and advances past it.
            // If advancing would move the iterator off the end of the current line, this View
            // will move to the next line.
            rune read();
        };

        // Returns the line at index i of this Source object.
        const ustring& operator[](u32 i) const;

        // Returns the number of lines in this Source object.
        u32 size() const;

        // Returns a Source::Pos spanning the line at index i within this source file.
        Pos line_span(u32 i) const;

        // Returns a Source::Pos spanning the entire source file.
        Pos full_span() const;

        // Reads text from the provided stream until encountering a 
        // line break. Adds the text as a new line to this source.
        View expand_line(stream& io);

        // Returns the path this source originated from, if one exists.
        const optional<ustring>& path() const;
    };

    // Returns a new Source::Pos, representing the smallest range that
    // encompasses every character in both a and b.
    Source::Pos span(Source::Pos a, Source::Pos b);
}

void write(stream& io, const basil::Source::Pos& pos);

#endif
