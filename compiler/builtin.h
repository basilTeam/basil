#ifndef BASIL_BUILTIN_H
#define BASIL_BUILTIN_H

#include "util/rc.h"
#include "value.h"

namespace basil {
    // Represents the result of evaluating a source code term. All evaluations
    // produce both an environment and a value. The value is whatever value we
    // get from evaluating a source code term. The environment 'env' is
    // the environment resulting from any environmental changes the evaluation
    // produced, in which the next term may be evaluated.
    struct EvalResult {
        rc<Env> env;
        Value value;
    };

    // Represents a built-in function or macro.
    struct Builtin {
        Type type;
        rc<Form> form;

        // Built-in behavior for evaluating at compile-time. If nullptr, forces
        // this builtin to be compiled and executed in binary.
        EvalResult (*comptime)(rc<Env> env, const Value& args);

        // Built-in behavior for compiling this builtin to binary. If nullptr,
        // forces this builtin to be evaluated at compile-time.
        EvalResult (*runtime)(rc<Env> env, const Value& args);
    };

    extern Builtin
        DEF, DO, EVAL, // Special forms/core functionality.
        IF, WHILE, // Control flow.
        ADD, SUB, MUL, DIV, MOD, // Basic arithmetic.
        INCR, DECR, // Increment / decrement.
        LESS, LESS_EQUAL, GREATER, GREATER_EQUAL, EQUAL, NOT_EQUAL, // Comparisons.
        AND, OR, XOR, NOT, // Logic.
        HEAD, TAIL, CONS, // List manipulation.
        LENGTH, FIND, SUBSTR, // String manipulation.
        LIST, ARRAY, TUPLE, // Data constructors.
        ASSIGN; // Mutation.

    // Adds all built-in functions and macros to the provided environment.
    void add_builtins(rc<Env> env);
}

#endif