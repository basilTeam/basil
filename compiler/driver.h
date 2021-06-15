#ifndef BASIL_DRIVER_H
#define BASIL_DRIVER_H

#include "env.h"
#include "eval.h"
#include "token.h"

namespace basil {
    // Initializes all necessary state for the compiler.
    void init();

    // Safely cleans up all resources before the compiler terminates.
    void deinit();

    enum PrintFlag {
        PRINT_TOKENS,
        PRINT_PARSED,
        NUM_PRINT_FLAGS
    };

    Source load_step(const char* const& str);
    vector<Token> lex_step(const Source& source);
    Value parse_step(const vector<Token>& tokens);
    Value resolve_step(const Value& term);
    Value eval_step(const Value& term);

    template<typename T>
    T compile(const T& input) {
        return input;
    }

    template<typename T, typename F>
    auto compile_single(const T& input, F func) {
        return func(input);
    }

    template<typename T, typename F, typename ...Args>
    auto compile(const T& input, F func, Args... funcs) {
        auto val = compile_single(input, func);
        return compile(val, funcs...);
    }

    void enable_print(PrintFlag flag);
    void disable_print(PrintFlag flag);
    bool should_print(PrintFlag flag);

    void repl(rc<Env> env);
}

#endif