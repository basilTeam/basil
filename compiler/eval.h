/*
 * Copyright (c) 2021, the Basil authors
 * All rights reserved.
 *
 * This source code is licensed under the 3-Clause BSD License, the full text
 * of which can be found in the LICENSE file in the root directory
 * of this project.
 */

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

    // Attempts to deduce the form of a value based on its type.
    rc<Form> infer_form(Type type);

    // Don't call this unless you know what you're doing! Resolves the form of the
    // provided term. Returns the resulting environment.
    void resolve_form(rc<Env> env, Value& term);

    // Holds metadata used in reporting an overload resolution error.
    struct OverloadError {
        bool ambiguous; // True if the error describes conflicting overloads.
        vector<pair<Type, u32>> mismatches;
    };

    // Resolves an overloaded function call, given a list of valid overload types
    // and a list of the types of its parameters. Runtime types and type variables 
    // generally behave like their underlying types for the purposes of this 
    // function.
    // Returns a valid type from overloads if resolution succeeds, or an OverloadError
    // instance otherwise.
    either<Type, OverloadError> resolve_call(rc<Env> env, const vector<Type>& overloads, Type args);

    // Stores measurements about function call 
    struct PerfInfo {
        struct PerfFrame {
            Value call_term;
            rc<InstTable> fn;
            u32 count;
            bool comptime = false, ismeta = false, instantiating = false;
        };

        u32 max_depth, max_count;
        vector<PerfFrame> counts;
        bool exceeded;
        PerfInfo();

        // Open a perf frame for a function call.
        void begin_call(const Value& term, rc<InstTable> fn, u32 base_cost);

        // End call and add its cost to the enclosing call's cost.
        void end_call();

        // End call without incrementing the perf count.
        void end_call_without_add();

        void set_max_depth(u32 max_depth);
        void set_max_count(u32 max_count);

        // Get the perf count of the top open frame.
        u32 current_count() const;

        // Mark this call frame (and its descendents) as containing a compile-time-only
        // operation.
        void make_comptime();
        bool is_comptime() const;

        // Mark this call frame (and its descendents) as part of a runtime function
        // instantiation - this means cost is ignored, independent of comptime or meta
        // properties, so we compile the whole function body.
        void make_instantiating();
        bool is_instantiating() const;

        // Mark this call frame (and its descendents) as evaluated at compile-time,
        // regardless of the contents (that is, no cost analysis will be done, and
        // stateful operations are permitted).
        // Meta call frames are also comptime.
        void make_meta();

        // I'm so meta even this acronym!
        bool is_meta() const;

        // Return whether the max depth was exceeded within a function call.
        // Also unsets the flag denoting exceeded depth - callers should 
        // always handle this case immediately.
        bool depth_exceeded();
    };

    // Get a reference to the global perf info instance.
    PerfInfo& get_perf_info();

    // Invokes the provided function on the provided argument(s) in the given environment.
    Value call(rc<Env> env, Value func_term, Value func, const Value& args);

    // Evaluates the code value 'term' within the environment 'env'.
    Value eval(rc<Env> env, Value& term);
}

#endif