#include "builtin.h"
#include "eval.h"
#include "driver.h"

namespace basil {
    // Infers a form from the provided type.
    rc<Form> infer_form(Type type) {
        switch (type.kind()) {
            case K_FUNCTION: {
                vector<Param> params;
                params.push(P_SELF);
                for (u32 i = 0; i < t_arity(type); i ++) params.push(P_VAR);
                return f_callable(0, ASSOC_RIGHT, params);
            }
            default:
                return F_TERM;
        }
    }

    optional<GroupResult> try_group(rc<Env> env, vector<Value>&& params, rc<StateMachine> sm,
        list_iterator it, list_iterator end,
        Associativity outerassoc, i64 outerprec) {
        i64 n = -1; // tracks number of params in best match so far, or -1 if no match has been found
        optional<const Callable&> best_match = none<const Callable&>();
        list_iterator best = it;

        if (auto match = sm->match()) { // check trivial match (no additional args needed)
            n = params.size();
            best_match = match;
            // 'best' already equals 'it', so it's already pointing in the right place
        }

        while (!sm->is_finished() && it != end) {
            if (sm->precheck_keyword(*it)) { // keyword matches
                params.push(*it);
                sm->advance(*it ++);
            }
            else if (sm->precheck_term(*it)) { // matches that avoid grouping
                params.push(*it);
                sm->advance(*it ++); 
            }
            else { // other matches
                GroupResult gr = next_group(env, it, end, outerassoc, outerprec);
                env = gr.env;
                it = gr.next;
                params.push(gr.value);
                sm->advance(gr.value);
            }
            if (auto match = sm->match()) {
                best = it, best_match = match, n = params.size();
            }
        }
        if (best_match) {
            while (params.size() > n) params.pop(); // remove any extra arguments we added while exploring

            if (params[0].form->kind == FK_OVERLOADED) {
                rc<Overloaded> overloaded = (rc<Overloaded>)params[0].form->invokable;
                rc<Callable> selected = nullptr;
                for (rc<Callable> callable : overloaded->overloads) 
                    if (callable.raw() == &*best_match) selected = callable; // find the refcell of the best match
                params[0].form->kind = FK_CALLABLE;
                params[0].form->invokable = selected; // use best match instead of overload
            }

            Source::Pos resultpos = span(params.front().pos, params.back().pos); // params can never be empty, so this is safe
            if (params.size() >= 2) resultpos = span(params[1].pos, resultpos); // this handles the infix left hand operand
            Value result = v_list(resultpos, t_list(T_ANY), params);
            rc<Env> new_env = resolve_form(env, result);
            return some<GroupResult>(GroupResult{
                new_env,
                result,
                best
            });
        }
        return none<GroupResult>(); // no best match found
    }

    // Pulls the next expression from the iterator range.
    // Panics if called on an empty iterator range!
    // Resulting value, if present, has a resolved form.
    GroupResult next_group(rc<Env> env, 
        list_iterator it, list_iterator end, 
        Associativity outerassoc, i64 outerprec) {
        
        if (it == end) panic("Tried to pull group from empty iterator range!");
        
        Value term = *it;
        env = resolve_form(env, term);
        if (term.form->has_prefix_case()) { // try prefix application regardless of minprecedence
            vector<Value> params;
            params.push(term); // include term as a param

            rc<StateMachine> sm = term.form->start();
            sm->advance(term); // move past first term
            list_iterator it_copy = it; // we don't want to actually move 'it'
            ++ it_copy;
            optional<GroupResult> gr = try_group(env, static_cast<vector<Value>&&>(params), sm, it_copy, end, term.form->assoc, term.form->precedence);

            if (gr) {
                env = gr->env;
                env = resolve_form(env, gr->value);
                term = gr->value;
                it = gr->next; 
            }
            else ++ it; // advance just past this term
        }
        else ++ it; // move past this single term

        while (it != end) { // loop until we fail to find a suitable infix operator
            Value op = *it;
            env = resolve_form(env, op);
            if (op.form->has_infix_case() && 
                (op.form->precedence > outerprec 
                 || (outerassoc == ASSOC_RIGHT && op.form->precedence == outerprec))) { // try infix application

                vector<Value> params;
                params.push(op); // add op and term to params
                params.push(term);

                rc<StateMachine> sm = op.form->start();
                sm->advance(term); // move past lhs
                sm->advance(op); // move past op
                list_iterator it_copy = it; // we don't want to actually move 'it'
                ++ it_copy;
                optional<GroupResult> gr = 
                    try_group(env, static_cast<vector<Value>&&>(params), sm, it_copy, end, op.form->assoc, op.form->precedence);

                if (gr) {
                    env = gr->env;
                    env = resolve_form(env, gr->value);
                    term = gr->value;
                    it = gr->next;
                }
                else break; // next term must not have been in a suitable spot, so we exit
            }
            else break; // next term is not a suitable infix operator, or had lower precedence
        }
        return GroupResult{
            env,
            term,
            it
        };
    }

