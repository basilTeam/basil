/*
 * Copyright (c) 2021, the Basil authors
 * All rights reserved.
 *
 * This source code is licensed under the 3-Clause BSD License, the full text
 * of which can be found in the LICENSE file in the root directory
 * of this project.
 */

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