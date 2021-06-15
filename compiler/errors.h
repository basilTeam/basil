#ifndef BASIL_ERRORS_H
#define BASIL_ERRORS_H

#include "source.h"
#include "util/rc.h"

namespace basil {
    // Reports an error at the provided source position with the provided message.
    void err(Source::Pos pos, const ustring& msg);

    // Returns the number of errors that have been reported so far.
    u32 error_count();

    // Prints all current errors to the provided output stream. If a source is provided,
    // represented by a non-null second argument, the relevant code for each error will 
    // be highlighted.
    void print_errors(stream& io, rc<Source> src);

    // Discards all errors.
    void discard_errors();
    
    // Reports an error at the provided source position with a message constructed from 
    // the remaining arguments.
    template<typename... Args>
    void err(Source::Pos pos, const Args&... args) {
        err(pos, format<ustring>(args...));
    }
}

#endif