    rc<Env> group(rc<Env> env, Value& term) {
        vector<Value> results; // all the values in this group
        results.clear();

        list_iterable iterable = iter_list(term);
        list_iterator begin = iterable.begin(), end = iterable.end();
        while (begin != end) {
            GroupResult gr = next_group(env, begin, end, ASSOC_RIGHT, I64_MIN); // assume lowest precedence at first
            
            env = gr.env;
            resolve_form(env, gr.value); // resolve new group's form
            results.push(gr.value);
            begin = gr.next;
        }

        if (results.size() == 0) panic("Somehow found no groups within list!");
        else if (results.size() == 1) term = results.front();
        else {
            Source::Pos termpos = span(results.front().pos, results.back().pos);
            term = v_list(termpos, t_list(T_ANY), results);
        }
        return env;
    }

    rc<Env> resolve_form(rc<Env> env, Value& term) {
        if (term.form) return env; // don't re-resolve forms
        switch (term.type.kind()) {
            case K_INT:
            case K_FLOAT:
            case K_DOUBLE:
            case K_CHAR:
            case K_STRING:
            case K_VOID:
                term.form = F_TERM;
                return env;
            case K_SYMBOL: {
                auto found = env->find(term.data.sym); // try and look up the variable's form
                if (found) {
                    if (found->form) { // if we found it, return it
                        term.form = found->form;
                    }
                    else { // otherwise, try to guess the form from the type
                        term.form = infer_form(found->type);
                    }
                }
                else term.form = F_TERM; // default to 'term'
                return env;
            }
            case K_LIST: { // the spooky one...
                if (!v_head(term).form) env = group(env, term); // group all terms within the list first

                if (!term.type.of(K_LIST)) // make sure that it's still some kind of list
                    term = v_cons(term.pos, t_list(T_ANY), term, v_void(term.pos)).with(term.form);

                if (v_head(term).form->kind == FK_CALLABLE) {
                    auto callback = ((const rc<Callable>)v_head(term).form->invokable)->callback;
                    if (callback) {
                        term.form = (*callback)(env, term); // apply callback and finish resolving
                        return env;
                    }
                    else term.form = F_TERM; // default to F_TERM if the callable has no special callback
                }
                else term.form = F_TERM; // default to F_TERM if the first element is not callable
                return env;
            }
            case K_ERROR: {
                term.form = F_TERM; // default to F_TERM if an error already occurred
                return env;
            }
            default:
                panic("Unknown term in form evaluation!");
                return env;  
        }
    }

    void unbind(rc<Env> env) {
        for (rc<Env>& child : env->children) if (child) {
            unbind(child);
            child->parent = nullptr;
        }
        env->children.clear();
    }
    
    rc<Env> root;

    void free_root_env() {
        if (root) unbind(root);
        root = nullptr;
    }

    rc<Env> root_env() {
        if (root) return root; // return existing root if possible

        root = ref<Env>(); // allocate empty, parentless environment
        add_builtins(root);
        return root;
    }

