#include "builtin.h"
#include "env.h"
#include "eval.h"
#include "forms.h"

namespace basil {
    Builtin 
        DEF,
        ADD, SUB, MUL;

    bool builtins_inited = false;

    // common operator precedence levels
    static const i64 
        PREC_ANNOTATED = 400, // annotations and property accesses, like infix : and .
        PREC_PREFIX = 300, // simple prefix operators like 'not'
        PREC_MUL = 200, // multiplication, division, modulus
        PREC_ADD = 100, // addition and subtraction
        PREC_DEFAULT = 0, // default for most procedures
        PREC_COMPARE = -100, // comparison operators like < or ==
        PREC_LOGIC = -200, // base for various logic operators like 'and' and 'or'
        PREC_COMPOUND = -300, // type operators and tuple constructors like , or |
        PREC_CONTROL = -400, // control-flow procedures like if or while
        PREC_STRUCTURE = -500, // program structure stuff like def, macro, and use
        PREC_QUOTE = -600; // quote and splice and similar constructs

    // This is a bit of a weird function, just used to avoid duplication in form-level and value-level
    // definitions. Runs through an argument list (that is neither grouped nor evaluated) and populates
    // the provided lists of arg names and parameters. Returns the symbol value corresponding to the name,
    // or an error if an error occurs.
    //
    // Expects 'term' to be a list with at least one element.
    Value parse_params(rc<Env> env, const Value& term, vector<Symbol>& args, vector<Param>& params) {
        auto iterable = iter_list(term);
        auto it = iterable.begin(), end = iterable.end();
        bool has_self = false;
        Value self;

        if (v_tail(term).type == T_VOID) { // catch single-element lists
            err(term.pos, "Parameter list must have more than one parameter.");
            return v_error({});
        }

        while (it != end) {
            const Value& v = *it ++;
            if (v.type == T_SYMBOL) {
                if (it == end) {} // don't do anything if we hit the end
                else if (it->type == T_SYMBOL && it->data.sym == S_QUESTION) { // argument
                    args.push(v.data.sym);
                    params.push(P_VAR);
                    it ++; // consume ?
                }
                else { // keyword
                    if (!has_self) {
                        params.push(P_SELF); // treat first keyword as self
                        self = v;
                        has_self = true;
                    }
                    else params.push(p_keyword(v.data.sym));
                }
            }
            else if (v.type.of(K_LIST)) { // non-empty list
                const Value& vh = v_head(v);
                if (vh.type == T_SYMBOL && vh.data.sym == S_QUOTE) { // quoted argument
                    const Value& vt = v_tail(v);
                    if (vt.type == T_VOID) {
                        err(v.pos, "Expected symbol in quoted parameter, but no symbol is provided here.");
                        return v_error({});
                    }
                    if (v_head(vt).type != T_SYMBOL) {
                        err(v.pos, "Expected symbol in quoted parameter, but '", v_head(vt), "' is not a symbol.");
                        return v_error({});
                    }
                    if (it == end) {
                        err(v.pos, "Expected '?' symbol after quoted parameter name, but ",
                            "no more terms are available in this list.");
                        return v_error({});
                    }
                    if (it->type != T_SYMBOL || it->data.sym != S_QUESTION) {
                        err(v.pos, "Expected '?' symbol after quoted parameter name, but '", *it, "' was provided.");
                        return v_error({});
                    }
                    args.push(v_head(vt).data.sym); // store quoted param name
                    params.push(P_QUOTED);
                    it ++; // consume ?
                }
                else {
                    err(v.pos, "Unexpected list term in parameter list: only symbols and quote forms are permitted; ",
                        "found '", v, "' instead.");
                    return v_error({});
                }
            }
        }

        if (!has_self) { // make sure we defined a name
            err(term.pos, "Parameter list is missing a name: either the first or second parameters must ",
                "be a keyword.");
            return v_error({});
        }

        return self;
    }

