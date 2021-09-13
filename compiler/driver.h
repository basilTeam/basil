#ifndef BASIL_DRIVER_H
#define BASIL_DRIVER_H

#include "env.h"
#include "eval.h"
#include "token.h"

#define BASIL_MAJOR_VERSION 1
#define BASIL_MINOR_VERSION 0
#define BASIL_PATCH_VERSION 0

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

    optional<ustring> locate_source(const char* path);

    rc<Source> load_step(const char* const& str);
    vector<Token> lex_step(rc<Source> source);
    Value parse_step(const vector<Token>& tokens);
    Value resolve_step(const Value& term);
    Value eval_step(const Value& term);

    template<typename T>
    T compile(const T& input) {
        return input;
    }

    template<typename T, typename F>
    auto compile(const T& input, F func) {
        return func(input);
    }

    template<typename T, typename F, typename ...Args>
    auto compile(const T& input, F func, Args... funcs) {
        auto val = compile(input, func);
        return compile(val, funcs...);
    }

    // These functions permit configurable debugging output for different
    // compilation phases.
    void enable_print(PrintFlag flag);
    void disable_print(PrintFlag flag);
    bool should_print(PrintFlag flag);
    
    // Returns whether or not the view is out of input. Normally, this is
    // the same as TokenView::operator bool. If the compiler is in REPL-mode,
    // returns false if additional input was read from stdin.
    bool out_of_input(TokenView& view);

    // Returns whether the compiler is running in REPL mode.
    bool is_repl();

    // Runs the REPL mode of the compiler.
    void repl();

    // Runs the "run file" mode of the compiler.
    void run(const char* filename);

    // Loads a file and returns the top-level environment before code generation.
    optional<rc<Env>> load(const char* filename);
}

#endif