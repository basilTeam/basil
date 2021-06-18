#include "builtin.h"
#include "env.h"
#include "eval.h"
#include "forms.h"

namespace basil {
    Builtin
        DEF, DO, EVAL,
        IF, WHILE,
        ADD, SUB, MUL, DIV, MOD,
        INCR, DECR,
        LESS, LESS_EQUAL, GREATER, GREATER_EQUAL, EQUAL, NOT_EQUAL,
        AND, OR, XOR, NOT,
        HEAD, TAIL, CONS,
        LENGTH, FIND, SUBSTR,
        LIST, ARRAY, TUPLE,
        ASSIGN;

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
                if (it != end && it->type == T_SYMBOL && it->data.sym == S_QUESTION) { // argument
                    args.push(v.data.sym);
                    params.push(P_VAR);
                    it ++; // consume ?
                }
                else if (it != end && it->type == T_SYMBOL && it->data.sym == S_ELLIPSIS) { // variadic 
                    const Value& ellipsis = *it ++; // consume and record ...
                    if (it == end) {
                        err(ellipsis.pos, "Expected '?' symbol after variadic parameter, ",
                            "but no symbol is provided here.");
                        return v_error({});
                    }
                    if (it->type != T_SYMBOL || it->data.sym != S_QUESTION) {
                        err(it->pos, "Expected '?' symbol after variadic parameter, but '", *it, "' was provided.");
                        return v_error({});
                    }
                    args.push(v.data.sym);
                    params.push(P_VARIADIC);
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
                        err(v.pos, "Expected '?' or '...' symbol after quoted parameter name, but ",
                            "no more terms are available in this list.");
                        return v_error({});
                    }
                    bool variadic = false;
                    if (it->type == T_SYMBOL && it->data.sym == S_ELLIPSIS) 
                        variadic = true, it ++; // consume ellipsis
                    if (it->type != T_SYMBOL || it->data.sym != S_QUESTION) {
                        err(v.pos, "Expected '?' symbol after quoted parameter name, but '", *it, "' was provided.");
                        return v_error({});
                    }
                    args.push(v_head(vt).data.sym); // store quoted param name
                    params.push(variadic ? P_QUOTED_VARIADIC : P_QUOTED);
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
        DO = {
            t_func(t_list(T_ANY), T_ANY), // type
            f_callable(PREC_QUOTE - 1, ASSOC_RIGHT, P_SELF, P_VARIADIC), // form
            [](rc<Env> env, const Value& args) -> EvalResult {
                auto iterable = iter_list(args);
                auto a = iterable.begin(), b = a;
                ++ b;
                while (b != iterable.end()) ++ a, ++ b; // loop until the end
                return { env, *a }; // return the last value 
            },
            nullptr
        };
        EVAL = {
            t_func(T_ANY, T_ANY), // type
            f_callable(PREC_DEFAULT, ASSOC_LEFT, P_SELF, P_QUOTED),
            [](rc<Env> env, const Value& arg) -> EvalResult {
                Value temp = arg;
                return eval(env, temp);
            },
            nullptr
        };
        IF = {
            t_func(t_tuple(T_BOOL, T_ANY, T_ANY), T_ANY), // type
            f_callable(PREC_CONTROL, ASSOC_RIGHT, P_SELF, P_VAR, P_QUOTED, p_keyword(symbol_from("else")), P_QUOTED),
            [](rc<Env> env, const Value& args) -> EvalResult {
                Value if_true = v_at(args, 1), if_false = v_at(args, 2);
                if (v_at(args, 0).data.b) return eval(env, if_true);
                else return eval(env, if_false);
            },
            nullptr
        };
        WHILE = {
            t_func(t_tuple(T_ANY, T_ANY), T_VOID), // type
            f_callable(PREC_CONTROL, ASSOC_RIGHT, P_SELF, P_QUOTED, P_QUOTED), // form
            [](rc<Env> env, const Value& args) -> EvalResult {
                Value cond = v_at(args, 0), body = v_at(args, 1);
                EvalResult cond_eval = eval(env, cond);
                env = cond_eval.env;
                while (cond_eval.value.type == T_BOOL && cond_eval.value.data.b) {
                    EvalResult body_eval = eval(env, body); // eval body
                    env = body_eval.env;
                    cond_eval = eval(env, cond); // re-eval condition
                    env = cond_eval.env;
                }
                if (cond_eval.value.type != T_BOOL) {
                    err(cond_eval.value.pos, "Condition in 'while' expression must evaluate to a boolean; got '",
                        cond_eval.value.type, "' instead.");
                    return { env, v_error({}) };
                }
                return { env, v_void({}) };
            },
            nullptr
        };
        ADD = {
            t_func(t_tuple(T_INT, T_INT), T_INT), // type
            f_callable(PREC_ADD, ASSOC_LEFT, P_VAR, P_SELF, P_VAR), // form
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
            f_callable(PREC_ADD, ASSOC_LEFT, P_VAR, P_SELF, P_VAR), // form
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
            f_callable(PREC_MUL, ASSOC_LEFT, P_VAR, P_SELF, P_VAR), // form
            [](rc<Env> env, const Value& args) -> EvalResult {
                return {
                    env,
                    v_int({}, v_tuple_at(args, 0).data.i * v_tuple_at(args, 1).data.i)
                };
            },
            nullptr
        };
        DIV = {
            t_func(t_tuple(T_INT, T_INT), T_INT), // type
            f_callable(PREC_MUL, ASSOC_LEFT, P_VAR, P_SELF, P_VAR), // form
            [](rc<Env> env, const Value& args) -> EvalResult {
                return {
                    env,
                    v_int({}, v_tuple_at(args, 0).data.i / v_tuple_at(args, 1).data.i)
                };
            },
            nullptr
        };
        MOD = {
            t_func(t_tuple(T_INT, T_INT), T_INT), // type
            f_callable(PREC_MUL, ASSOC_LEFT, P_VAR, P_SELF, P_VAR), // form
            [](rc<Env> env, const Value& args) -> EvalResult {
                return {
                    env,
                    v_int({}, v_tuple_at(args, 0).data.i % v_tuple_at(args, 1).data.i)
                };
            },
            nullptr
        };
        INCR = {
            t_func(T_INT, T_INT), // type
            f_callable(PREC_PREFIX, ASSOC_RIGHT, P_SELF, P_VAR), // form
            [](rc<Env> env, const Value& args) -> EvalResult {
                return { env, v_int({}, args.data.i + 1)};
            },
            nullptr
        };
        DECR = {
            t_func(T_INT, T_INT), // type
            f_callable(PREC_PREFIX, ASSOC_RIGHT, P_SELF, P_VAR), // form
            [](rc<Env> env, const Value& args) -> EvalResult {
                return { env, v_int({}, args.data.i - 1)};
            },
            nullptr
        };
        LESS = {
            t_func(t_tuple(T_INT, T_INT), T_BOOL),
            f_callable(PREC_COMPARE, ASSOC_LEFT, P_VAR, P_SELF, P_VAR), // form
            [](rc<Env> env, const Value& args) -> EvalResult {
                return { env,
                    v_bool({}, v_tuple_at(args, 0).data.i < v_tuple_at(args, 1).data.i) };
            },
            nullptr
        };
        LESS_EQUAL = {
            t_func(t_tuple(T_INT, T_INT), T_BOOL),
            f_callable(PREC_COMPARE, ASSOC_LEFT, P_VAR, P_SELF, P_VAR), // form
            [](rc<Env> env, const Value& args) -> EvalResult {
                return { env,
                    v_bool({}, v_tuple_at(args, 0).data.i <= v_tuple_at(args, 1).data.i) };
            },
            nullptr
        };
        GREATER = {
            t_func(t_tuple(T_INT, T_INT), T_BOOL),
            f_callable(PREC_COMPARE, ASSOC_LEFT, P_VAR, P_SELF, P_VAR), // form
            [](rc<Env> env, const Value& args) -> EvalResult {
                return { env,
                    v_bool({}, v_tuple_at(args, 0).data.i > v_tuple_at(args, 1).data.i) };
            },
            nullptr
        };
        GREATER_EQUAL = {
            t_func(t_tuple(T_INT, T_INT), T_BOOL),
            f_callable(PREC_COMPARE, ASSOC_LEFT, P_VAR, P_SELF, P_VAR), // form
            [](rc<Env> env, const Value& args) -> EvalResult {
                return { env,
                    v_bool({}, v_tuple_at(args, 0).data.i >= v_tuple_at(args, 1).data.i) };
            },
            nullptr
        };
        EQUAL = {
            t_func(t_tuple(T_ANY, T_ANY), T_BOOL), // type
            f_callable(PREC_COMPARE, ASSOC_LEFT, P_VAR, P_SELF, P_VAR), // form
            [](rc<Env> env, const Value& args) -> EvalResult {
                return { env,
                    v_bool({}, v_tuple_at(args, 0) == v_tuple_at(args, 1)) };
            },
            nullptr
        };
        NOT_EQUAL = {
            t_func(t_tuple(T_ANY, T_ANY), T_BOOL), // type
            f_callable(PREC_COMPARE, ASSOC_LEFT, P_VAR, P_SELF, P_VAR), // form
            [](rc<Env> env, const Value& args) -> EvalResult {
                return { env,
                    v_bool({}, v_tuple_at(args, 0) != v_tuple_at(args, 1)) };
            },
            nullptr
        };
        AND = {
            t_func(t_tuple(T_BOOL, T_ANY), T_BOOL), // type
            f_callable(PREC_LOGIC, ASSOC_LEFT, P_VAR, P_SELF, P_QUOTED), // form
            [](rc<Env> env, const Value& args) -> EvalResult {
                if (!v_at(args, 0).data.b) return { env, v_at(args, 0) }; // short circuit false
                else {
                    Value rhs = v_at(args, 1);
                    return eval(env, rhs);
                }
            },
            nullptr
        };
        XOR = {
            t_func(t_tuple(T_BOOL, T_BOOL), T_BOOL), // type
            f_callable(PREC_LOGIC - 33, ASSOC_LEFT, P_VAR, P_SELF, P_VAR), // form
            [](rc<Env> env, const Value& args) -> EvalResult {
                return { env, v_bool({}, v_at(args, 0).data.b ^ v_at(args, 1).data.b) };
            },
            nullptr
        };
        OR = {
            t_func(t_tuple(T_BOOL, T_ANY), T_BOOL), // type
            f_callable(PREC_LOGIC - 66, ASSOC_LEFT, P_VAR, P_SELF, P_QUOTED), // form
            [](rc<Env> env, const Value& args) -> EvalResult {
                if (v_at(args, 0).data.b) return { env, v_at(args, 0) }; // short circuit true
                else {
                    Value rhs = v_at(args, 1);
                    return eval(env, rhs);
                }
            },
            nullptr
        };
        NOT = {
            t_func(T_BOOL, T_BOOL), // type
            f_callable(PREC_PREFIX, ASSOC_RIGHT, P_SELF, P_VAR), // form
            [](rc<Env> env, const Value& arg) -> EvalResult {
                return { env, v_bool({}, !arg.data.b) };
            },
            nullptr
        };
        HEAD = {
            t_func(t_list(T_ANY), T_ANY), // type
            f_callable(PREC_PREFIX, ASSOC_LEFT, P_VAR, P_SELF), // form
            [](rc<Env> env, const Value& arg) -> EvalResult {
                return {
                    env,
                    v_head(arg)
                };
            },
            nullptr
        };
        TAIL = {
            t_func(t_list(T_ANY), t_list(T_ANY)), // type
            f_callable(PREC_PREFIX, ASSOC_LEFT, P_VAR, P_SELF), // form
            [](rc<Env> env, const Value& arg) -> EvalResult {
                return {
                    env,
                    v_tail(arg)
                };
            },
            nullptr
        };
        CONS = {
            t_func(t_tuple(T_ANY, t_list(T_ANY)), T_ANY), // type
            f_callable(PREC_DEFAULT - 50, ASSOC_RIGHT, P_VAR, P_SELF, P_VAR), // form
            [](rc<Env> env, const Value& args) -> EvalResult {
                return {
                    env,
                    v_cons({}, v_at(args, 1).type, v_at(args, 0), v_at(args, 1))
                };
            },
            nullptr
        };
        LENGTH = {
            t_func(T_STRING, T_INT),
            f_callable(PREC_DEFAULT, ASSOC_LEFT, P_VAR, P_SELF),
            [](rc<Env> env, const Value& arg) -> EvalResult {
                return {
                    env,
                    v_int({}, arg.data.string.raw()->data.size())
                };
            },
            nullptr
        };
        FIND = {
            t_func(t_tuple(T_CHAR, T_STRING), T_INT),
            f_callable(PREC_DEFAULT, ASSOC_LEFT, P_SELF, P_VAR, P_VAR),
            [](rc<Env> env, const Value& args) -> EvalResult {
                auto start = v_tuple_at(args, 1).data.string->data.begin(),
                    end = v_tuple_at(args, 1).data.string->data.end();
                rune ch = v_tuple_at(args, 0).data.ch;
                u32 idx = 0;
                while (*start != ch && start != end) ++ start, ++ idx;
                return { env, v_int({}, start == end ? -1 : idx) };
            },
            nullptr
        };
        LIST = {
            t_func(t_list(T_ANY), T_ANY),
            f_callable(PREC_DEFAULT, ASSOC_RIGHT, P_SELF, P_VARIADIC),
            [](rc<Env> env, const Value& args) -> EvalResult {
                return { env, args };
            },
            nullptr
        };
        ARRAY = {
            t_func(t_list(T_ANY), T_ANY),
            f_callable(PREC_DEFAULT, ASSOC_RIGHT, P_SELF, P_VARIADIC),
            [](rc<Env> env, const Value& args) -> EvalResult {
                vector<Value> elements;
                for (const Value& v : iter_list(args)) elements.push(v);
                return { env, v_array({}, infer_array(elements), elements) };
            },
            nullptr
        };
        TUPLE = {
            t_func(t_tuple(T_ANY, T_ANY), T_ANY),
            f_callable(PREC_STRUCTURE, ASSOC_LEFT, P_VAR, P_SELF, P_QUOTED_VARIADIC),
            [](rc<Env> env, const Value& args) -> EvalResult {
                vector<Value> elements;
                elements.push(v_at(args, 0)); // lhs is always an element
                const Value& rest = v_at(args, 1);
                Value last = rest;
                bool expects_comma = false;
                for (Value v : iter_list(rest)) {
                    last = v;
                    if (expects_comma && (v.type != T_SYMBOL || v.data.sym != S_COMMA)) {
                        err(v.pos, "Expected ',' symbol in tuple constructor; found '", v, "' instead.");
                        return { env, v_error({}) };
                    }
                    else if (!expects_comma && v.type == T_SYMBOL && v.data.sym == S_COMMA) {
                        err(v.pos, "Found unexpected comma in tuple constructor - was expecting a tuple ",
                            "element in this position.");
                        return { env, v_error({}) };
                    }
                    else if (!expects_comma) {
                        auto er = eval(env, v);
                        if (er.value.type == T_ERROR) return er; // propagate errors
                        env = er.env;
                        elements.push(er.value);
                    }
                    expects_comma = !expects_comma; // alternate between commas and elements
                }
                if (!expects_comma) {
                    err(last.pos, "Unexpected trailing comma at the end of tuple constructor.");
                    return { env, v_error({}) };
                }
                return { env, v_tuple({}, infer_tuple(elements), elements) };
            },
            nullptr
        };
    }

    void add_builtins(rc<Env> env) {
        if (!builtins_inited) builtins_inited = true, init_builtins();

        env->def(symbol_from("def"), v_func(DEF));
        env->def(symbol_from("do"), v_func(DO));
        env->def(symbol_from("eval"), v_func(EVAL));
        env->def(symbol_from("if"), v_func(IF));
        env->def(symbol_from("while"), v_func(WHILE));

        env->def(symbol_from("+"), v_func(ADD));
        env->def(symbol_from("-"), v_func(SUB));
        env->def(symbol_from("*"), v_func(MUL));
        env->def(symbol_from("/"), v_func(DIV));
        env->def(symbol_from("%"), v_func(MOD));
        env->def(symbol_from("++"), v_func(INCR));
        env->def(symbol_from("--"), v_func(DECR));
        env->def(symbol_from("<"), v_func(LESS));
        env->def(symbol_from("<="), v_func(LESS_EQUAL));
        env->def(symbol_from(">"), v_func(GREATER));
        env->def(symbol_from(">="), v_func(GREATER_EQUAL));
        env->def(symbol_from("=="), v_func(EQUAL));
        env->def(symbol_from("!="), v_func(NOT_EQUAL));
        env->def(symbol_from("and"), v_func(AND));
        env->def(symbol_from("xor"), v_func(XOR));
        env->def(symbol_from("or"), v_func(OR));
        env->def(symbol_from("not"), v_func(NOT));
        env->def(symbol_from("head"), v_func(HEAD));
        env->def(symbol_from("tail"), v_func(TAIL));
        env->def(symbol_from("::"), v_func(CONS));
        env->def(symbol_from("length"), v_func(LENGTH));
        env->def(symbol_from("find"), v_func(FIND));
        env->def(symbol_from("list"), v_func(LIST));
        env->def(symbol_from("array"), v_func(ARRAY));
        env->def(symbol_from(","), v_func(TUPLE));

        env->def(symbol_from("true"), v_bool({}, true));
        env->def(symbol_from("false"), v_bool({}, false));
    }
}