#include "builtin.h"
#include "env.h"
#include "eval.h"
#include "forms.h"
#include "driver.h"

namespace basil {
    Builtin
        DEF, DEF_TYPED, LAMBDA, DO, EVAL, QUOTE, INFIX_DEF, // special forms, core behavior
        IMPORT, MODULE, USE, USE_AS, // modules
        AT_MODULE, DOT_MODULE, // accessors
        IF, IF_ELSE, WHILE, ARROW, MATCHES, MATCH, // conditionals
        ADD_INT, ADD_FLOAT, ADD_DOUBLE, SUB, MUL, DIV, MOD, // arithmetic
        INCR, DECR, // increment/decrement
        LESS, LESS_EQUAL, GREATER, GREATER_EQUAL, EQUAL, NOT_EQUAL, // comparisons
        AND, OR, XOR, NOT, // logic
        HEAD, TAIL, CONS, // list operations
        LENGTH, FIND, SUBSTR, // string operations
        LIST, ARRAY, TUPLE, NEW, // data constructors
        UNION_TYPE, NAMED_TYPE, FN_TYPE, REF_TYPE, JUST, TYPE_VAR, TYPEOF, IS, ANNOTATE, // type operators
        ASSIGN, // mutation
        DEBUG; // debug

    bool builtins_inited = false;

