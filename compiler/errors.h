/*
 * Copyright (c) 2021, the Basil authors
 * All rights reserved.
 *
 * This source code is licensed under the 3-Clause BSD License, the full text
 * of which can be found in the LICENSE file in the root directory
 * of this project.
 */

#ifndef BASIL_ERRORS_H
#define BASIL_ERRORS_H

#include "source.h"
#include "util/rc.h"

namespace basil {
    // Returns the number of errors that have been reported so far.
    u32 error_count();

    // Prints all current errors to the provided output stream. If a source is provided,
    // represented by a non-null second argument, the relevant code for each error will 
    // be highlighted.
    void print_errors(stream& io, rc<Source> src);

    // Discards all errors.
    void discard_errors();

    // Reports an error at the provided source position with the provided message.
    void err(Source::Pos pos, const ustring& msg);
    
    // Reports an error at the provided source position with a message constructed from 
    // the remaining arguments.
    template<typename... Args>
    void err(Source::Pos pos, const Args&... args) {
        err(pos, format<ustring>(args...));
    }
    
    // Reports a note associated with the most recent error, at the provided source 
    // position with the provided message.
    void note(Source::Pos pos, const ustring& msg);
    
    // Reports a note at the provided source position with a message constructed from 
    // the remaining arguments.
    template<typename... Args>
    void note(Source::Pos pos, const Args&... args) {
        note(pos, format<ustring>(args...));
    }
}

#endif