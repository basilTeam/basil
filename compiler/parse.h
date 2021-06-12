#ifndef BASIL_PARSE_H
#define BASIL_PARSE_H

#include "token.h"
#include "value.h"

namespace basil {
    // Parses a single Basil term from the token view and returns
    // its value representation. 
    // Returns an error value in the event of a syntax error.
    optional<Value> parse(TokenView& view);
}

#endif