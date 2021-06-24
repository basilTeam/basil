#ifndef BASIL_BUILTIN_H
#define BASIL_BUILTIN_H

#include "util/rc.h"
#include "value.h"

namespace basil {
    // Represents a built-in function or macro.
    struct Builtin {
        Type type;
        rc<Form> form;

        // Built-in behavior for evaluating at compile-time. If nullptr, forces
        // this builtin to be compiled and executed in binary.
        Value (*comptime)(rc<Env> env, const Value& args);

        // Built-in behavior for compiling this builtin to binary. If nullptr,
        // forces this builtin to be evaluated at compile-time.
        Value (*runtime)(rc<Env> env, const Value& args);
    };

    // Adds all built-in functions and macros to the provided environment.
    void add_builtins(rc<Env> env);
}

#endif