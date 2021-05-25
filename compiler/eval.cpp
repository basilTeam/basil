#include "eval.h"

namespace basil {
    // Infers a form from the provided type.
    rc<Form> infer_form(Symbol name, Type type) {
        switch (type.kind()) {
            case K_FUNCTION: {
                vector<Param> params;
                params.push(p_keyword(name));
                for (u32 i = 0; i < t_arity(type); i ++) params.push(P_VAR);
                return f_callable(0, params);
            }
            default:
                return F_TERM;
        }
    }

    void resolve_form(rc<Env> env, Value& term) {
        switch (term.type.kind()) {
            case K_INT:
            case K_FLOAT:
            case K_DOUBLE:
            case K_CHAR:
            case K_STRING:
            case K_VOID:
                term.form = F_TERM;
                return;
            case K_SYMBOL: {
                auto found = env->find(term.data.sym);
                if (found) {
                    if (found->form) term.form = found->form;
                    else term.form = infer_form(found->data.sym, found->type);
                }
                else term.form = F_TERM;
                return;
            }
            case K_LIST:
                
        }
    }

    GroupResult next_term(rc<Env> env, list_iterator it, list_iterator end) {
        const Value& v = *it ++;
        return { env, v, it };
    }

    GroupResult next_group(rc<Env> env, list_iterator it, list_iterator end) {
        Value& v = *it;
        if (!v.form) {
            resolve_form(env, v);
        }
    }

    EvalResult eval(rc<Env> env, Value& term) {
        switch (term.type.kind()) {
            case K_INT:
            case K_FLOAT:
            case K_DOUBLE:
            case K_CHAR:
            case K_STRING:
            case K_VOID:
                return {env, term}; // constants eval to themselves
            case K_SYMBOL: { // variables are looked up in the current env
                auto var = env->find(term.data.sym);
                if (var) return {env, *var};
                else {
                    err(term.pos, "Undefined variable '", var, "'.");
                    return {env, v_error(term.pos)};
                }
            }
            case K_LIST: { // non-empty lists eval to the results of applying functions
                static vector<Value> args;
                args.clear();

                Value proc = v_error(term.pos);
                if (!term.form) { // we know the list is ungrouped if any of its elements have unresolved forms 
                    list_iterable iterable = iter_list(term);
                    list_iterator it = iterable.begin();

                    GroupResult gr = next_group(env, term, it, iterable.end()); // grab first term
                    if (gr.value.type == T_ERROR) return gr.value; // propagate error
                    list_iterator it_peek = it;
                    if (++ it_peek == iterable.end()) { // single group in list
                        term = gr.value;
                        return eval(env, term); // re-evaluate now that we've resolved the form
                    }

                    // otherwise, we still need to work through the list and patch in all the groups
                    *it ++ = gr.value;

                    EvalResult er = eval(env, gr.value); // evaluate first term

                    env = er.env, proc = er.value;
                    while (it != iterable.end()) {
                        GroupResult gr = next_group(env, term, it, iterable.end()); // grab first term
                        *it ++ = gr.value;

                        EvalResult er = eval(env, gr.value); // evaluate arg
                        env = er.env, args.push(er.value);
                    }
                }
                else {
                    list_iterable iterable = iter_list(term);
                    list_iterator it = iterable.begin();
                    
                    EvalResult er = eval(env, *it ++);
                    env = er.env, proc = er.value;
                    while (it != iterable.end()) {
                        EvalResult er = eval(env, *it ++);
                        env = er.env, args.push(er.value);
                    }
                }

                // TODO: expand splices
                // TODO: expand macros
                // TODO: apply head to arguments
            }
            default:
                err(term.pos, "Could not evaluate term '", term, "'.");
                return {env, v_error(term.pos)};
        }
    }
}