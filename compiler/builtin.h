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
    // Various bit-flags used to characterize built-in functions.
    enum BuiltinFlags {
        BF_NONE = 0,
        BF_COMPTIME = 1,    // Can this built-in be computed at compile-time?
        BF_RUNTIME = 2,     // Can this built-in be computed at runtime?
        BF_STATEFUL = 4,    // Is this built-in stateful or effectful?
        BF_PRESERVING = 8,  // Does this built-in preserve quoted arguments when lowering them?
        BF_AST_ANYLIST = 16 // Does this built-in avoid unifying generic variadics when lowering?
    };

    // Represents a built-in function or macro.
    struct Builtin {
        Type type;
        rc<Form> form;

        // The flags describing this particular built-in function.
        BuiltinFlags flags;

        // Built-in behavior for evaluating at compile-time. If nullptr, forces
        // this built-in to be compiled and executed in binary.
        Value (*comptime)(rc<Env> env, const Value& call_term, const Value& args);

        // Built-in behavior for compiling this built-in to binary. If nullptr,
        // forces this built-in to be evaluated at compile-time.
        rc<AST> (*runtime)(rc<Env> env, const Value& call_term, const Value& args);
    };

    // Adds all built-in functions and macros to the provided environment.
    void add_builtins(rc<Env> env);
}

#endif