    // common operator precedence levels
    static const i64
        PREC_ANNOTATED = 500, // annotations and property accesses, like infix : and .
        PREC_PREFIX = 400, // simple prefix operators like 'not'
        PREC_MUL = 300, // multiplication, division, modulus
        PREC_ADD = 200, // addition and subtraction
        PREC_TYPE = 100, // type construction operators
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
    Value parse_params(rc<Env> env, const Value& term, vector<Symbol>& args, vector<Param>& params, bool is_lambda) {
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
                    params.push(p_var(v.data.sym));
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
                    params.push(p_variadic(v.data.sym));
                    it ++; // consume ?
                }
                else if (!is_lambda) { // keyword
                    if (!has_self) {
                        params.push(P_SELF); // treat first keyword as self
                        self = v;
                        has_self = true;
                    }
                    else params.push(p_keyword(v.data.sym));
                }
                else {
                    err(it->pos, "Keywords are not permitted in lambda parameter list.");
                    return v_error({});
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
                    Symbol name = v_head(vt).data.sym;
                    args.push(name); // store quoted param name
                    params.push(variadic ? p_quoted_variadic(name) : p_quoted(name));
                    it ++; // consume ?
                }
                else {
                    err(v.pos, "Unexpected list term in parameter list: only symbols and quote forms are permitted; ",
                        "found '", v, "' instead.");
                    return v_error({});
                }
            }
        }

        if (is_lambda) { // return early if we're a lambda
            return v_void({});
        }

        if (!has_self) { // make sure we defined a name if we're not a lambda
            err(term.pos, "Parameter list is missing a name: either the first or second parameters must ",
                "be a keyword.");
            return v_error({});
        }

        return self;
    }

    // Handles form-level predefinition.
    template<bool HAS_TYPE>
    rc<Form> define_form(rc<Env> env, const Value& term) {
        Value params = v_head(v_tail(term)), body = HAS_TYPE ? v_head(v_tail(v_tail(v_tail(v_tail(term))))) : v_head(v_tail(v_tail(term)));
        if (params.type == T_SYMBOL) { // defining var
            Symbol name = params.data.sym;
            if (!env->find(name)) env->def(name, v_undefined(term.pos, name, body.form)); // define a placeholder variable
        }
        else if (params.type.of(K_LIST)) { // defining procedure
            vector<Symbol> arg_names;
            vector<Param> new_params; // params for the newly-defined procedure
            auto result = parse_params(env, params, arg_names, new_params, false);
            if (result.type == T_ERROR) return F_TERM;

            rc<Form> form = f_callable(0, new_params[0].kind == PK_SELF ? ASSOC_RIGHT : ASSOC_LEFT, new_params);

            auto location = locate(env, result.data.sym); // try and find existing definitions

            if (!location) { // fresh definition
                env->def(result.data.sym, v_undefined(term.pos, result.data.sym, form)); // define a placeholder function
                return F_TERM;
            }

            if (!location->is(env)) { // redefining further removed definition
                env->def(result.data.sym, *((*location)->find(result.data.sym))); // define symbol locally
            }

            auto existing = *env->find(result.data.sym);

            rc<Form> existing_form = existing.form;
            if (form->assoc != existing_form->assoc) {
                form->assoc = existing_form->assoc;
                // err(term.pos, "Tried to define overload of function '", result.data.sym, "' with different ",
                //     "associativity: existing form is ", existing_form->assoc == ASSOC_LEFT ? "left" : "right", 
                //     " associative, while new form is ", form->assoc == ASSOC_LEFT ? "left" : "right", " associative.");
                // return F_TERM;
            }
            if (form->precedence != existing_form->precedence) {
                form->precedence = existing_form->precedence;
                // err(term.pos, "Tried to define overload of function '", result.data.sym, "' with different ",
                //     "precedence: existing form has precedence ", existing_form->precedence, " while new form has ", 
                //     "precedence ", form->precedence, ".");
                // return F_TERM;
            }

            rc<Form> overloaded;
            if (existing_form->kind == FK_CALLABLE) {
                overloaded = f_overloaded(form->precedence, form->assoc, form, existing_form);
                env->def(result.data.sym, v_intersect(existing.pos, t_intersect(existing.type, T_UNDEFINED),
                    existing.type, existing, T_UNDEFINED, v_undefined(term.pos, result.data.sym, form)).with(overloaded));
            }
            else if (existing_form->kind == FK_OVERLOADED) {
                overloaded = f_overloaded(form->precedence, form->assoc, existing_form, form);
                auto values = existing.data.isect->values;
                values.put(T_UNDEFINED, v_undefined(term.pos, result.data.sym, form));
                env->def(result.data.sym, v_intersect(existing.pos, t_intersect_with(existing.type, T_UNDEFINED),
                    values).with(overloaded));
            }
        }
        return F_TERM; // def forms are not further applicable
    }

    // Handles a value-level definition.
    Value define(rc<Env> env, const Value& args, optional<Value> type_expr) {
        Value params = v_at(args, 0), body = v_at(args, type_expr ? 2 : 1);
        if (params.type == T_SYMBOL) { // defining var
            Symbol name = params.data.sym;
            Value init = eval(env, body); // get the initial value
            init.form = body.form;
            if (init.type == T_ERROR) return init; // propagate errors

            if (type_expr) {
                Value type_value = eval(env, *type_expr);
                if (!type_value.type.coerces_to(T_TYPE)) {
                    err(type_expr->pos, "Expected type expression in annotation, found value '", type_value, "'",
                        " of incompatible type '", type_value.type, "'.");
                    return v_error({});
                }
                type_value = coerce(type_value, T_TYPE);
                Type type = type_value.data.type;
                if (!init.type.coerces_to(type)) {
                    err(init.pos, "Inferred variable type '", init.type, 
                        "' is incompatible with declared type '", type, "'.");
                    return v_error({});
                }
                init = coerce(init, type);
            }

            env->def(name, init);
            return v_void({}); // define doesn't return a value
        }
        else if (params.type.of(K_LIST)) { // defining procedure
            vector<Symbol> arg_names;
            vector<Param> new_params; // params for the newly-defined procedure
            auto result = parse_params(env, params, arg_names, new_params, false);
            if (result.type == T_ERROR) return result; // propagate errors

            rc<Form> form = f_callable(0, new_params[0].kind == PK_SELF ? ASSOC_RIGHT : ASSOC_LEFT, new_params);
            rc<Env> fn_env = extend(env); // extend parent environment

            Type fntype; // figure out a generic function type
            if (arg_names.size() == 1) fntype = t_func(T_ANY, T_ANY);
            else {
                vector<Type> arg_types;
                for (auto p : new_params) {
                    if (is_variadic(p.kind)) 
                        arg_types.push(t_list(T_ANY));
                    else if (p.kind != PK_KEYWORD && p.kind != PK_SELF) 
                        arg_types.push(T_ANY);
                }
                fntype = t_func(t_tuple(arg_types), T_ANY);
            }

            if (type_expr) {
                Value type_value = eval(fn_env, *type_expr);
                if (!type_value.type.coerces_to(T_TYPE)) {
                    err(type_expr->pos, "Expected type expression in annotation, found value '", type_value, "'",
                        " of incompatible type '", type_value.type, "'.");
                    return v_error({});
                }
                type_value = coerce(type_value, T_TYPE);
                Type type = type_value.data.type;
                if (!type.coerces_to(fntype)) {
                    err(params.pos, "Inferred function type '", fntype, 
                        "' is incompatible with declared type '", type, "'.");
                    return v_error({});
                }
                fntype = type;
            }

            Value func = v_func(args.pos, fntype, fn_env, arg_names, body).with(form);

            auto it = env->find(result.data.sym); // look for existing definition
            if (it->type == T_UNDEFINED) { // fresh definition
                // even if we previously defined the value with an overloaded form during form resolution,
                // we don't know that those same definitions will occur during evaluation (due to control
                // flow, loops, etc). so, we start out with just a callable form again, and overload it
                // as we find new definitions.
                env->def(result.data.sym, func);
            }
            else {
                Value existing = *it;      
                rc<Form> existing_form = existing.form;      
                if (form->assoc != existing_form->assoc) {
                    form->assoc = existing_form->assoc;
                    // err(term.pos, "Tried to define overload of function '", result.data.sym, "' with different ",
                    //     "associativity: existing form is ", existing_form->assoc == ASSOC_LEFT ? "left" : "right", 
                    //     " associative, while new form is ", form->assoc == ASSOC_LEFT ? "left" : "right", " associative.");
                    // return F_TERM;
                }
                if (form->precedence != existing_form->precedence) {
                    form->precedence = existing_form->precedence;
                    // err(term.pos, "Tried to define overload of function '", result.data.sym, "' with different ",
                    //     "precedence: existing form has precedence ", existing_form->precedence, " while new form has ", 
                    //     "precedence ", form->precedence, ".");
                    // return F_TERM;
                }
                if (existing.type.of(K_FUNCTION)) {
                    if (existing.type == fntype) {
                        err(params.pos, "Tried to define overload with type '", fntype, "' for function '",
                            result.data.sym, "', but an overload with that type already exists.");
                        return v_error({});
                    }
                    auto overloaded = f_overloaded(
                        existing.form->precedence, existing.form->assoc, 
                        form, existing.form
                    );
                    // println("existing form = ", existing.form, ", func form = ", form);
                    // println("unified ", existing.form, " and ", form, " to ", overloaded);
                    // we aren't checking that the overloaded is present here because we assume
                    // we would have caught such errors in the broader form resolution phase.
                    Value isect = v_intersect(existing.pos, t_intersect(existing.type, fntype), 
                        existing.type, existing, fntype, func).with(overloaded);
                    env->def(result.data.sym, isect); // redefine as intersect
                }
                else if (existing.type.of(K_INTERSECT)) {
                    if (t_intersect_has(existing.type, fntype)) {
                        err(params.pos, "Tried to define overload with type '", fntype, "' for function '",
                            result.data.sym, "', but an overload with that type already exists.");
                        return v_error({});
                    }
                    auto overloaded = f_overloaded(form->precedence, form->assoc, existing.form, form);
                    // println("existing form = ", existing.form, ", func form = ", form);
                    // println("unified ", existing.form, " and ", form, " to ", overloaded);
                    auto existing_values = existing.data.isect->values;
                    existing_values.erase(T_UNDEFINED);
                    existing_values.put(fntype, func);
                    env->def(result.data.sym, v_intersect(existing.pos, 
                        t_intersect_with(t_intersect_without(existing.type, T_UNDEFINED), fntype),
                        existing_values).with(overloaded));
                }
                else {
                    err(params.pos, "Could not define function '", result.data.sym, "': name is already ",
                        "defined as non-function type '", existing.type, "'.");
                    return v_error({});
                }
            }

            return v_void(args.pos); // define doesn't return a value
        }
        else {
            err(params.pos, "Expected either symbol or parameter list in definition; found '",
                params, "' instead.");
            return v_error({});
        }
    }

    rc<Form> lambda_form(rc<Env> env, const Value& term) {
        Value params = v_head(v_tail(term)), body = v_head(v_tail(v_tail(term)));
        if (params.type.of(K_LIST)) {
            vector<Symbol> new_args;
            vector<Param> new_params;
            new_params.push(P_SELF);
            Value result = parse_params(env, params, new_args, new_params, true);
            if (result.type == T_ERROR) return F_TERM;
            return f_callable(0, ASSOC_RIGHT, new_params);
        }
        else {
            err(params.pos, "Expected a parameter list in this lambda expression, found non-list term '",
                params, "' instead.");
            return F_TERM;
        }
    }

    Value lambda(rc<Env> env, const Value& args) {
        Value params = v_at(args, 0), body = v_at(args, 1);
        if (params.type.of(K_LIST)) {
            vector<Symbol> arg_names;
            vector<Param> new_params;
            new_params.push(P_SELF);
            Value result = parse_params(env, params, arg_names, new_params, true);
            if (result.type == T_ERROR) return v_error({}); // propagate errors

            rc<Form> form = f_callable(0, new_params[0].kind == PK_SELF ? ASSOC_RIGHT : ASSOC_LEFT, new_params);

            Type fntype; // figure out a generic function type
            if (arg_names.size() == 1) fntype = t_func(T_ANY, T_ANY);
            else {
                vector<Type> arg_types;
                for (auto s : arg_names) arg_types.push(T_ANY);
                fntype = t_func(t_tuple(arg_types), T_ANY);
            }

            rc<Env> fn_env = extend(env); // extend parent environment
            return v_func(args.pos, fntype, fn_env, arg_names, body).with(form);
        }
        else {
            err(params.pos, "Expected a parameter list in this lambda expression, found non-list term '",
                params, "' instead.");
            return v_error({});
        }
    }
    
    // Returns Value(true) if the provided value 'v' matches the pattern, 
    // returns Value(false) otherwise.
    Value match_case(rc<Env> env, const Value& pattern, const Value& v) {
        switch (pattern.type.kind()) {
            case K_INT:
            case K_DOUBLE:
            case K_FLOAT:
            case K_VOID:
            case K_STRING:
            case K_CHAR:    // value match
                return v_bool(pattern.pos, v == pattern);
            case K_SYMBOL: { // variable binding pattern
                Value pattern_copy = pattern;
                Value var = eval(env, pattern_copy);
                if (var.type == T_ERROR) return v_error({});
                else return v_bool(pattern.pos, v == var);
            }
            case K_LIST: {
                if (v_head(pattern).type != T_SYMBOL) { // we only match to operator-type patterns
                    err(pattern.pos, "Unknown matching pattern '", pattern, "': expected some ",
                        "operator symbol (such as '::' or ',') to match over.");
                    return v_error({});
                }
                Symbol op = v_head(pattern).data.sym;
 
                if (op == S_QUESTION) { // var binding
                    if (v_tail(pattern).type == T_VOID) {
                        err(pattern.pos, "Expected name in binding pattern.");
                        return v_error({});
                    }
                    if (v_head(v_tail(pattern)).type != T_SYMBOL) {
                        err(v_head(v_tail(pattern)).pos, "Expected name in binding pattern, but given '",
                            v_head(v_tail(pattern)), "' instead.");
                        return v_error({});
                    }
                    if (v_tail(v_tail(pattern)).type != T_VOID) {
                        err(v_head(v_tail(v_tail(pattern))).pos, "Too many terms in binding pattern.");
                        return v_error({});
                    }
                    Symbol name = v_head(v_tail(pattern)).data.sym;
                    env->def(name, v);
                    return v_bool(pattern.pos, true);
                }
                else if (op == S_COLON) { // annotation pattern
                    if (v_tail(pattern).type == T_VOID) {
                        err(pattern.pos, "Expected subpattern in first position of annotation pattern.");
                        return v_error({});
                    }
                    if (v_tail(v_tail(pattern)).type == T_VOID) {
                        err(pattern.pos, "Expected type value in annotation pattern.");
                        return v_error({});
                    }
                    if (v_tail(v_tail(v_tail(pattern))).type != T_VOID) {
                        err(v_head(v_tail(v_tail(v_tail(pattern)))).pos, "Too many terms in annotation pattern.");
                        return v_error({});
                    }

                    Value recur = v_head(v_tail(pattern)), type_expr = v_head(v_tail(v_tail(pattern)));
                    Value type = eval(env, type_expr);
                    if (type.type == T_ERROR) return v_error({});
                    if (type.type != T_TYPE) {
                        err(type.pos, "Expected type value in annotation pattern, but given '", 
                            type, "' instead.");
                        return v_error({});
                    }
                    if (v.type.coerces_to(type.data.type)) // match values of compatible type
                        return match_case(env, recur, coerce(v, type.data.type));
                    else if (v.type.of(K_UNION) && t_union_has(v.type, type.data.type) // match unions
                        && v.data.u->value.type == type.data.type)
                        return match_case(env, recur, v.data.u->value);
                    else // we couldn't match the type, so we return false
                        return v_bool(pattern.pos, false);
                }
                else if (op == S_CONS) { // list destructuring pattern
                    if (v_tail(pattern).type == T_VOID) {
                        err(pattern.pos, "Expected subpattern in first position of list pattern.");
                        return v_error({});
                    }
                    if (v_tail(v_tail(pattern)).type == T_VOID) {
                        err(pattern.pos, "Expected subpattern in second position of list pattern.");
                        return v_error({});
                    }
                    if (v_tail(v_tail(v_tail(pattern))).type != T_VOID) {
                        err(v_head(v_tail(v_tail(v_tail(pattern)))).pos, "Too many terms in list pattern.");
                        return v_error({});
                    }

                    if (!v.type.of(K_LIST)) return v_bool(pattern.pos, false); // this pattern only matches lists
                    Value left = match_case(env, v_head(v_tail(pattern)), v_head(v)),
                          right = match_case(env, v_head(v_tail(v_tail(pattern))), v_tail(v));

                    if (left.type == T_ERROR || right.type == T_ERROR) // propagate errors
                        return v_error({});

                    return v_bool(pattern.pos, left.data.b && right.data.b);
                }
                else if (op == S_COMMA) { // tuple destructuring pattern
                    if (!v.type.of(K_TUPLE)) return v_bool(pattern.pos, false); // this pattern only matches tuples
                    vector<Value> subpatterns;
                    list_iterable iter = iter_list(v_tail(pattern));
                    bool first = true, expects_comma = false;
                    for (const Value& v : iter) {
                        if (first) {
                            first = false;
                            subpatterns.push(v);
                            continue;
                        }
                        else if (!expects_comma) {
                            if (v.type == T_SYMBOL && v.data.sym == S_COMMA) {
                                err(v.pos, "Unexpected '", v, "' in tuple pattern: expected a subpattern.");
                                return v_error({});
                            }
                            else subpatterns.push(v);
                        }
                        else {
                            if (v.type != T_SYMBOL || v.data.sym != S_COMMA) {
                                err(v.pos, "Expected ',' in tuple pattern, but found '", v, "' instead.");
                                return v_error({});
                            }
                        }
                        expects_comma = !expects_comma;
                    }
                    if (subpatterns.size() < 2) {
                        err(pattern.pos, "At least two subpatterns required in tuple pattern.");
                        return v_error({});
                    }
                    if (subpatterns.size() != v_len(v)) return v_bool(pattern.pos, false); // size mismatch
                    for (u32 i = 0; i < subpatterns.size(); i ++) {
                        Value result = match_case(env, subpatterns[i], v_at(v, i));
                        if (result.type == T_ERROR) return v_error({});
                        if (result.data.b == false) return v_bool(pattern.pos, false);
                    }
                    return v_bool(pattern.pos, true);
                }
                else if (op == S_OF) {
                    if (v_tail(pattern).type == T_VOID) {
                        err(pattern.pos, "Expected symbol in first position of named pattern.");
                        return v_error({});
                    }
                    if (v_head(v_tail(pattern)).type != T_SYMBOL) {
                        err(v_head(v_tail(pattern)).pos, "Expected symbol in first position of named pattern, ",
                            "but found '", v_head(v_tail(pattern)), "' instead.");
                        return v_error({});
                    }
                    if (v_tail(v_tail(pattern)).type == T_VOID) {
                        err(pattern.pos, "Expected subpattern in second position of named pattern.");
                        return v_error({});
                    }
                    if (v_tail(v_tail(v_tail(pattern))).type != T_VOID) {
                        err(v_head(v_tail(v_tail(v_tail(pattern)))).pos, "Too many terms in named pattern.");
                        return v_error({});
                    }
                    Value name = v_head(v_tail(pattern)), subpattern = v_head(v_tail(v_tail(pattern)));
                    const Value& v_or_inner = v.type.of(K_UNION) ? v.data.u->value : v; // match on union member if it's a union
                    
                    if (v_or_inner.type.of(K_NAMED)) {
                        if (t_get_name(v_or_inner.type) == name.data.sym) { // if it's the right named type
                            return match_case(env, subpattern, v_or_inner.data.named->value);
                        }
                        else return v_bool(pattern.pos, false);
                    }  
                    else return v_bool(pattern.pos, false);
                }
                else {
                    err(pattern.pos, "Unknown matching pattern '", pattern, "': operator '", 
                        op, "' has no implemented matching behavior.");
                    return v_error({});
                }
            }
            default:
                return v_bool(pattern.pos, false); // no match found   
        }
    }

    void init_builtins() {
        DEF = {
            t_func(t_tuple(T_ANY, T_ANY), T_VOID), // type
            f_callable(PREC_STRUCTURE, ASSOC_RIGHT, define_form<false>, P_SELF, p_term("params"), p_term("body")), // form
            [](rc<Env> env, const Value& args) -> Value { // comptime
                return define(env, args, none<Value>());
            },
            nullptr // we shouldn't be invoking def on runtime args because it doesn't eval its arguments
        };
        DEF_TYPED = {
            t_func(t_tuple(T_ANY, T_ANY, T_ANY), T_VOID), // type
            f_callable(PREC_STRUCTURE, ASSOC_RIGHT, define_form<true>, P_SELF, p_term("params"), 
                p_keyword(":"), p_quoted("type"), p_term("body")), // form
            [](rc<Env> env, const Value& args) -> Value { // comptime
                return define(env, args, some<Value>(v_at(args, 1)));
            },
            nullptr // we shouldn't be invoking def on runtime args because it doesn't eval its arguments
        };
        INFIX_DEF = {
            t_func(t_tuple(T_SYMBOL, T_ANY), T_VOID), // type
            f_callable(PREC_STRUCTURE, ASSOC_RIGHT, define_form<false>, p_quoted("name"), P_SELF, p_quoted("init")),
            [](rc<Env> env, const Value& args) -> Value {
                return define(env, args, none<Value>());
            },
            nullptr
        };
        LAMBDA = {
            t_func(t_tuple(T_ANY, T_ANY), T_VOID), // type
            f_callable(PREC_STRUCTURE, ASSOC_RIGHT, lambda_form, P_SELF, p_term("params"), p_term("body")), // form
            lambda, // comptime
            nullptr // we shouldn't be invoking def on runtime args because it doesn't eval its arguments
        };
        DO = {
            t_func(t_list(T_ANY), T_ANY), // type
            f_callable(PREC_QUOTE - 1, ASSOC_RIGHT, P_SELF, p_variadic("exprs")), // form
            [](rc<Env> env, const Value& args) -> Value {
                auto iterable = iter_list(args);
                auto a = iterable.begin(), b = a;
                ++ b;
                while (b != iterable.end()) ++ a, ++ b; // loop until the end
                return *a; // return the last value 
            },
            nullptr
        };
        EVAL = {
            t_func(T_ANY, T_ANY), // type
            f_callable(PREC_DEFAULT, ASSOC_LEFT, P_SELF, p_var("expr")),
            [](rc<Env> env, const Value& arg) -> Value {
                Value temp = arg;
                return eval(env, temp);
            },
            nullptr
        };
        QUOTE = {
            t_func(T_ANY, T_ANY), // type
            f_callable(PREC_QUOTE, ASSOC_LEFT, P_SELF, p_quoted("expr")),
            [](rc<Env> env, const Value& arg) -> Value {
                return arg;
            },
            nullptr
        };
        IMPORT = {
            t_func(T_STRING, T_MODULE), // type
            f_callable(PREC_CONTROL, ASSOC_RIGHT, 
                (FormCallback)[](rc<Env> env, const Value& term) -> rc<Form> { // form callback
                    if (v_tail(term).type == T_VOID || v_head(v_tail(term)).type != T_STRING) 
                        return F_TERM; // expecting a string in 2nd position

                    optional<rc<Env>> mod_env = load(v_head(v_tail(term)).data.string->data.raw());
                    if (!mod_env) return F_TERM;
                    
                    map<Value, rc<Form>> forms;
                    for (const auto& [k, v] : (*mod_env)->values) {
                        forms.put(v_symbol({}, k), v.form);
                    }
                    rc<Form> compound = f_compound(*mod_env, forms);
                    return compound;
                }, P_SELF, p_var("path")),
            [](rc<Env> env, const Value& arg) -> Value {
                optional<rc<Env>> mod_env = load(arg.data.string->data.raw());
                return mod_env ? v_module({}, *mod_env) : v_error({});
            },
            nullptr
        };
        MODULE = {
            t_func(t_list(T_ANY), T_MODULE), // type
            f_callable(PREC_CONTROL, ASSOC_RIGHT, 
                (FormCallback)[](rc<Env> env, const Value& term) -> rc<Form> { // form callback
                    Value body = v_cons(term.pos, t_list(T_ANY), v_symbol(term.pos, S_DO), v_tail(term));
                    rc<Env> mod_env = extend(env);
                    resolve_form(mod_env, body);
                    map<Value, rc<Form>> forms;
                    for (const auto& [k, v] : mod_env->values) {
                        forms.put(v_symbol({}, k), v.form);
                    }
                    return f_compound(mod_env, forms);
                }, P_SELF, p_term_variadic("contents")),
            [](rc<Env> env, const Value& arg) -> Value {
                rc<Env> mod_env = extend(env);
                Value body = v_cons(arg.pos, t_list(T_ANY), v_symbol(arg.pos, S_DO), arg);
                Value result = eval(mod_env, body);
                Value mod = v_module({}, mod_env);
                return result.type != T_ERROR ? v_module({}, mod_env) : v_error({});
            },
            nullptr
        };
        USE = {
            t_func(T_STRING, T_VOID), // type
            f_callable(PREC_STRUCTURE, ASSOC_RIGHT,
                (FormCallback)[](rc<Env> env, const Value& term) -> rc<Form> { // form callback
                    Value body = v_cons(term.pos, t_list(T_ANY), v_symbol(term.pos, S_DO), v_tail(term));
                    if (v_tail(term).type == T_VOID || v_head(v_tail(term)).type != T_STRING) 
                        return F_TERM; // expecting a string in 2nd position

                    optional<rc<Env>> mod_env = load(v_head(v_tail(term)).data.string->data.raw());
                    if (!mod_env) return F_TERM;
                    
                    for (const auto& [k, v] : (*mod_env)->values) {
                        env->def(k, v);
                    }
                    return F_TERM;
                }, P_SELF, p_var("path")), // form,
            [](rc<Env> env, const Value& arg) -> Value {
                optional<rc<Env>> mod_env = load(arg.data.string->data.raw());
                if (!mod_env) return v_error({});
                else {
                    for (const auto& [k, v] : (*mod_env)->values)
                        env->def(k, v); // copy definitions from module
                    return v_void({});
                }
            },
            nullptr
        };
        AT_MODULE = {
            t_func(t_tuple(T_MODULE, T_SYMBOL), T_ANY), // type
            f_callable(PREC_ANNOTATED, ASSOC_LEFT, p_var("module"), P_SELF, p_var("key")),
            [](rc<Env> env, const Value& args) -> Value {
                auto find = v_at(args, 0).data.mod->env->find(v_at(args, 1).data.sym);
                if (!find) {
                    err(v_at(args, 1).pos, "Could not resolve variable '", v_at(args, 1), "' in "
                        "the provided module.");
                    return v_error({});
                }
                return *find;
            },
            nullptr
        };
        DOT_MODULE = {
            t_func(t_tuple(T_MODULE, T_SYMBOL), T_ANY), // type
            f_callable(PREC_ANNOTATED, ASSOC_LEFT, 
                (FormCallback)[](rc<Env> env, const Value& term) -> rc<Form> {
                    if (v_tail(term).type == T_VOID || v_tail(v_tail(term)).type == T_VOID) return F_TERM;
                    rc<Form> mod = v_head(v_tail(term)).form;
                    if (mod->kind != FK_COMPOUND) return F_TERM; // we're only concerned with module forms
                    if (v_head(v_tail(v_tail(term))).type != T_SYMBOL) return F_TERM; // expecting a symbol
                    auto it = mod->compound->members.find(v_head(v_tail(v_tail(term))));
                    if (it != mod->compound->members.end()) {
                        return it->second;
                    }
                    else return F_TERM;
                }, p_var("module"), P_SELF, p_quoted("key")),
            [](rc<Env> env, const Value& args) -> Value {
                auto find = v_at(args, 0).data.mod->env->find(v_at(args, 1).data.sym);
                if (!find) {
                    err(v_at(args, 1).pos, "Could not resolve variable '", v_at(args, 1), "' in "
                        "the provided module.");
                    return v_error({});
                }
                return *find;
            },
            nullptr
        };
        // USE = {
        //     t_func(T_STRING, T_MODULE), // type

        // },
        // USE_AS = {
        //     t_func(t_tuple(T_STRING, T_SYMBOL), T_VOID), // type
        //     f_callable(PREC_STRUCTURE, ASSOC_RIGHT, P_SELF, p_var("path"), p_keyword("as"), p_quoted("name")), // form
        //     [](rc<Env> env, const Value& arg) -> Value {
        //         rc<Env> mod_env = load(v_at(arg, 0).data.string->data.raw());
        //         Value mod = v_module()
        //         env->def(v_at(arg, 1).data.sym, mod);
        //         return arg;
        //     },
        //     nullptr
        // },
        IF = {
            t_func(t_tuple(T_BOOL, T_ANY), T_VOID), // type
            f_callable(PREC_CONTROL, ASSOC_RIGHT, P_SELF, p_var("cond"), p_keyword("then"), p_quoted("if-true")),
            [](rc<Env> env, const Value& args) -> Value {
                Value if_true = v_at(args, 1);
                if (v_at(args, 0).data.b) eval(env, if_true);
                return v_void({});
            },
            nullptr
        };
        IF_ELSE = {
            t_func(t_tuple(T_BOOL, T_ANY, T_ANY), T_ANY), // type
            f_callable(PREC_CONTROL, ASSOC_RIGHT, P_SELF, p_var("cond"), p_keyword("then"), p_quoted("if-true"), 
                p_keyword("else"), p_quoted("if-false")),
            [](rc<Env> env, const Value& args) -> Value {
                Value if_true = v_at(args, 1), if_false = v_at(args, 2);
                if (v_at(args, 0).data.b) return eval(env, if_true);
                else return eval(env, if_false);
            },
            nullptr
        };
        WHILE = {
            t_func(t_tuple(T_ANY, T_ANY), T_VOID), // type
            f_callable(PREC_CONTROL, ASSOC_RIGHT, P_SELF, p_quoted("cond"), p_quoted("body")), // form
            [](rc<Env> env, const Value& args) -> Value {
                Value cond = v_at(args, 0), body = v_at(args, 1);
                Value cond_eval = eval(env, cond);
                while (cond_eval.type == T_BOOL && cond_eval.data.b) {
                    Value body_eval = eval(env, body); // eval body
                    cond_eval = eval(env, cond); // re-eval condition
                }
                if (cond_eval.type != T_BOOL) {
                    err(cond_eval.pos, "Condition in 'while' expression must evaluate to a boolean; got '",
                        cond_eval.type, "' instead.");
                    return v_error({});
                }
                return v_void({});
            },
            nullptr
        };
        ARROW = {
            t_func(t_tuple(T_ANY, T_ANY), T_BOOL),
            f_callable(PREC_CONTROL - 20, ASSOC_RIGHT, p_quoted("pattern"), P_SELF, p_quoted("body")),
            [](rc<Env> env, const Value& args) -> Value {
                return v_void({}); // should not be executed, but maybe we can repurpose this for arrow functions?
            },
            nullptr
        };
        MATCHES = {
            t_func(t_tuple(T_ANY, T_ANY), T_BOOL),
            f_callable(PREC_COMPARE - 20, ASSOC_LEFT, p_var("value"), P_SELF, p_quoted("case")),
            [](rc<Env> env, const Value& args) -> Value {
                return match_case(env, v_at(args, 1), v_at(args, 0));
            },
            nullptr
        };
        MATCH = {
            t_func(t_tuple(T_ANY, t_list(T_ANY)), T_ANY),
            f_callable(PREC_CONTROL - 40, ASSOC_RIGHT, P_SELF, p_var("value"), p_quoted("cases")),
            [](rc<Env> env, const Value& args) -> Value {
                if (v_at(args, 1).type == T_VOID) { // third argument is just flat out empty
                    err(v_at(args, 1).pos, "No cases provided in 'match' expression.");
                    return v_error({});
                }
                if (v_head(v_at(args, 1)).type != T_SYMBOL
                    || v_head(v_at(args, 1)).data.sym != S_WITH) { // no 'with' present
                    err(v_head(v_at(args, 1)).pos, "Expected 'with' symbol before match expression cases, found '",
                        v_head(v_at(args, 1)), "' instead.");
                    return v_error({});
                }
                if (v_tail(v_at(args, 1)).type == T_VOID) { // no cases after 'with'
                    err(v_at(args, 1).pos, "No cases provided in 'match' expression.");
                    return v_error({});
                }
                for (const Value& c : iter_list(v_tail(v_at(args, 1)))) {
                    if (!c.type.of(K_LIST) || v_head(c).type != T_SYMBOL 
                        || v_head(c).data.sym != S_CASE_ARROW) {
                        err(c.pos, "Expected case arrow expression, with form '", p_quoted("pattern"), " ",
                            p_keyword("=>"), " ", p_quoted("body"), "', but found '", c, "' instead.");
                        return v_error({});
                    }
                    if (v_tail(c).type == T_VOID) {
                        err(c.pos, "No pattern expression provided in case.");
                        return v_error({});
                    }
                    if (v_tail(v_tail(c)).type == T_VOID) {
                        err(c.pos, "No body expression provided in case.");
                        return v_error({});
                    }
                    Value result = match_case(env, v_head(v_tail(c)), v_at(args, 0));
                    if (result.type == T_ERROR) return v_error({});
                    else if (result.data.b) {
                        Value body = v_head(v_tail(v_tail(c)));
                        return eval(env, body); // eval body
                    }
                }
                err(v_at(args, 0).pos, "Provided value '", v_at(args, 0), "' did not match any case.");
                return v_error({});
            },
            nullptr
        };
        ADD_INT = {
            t_func(t_tuple(T_INT, T_INT), T_INT), // type
            f_callable(PREC_ADD, ASSOC_LEFT, p_var("lhs"), P_SELF, p_var("rhs")), // form
            [](rc<Env> env, const Value& args) -> Value {
                return v_int({}, v_tuple_at(args, 0).data.i + v_tuple_at(args, 1).data.i);
            },
            nullptr
        };
        ADD_FLOAT = {
            t_func(t_tuple(T_FLOAT, T_FLOAT), T_FLOAT), // type
            f_callable(PREC_ADD, ASSOC_LEFT, p_var("lhs"), P_SELF, p_var("rhs")), // form
            [](rc<Env> env, const Value& args) -> Value {
                return v_float({}, v_tuple_at(args, 0).data.f32 + v_tuple_at(args, 1).data.f32);
            },
            nullptr
        };
        ADD_DOUBLE = {
            t_func(t_tuple(T_DOUBLE, T_DOUBLE), T_DOUBLE), // type
            f_callable(PREC_ADD, ASSOC_LEFT, p_var("lhs"), P_SELF, p_var("rhs")), // form
            [](rc<Env> env, const Value& args) -> Value {
                return v_double({}, v_tuple_at(args, 0).data.f64 + v_tuple_at(args, 1).data.f64);
            },
            nullptr
        };
        SUB = {
            t_func(t_tuple(T_INT, T_INT), T_INT), // type
            f_callable(PREC_ADD, ASSOC_LEFT, p_var("lhs"), P_SELF, p_var("rhs")), // form
            [](rc<Env> env, const Value& args) -> Value {
                return v_int({}, v_tuple_at(args, 0).data.i - v_tuple_at(args, 1).data.i);
            },
            nullptr
        };
        MUL = {
            t_func(t_tuple(T_INT, T_INT), T_INT), // type
            f_callable(PREC_MUL, ASSOC_LEFT, p_var("lhs"), P_SELF, p_var("rhs")), // form
            [](rc<Env> env, const Value& args) -> Value {
                return v_int({}, v_tuple_at(args, 0).data.i * v_tuple_at(args, 1).data.i);
            },
            nullptr
        };
        DIV = {
            t_func(t_tuple(T_INT, T_INT), T_INT), // type
            f_callable(PREC_MUL, ASSOC_LEFT, p_var("lhs"), P_SELF, p_var("rhs")), // form
            [](rc<Env> env, const Value& args) -> Value {
                return v_int({}, v_tuple_at(args, 0).data.i / v_tuple_at(args, 1).data.i);
            },
            nullptr
        };
        MOD = {
            t_func(t_tuple(T_INT, T_INT), T_INT), // type
            f_callable(PREC_MUL, ASSOC_LEFT, p_var("lhs"), P_SELF, p_var("rhs")), // form
            [](rc<Env> env, const Value& args) -> Value {
                return v_int({}, v_tuple_at(args, 0).data.i % v_tuple_at(args, 1).data.i);
            },
            nullptr
        };
        INCR = {
            t_func(T_INT, T_INT), // type
            f_callable(PREC_PREFIX, ASSOC_RIGHT, P_SELF, p_var("operand")), // form
            [](rc<Env> env, const Value& args) -> Value {
                return v_int({}, args.data.i + 1);
            },
            nullptr
        };
        DECR = {
            t_func(T_INT, T_INT), // type
            f_callable(PREC_PREFIX, ASSOC_RIGHT, P_SELF, p_var("operand")), // form
            [](rc<Env> env, const Value& args) -> Value {
                return v_int({}, args.data.i - 1);
            },
            nullptr
        };
        LESS = {
            t_func(t_tuple(T_INT, T_INT), T_BOOL),
            f_callable(PREC_COMPARE, ASSOC_LEFT, p_var("lhs"), P_SELF, p_var("rhs")), // form
            [](rc<Env> env, const Value& args) -> Value {
                return v_bool({}, v_tuple_at(args, 0).data.i < v_tuple_at(args, 1).data.i);
            },
            nullptr
        };
        LESS_EQUAL = {
            t_func(t_tuple(T_INT, T_INT), T_BOOL),
            f_callable(PREC_COMPARE, ASSOC_LEFT, p_var("lhs"), P_SELF, p_var("rhs")), // form
            [](rc<Env> env, const Value& args) -> Value {
                return v_bool({}, v_tuple_at(args, 0).data.i <= v_tuple_at(args, 1).data.i);
            },
            nullptr
        };
        GREATER = {
            t_func(t_tuple(T_INT, T_INT), T_BOOL),
            f_callable(PREC_COMPARE, ASSOC_LEFT, p_var("lhs"), P_SELF, p_var("rhs")), // form
            [](rc<Env> env, const Value& args) -> Value {
                return v_bool({}, v_tuple_at(args, 0).data.i > v_tuple_at(args, 1).data.i);
            },
            nullptr
        };
        GREATER_EQUAL = {
            t_func(t_tuple(T_INT, T_INT), T_BOOL),
            f_callable(PREC_COMPARE, ASSOC_LEFT, p_var("lhs"), P_SELF, p_var("rhs")), // form
            [](rc<Env> env, const Value& args) -> Value {
                return v_bool({}, v_tuple_at(args, 0).data.i >= v_tuple_at(args, 1).data.i);
            },
            nullptr
        };
        EQUAL = {
            t_func(t_tuple(T_ANY, T_ANY), T_BOOL), // type
            f_callable(PREC_COMPARE, ASSOC_LEFT, p_var("lhs"), P_SELF, p_var("rhs")), // form
            [](rc<Env> env, const Value& args) -> Value {
                return v_bool({}, v_tuple_at(args, 0) == v_tuple_at(args, 1));
            },
            nullptr
        };
        NOT_EQUAL = {
            t_func(t_tuple(T_ANY, T_ANY), T_BOOL), // type
            f_callable(PREC_COMPARE, ASSOC_LEFT, p_var("lhs"), P_SELF, p_var("rhs")), // form
            [](rc<Env> env, const Value& args) -> Value {
                return v_bool({}, v_tuple_at(args, 0) != v_tuple_at(args, 1));
            },
            nullptr
        };
        AND = {
            t_func(t_tuple(T_BOOL, T_ANY), T_BOOL), // type
            f_callable(PREC_LOGIC, ASSOC_LEFT, p_var("lhs"), P_SELF, p_quoted("rhs")), // form
            [](rc<Env> env, const Value& args) -> Value {
                if (!v_at(args, 0).data.b) return v_at(args, 0); // short circuit false
                else {
                    Value rhs = v_at(args, 1);
                    return eval(env, rhs);
                }
            },
            nullptr
        };
        XOR = {
            t_func(t_tuple(T_BOOL, T_BOOL), T_BOOL), // type
            f_callable(PREC_LOGIC - 33, ASSOC_LEFT, p_var("lhs"), P_SELF, p_var("rhs")), // form
            [](rc<Env> env, const Value& args) -> Value {
                return v_bool({}, v_at(args, 0).data.b ^ v_at(args, 1).data.b);
            },
            nullptr
        };
        OR = {
            t_func(t_tuple(T_BOOL, T_ANY), T_BOOL), // type
            f_callable(PREC_LOGIC - 66, ASSOC_LEFT, p_var("lhs"), P_SELF, p_quoted("rhs")), // form
            [](rc<Env> env, const Value& args) -> Value {
                if (v_at(args, 0).data.b) return v_at(args, 0); // short circuit true
                else {
                    Value rhs = v_at(args, 1);
                    return eval(env, rhs);
                }
            },
            nullptr
        };
        NOT = {
            t_func(T_BOOL, T_BOOL), // type
            f_callable(PREC_PREFIX, ASSOC_RIGHT, P_SELF, p_var("operand")), // form
            [](rc<Env> env, const Value& arg) -> Value {
                return v_bool({}, !arg.data.b);
            },
            nullptr
        };
        HEAD = {
            t_func(t_list(T_ANY), T_ANY), // type
            f_callable(PREC_PREFIX, ASSOC_LEFT, p_var("list"), P_SELF), // form
            [](rc<Env> env, const Value& arg) -> Value {
                return v_head(arg);
            },
            nullptr
        };
        TAIL = {
            t_func(t_list(T_ANY), t_list(T_ANY)), // type
            f_callable(PREC_PREFIX, ASSOC_LEFT, p_var("list"), P_SELF), // form
            [](rc<Env> env, const Value& arg) -> Value {
                return v_tail(arg);
            },
            nullptr
        };
        CONS = {
            t_func(t_tuple(T_ANY, t_list(T_ANY)), T_ANY), // type
            f_callable(PREC_DEFAULT - 50, ASSOC_RIGHT, p_var("head"), P_SELF, p_var("tail")), // form
            [](rc<Env> env, const Value& args) -> Value {
                Type ht = v_at(args, 0).type, tt = v_at(args, 1).type;
                Type lt = (tt.of(K_LIST) && ht.coerces_to(t_list_element(tt))) ? tt : t_list(ht);
                return v_cons({}, lt, 
                    coerce(v_at(args, 0), t_list_element(lt)), v_at(args, 1));
            },
            nullptr
        };
        LENGTH = {
            t_func(T_STRING, T_INT),
            f_callable(PREC_DEFAULT, ASSOC_LEFT, p_var("str"), P_SELF),
            [](rc<Env> env, const Value& arg) -> Value {
                return v_int({}, arg.data.string.raw()->data.size());
            },
            nullptr
        };
        FIND = {
            t_func(t_tuple(T_CHAR, T_STRING), T_INT),
            f_callable(PREC_DEFAULT, ASSOC_LEFT, P_SELF, p_var("char"), p_var("str")),
            [](rc<Env> env, const Value& args) -> Value {
                auto start = v_tuple_at(args, 1).data.string->data.begin(),
                    end = v_tuple_at(args, 1).data.string->data.end();
                rune ch = v_tuple_at(args, 0).data.ch;
                u32 idx = 0;
                while (*start != ch && start != end) ++ start, ++ idx;
                return v_int({}, start == end ? -1 : idx);
            },
            nullptr
        };
        LIST = {
            t_func(t_list(T_ANY), T_ANY),
            f_callable(PREC_DEFAULT, ASSOC_RIGHT, P_SELF, p_variadic("items")),
            [](rc<Env> env, const Value& args) -> Value {
                return args;
            },
            nullptr
        };
        ARRAY = {
            t_func(t_list(T_ANY), T_ANY),
            f_callable(PREC_DEFAULT, ASSOC_RIGHT, P_SELF, p_variadic("items")),
            [](rc<Env> env, const Value& args) -> Value {
                vector<Value> elements;
                for (const Value& v : iter_list(args)) elements.push(v);
                return v_array({}, infer_array(elements), elements);
            },
            nullptr
        };
        TUPLE = {
            t_func(t_tuple(T_ANY, t_list(T_ANY)), T_ANY),
            f_callable(PREC_STRUCTURE, ASSOC_LEFT, p_var("first"), P_SELF, p_quoted_variadic("rest")),
            [](rc<Env> env, const Value& args) -> Value {
                vector<Value> elements;
                elements.push(v_at(args, 0)); // lhs is always an element
                const Value& rest = v_at(args, 1);
                Value last = rest;
                bool expects_comma = false;
                for (Value v : iter_list(rest)) {
                    last = v;
                    if (expects_comma && (v.type != T_SYMBOL || v.data.sym != S_COMMA)) {
                        err(v.pos, "Expected ',' symbol in tuple constructor; found '", v, "' instead.");
                        return v_error({});
                    }
                    else if (!expects_comma && v.type == T_SYMBOL && v.data.sym == S_COMMA) {
                        err(v.pos, "Found unexpected comma in tuple constructor - was expecting a tuple ",
                            "element in this position.");
                        return v_error({});
                    }
                    else if (!expects_comma) {
                        auto val = eval(env, v);
                        if (val.type == T_ERROR) return val; // propagate errors
                        elements.push(val);
                    }
                    expects_comma = !expects_comma; // alternate between commas and elements
                }
                if (!expects_comma) {
                    err(last.pos, "Unexpected trailing '", last, "' at the end of tuple constructor.");
                    return v_error({});
                }
                return v_tuple({}, infer_tuple(elements), elements);
            },
            nullptr
        };
        UNION_TYPE = {
            t_func(t_tuple(T_TYPE, t_list(T_ANY)), T_TYPE),
            f_callable(PREC_TYPE - 20, ASSOC_LEFT, p_var("first"), P_SELF, p_quoted_variadic("rest")),
            [](rc<Env> env, const Value& args) -> Value {
                set<Type> elements;
                elements.insert(v_at(args, 0).data.type); // lhs is always an element
                const Value& rest = v_at(args, 1);
                Value last = rest;
                bool expects_pipe = false;
                for (Value v : iter_list(rest)) {
                    last = v;
                    if (expects_pipe && (v.type != T_SYMBOL || v.data.sym != S_PIPE)) {
                        err(v.pos, "Expected '|' symbol in union type; found '", v, "' instead.");
                        return v_error({});
                    }
                    else if (!expects_pipe && v.type == T_SYMBOL && v.data.sym == S_PIPE) {
                        err(v.pos, "Found unexpected '|' symbol in union type constructor - was expecting a type ",
                            "value in this position.");
                        return v_error({});
                    }
                    else if (!expects_pipe) {
                        auto val = eval(env, v);
                        if (val.type == T_ERROR) return val; // propagate errors
                        if (!val.type.coerces_to(T_TYPE)) {
                            err(v.pos, "Expected type value in union type constructor, but found unexpected ",
                                "value '", val, "' of incompatible type '", val.type, "'.");
                            return v_error({});
                        }
                        Value coerced = coerce(val, T_TYPE);
                        elements.insert(coerced.data.type);
                    }
                    expects_pipe = !expects_pipe; // alternate between commas and elements
                }
                if (!expects_pipe) {
                    err(last.pos, "Unexpected trailing '", last, "' at the end of union type constructor.");
                    return v_error({});
                }
                if (elements.size() < 2) {
                    err(args.pos, "Union types must have at least two distinct members, but this one ",
                        "only contains the type '", *elements.begin(), "'.");
                    return v_error({});
                }
                return v_type({}, t_union(elements));
            },
            nullptr
        };
        NAMED_TYPE = {
            t_func(t_tuple(T_SYMBOL, T_ANY), T_ANY),
            f_callable(PREC_TYPE, ASSOC_RIGHT, p_quoted("name"), P_SELF, p_var("base")),
            [](rc<Env> env, const Value& args) -> Value {
                Symbol name = v_at(args, 0).data.sym;
                const Value& base = v_at(args, 1);
                return v_named({}, t_named(name, base.type), base);
            },
            nullptr
        };
        FN_TYPE = {
            t_func(t_tuple(T_TYPE, T_TYPE), T_TYPE),
            f_callable(PREC_TYPE - 40, ASSOC_LEFT, p_var("arg"), P_SELF, p_var("ret")),
            [](rc<Env> env, const Value& args) -> Value {
                Type arg = v_at(args, 0).data.type, ret = v_at(args, 1).data.type;
                return v_type({}, t_func(arg, ret));
            },
            nullptr
        };
        TYPE_VAR = {
            t_func(T_SYMBOL, T_TYPE),
            f_callable(PREC_ANNOTATED + 20, ASSOC_LEFT, p_quoted("name"), P_SELF),
            [](rc<Env> env, const Value& arg) -> Value {
                Value tvar = v_type({}, t_var(arg.data.sym));
                env->def(arg.data.sym, tvar);
                return tvar;
            },
            nullptr
        };
        JUST = {
            t_func(T_SYMBOL, T_TYPE),
            f_callable(PREC_PREFIX, ASSOC_RIGHT, P_SELF, p_quoted("name")),
            [](rc<Env> env, const Value& arg) -> Value {
                return v_type({}, t_named(arg.data.sym));
            },
            nullptr
        };
        TYPEOF = {
            t_func(T_ANY, T_TYPE),
            f_callable(PREC_COMPARE + 1, ASSOC_RIGHT, P_SELF, p_var("value")),
            [](rc<Env> env, const Value& arg) -> Value {
                Type t = arg.type;
                if (t.is_tvar()) t = t_tvar_concrete(t);
                return v_type({}, t);
            },
            nullptr
        };
        IS = {
            t_func(t_tuple(T_ANY, T_TYPE), T_BOOL),
            f_callable(PREC_COMPARE, ASSOC_LEFT, p_var("value"), P_SELF, p_var("type")),
            [](rc<Env> env, const Value& args) -> Value {
                Type arg = v_at(args, 0).type, compare = v_at(args, 1).data.type;
                if (arg == compare) return v_bool({}, true);
                else if (arg.of(K_UNION) && v_at(args, 0).data.u->value.type == compare)
                    return v_bool({}, true);
                return v_bool({}, false);
            },
            nullptr
        };
        ANNOTATE = {
            t_func(t_tuple(T_ANY, T_TYPE), T_ANY),
            f_callable(PREC_ANNOTATED, ASSOC_LEFT, p_var("value"), P_SELF, p_var("type")),
            [](rc<Env> env, const Value& args) -> Value {
                if (v_at(args, 0).type.coerces_to(v_at(args, 1).data.type)) {
                    return coerce(v_at(args, 0), v_at(args, 1).data.type);
                }
                else {
                    err(v_at(args, 0).pos, "Invalid type annotation: could not coerce value '", 
                        v_at(args, 0), "' to type '", v_at(args, 1), "'.");
                    return v_error({});
                }
            },
            nullptr
        };
        DEBUG = {
            t_func(T_ANY, T_VOID),
            f_callable(PREC_ANNOTATED, ASSOC_RIGHT, P_SELF, p_var("value")),
            [](rc<Env> env, const Value& arg) -> Value {
                println("DEBUG: ", arg, " # ", arg.form, " prec=", arg.form->precedence, " assoc=", 
                    arg.form->assoc == ASSOC_LEFT ? "left" : "right", " : ", arg.type);
                return v_void({});
            },
            nullptr
        };
    }

    void add_builtins(rc<Env> env) {
        if (!builtins_inited) builtins_inited = true, init_builtins();

        env->def(symbol_from("def"), v_intersect(&DEF, &DEF_TYPED));
        env->def(symbol_from(":="), v_func(INFIX_DEF));
        env->def(symbol_from("lambda"), v_func(LAMBDA));
        env->def(symbol_from("λ"), v_func(LAMBDA));
        env->def(symbol_from("do"), v_func(DO));
        env->def(symbol_from("eval"), v_func(EVAL));
        env->def(symbol_from("quote"), v_func(QUOTE));
        env->def(symbol_from("if"), v_intersect(&IF, &IF_ELSE));
        env->def(symbol_from("while"), v_func(WHILE));
        env->def(symbol_from("matches"), v_func(MATCHES));
        env->def(symbol_from("=>"), v_func(ARROW));
        env->def(symbol_from("match"), v_func(MATCH));
        env->def(symbol_from("import"), v_func(IMPORT));
        env->def(symbol_from("module"), v_func(MODULE));
        env->def(symbol_from("use"), v_func(USE));

        env->def(symbol_from("+"), v_intersect(&ADD_INT, &ADD_FLOAT, &ADD_DOUBLE));
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
        env->def(symbol_from("|"), v_func(UNION_TYPE));
        env->def(symbol_from("of"), v_func(NAMED_TYPE));
        env->def(symbol_from("just"), v_func(JUST));
        env->def(symbol_from("->"), v_func(FN_TYPE));
        env->def(symbol_from("?"), v_func(TYPE_VAR));
        env->def(symbol_from("typeof"), v_func(TYPEOF));
        env->def(symbol_from("is"), v_func(IS));
        env->def(symbol_from(":"), v_func(ANNOTATE));
        env->def(symbol_from("debug"), v_func(DEBUG));
        env->def(symbol_from("at"), v_func(AT_MODULE));
        env->def(symbol_from("."), v_func(DOT_MODULE));

        // type primitives
        env->def(symbol_from("Int"), v_type({}, T_INT));
        env->def(symbol_from("Float"), v_type({}, T_FLOAT));
        env->def(symbol_from("Double"), v_type({}, T_DOUBLE));
        env->def(symbol_from("Char"), v_type({}, T_CHAR));
        env->def(symbol_from("String"), v_type({}, T_STRING));
        env->def(symbol_from("Bool"), v_type({}, T_BOOL));
        env->def(symbol_from("Type"), v_type({}, T_TYPE));
        env->def(symbol_from("Void"), v_type({}, T_VOID));
        env->def(symbol_from("Symbol"), v_type({}, T_SYMBOL));
        env->def(symbol_from("Any"), v_type({}, T_ANY));

        // constants
        env->def(symbol_from("true"), v_bool({}, true));
        env->def(symbol_from("false"), v_bool({}, false));
        env->def(symbol_from("π"), v_double({}, 3.141592653589793238462643383279502884197169399375105820974944592307));
    }
}