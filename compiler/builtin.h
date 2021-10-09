/*
 * Copyright (c) 2021, the Basil authors
 * All rights reserved.
 *
 * This source code is licensed under the 3-Clause BSD License, the full text
 * of which can be found in the LICENSE file in the root directory
 * of this project.
 */

#ifndef BASIL_BUILTIN_H
#define BASIL_BUILTIN_H

#include "util/rc.h"
#include "value.h"
#include "ast.h"

namespace basil {
    // Represents a built-in function or macro.
    struct Builtin {
        Type type;
        rc<Form> form;

        // Built-in behavior for evaluating at compile-time. If nullptr, forces
        // this builtin to be compiled and executed in binary.
        Value (*comptime)(rc<Env> env, const Value& call_term, const Value& args);

        // Built-in behavior for compiling this builtin to binary. If nullptr,
        // forces this builtin to be evaluated at compile-time.
        rc<AST> (*runtime)(rc<Env> env, const Value& call_term, const Value& args);

        // Whether or not we should evaluate quoted parameters for this builtin
        // when lowering to runtime AST.
        bool preserve_quotes = false;
    };

    // Adds all built-in functions and macros to the provided environment.
    void add_builtins(rc<Env> env);
}

#endif