    EvalResult call(rc<Env> env, Value func, const Value& args) {
        // TODO: prune overloads with wrong forms

        if (func.type.of(K_INTERSECT)) {
            // TODO: overload resolution by type
        }
        else if (func.type.of(K_FUNCTION)) {
            if (!args.type.coerces_to(t_arg(func.type))) {
                err(args.pos, "Incompatible arguments for function! Expected '",
                    t_arg(func.type), "', got '", args.type, "'.");
                return { env, v_error({}) };
            }
        }
        else panic("Tried to call non-callable value!");

        // at this point we can assume 'func' is a function value, and args are compatible with it.
        rc<Function> fndata = func.data.fn;
        if (fndata->builtin) {
            return fndata->builtin->comptime(env, args); // for now, assume compile time
        }
        else {
            static int n = 0;
            n ++;
            rc<Env> fn_env = func.data.fn->env;
            const vector<Symbol>& fn_args = func.data.fn->args;
            vector<optional<Value>> existing;
            if (fn_args.size() == 1) { // bind args to single argument
                if (auto found = fn_env->find(fn_args[0])) existing.push(some<Value>(*found)); // save previous arg value
                else existing.push(none<Value>());

                fn_env->def(fn_args[0], args);
            }
            else for (u32 i = 0; i < v_len(args); i ++) { // bind args to multiple arguments
                if (auto found = fn_env->find(fn_args[i])) existing.push(some<Value>(*found)); // save previous arg value
                else existing.push(none<Value>());

                fn_env->def(fn_args[i], v_at(args, i));
            }
            rc<Value> fn_body = v_resolve_body(func, args); // copy function body
            EvalResult result = eval(fn_env, *fn_body); // eval it

            for (u32 i = 0; i < existing.size(); i ++) 
                if (existing[i]) *fn_env->find(fn_args[i]) = *existing[i]; // restore any previous values
            
            return result;
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
                if (!var
                    || var->type == T_UNDEFINED) {
                    // undefined is a placeholder for values that exist at form
                    // resolution but are not actually defined during evaluation
                    err(term.pos, "Undefined variable '", term.data.sym, "'.");
                    return {env, v_error(term.pos)};
                }
                return {env, *var};
            }
            case K_LIST: { // non-empty lists eval to the results of applying functions
                if (!term.form) env = resolve_form(env, term);
                // TODO: expand splices
                // TODO: expand macros

                auto head_result = eval(env, v_head(term)); // evaluate first term
                env = head_result.env;
                Value head = head_result.value;

                if (v_tail(term).type == T_VOID) 
                    return {env, head.with(infer_form(head.type))}; // return the function if no args, e.g. (+)

                if (head.type.of(K_FUNCTION)) {
                    vector<Value> args;
                    vector<Value> varargs; // we use this to accumulate arguments for an individual variadic parameter
                    rc<Callable> form = (rc<Callable>)head.form->start();

                    // we basically re-run the form over the argument list here to figure out where to evaluate
                    for (Value& arg : iter_list(v_tail(term))) { // evaluate each argument
                        while (form->current_param() && form->current_param()->kind == PK_SELF) // skip self parameter
                            form->advance(v_void({}));
                        
                        form->precheck_keyword(arg); // leave variadics for keyword if possible

                        // if we were building a variadic list, and aren't on a variadic anymore,
                        // we clear the varargs and push them as a list onto the arg array
                        if (form->current_param()->kind != PK_VARIADIC 
                            && form->current_param()->kind != PK_QUOTED_VARIADIC
                            && varargs.size() > 0) {
                            args.push(v_list(
                                span(varargs.front().pos, varargs.back().pos),
                                infer_list(varargs),
                                varargs
                            ));
                            varargs.clear();
                        }
                        if (form->current_param() &&  // skip evaluation
                            (form->current_param()->kind == PK_TERM 
                             || form->current_param()->kind == PK_QUOTED 
                             || form->current_param()->kind == PK_QUOTED_VARIADIC)) {
                                 
                            if (form->current_param()->kind == PK_QUOTED_VARIADIC) varargs.push(arg);
                            else args.push(arg);
                        }
                        else if (form->current_param()->kind != PK_KEYWORD) { // evaluate but ignore keywords
                            auto arg_result = eval(env, arg);
                            env = arg_result.env;
                            if (arg_result.value.type == T_ERROR) return { env, v_error({}) }; // propagate errors

                            if (form->current_param()->kind == PK_VARIADIC) varargs.push(arg_result.value);
                            else args.push(arg_result.value);
                        }
                        form->advance(arg);
                    }
                    if (varargs.size() > 0) { // push any remaining varargs as list onto args
                        args.push(v_list(
                            span(varargs.front().pos, varargs.back().pos),
                            infer_list(varargs),
                            varargs
                        ));
                    }

                    if (args.size() == 0) {
                        err(term.pos, "Procedure must be called on one or more arguments; zero given.");
                        return { env, v_error({}) };
                    }

                    Value args_value = args.size() == 1 ? args[0]
                        : v_tuple(term.pos, infer_tuple(args), args);

                    EvalResult result = call(env, head, args_value);
                    result.value.form = infer_form(result.value.type);
                    result.value.pos = term.pos; // prefer term pos over any other pos determined within call()
                    return result;
                }
                else {
                    err(term.pos, "Could not evaluate list '", term, "'.");
                }
            }
            case K_ERROR:
                return {env, v_error(term.pos)}; // return an error if an error already occurred
            default:
                err(term.pos, "Could not evaluate term '", term, "'.");
                return {env, v_error(term.pos)};
        }
    }
}
