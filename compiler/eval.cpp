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

    GroupResult backtrack_group(rc<Env> env, Value term, Value op, list_iterator it, list_iterator end,
        Associativity outerassoc, i64 outerprec);
    void backtrack(rc<Env> env, Value& back, const GroupResult& gr, 
        list_iterator& begin, list_iterator end);
    GroupResult retry_group(rc<Env> env, const GroupResult& outer_group, 
        list_iterator it, list_iterator end);

    either<GroupResult, GroupError> try_group(rc<Env> env, vector<Value>&& params, FormKind fk, rc<StateMachine> sm,
        list_iterator it, list_iterator end,
        Associativity outerassoc, i64 outerprec) {
        i64 n = -1; // tracks number of params in best match so far, or -1 if no match has been found
        optional<rc<Callable>> best_match = none<rc<Callable>>();
        list_iterator best = it;
        bool is_infix = params.size() > 1;

        if (auto match = sm->match()) { // check trivial match (no additional args needed)
            n = params.size();

            // we need all this machinery to get the refcall associated with the match, instead
            // of a non-owning reference.
            if (params[0].form->kind == FK_CALLABLE) best_match = some<rc<Callable>>((rc<Callable>)sm);
            else if (params[0].form->kind == FK_OVERLOADED) {
                rc<Callable> selected = nullptr;
                for (rc<Callable> callable : ((rc<Overloaded>)sm)->overloads) 
                    if (callable.raw() == &*match) selected = callable; // find the refcell of the best match
                best_match = some<rc<Callable>>((rc<Callable>)selected);
            }
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
                if (gr.value.form->has_infix_case() && params.size() > (is_infix ? 2 : 1)
                    && (gr.value.form->precedence > outerprec 
                    || (gr.value.form->precedence == outerprec && outerassoc == ASSOC_RIGHT && is_infix))) { 
                    // if we get an argument that is infix but not applied to anything, we need to consider
                    // backtracking
                    it = gr.next;
                    backtrack(env, params.back(), gr, it, end);
                }
                else { // otherwise, we continue past this group
                    it = gr.next;
                    while (gr.value.form->has_prefix_case() && it != end) {
                        GroupResult ogr = retry_group(env, gr, it, end);
                        it = ogr.next;
                        if (ogr.value.type != T_ERROR) gr = ogr;
                        else break; // retrying didn't make progress, so we exit
                    }
                    params.push(gr.value);
                    sm->advance(params.back());
                }
            }
            if (auto match = sm->match()) {
                best = it, n = params.size();

                // we need all this machinery to get the refcall associated with the match, instead
                // of a non-owning reference.
                if (params[0].form->kind == FK_CALLABLE) best_match = some<rc<Callable>>((rc<Callable>)sm);
                else if (params[0].form->kind == FK_OVERLOADED) {
                    rc<Callable> selected = nullptr;
                    for (rc<Callable> callable : ((rc<Overloaded>)sm)->overloads) 
                        if (callable.raw() == &*match) selected = callable; // find the refcell of the best match
                    best_match = some<rc<Callable>>((rc<Callable>)selected);
                }
            }
        }
        if (best_match) {
            while (params.size() > n) params.pop(); // remove any extra arguments we added while exploring

            params[0].form = ref<Form>(FK_CALLABLE, params[0].form->precedence, params[0].form->assoc);
            params[0].form->invokable = *best_match; // use best match instead of overload

            Source::Pos resultpos = span(params.front().pos, params.back().pos); // params can never be empty, so this is safe
            if (params.size() >= 2) resultpos = span(params[1].pos, resultpos); // this handles the infix left hand operand
            Value result = v_list(resultpos, t_list(T_ANY), move(params));
            resolve_form(env, result);
            return GroupResult{
                result,
                best,
                outerprec
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
            auto gr = try_group(env, move(params), term.form->kind, sm, 
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
                auto gr = try_group(env, move(params), op.form->kind, sm, 
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

    GroupResult backtrack_group(rc<Env> env, Value term, Value op, list_iterator it, list_iterator end,
        Associativity outerassoc, i64 outerprec) {
        do { // loop until we fail to find a suitable infix operator
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
                auto gr = try_group(env, move(params), op.form->kind, sm, 
                    it_copy, end, op.form->assoc, op.form->precedence);

                if (gr.is_left()) { // if grouping succeeded
                    resolve_form(env, gr.left().value);
                    term = gr.left().value;
                    it = gr.left().next;
                }
                else {
                    report_group_error(gr.right(), op, params);
                    break; // next term must not have been in a suitable spot, so we exit
                }
            }
            else break; // next term is not a suitable infix operator, or had lower precedence

            if (it != end) op = *it;
        } while (it != end);

        return GroupResult{
            term,
            it
        };
    }

    void backtrack(rc<Env> env, Value& back, const GroupResult& gr, 
        list_iterator& begin, list_iterator end) {
        // we only apply when !back.form, since we don't want to interfere with parenthesized lists
        if (!back.form && back.type.of(K_LIST) && 
            (gr.value.form->precedence > v_head(back).form->precedence
             || (gr.value.form->precedence == v_head(back).form->precedence 
                 && v_head(back).form->assoc == ASSOC_RIGHT))) {
            // if our new operator has higher precedence than the outermost expr, we delve deeper
            list_iterable iter = iter_list(back);
            list_iterator one = iter.begin(), two = iter.begin();
            two ++;
            while (two != iter.end()) one ++, two ++; // find end of list. this is slow...
            backtrack(env, *one, gr, begin, end); 
        }
        else {
            // if the outermost expr has higher precedence, or if we found a constant, we try to
            // apply our new operator
            GroupResult bg = backtrack_group(env, back, gr.value, begin, end, gr.value.form->assoc, I64_MIN);
            back = bg.value;
            begin = bg.next;
        }
    }

    GroupResult retry_group(rc<Env> env, const GroupResult& outer_group, 
        list_iterator it, list_iterator end) {
        if (it == end) panic("Tried to retry grouping when no elements remain in list!");
        vector<Value> params;
        Value term = outer_group.value;
        params.push(term); // include term as a param

        rc<StateMachine> sm = term.form->start();
        sm->advance(term); // move past first term
        list_iterator it_copy = it; // we don't want to actually move 'it'
        auto gr = try_group(env, move(params), term.form->kind, sm, 
            it_copy, end, term.form->assoc, term.form->precedence);

        if (gr.is_left()) { // if grouping succeeded
            resolve_form(env, gr.left().value);
            return gr.left();
        }
        else if (params.size() == 1) { // if we didn't match any other parameters
            ++ it; // leave the term in place, not applied to anything
            return GroupResult {
                v_error({}), // we use v_error here to signify that grouping failed
                it,          // ...but we still want the new value of 'it'
                0
            };
        }
        else {
            report_group_error(gr.right(), term, params);
            ++ it; // advance just past this term
            return GroupResult {
                v_error({}),
                it,
                0
            };
        }
    }

    void group(rc<Env> env, Value& term) {
        vector<Value> results; // all the values in this group
        results.clear();

        list_iterable iterable = iter_list(term);
        list_iterator begin = iterable.begin(), end = iterable.end();
        while (begin != end) {
            GroupResult gr = next_group(env, begin, end, ASSOC_RIGHT, I64_MIN); // assume lowest precedence at first
            
            resolve_form(env, gr.value); // resolve new group's form
            begin = gr.next;
            results.push(gr.value);
        }

        if (results.size() == 0) panic("Somehow found no groups within list!");
        else if (results.size() == 1) term = results.front();
        else {
            Source::Pos termpos = span(results.front().pos, results.back().pos);
            term = v_list(termpos, t_list(T_ANY), move(results));
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
                    term.form = to_prefix(v_head(term).form); // always treat single-element lists as expressions
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
                    else if (v_head(term).type.of(K_SYMBOL)) {
                        auto lookup = env->find(v_head(term).data.sym);
                        if (lookup && lookup->type.of(K_FUNCTION) && !lookup->data.fn->builtin
                            && lookup->form->kind == FK_CALLABLE) {
                            vector<rc<Form>> args;
                            rc<Callable> form = (rc<Callable>)v_head(term).form->start();
                            bool on_variadic = false;

                            for (Value& arg : iter_list(v_tail(term))) { // visit each argument
                                if (form->current_param() && form->current_param()->kind == PK_SELF) // skip self parameter
                                    form->advance(v_void({}));

                                if (form->is_finished()) {
                                    break; // fallthrough, defaulting to term form
                                }

                                form->precheck_keyword(arg); // leave variadics for keyword if possible

                                if (!is_variadic(form->current_param()->kind) && on_variadic) {
                                    args.push(F_TERM); // variadics are always terms
                                }
                                if (is_variadic(form->current_param()->kind)) {
                                    on_variadic = true;
                                }
                                else if (form->current_param()->kind == PK_TERM) { // terms are always terms
                                    args.push(F_TERM);
                                }
                                else {
                                    args.push(arg.form ? arg.form : F_TERM); // push form, defaulting to term
                                }
                                form->advance(arg);
                            }
                            if (on_variadic) args.push(F_TERM);

                            term.form = v_resolve_body(*lookup->data.fn, args)->base->form;
                            if (!term.form) term.form = F_TERM; // default to term
                            return;
                        }
                        // fallthrough
                    }
                    term.form = F_TERM; // default to F_TERM if the callable has no special callback or was unresolved
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

    void expand_splices(rc<Env> env, Value& term) {
        if (term.type.of(K_LIST)) {
            auto iterable = iter_list(term);
            auto it = iterable.begin();
            while (it != iterable.end()) {
                if (it->form) expand_splices(env, *it); // recursively expand splices for parsed blocks
                if (it->type.of(K_LIST) && v_head(*it).type == T_SYMBOL && v_head(*it).data.sym == S_SPLICE) {
                    vector<Value> to_splice;
                    for (Value v : iter_list(v_tail(*it))) // evaluate tail
                        to_splice.push(eval(env, v));
                    list_iterator next = it; // store previous next
                    next ++;

                    List* l = it.list; // splice in values
                    for (i64 i = i64(to_splice.size()) - 1; i >= 0; i --)
                        l->tail = ref<List>(to_splice[i], l->tail);
                    it = next;
                }
                else it ++;
            }
        }
    }

    static int call_count = 0;

    // stores information for reporting an overload mismatch error
    using PriorityError = pair<Type, u32>;

    either<i64, PriorityError> overload_priority(Type fn_args, Type args) {
        u32 len = fn_args.of(K_TUPLE) ? t_tuple_len(fn_args) : 1; // number of arguments

        // we prioritize each argument by a certain value, based on how good of a match it is.
        // exact matches are best, followed by generic matches (e.g. Int -> T?), followed by
        // matches that require coercion (e.g. Int -> Float), followed by union matches
        // (e.g. (Int | String) -> Int). union matches are only permitted if we found in advance
        // that the union has a valid value for every overload.

        const i64 UNION_PRIORITY = 1, // the priority of a union argument being coerced to one of its members
                  COERCE_PRIORITY = len + 1, // priority of an argument that needs coercion
                  GENERIC_PRIORITY = COERCE_PRIORITY * COERCE_PRIORITY, // priority of an argument coerced to a generic
                  EQUAL_PRIORITY = GENERIC_PRIORITY * COERCE_PRIORITY; // priority of an argument with the right type

        i64 priority = 0;
        for (u32 i = 0; i < len; i ++) {
            Type fn_arg = len == 1 ? fn_args : t_tuple_at(fn_args, i); // get desired arg at position
            Type arg = len == 1 ? args : t_tuple_at(args, i); // get provided arg at position    

            // in general, we avoid potentially binding coercion here, since we don't yet know which
            // overloads are possible we don't want to prematurely constrain type variables to one
            // that might ultimately be invalidated.
            
            if (arg == fn_arg) // if arg matches exactly
                priority += EQUAL_PRIORITY;
            else if (arg.nonbinding_coerces_to_generic(fn_arg)) // if arg matches generic target
                priority += GENERIC_PRIORITY;
            else if (arg.nonbinding_coerces_to(fn_arg)) // if arg can coerce to target
                priority += COERCE_PRIORITY; 
            else if (arg.of(K_UNION) && fn_arg.coerces_to(arg)) // if arg is valid union containing target
                priority += UNION_PRIORITY; 
            else return PriorityError{ fn_arg, i }; // this argument is incompatible; we can't use this overload
        }

        return priority;
    }

    // Returns whether an argument tuple is runtime-only.
    bool is_args_runtime(Type args) {
        if (args.of(K_TUPLE)) {
            for (u32 i = 0; i < t_tuple_len(args); i ++) {
                Type t = t_tuple_at(args, i);
                if (t.of(K_RUNTIME)) return true;
            }
            return false;
        }
        else return args.of(K_RUNTIME);
    }

    // Removes the "runtime" wrapper around any argument types, if present.
    Type strip_runtime(Type args) {
        if (args.of(K_TUPLE)) {
            vector<Type> nonruntime;
            for (u32 i = 0; i < t_tuple_len(args); i ++) {
                Type t = t_tuple_at(args, i);
                nonruntime.push(t.of(K_RUNTIME) ? t_runtime_base(t) : t);
            }
            return t_tuple(nonruntime);
        }
        else if (args.of(K_RUNTIME)) return t_runtime_base(args);
        else return args;
    }

    either<Type, OverloadError> resolve_call(rc<Env> env, const vector<Type>& overloads_in, Type args) {
        if (overloads_in.size() == 0) panic("Cannot resolve empty list of overloads!");
        if (overloads_in.size() == 1) return overloads_in[0];
        static vector<either<i64, PriorityError>> priorities;
        priorities.clear();

        for (const auto& fn : overloads_in) { // compute priorities for each case
            // println("found overload ", fn);
            priorities.push(overload_priority(t_arg(fn), args)); 
        }
    
        i64 max_priority = -1; // compute max priority
        for (int i = 0; i < priorities.size(); i ++) {
            if (priorities[i].is_left()) {
                max_priority = priorities[i].left() > max_priority 
                    ? priorities[i].left() : max_priority;
            }
        }

        // for (int i = 0; i < priorities.size(); i ++) {
        //     if (priorities[i].is_left()) 
        //         println("overload ", overloads_in[i], " on ", 
        //             args, " has priority ", priorities[i].left());
        //     else 
        //         println("overload ", overloads_in[i], " mismatches ", args, 
        //             " at ", priorities[i].right().second);
        // }

        if (max_priority == -1) { // all functions were errors
            vector<PriorityError> errors;
            for (u32 i = 0; i < priorities.size(); i ++) errors.push({ overloads_in[i], priorities[i].right().second });
            return OverloadError{ false, errors };
        }

        vector<Type> overloads = overloads_in; // copy so we can prune mismatches

        u32 out = 0; // remove all cases with less than max priority
        for (u32 i = 0; i < overloads.size(); i ++) {
            if (priorities[i].is_left() && priorities[i].left() == max_priority)
                overloads[out ++] = overloads[i];
        }
        while (overloads.size() > out) overloads.pop();

        if (overloads.size() == 0) 
            panic("Somehow got no valid function, even though max_priority > -1!");
        if (overloads.size() > 1) { // conflict
            vector<PriorityError> errors;
            for (Type t : overloads) errors.push({ t, 0 });
            return OverloadError{ true, move(errors) };
        }

        return overloads[0];
    }

    Value coerce_rt(rc<Env> env, const Param& param, bool is_runtime, const Value& v, Type dest) {
        if ((v.type.of(K_RUNTIME) || is_runtime) && !dest.of(K_RUNTIME)) {
            dest = t_runtime(t_lower(dest));
        }
        if (dest.of(K_RUNTIME) && !v.type.of(K_RUNTIME) && !is_evaluated(param.kind)) { // evaluate quoted stuff before runtime
            Value v2 = v;
            v2 = eval(env, v2);
            if (v2.type == T_ERROR) return v_error({});
            return coerce(env, v2, t_runtime(v2.type));
        }
        return coerce(env, v, dest);
    }

    Value coerce_args(rc<Env> env, const vector<Param>& params, bool is_runtime, const Value& args, Type dest) {
        if (args.type.of(K_TUPLE)) {
            vector<Value> coerced;
            for (u32 i = 0; i < v_tuple_len(args); i ++) {
                coerced.push(coerce_rt(env, params[i], is_runtime, v_at(args, i), t_tuple_at(dest, i)));
                if (coerced.back().type == T_ERROR) return v_error({});
            }
            return v_tuple(args.pos, infer_tuple(coerced), move(coerced));
        }
        else return coerce_rt(env, params[0], is_runtime, args, dest);
    }

    PerfInfo::PerfInfo():
        max_depth(50), max_count(50), exceeded(false) {}

    void PerfInfo::begin_call(const Value& term, u32 base_cost) {
        if (counts.size() >= max_depth) exceeded = true;
        counts.push({term, base_cost, false});
    }

    void PerfInfo::end_call() {
        u32 count = counts.back().count;
        counts.pop();
        if (counts.size()) counts.back().count += count;
    }

    void PerfInfo::end_call_without_add() {
        counts.pop();
    }

    void PerfInfo::set_max_depth(u32 max_depth_in) {
        max_depth = max_depth_in;
    }

    void PerfInfo::set_max_count(u32 max_count_in) {
        max_count = max_count_in;
    }

    u32 PerfInfo::current_count() const {
        if (counts.size() == 0) return 0; // root always has no cost
        return counts.back().count;
    }

    void PerfInfo::make_comptime() {
        if (counts.size()) counts.back().comptime = true;
    }

    bool PerfInfo::is_comptime() const {
        return counts.size() > 0 ? counts.back().comptime : false;
    }

    bool PerfInfo::depth_exceeded() {
        bool exc = exceeded;
        exceeded = false;
        return exc;
    }

    static PerfInfo perfinfo; // shared instance

    PerfInfo& get_perf_info() {
        return perfinfo;
    }

    Value call(rc<Env> env, Value call_term, Value func, const Value& args_in) {
        if ((perfinfo.current_count() >= perfinfo.max_count || perfinfo.counts.size() >= perfinfo.max_depth)
            && !perfinfo.is_comptime()) {
            // println("skipping call to ", func_term, " ", args_in, " - current cost is ", perfinfo.current_count());
            return v_error({}); // return error value if current function is too expensive
        }

        Value func_term = v_head(call_term);    
        Value args = args_in;
        
        vector<Type> fixed_args_type;

        // resolve types of each runtime argument
        if (args.type.of(K_TUPLE)) for (int i = 0; i < v_len(args); i ++) {
            if (v_at(args, i).type.of(K_RUNTIME)) fixed_args_type.push(v_at(args, i).data.rt->ast->type(env));
            else fixed_args_type.push(v_at(args, i).type);
        }
        else if (args.type.of(K_RUNTIME)) fixed_args_type.push(args.data.rt->ast->type(env));
        else fixed_args_type.push(args.type);


        // overload resolution is done independently of runtime typing, so we clean up
        // our argument type to accommodate that.
        Type args_type = fixed_args_type.size() == 1 ? fixed_args_type[0] : t_tuple(fixed_args_type);
        args_type = strip_runtime(args_type);

        // grab the parameters so that we know which arguments have been quoted
        // we'll need to evaluate these arguments later if we lower to runtime, so that
        // we don't try and lower any quoted lists directly to AST.
        vector<Param> params;
        if (func_term.form->kind != FK_CALLABLE) // sanity check
            panic("Expected called function's form to be resolved to callable!");
        // we can assume that the function's form has been resolved to a single callable because
        // we have either completed form resolution or inferred a form based on type.
        rc<Callable> callable = func_term.form->invokable;
        for (u32 i = 0; i < callable->parameters->size(); i ++) {
            const Param& p = (*callable->parameters)[i];
            if (p.kind != PK_KEYWORD && p.kind != PK_SELF) params.push(p);
        }

        Type fntype = t_runtime_base(func.type);

        if (fntype.of(K_INTERSECT)) { // we'll perform overload resolution among the intersection's cases
            vector<Type> valid;
            // println("func form = ", func.form);
            for (const auto& [t, v] : func.data.isect->values) {
                // println("  value form = ", v.form);
                Type bt = t_runtime_base(t);
                if (v.form && v.form == func_term.form && bt.of(K_FUNCTION)) {
                    valid.push(t_runtime_base(bt)); // select only those cases that have the same form we selected
                }
            }

            if (valid.size() == 0) {
                err(call_term.pos, "No overloads of function '", func_term, "' matched the desired form '",
                    func_term.form, "'.");
                return v_error({});
            }

            // perform type-based overload resolution
            auto resolved = resolve_call(env, valid, args_type);

            // handle errors if necessary
            if (resolved.is_right()) { // error
                if (resolved.right().ambiguous) {
                    if (!t_is_concrete(args_type)) {
                        t_tvar_enable_isect();
                        vector<Type> isect_types;
                        map<Type, Value> isect_values;
                        for (const auto& mismatch : resolved.right().mismatches) {
                            Type case_arg = t_arg(mismatch.first);
                            args_type.coerces_to(case_arg); // we call this for typevar-related side effects
                            isect_values[mismatch.first] = func.data.isect->values[mismatch.first];
                            isect_types.push(mismatch.first);
                        }
                        t_tvar_disable_isect();
                        func = v_intersect(func.pos, t_intersect(isect_types), move(isect_values));
                    }
                    else {
                        err(args.pos, "Ambiguous call to overloaded function '", func_term, "'.");
                        for (u32 i = 0; i < resolved.right().mismatches.size(); i ++) {
                            const auto& mismatch = resolved.right().mismatches[i];
                            const Value& v = func.data.isect->values[mismatch.first];
                            note(v.pos, "Candidate function found of type '", v.type, "'.");
                        }
                        return v_error({});
                    }
                }
                else {
                    err(args.pos, "Incompatible arguments '", args_type, "' for function '", func_term, "'.");
                    for (u32 i = 0; i < resolved.right().mismatches.size(); i ++) {
                        const auto& mismatch = resolved.right().mismatches[i];
                        const Value& v = func.data.isect->values[mismatch.first];
                        Value arg = args.type.of(K_TUPLE) ? v_at(args, mismatch.second) : args;
                        note(v.pos, "Candidate function found of type '", v.type, "', but ",
                            "given incompatible argument '", arg, "' of type '", strip_runtime(arg.type), "'.");
                    }
                    return v_error({});
                }
            }
            else func = func.data.isect->values[resolved.left()];
            fntype = t_runtime_base(func.type);

            if (fntype.of(K_FUNCTION) && !args_type.coerces_to(t_arg(fntype))) {
                panic("Somehow ended up with incompatible arguments for overloaded function!");
            }
        }
        else if (fntype.of(K_FUNCTION)) {
            if (!args_type.coerces_to(t_arg(fntype))) {
                err(args.pos, "Incompatible arguments for function '", func_term, "'. Expected '",
                    t_arg(fntype), "', got '", args_type, "'.");
                return v_error({});
            }
        }
        else panic("Tried to call non-callable value!");

        bool is_runtime = is_args_runtime(args.type) || fntype.of(K_RUNTIME) || fntype.of(K_INTERSECT);

        args = coerce_args(env, params, is_runtime, args, func.type.of(K_INTERSECT) ? args_type : t_arg(fntype));
        // println("args_type = ", args_type, " and coerced to ", args);
        if (args.type == T_ERROR) return v_error({});

        if (func.type == T_ERROR) {
            return v_error({});
        }
        else if (func.type.of(K_INTERSECT)) {
            // if we're still in an intersect at this point, we'll figure things out in the AST phase

            map<Type, rc<Function>> cases;
            for (const auto& [t, v] : func.data.isect->values) cases[t] = v.data.fn;
            vector<rc<AST>> arg_nodes;
            if (args.type.of(K_TUPLE)) for (const Value& v : v_tuple_elements(args))
                arg_nodes.push(v.data.rt->ast);
            else arg_nodes.push(args.data.rt->ast);

            return v_runtime({}, t_runtime(T_ANY), ast_call(
                args.pos, 
                ast_overload(func.pos, func.type, cases),
                arg_nodes
            ));
        }
        else if (func.type.of(K_FUNCTION) && func.data.fn->builtin) {
            const auto& builtin = func.data.fn->builtin;
            if (is_runtime) {
                if (!builtin->runtime) {
                    err(call_term.pos, "Compile-time only function '", func_term, 
                        "' was invoked on runtime-only arguments.");
                    return v_error({});
                }
                perfinfo.begin_call(call_term, 1); // builtins are considered cheap
                rc<AST> ast = builtin->runtime(env, call_term, args);
                perfinfo.end_call();
                return v_runtime({}, t_runtime(ast->type(env)), ast);
            }
            else {
                perfinfo.begin_call(call_term, 1);
                if (!builtin->runtime) perfinfo.make_comptime();
                Value result = builtin->comptime(env, call_term, args);
                perfinfo.end_call();
                return result;
            }
        }
        else if (func.type.of(K_RUNTIME)) {
            vector<rc<AST>> arg_nodes;
            if (args.type.of(K_TUPLE)) for (const Value& v : v_tuple_elements(args))
                arg_nodes.push(v.data.rt->ast);
            else arg_nodes.push(args.data.rt->ast);

            return v_runtime(
                {},
                t_runtime(t_ret(t_runtime_base(func.type))),
                ast_call(func.pos, func.data.rt->ast, arg_nodes)
            );
        }
        else {
            // if it's a compile-time call, we try to evaluate the body - but it might run into 
            Value result;
            if (!is_runtime) {
                if (func.type.of(K_RUNTIME)) // check just in case
                    panic("Somehow got runtime function in compile-time function call!");

                perfinfo.begin_call(call_term, 1); // user-defined functions are considered more expensive
                // println("beginning call to ", func_term, " ", args_in);
                static int n = 0;
                n ++;
                rc<InstTable> fn_body = v_resolve_body(*func.data.fn, args); // resolve function body

                if (fn_body->is_instantiating(args_type)) {
                    // we've called a function that is currently in the process of being compiled,
                    // so we skip its compile-time evaluation to avoid infinite recursion in the
                    // compiler
                    is_runtime = true;
                }
                else {
                    rc<Env> fn_env = fn_body->env;
                    const vector<Symbol>& fn_args = func.data.fn->args;
                    vector<pair<Symbol, Value>> existing;
                    for (const auto& p : fn_env->values) existing.push(p); // save existing environment 
                    if (fn_args.size() == 1) { // bind args to single argument
                        fn_env->def(fn_args[0], args);
                    }
                    else for (u32 i = 0; i < v_len(args); i ++) { // bind args to multiple arguments
                        fn_env->def(fn_args[i], v_at(args, i));
                    }
                    result = eval(fn_body->env, *fn_body->base); // eval it
                    for (u32 i = 0; i < existing.size(); i ++) 
                        fn_env->def(existing[i].first, existing[i].second); // restore any previous values
                }
            }
            
            // if we couldn't resolve a compile-time value (either the function/args were runtime, or
            // the compiler determined it was too expensive), instantiate a runtime call and return
            // that.
            if (is_runtime 
                || perfinfo.current_count() >= perfinfo.max_count 
                || perfinfo.depth_exceeded()) {
                // if we exceeded the perf limit, pretend the call didn't happen - clear its perf
                // frame

                if (!is_runtime) {
                    perfinfo.end_call_without_add();
                }

                rc<AST> fn_ast = nullptr;

                // println("trying to lower call to ", func_term, " ", args, " of type ", func.type);

                if (func.type.of(K_FUNCTION)) {
                    // instantiate runtime call
                    rc<FnInst> inst = func.data.fn->inst(args_type, args);
                    if (!inst) return v_error({}); // propagate errors

                    fntype = inst->func->type(env);
                    fn_ast = inst->func;
                }
                else if (func.type.of(K_RUNTIME)) {
                    fn_ast = func.data.rt->ast;
                }
                else {
                    panic("Somehow got neither a function nor runtime value in function call!");
                }

                // we pretend the runtime call is a builtin with cost one
                perfinfo.begin_call(call_term, 1);
                perfinfo.end_call();

                // try coercion again, with is_runtime always true
                if (fntype == T_ERROR) return v_error({});
                args = coerce_args(env, params, true, args, t_arg(fntype));
                if (args.type == T_ERROR) return v_error({});

                vector<rc<AST>> arg_nodes;
                if (args.type.of(K_TUPLE)) for (const Value& v : v_tuple_elements(args))
                    arg_nodes.push(v.data.rt->ast);
                else arg_nodes.push(args.data.rt->ast);

                return v_runtime({}, t_runtime(t_ret(fntype)), ast_call(
                    args.pos, 
                    fn_ast,
                    arg_nodes
                ));
            }

            perfinfo.end_call();
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
                if (var->type == T_TYPE && var->data.type.is_tvar())
                    return v_type(var->pos, t_tvar_concrete(var->data.type));
                if (var->type.of(K_RUNTIME))
                    return v_runtime(var->pos, var->type, ast_var(var->pos, env, term.data.sym));
                return *var;
            }
            case K_LIST: { // non-empty lists eval to the results of applying functions
                if (error_count()) 
                    return v_error({}); // return error value if form resolution went awry

                // expand_splices(env, term);

                Value head = eval(env, v_head(term)); // evaluate first term

                if (v_tail(term).type == T_VOID) 
                    return head.with(infer_form(head.type)); // return the function if no args, e.g. (+)

                if (t_runtime_base(head.type).of(K_FUNCTION) 
                    || (t_runtime_base(head.type).of(K_INTERSECT) && t_intersect_procedural(head.type))) {
                    vector<Value> args;
                    vector<Value> varargs; // we use this to accumulate arguments for an individual variadic parameter
                    rc<Callable> form = (rc<Callable>)v_head(term).form->start();

                    // we basically re-run the form over the argument list here to figure out where to evaluate
                    for (Value& arg : iter_list(v_tail(term))) { // visit each argument
                        if (form->current_param() && form->current_param()->kind == PK_SELF) // skip self parameter
                            form->advance(v_void({}));

                        if (form->is_finished()) {
                            err(arg.pos, "Too many parameters provided to function '", v_head(term), "': ",
                                "found term '", arg, "' after last matching parameter.");
                            return v_error({});
                        }

                        form->precheck_keyword(arg); // leave variadics for keyword if possible

                        // if we were building a variadic list, and aren't on a variadic anymore,
                        // we clear the varargs and push them as a list onto the arg array
                        if (form->current_param() && !is_variadic(form->current_param()->kind)
                            && varargs.size() > 0) {
                            args.push(v_list(
                                span(varargs.front().pos, varargs.back().pos),
                                infer_list(varargs),
                                move(varargs)
                            ));
                            varargs.clear();
                        }
                        if (form->current_param() &&  // skip evaluation
                            (form->current_param()->kind == PK_TERM 
                             || form->current_param()->kind == PK_TERM_VARIADIC
                             || form->current_param()->kind == PK_QUOTED 
                             || form->current_param()->kind == PK_QUOTED_VARIADIC)) {

                            if (form->current_param()->kind == PK_QUOTED_VARIADIC
                                || form->current_param()->kind == PK_TERM_VARIADIC) varargs.push(arg);
                            else args.push(arg);
                        }
                        else if (form->current_param() && form->current_param()->kind != PK_KEYWORD) { // evaluate but ignore keywords
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
                            move(varargs)
                        ));
                    }

                    if (args.size() == 0) {
                        err(term.pos, "Procedure '", v_head(term), "' must be called on one or more arguments; zero given.");
                        return v_error({});
                    }
                    for (const Value& v : args) if (v.type == T_ERROR) return v_error({}); // propagate errors

                    Value args_value = args.size() == 1 ? args[0]
                        : v_tuple(span(args.front().pos, args.back().pos), infer_tuple(args), move(args));

                    Value result = call(env, term, head, args_value);
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
