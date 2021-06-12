#ifndef BASIL_DRIVER_H
#define BASIL_DRIVER_H

#include "env.h"
#include "builtin.h"
#include "token.h"

namespace basil {
    enum PrintFlag {
        PRINT_TOKENS,
        PRINT_PARSED,
        NUM_PRINT_FLAGS
    };

    Source load_step(const char* const& str);
    vector<Token> lex_step(const Source& source);
    Value parse_step(const vector<Token>& tokens);
    auto resolve_step(rc<Env> env);
    auto eval_step(rc<Env> env);

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