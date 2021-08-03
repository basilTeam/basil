#ifndef BASIL_EVAL_H
#define BASIL_EVAL_H

#include "util/rc.h"
#include "util/either.h"
#include "env.h"
#include "value.h"
#include "builtin.h"
#include "forms.h"

namespace basil {
    // Untracks root env. Should only be called during deinit!
    void free_root_env();

    // Returns the root environment of the compilation unit, allocating
    // and populating it the first time this function is called.
    rc<Env> root_env();

    // Represents a grouped expression. Includes a value representing the 
    // expression (code, not data values) with a resolved form. Also
    // includes an iterator pointing to the term immediately
    // after the returned group, and the precedence of the rightmost
    // applied operator.
    struct GroupResult {
        Value value;
        list_iterator next;
        i64 precedence;
    };

    // Used to provide all the necessary error information from a failed grouping.
    struct GroupError {
        vector<rc<Callable>> candidates;
    };

    // Don't call this unless you know what you're doing! It's only made publicly
    // available in this header to ease testing. This is a mutually recursive method
    // with 'next_group', used to attempt grouping of terms using a state machine.
    // Returns a some optional if a group was successfully created; returns none if
    // the state machine could not match the input.
    //
    // 'params' is initialized outside this function (generally within 'next_group'). It
    // is modified through the evaluation of 'try_group' as new parameters are discovered.
    // But it is initially provided one or two parameters from the caller, representing the
    // function name (for prefix forms) or the first argument and function name (for
    // infix forms).
    either<GroupResult, GroupError> try_group(rc<Env> env, vector<Value>& params, FormKind fk, rc<StateMachine> sm, 
        list_iterator it, list_iterator end, 
        Associativity outerassoc, i64 outerprec);

    // Don't call this unless you know what you're doing! Pulls the next complete expression 
    // from the iterator range. Guaranteed to not be empty. Will panic if called on an empty 
    // range!
    // 
    // 'outerassoc' and 'outerprec' are used to pass along the properties of the
    // preceding form. If calling this without a preceding form, pass I64_MIN to
    // 'outerprec' and any associativity for 'outerassoc' (it doesn't matter).
    GroupResult next_group(rc<Env> env, 
        list_iterator it, list_iterator end, 
        Associativity outerassoc, i64 outerprec);

    // Don't call this unless you know what you're doing! Finds all groups within the
    // provided list term, and replaces the term with a list of those groups. Returns
    // the resulting environment.
    void group(rc<Env> env, Value& term);

    // Don't call this unless you know what you're doing! Resolves the form of the
    // provided term. Returns the resulting environment.
    void resolve_form(rc<Env> env, Value& term);

    // Invokes the provided function on the provided argument(s) in the given environment.
    Value call(rc<Env> env, Value func_term, Value func, const Value& args);

    // Evaluates the code value 'term' within the environment 'env'.
    Value eval(rc<Env> env, Value& term);
}

#endif