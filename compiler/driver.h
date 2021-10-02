#ifndef BASIL_DRIVER_H
#define BASIL_DRIVER_H

#include "env.h"
#include "eval.h"
#include "token.h"
#include "obj.h"

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

    optional<rc<Object>> load_artifact(const char* path);
    optional<rc<Section>> lex_and_parse(rc<Section> section);
    optional<rc<Section>> eval_section(rc<Section> section);
    optional<rc<Section>> to_ast(rc<Section> section);
    optional<rc<Section>> advance_section(rc<Section> section, SectionType target);

    rc<Source> load_step(const char* const& str);
    vector<Token> lex_step(rc<Source> source);
    Value parse_step(const vector<Token>& tokens);
    Value resolve_step(const Value& term);
    Value eval_step(const Value& term);
    rc<AST> ast_step(const Value& value);
    map<Symbol, rc<IRFunction>> ssa_step(const rc<AST>& ast);

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

    // Compiles a single Basil source file to a Basil object.
    void compile(const char* filename, SectionType target);

    // Runs the "run file" mode of the compiler.
    void run(const char* filename);

    // Loads a file and returns the top-level environment before code generation.
    optional<rc<Env>> load(const char* filename);
}

#endif