    // Handles form-level predefinition.
    rc<Form> define_form(rc<Env> env, const Value& term) {
        Value params = v_head(v_tail(term)), body = v_head(v_tail(v_tail(term)));
        if (params.type == T_SYMBOL) { // defining var
            Symbol name = params.data.sym;
            env->def(name, v_undefined(term.pos, name, F_TERM)); // define a placeholder variable
        }
        else if (params.type.of(K_LIST)) { // defining procedure
            vector<Symbol> arg_names;
            vector<Param> new_params; // params for the newly-defined procedure
            auto result = parse_params(env, params, arg_names, new_params);
            if (result.type != T_ERROR) { 
                rc<Form> form = f_callable(0, ASSOC_LEFT, new_params);
                env->def(result.data.sym, v_undefined(term.pos, result.data.sym, form)); // define a placeholder function
            }
        }
        return F_TERM; // def forms are not further applicable
    }

    // Handles a value-level definition.
    EvalResult define(rc<Env> env, const Value& args) {
        Value params = v_at(args, 0), body = v_at(args, 1);
        if (params.type == T_SYMBOL) { // defining var
            Symbol name = params.data.sym;
            EvalResult init = eval(env, body); // get the initial value
            if (init.value.type == T_ERROR) return init; // propagate errors
            init.env->def(name, init.value);
            return { init.env, v_void({}) }; // define doesn't return a value
        }
        else if (params.type.of(K_LIST)) { // defining procedure
            vector<Symbol> arg_names;
            vector<Param> new_params; // params for the newly-defined procedure
            auto result = parse_params(env, params, arg_names, new_params);
            if (result.type == T_ERROR) return { env, result }; // propagate errors
            
            rc<Form> form = f_callable(0, ASSOC_LEFT, new_params);

            Type fntype; // figure out a generic function type
            if (arg_names.size() == 1) fntype = t_func(T_ANY, T_ANY);
            else {
                vector<Type> arg_types;
                for (auto s : arg_names) arg_types.push(T_ANY);
                fntype = t_func(t_tuple(arg_types), T_ANY); 
            }

            rc<Env> fn_env = extend(env); // extend parent environment
            env->def(result.data.sym, v_func(args.pos, fntype, fn_env, arg_names, body).with(form)); 
            return { env, v_void({}) }; // define doesn't return a value
        }
        else {
            err(params.pos, "Expected either symbol or parameter list in definition; found '",
                params, "' instead.");
            return { env, v_error({}) };
        }
    }

    Builtin init_define() {
        return {
            t_func(t_tuple(T_ANY, T_ANY), T_VOID), // type
            f_callable(PREC_STRUCTURE, ASSOC_RIGHT, define_form, P_SELF, P_TERM, P_TERM), // form
            define, // comptime
            nullptr // we shouldn't be invoking def on runtime args because it doesn't eval its arguments
        };
    }

    void init_builtins() {
        DEF = init_define();
        ADD = {
            t_func(t_tuple(T_INT, T_INT), T_INT), // type
            f_callable(20, ASSOC_LEFT, P_VAR, P_SELF, P_VAR), // form
            [](rc<Env> env, const Value& args) -> EvalResult {
                return {
                    env,
                    v_int({}, v_tuple_at(args, 0).data.i + v_tuple_at(args, 1).data.i)
                };
            },
            nullptr
        };
        SUB = {
            t_func(t_tuple(T_INT, T_INT), T_INT), // type
            f_callable(20, ASSOC_LEFT, P_VAR, P_SELF, P_VAR), // form
            [](rc<Env> env, const Value& args) -> EvalResult {
                return {
                    env,
                    v_int({}, v_tuple_at(args, 0).data.i - v_tuple_at(args, 1).data.i)
                };
            },
            nullptr
        };
        MUL = {
            t_func(t_tuple(T_INT, T_INT), T_INT), // type
            f_callable(40, ASSOC_LEFT, P_VAR, P_SELF, P_VAR), // form
            [](rc<Env> env, const Value& args) -> EvalResult {
                return {
                    env,
                    v_int({}, v_tuple_at(args, 0).data.i * v_tuple_at(args, 1).data.i)
                };
            },
            nullptr
        };
    }

    void add_builtins(rc<Env> env) {
        if (!builtins_inited) builtins_inited = true, init_builtins();

        env->def(symbol_from("def"), v_func(DEF));

        env->def(symbol_from("+"), v_func(ADD));
        env->def(symbol_from("-"), v_func(SUB));
        env->def(symbol_from("*"), v_func(MUL));
    }
}