#include "builtin.h"
#include "eval.h"
#include "driver.h"
#include "forms.h"

namespace basil {
    // Infers a form from the provided type.
    rc<Form> infer_form(Type type) {
        switch (type.kind()) {
            case K_FUNCTION: {
                vector<Param> params;
                params.push(P_SELF);
                for (u32 i = 0; i < t_arity(type); i ++) params.push(p_var(S_NONE)); // anonymous parameters
                return f_callable(0, ASSOC_RIGHT, params);
            }
            case K_INTERSECT: {
                if (t_intersect_procedural(type)) {
                    vector<rc<Form>> forms;
                    for (Type t : t_intersect_members(type)) forms.push(infer_form(t));
                    return *f_overloaded(forms[0]->precedence, forms[0]->assoc, forms);
                }
                else return F_TERM; // non-procedural intersects are not applied
            }
            default:
                return F_TERM;
        }
    }

    either<GroupResult, GroupError> try_group(rc<Env> env, vector<Value>&& params, FormKind fk, rc<StateMachine> sm,
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
                params[0].form = ref<Form>(FK_CALLABLE, params[0].form->precedence, params[0].form->assoc);
                params[0].form->invokable = selected; // use best match instead of overload
            }

            Source::Pos resultpos = span(params.front().pos, params.back().pos); // params can never be empty, so this is safe
            if (params.size() >= 2) resultpos = span(params[1].pos, resultpos); // this handles the infix left hand operand
            Value result = v_list(resultpos, t_list(T_ANY), params);
            resolve_form(env, result);
            return GroupResult{
                result,
                best
            };
        }

        vector<rc<Callable>> mismatches;

        // this should probably be better abstracted...but it works for now

        // find all the individual callable machines
        vector<rc<Callable>> callables;
        if (fk == FK_CALLABLE) callables.push((rc<Callable>)sm);
        else if (fk == FK_OVERLOADED) {
            i64 min_count = -1;
            for (rc<Callable> callable : ((rc<Overloaded>)sm)->overloads) {
                if (callable->advances > min_count) {
                    callables.clear();
                    min_count = callable->advances;
                }
                callables.push(callable);
            }
        }

        return GroupError{
            callables
        }; // no best match found
    }

    // Reports a grouping error.
    void report_group_error(const GroupError& error, const Value& term, const vector<Value>& params) {
        err(term.pos, "Couldn't figure out how to apply procedure '", term, "'.");
        for (rc<Callable> callable : error.candidates) {
            buffer msg;
            write(msg, "Candidate '");
            write_with_self(msg, term, callable);
            write(msg, "' matched ");
            bool first = true;
            for (u32 i = 1; i < callable->advances; i ++) {
                if (!first) write(msg, callable->advances > 3 ? ", " : " "); // skip comma for 2 args
                if (!first && i == callable->advances - 1) write(msg, "and ");
                first = false;
                write(msg, "'", params[i], "'");
            }
            write(msg, ", ");
            Source::Pos notepos;
            if (callable->wrong_value) {
                notepos = callable->wrong_value->pos;
                write(msg, "but found '", *callable->wrong_value, "' instead of '",
                    (*callable->parameters)[callable->index], "'.");
            }
            else {
                write(msg, "but could not find matching term for '", 
                    ITALICWHITE, (*callable->parameters)[callable->index], RESET, "'.");
            }
            note(notepos, ustring(msg));
        }
    }

    // Pulls the next expression from the iterator range.
    // Panics if called on an empty iterator range!
    // Resulting value, if present, has a resolved form.
    GroupResult next_group(rc<Env> env, 
        list_iterator it, list_iterator end, 
        Associativity outerassoc, i64 outerprec) {
        
        if (it == end) panic("Tried to pull group from empty iterator range!");

        Value term = *it;
        resolve_form(env, term);
        if (term.form->has_prefix_case()) { // try prefix application regardless of minprecedence
            vector<Value> params;
            params.push(term); // include term as a param

            rc<StateMachine> sm = term.form->start();
            sm->advance(term); // move past first term
            list_iterator it_copy = it; // we don't want to actually move 'it'
            ++ it_copy;
            auto gr = try_group(env, static_cast<vector<Value>&&>(params), term.form->kind, sm, 
                it_copy, end, term.form->assoc, term.form->precedence);

            if (gr.is_left()) { // if grouping succeeded
                resolve_form(env, gr.left().value);
                term = gr.left().value;
                it = gr.left().next; 
            }
            else if (params.size() == 1) { // if we didn't match any other parameters
                ++ it; // leave the term in place, not applied to anything
            }
            else {
                report_group_error(gr.right(), term, params);
                ++ it; // advance just past this term
            }
        }
        else ++ it; // move past this single term

        while (it != end) { // loop until we fail to find a suitable infix operator
            Value op = *it;
            resolve_form(env, op);
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
                auto gr = try_group(env, static_cast<vector<Value>&&>(params), op.form->kind, sm, 
                    it_copy, end, op.form->assoc, op.form->precedence);

                if (gr.is_left()) { // if grouping succeeded
                    resolve_form(env, gr.left().value);
                    term = gr.left().value;
                    it = gr.left().next;
                }
                else {
                    report_group_error(gr.right(), op, params);
                    ++ it;
                    break; // next term must not have been in a suitable spot, so we exit
                }
            }
            else break; // next term is not a suitable infix operator, or had lower precedence
        }
        return GroupResult{
            term,
            it
        };
    }

    void group(rc<Env> env, Value& term) {
        vector<Value> results; // all the values in this group
        results.clear();

        list_iterable iterable = iter_list(term);
        list_iterator begin = iterable.begin(), end = iterable.end();
        while (begin != end) {
            GroupResult gr = next_group(env, begin, end, ASSOC_RIGHT, I64_MIN); // assume lowest precedence at first
            
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
    }

    rc<Form> to_prefix(rc<Form> src) {
        if (src->kind == FK_CALLABLE) {
            vector<Param> params = *((rc<Callable>)src->invokable)->parameters;
            if (params.size() > 1 && params[1].kind == PK_SELF) {
                params[1] = params[0]; // move first parameter up
                params[0] = P_SELF;
            }
            return f_callable(src->precedence, src->assoc, params);
        }
        else if (src->kind == FK_OVERLOADED) {
            vector<rc<Form>> overloads;
            for (rc<Callable> callable : ((rc<Overloaded>)src->invokable)->overloads) {
                vector<Param> params = *callable->parameters;
                if (params.size() > 1 && params[1].kind == PK_SELF) {
                    params[1] = params[0]; // move first parameter up
                    params[0] = P_SELF;
                }
                overloads.push(f_callable(src->precedence, src->assoc, params));
            }
            return *f_overloaded(src->precedence, src->assoc, overloads);
        }
        else return src;
    }

    void resolve_form(rc<Env> env, Value& term) {
        if (term.form) return; // don't re-resolve forms
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
                return;
            }
            case K_LIST: { // the spooky one...
                if (v_tail(term).type == T_VOID) { // single-element list
                    if (!v_head(term).form) resolve_form(env, v_head(term));
                    term.form = F_TERM; // always treat single-element lists as expressions
                    return;
                }

                if (!v_head(term).form) group(env, term); // group all terms within the list first

                if (!term.type.of(K_LIST)) // make sure that it's still some kind of list
                    term = v_cons(term.pos, t_list(T_ANY), term, v_void(term.pos)).with(term.form);

                if (v_head(term).form->kind == FK_CALLABLE) {
                    auto callback = ((const rc<Callable>)v_head(term).form->invokable)->callback;
                    if (callback) {
                        term.form = (*callback)(env, term); // apply callback and finish resolving
                        return;
                    }
                    else term.form = F_TERM; // default to F_TERM if the callable has no special callback
                }
                else term.form = F_TERM; // default to F_TERM if the first element is not callable
                return;
            }
            case K_ERROR: {
                term.form = F_TERM; // default to F_TERM if an error already occurred
                return;
            }
            default:
                panic("Unknown term in form evaluation!");
                return;  
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

    static int call_count = 0;

    Value call(rc<Env> env, Value func_term, Value func, const Value& args) {
        if (func.type.of(K_INTERSECT)) {
            vector<Value> valid;
            // println("func form = ", func.form);
            for (const auto& [t, v] : func.data.isect->values) {
                // println("  value form = ", v.form);
                if (v.form == func_term.form) valid.push(v);
            }
            if (valid.size() == 0) panic("Somehow found no overloads matching resolved form!");
            func = valid[0];
        }
        else if (func.type.of(K_FUNCTION)) {
            if (!args.type.coerces_to(t_arg(func.type))) {
                err(args.pos, "Incompatible arguments for function '", func_term, "'! Expected '",
                    t_arg(func.type), "', got '", args.type, "'.");
                return v_error({});
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
            vector<pair<Symbol, Value>> existing;
            for (const auto& p : fn_env->values) existing.push(p); // save existing environment 
            if (fn_args.size() == 1) { // bind args to single argument
                fn_env->def(fn_args[0], args);
            }
            else for (u32 i = 0; i < v_len(args); i ++) { // bind args to multiple arguments
                fn_env->def(fn_args[i], v_at(args, i));
            }
            rc<Value> fn_body = v_resolve_body(func, args); // copy function body
            Value result = eval(fn_env, *fn_body); // eval it

            for (u32 i = 0; i < existing.size(); i ++) 
                fn_env->def(existing[i].first, existing[i].second); // restore any previous values
            
            return result; // return enclosing env
        }
    }

    Value eval(rc<Env> env, Value& term) {
        if (!term.form) resolve_form(env, term);
        switch (term.type.kind()) {
            case K_INT:
            case K_FLOAT:
            case K_DOUBLE:
            case K_CHAR:
            case K_STRING:
            case K_VOID:
                return term; // constants eval to themselves
            case K_SYMBOL: { // variables are looked up in the current env
                auto var = env->find(term.data.sym);
                if (!var || var->type == T_UNDEFINED) {
                    // undefined is a placeholder for values that exist at form
                    // resolution but are not actually defined during evaluation
                    err(term.pos, "Undefined variable '", term.data.sym, "'.");
                    return v_error(term.pos);
                }
                return *var;
            }
            case K_LIST: { // non-empty lists eval to the results of applying functions
                if (error_count()) return v_error({}); // return error value if form resolution went awry

                // TODO: expand splices
                // TODO: expand macros

                Value head = eval(env, v_head(term)); // evaluate first term

                if (v_tail(term).type == T_VOID) 
                    return head.with(infer_form(head.type)); // return the function if no args, e.g. (+)

                if (head.type.of(K_FUNCTION) 
                    || (head.type.of(K_INTERSECT) && t_intersect_procedural(head.type))) {
                    vector<Value> args;
                    vector<Value> varargs; // we use this to accumulate arguments for an individual variadic parameter
                    rc<Callable> form = (rc<Callable>)v_head(term).form->start();

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
                            Value arg_value = eval(env, arg);
                            if (arg_value.type == T_ERROR) return v_error({}); // propagate errors

                            arg_value.pos = arg.pos;
                            if (form->current_param()->kind == PK_VARIADIC) varargs.push(arg_value);
                            else args.push(arg_value);
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
                        return v_error({});
                    }

                    Value args_value = args.size() == 1 ? args[0]
                        : v_tuple(term.pos, infer_tuple(args), args);

                    Value result = call(env, v_head(term), head, args_value);
                    if (!result.form) result.form = infer_form(result.type);
                    result.pos = term.pos; // prefer term pos over any other pos determined within call()
                    return result;
                }
                else if (head.type.of(K_ERROR)) {
                    return v_error(term.pos); // propagate error
                }
                else {
                    err(term.pos, "Could not evaluate list '", term, "'.");
                }
            }
            case K_ERROR:
                return v_error(term.pos); // return an error if an error already occurred
            default:
                err(term.pos, "Could not evaluate term '", term, "'.");
                return v_error(term.pos);
        }
    }
}
