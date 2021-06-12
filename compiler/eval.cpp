#include "builtin.h"
#include "eval.h"

namespace basil {
    // Infers a form from the provided type.
    rc<Form> infer_form(Symbol name, Type type) {
        switch (type.kind()) {
            case K_FUNCTION: {
                vector<Param> params;
                params.push(p_keyword(name));
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
                // params.push(*it); <-- don't actually add keywords to the parameter list
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
                        term.form = infer_form(found->data.sym, found->type);
                    }
                }
                else term.form = F_TERM; // default to 'term'
                return env;
            }
            case K_LIST: { // the spooky one...
                if (!v_head(term).form) env = group(env, term); // group all terms within the list first
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

    // We use this structure to wrap the root environment so that it can be
    // freed at the end of the program.
    struct Root {
        void unbind(rc<Env> env) {
            for (rc<Env>& child : env->children) if (child) {
                unbind(child);
                child = nullptr;
            }
        }

        ~Root() {
            if (root) unbind(root);
        }

        static rc<Env> root;
    };

    rc<Env> Root::root;

    rc<Env> root_env() {
        if (Root::root) return Root::root; // return existing root if possible

        Root::root = ref<Env>(); // allocate empty, parentless environment
        add_builtins(Root::root);
        return Root::root;
    }

    EvalResult call(rc<Env> env, Value func, const Value& args) {
        // TODO: prune overloads with wrong forms

        if (func.type.of(K_INTERSECT)) {
            // TODO: overload resolution by type
        }
        else if (func.type.of(K_FUNCTION)) {
            if (!args.type.coerces_to(t_arg(func.type)))
                err(args.pos, "Incompatible arguments for function! Expected '",
                    t_arg(func.type), "', got '", args.type, "'.");
        }
        else panic("Tried to call non-callable value!");

        // at this point we can assume 'func' is a function value, and args are compatible with it.
        rc<Function> fndata = func.data.fn;
        if (fndata->builtin) {
            return fndata->builtin->comptime(env, args); // for now, assume compile time
        }
        else {
            // TODO: user-defined function application
            return { env, v_error({}) };
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
                    err(term.pos, "Undefined variable '", var, "'.");
                    return {env, v_error(term.pos)};
                }
                return {env, *var};
            }
            case K_LIST: { // non-empty lists eval to the results of applying functions
                if (!term.form) env = resolve_form(env, term); // resolve form if necessary

                // TODO: expand splices
                // TODO: expand macros

                auto head_result = eval(env, v_head(term)); // evaluate first term
                env = head_result.env;
                Value head = head_result.value;

                if (head.type.of(K_FUNCTION)) {
                    vector<Value> args;
                    for (Value& arg : iter_list(v_tail(term))) { // evaluate each argument
                        auto arg_result = eval(env, arg);
                        env = arg_result.env;
                        args.push(arg_result.value);
                    }
                    if (args.size() == 0) return {env, head}; // return the function if no args, e.g. (+)
                    else {
                        Value args_value = args.size() == 1 ? args[0]
                            : v_tuple(span(args.front().pos, args.back().pos), infer_tuple(args), args);
                        EvalResult result = call(env, head, args_value);
                        result.value.pos = term.pos; // prefer term pos over any other pos determined within call()
                        return result;
                    }
                }
                else {
                    err(term.pos, "Could not evaluate list '", term, "'.");
                }
            }
            default:
                err(term.pos, "Could not evaluate term '", term, "'.");
                return {env, v_error(term.pos)};
        }
    }
}
