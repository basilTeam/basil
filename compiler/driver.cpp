#include "driver.h"
#include "value.h"
#include "parse.h"
#include "token.h"
#include "source.h"
#include "eval.h"

namespace basil {
    void init() {
        init_types_and_symbols();
    }

    void deinit() {
        free_root_env();
        free_types();
    }

    rc<Source> load_step(const char* const& str) {
        buffer b;
        write(b, str);
        return ref<Source>(b);
    }

    vector<Token> lex_step(rc<Source> source) {
        Source::View view(*source);
        return lex_all(view);
    }

    Value parse_step(const vector<Token>& tokens) {
        vector<Token> tokens_copy = tokens;
        TokenView view(nullptr, tokens_copy);
        vector<Value> values;
        while (view)
            if (auto maybe_expr = parse(view))
                values.push(*maybe_expr);
        if (values.size() == 0) return v_void({});
        else if (values.size() == 1) return values[0];
        else return v_list(span(values.front().pos, values.back().pos), t_list(T_ANY), values);
    }

    Value resolve_step(const Value& term) {
        Value term_copy = term;
        auto new_env = resolve_form(root_env(), term_copy);
        return term_copy;
    }

    Value eval_step(const Value& term) {
        Value term_copy = term;
        Value result = eval(root_env(), term_copy).value;
        if (error_count()) print_errors(_stdout, nullptr), discard_errors();
        return result;
    }

    static bool repl_mode = false;

    bool is_repl() {
        return repl_mode;
    }
    
    bool out_of_input(TokenView& view) {
        if (!view) {
            if (repl_mode) { // try and read additional input
                print(". ");
                if (view.expand_line(_stdin)) return false; // successfully read additional input
                else {
                    print_errors(_stdout, view.source);
                    return true;
                }
            }
            else return true; // don't try and pull input if not REPL
        }
        else return false;
    }

    void repl() {
        repl_mode = true;
        rc<Source> source = ref<Source>();
        vector<Token> token_cache;
        TokenView view(source, token_cache);

        rc<Env> global = extend(root_env());

        bool done = false;
        while (!done) {
            print("? ");
            view.expand_line(_stdin);
            vector<Value> code;
            code.push(v_symbol({}, S_DO));
            bool parse_errored = false;
            while (view) {
                if (view.peek().kind == TK_NEWLINE) {
                    view.read();
                    break;
                }

                if (auto v = parse(view)) code.push(*v);
            }
            if (error_count()) {
                print_errors(_stdout, source);
                discard_errors();
                continue;
            }

            Value list = v_list(span(code.front().pos, code.back().pos), t_list(T_ANY), code);
            EvalResult result = eval(global, list);
            if (error_count()) {
                print_errors(_stdout, source);
                discard_errors();
                continue;
            }

            println("= ", ITALICBLUE, result.value, RESET);
            println("");
        }
    }

    bool check_file(const char* filename) {
        file f(filename, "r");
        return f;
    }

    void run(const char* filename) {
        rc<Source> source = ref<Source>(filename);
        Source::View view(*source);
        vector<Token> token_cache = lex_all(view);
        TokenView tview(source, token_cache);

        rc<Env> global = extend(root_env());
        vector<Value> program_terms;
        program_terms.push(v_symbol({}, S_DO)); // wrap the whole program in a giant "do"

        while (tview) {
            if (auto v = parse(tview)) {
                program_terms.push(*v);
            }
            if (error_count()) {
            print_errors(_stdout, source);
            discard_errors();
            return;
            }
        }
        if (error_count()) {
            print_errors(_stdout, source);
            discard_errors();
            return;
        }

        Value program = v_list(span(program_terms.front().pos, program_terms.back().pos), 
            t_list(T_ANY), program_terms);
        EvalResult result = eval(global, program);
        if (error_count()) {
            print_errors(_stdout, source);
            discard_errors();
            return;
        }
        println(result.value);
    }

    // Runs the "help" mode of the compiler.
    void help(const char* cmd) {
        // todo
    }
}