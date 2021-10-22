/*
 * Copyright (c) 2021, the Basil authors
 * All rights reserved.
 *
 * This source code is licensed under the 3-Clause BSD License, the full text
 * of which can be found in the LICENSE file in the root directory
 * of this project.
 */

#ifndef BASIL_FORM_H
#define BASIL_FORM_H

#include "util/vec.h"
#include "util/rc.h"
#include "util/option.h"
#include "type.h"
#include "value.h"

template<>
u64 hash(const rc<basil::Form>& form);

namespace basil {
    struct Env;

    enum FormKind {
        FK_TERM,        // Form for a non-applied, singular value. 
        FK_CALLABLE,    // Form that can be applied, like a single function.
        FK_OVERLOADED,  // Form of multiple callable forms, like an overloaded function.
        FK_COMPOUND,    // Form containing multiple other forms, indexed by values, for things like modules.
        NUM_FORM_KINDS
    };

    // Forward definitions.
    struct Form;
    struct Callable;

    // Abstract interface for a form state machine. Allows term-by-term
    // parsing and matching. 
    struct StateMachine {
        virtual ~StateMachine();

        // Returns whether this invokable has any prefix-style cases (<name> <value>*).
        // Non-const to permit caching.
        virtual bool has_prefix_case() = 0;

        // Returns whether this invokable has any infix-style cases (<value> <name> <value>*).
        // Non-const to permit caching.
        virtual bool has_infix_case() = 0;

        // Resets this state machine to the starting state.
        virtual void reset() = 0;

        // Returns whether the provided keyword is a valid step forward for this state 
        // machine.
        //
        // This one is a little nuanced. Basically, we want to give keywords priority
        // over other parameters. If any child machine matches a keyword with a keyword
        // parameter, all child machines that don't match that keyword with a keyword
        // parameter are stopped. But, if no child machine matches the keyword in this way,
        // none of them are stopped and we proceed to advance() as per usual.
        //
        // This is a separate method from advance() to allow keywords to be checked before
        // their forms are inspected and they are potentially applied to nearby parameters.
        virtual bool precheck_keyword(const Value& keyword) = 0;

        // This is similar to precheck_keyword, but for single terms instead of keywords.
        // It checks whether any child machine matches an ungrouped term, and if any do,
        // all child machines that don't match that term are stopped. 
        //
        // There are two main differences from precheck_keyword. The first is that this
        // should be lower priority than precheck_keyword, and called after it returns
        // false. The second difference is that a term cannot move a machine out of a
        // variadic parameter.
        virtual bool precheck_term(const Value& term) = 0;

        // Moves the state machine forward by the provided value.
        virtual void advance(const Value& value) = 0;

        // Returns true if the state machine cannot advance further.
        virtual bool is_finished() const = 0;

        // Returns the match this state machine found, if any.
        // Returns a none optional if the state machine isn't finished,
        // or no match was found.
        virtual optional<const Callable&> match() const = 0;

        // Returns a copy of this state machine. Generally, this involves duplicating
        // associated state, but storing constant information (like parameter lists) by
        // reference.
        virtual rc<StateMachine> clone() = 0;
    };

    // The type of a callback function used to dynamically resolve forms
    // during application.
    //
    // Form callbacks should NOT report errors. If an error occurs and the
    // callback cannot complete as intended, return a term form.
    using FormCallback = rc<Form>(*)(rc<Env>, const Value&);

    // The type of a callback function used to perform additional behavior
    // during form matching. This function is called whenever a state machine
    // reaches this parameter and matches it for the first time.
    // Returns the new environment to continue matching with after moving
    // past this parameter.
    //
    // Param callbacks should NOT report errors. If an error occurs and the
    // callback cannot complete as intended, return the original env.
    using ParamCallback = rc<Env>(*)(rc<Env>, const Value&);

    // Enumeration of all the different types of parameters permitted in
    // a callable form.
    enum ParamKind {
        PK_VARIABLE, // Parameter can bind to any single term.
        PK_VARIADIC, // Parameter can bind to any number of terms.
        PK_KEYWORD, // Parameter matches only the corresponding symbol. Part of the function signature.
        PK_TERM, // Parameter can bind to a single ungrouped term.
        PK_TERM_VARIADIC, // Parameter can bind to any number of ungrouped terms.
        PK_QUOTED, // Parameter can bind to any single term, with grouping, but skips evaluation.
        PK_QUOTED_VARIADIC, // Parameter can bind to any number of terms, but skips evaluation for all of them.
        PK_SELF, // Parameter can bind to any single term. Used to specifically denote the function name.
        NUM_PARAM_KINDS
    };

    // Represents a single parameter within a callable form.
    struct Param {
        Symbol name;
        ParamKind kind;

        // Returns whether this parameter matches the provided code value.
        bool matches(const Value& value);
    };

     // The self parameter never changes, so we use a constant for it.
    extern const Param P_SELF;

    // Constructs a variable parameter.
    Param p_var(Symbol name);
    Param p_var(const char* name);

    // Constructs a quoted parameter.
    Param p_quoted(Symbol name);
    Param p_quoted(const char* name);

    // Constructs a term parameter.
    Param p_term(Symbol name);
    Param p_term(const char* name);

    // Constructs a term variadic parameter.
    Param p_term_variadic(Symbol name);
    Param p_term_variadic(const char* name);

    // Constructs a variadic parameter.
    Param p_variadic(Symbol name);
    Param p_variadic(const char* name);

    // Constructs a quoted variadic parameter.
    Param p_quoted_variadic(Symbol name);
    Param p_quoted_variadic(const char* name);

    // Constructs a keyword parameter.
    Param p_keyword(Symbol name);
    Param p_keyword(const char* name);

    // Returns whether the provided ParamKind is variadic.
    bool is_variadic(ParamKind pk);

    // Returns whether the provided ParamKind is automatically evaluated.
    bool is_evaluated(ParamKind pk);

    // The form of all invokable terms. This includes both functions and macros.
    // Individual callables can either be infix or prefix. Prefix callables start
    // with a keyword parameter, and infix callables have their first keyword 
    // parameter as their second parameter. This first parameter, infix or prefix,
    // is the "name" of the callable form.
    //
    // Callable forms may be given a callback. When a callable form is applied to
    // a list of terms, the callback is applied to those terms in the current
    // environment, and the form it returns is used instead.
    struct Callable : public StateMachine {
        rc<vector<Param>> parameters;
        optional<FormCallback> callback;

        Callable(const vector<Param>& parameters, const optional<FormCallback>& callback);

        bool has_prefix_case() override;
        bool has_infix_case() override;
        void reset() override;
        bool precheck_keyword(const Value& keyword) override;
        bool precheck_term(const Value& term) override;
        void advance(const Value& value) override;
        bool is_finished() const override;
        optional<const Callable&> match() const override;
        rc<StateMachine> clone() override;
        
        optional<const Param&> current_param() const;
        bool operator==(const Callable& other) const;
        bool operator!=(const Callable& other) const;
        u64 hash();
        
        // internal state used in the state machine
        u32 index = 0;
        bool stopped;
        optional<u64> lazy_hash;
        u32 advances; // number of times this state machine has advanced without error
        optional<Value> wrong_value; // set to non-none when we encounter a mismatched value
        friend struct Overloaded;
    };

    // The form of an overloaded term. Terms of this form may be invoked in multiple
    // ways, tracked by the elements of the inner 'overloads' vector.
    struct Overloaded : public StateMachine {
        vector<rc<Callable>> overloads;

        Overloaded(const vector<rc<Callable>>& overloads);

        bool has_prefix_case() override;
        bool has_infix_case() override;
        void reset() override;
        bool precheck_keyword(const Value& keyword) override;
        bool precheck_term(const Value& term) override;
        void advance(const Value& value) override;
        bool is_finished() const override;
        optional<const Callable&> match() const override;
        rc<StateMachine> clone() override;

        bool operator==(const Overloaded& other) const;
        bool operator!=(const Overloaded& other) const;
        u64 hash();
        
        rc<set<Symbol>> mangled; // stores mangled forms of the different overloads to detect duplicates
        optional<bool> has_prefix, has_infix;
        optional<u64> lazy_hash;

        friend rc<Form> f_overloaded(i64, const vector<rc<Form>>&);
        friend rc<Form> f_add_overload(rc<Form>, rc<Form>);
    };

    // Data type for the contents of a compound form.
    struct Compound {
        map<Value, rc<Form>> members;

        Compound(const map<Value, rc<Form>>& members);
    };

    enum Associativity {
        ASSOC_LEFT, ASSOC_RIGHT
    };
    
    // Represents the form of a particular value. Forms represent how values are
    // (or are not) applied to other values. Forms are more abstract than function
    // or macro types, and are handled over a code sequence prior to macro expansion
    // or ordinary evaluation.
    struct Form {
        FormKind kind; // What kind of form this is.
        i64 precedence; // How tightly this form binds to arguments.
        Associativity assoc; // How this form associates with operators of the same precedence.
        rc<StateMachine> invokable; // A pointer to the invokable state machine associated with
                                    // this form. May be null!
        rc<Compound> compound; // A pointer to the compound data associated with this form. May be null!

        // Constructs a Form equal to F_TERM.
        Form();

        // Constructs a Form from the given kind and precedence.
        // Generally speaking, avoid using this! Use F_TERM or the f_callable/f_overloaded functions
        // instead.
        Form(FormKind kind, i64 precedence, Associativity assoc);

        // Returns whether this form is invokable. Currently equivalent to 
        // (kind == FK_CALLABLE || kind == FK_OVERLOADED), just a little clearer
        // in purpose.
        bool is_invokable() const;

        // Returns whether this form can be called in a prefix (<name> <value>*) fashion in
        // any of its overloads.
        bool has_prefix_case();

        // Returns whether this form can be called in an infix (<value> <name> <value>*) fashion in
        // any of its overloads.
        bool has_infix_case();

        // Returns the parsing state machine associated with this form, reset to
        // the starting position.
        // PANICS if this form is not invokable. Please check this before
        // calling!
        rc<StateMachine> start();

        bool operator==(const Form& other) const;
        bool operator!=(const Form& other) const;
        u64 hash() const;
    };

    bool operator==(rc<Form> a, rc<Form> b);
    bool operator!=(rc<Form> a, rc<Form> b);

    // The form of a single term. Terms represent constants, variables, and all sorts of
    // non-invokable values.
    extern const rc<Form> F_TERM;

    // Constructs a callable form from the provided parameter vector.
    // Panics if neither the first nor second parameters are a keyword, or the parameter
    // list is empty.
    rc<Form> f_callable(i64 precedence, Associativity assoc, const vector<Param>& parameters);
    template<typename ...Args>
    rc<Form> f_callable(i64 precedence, Associativity assoc, Args... args) {
        return f_callable(precedence, assoc, vector_of<Param>(args...));
    }

    // Constructs a callable form from the provided parameter vector, and
    // includes the provided callback.
    // Panics if neither the first nor second parameters are a keyword, the parameter
    // list is empty, or if the callback is null.
    rc<Form> f_callable(i64 precedence, Associativity assoc, FormCallback callback, const vector<Param>& parameters);
    template<typename ...Args>
    rc<Form> f_callable(i64 precedence, Associativity assoc, FormCallback callback, Args... args) {
        return f_callable(precedence, assoc, callback, vector_of<Param>(args...));
    }

    // Constructs a form for an overloaded term from the provided overload vector.
    // Overloads must be callable or overloaded. For provided overloaded forms, 
    // each of the forms within it are added to the new overloaded form, but not
    // the overloaded form itself - in other words, overloaded forms only ever
    // contain callable forms.
    rc<Form> f_overloaded(i64 precedence, Associativity assoc, const vector<rc<Form>>& overloads);
    template<typename ...Args>
    rc<Form> f_overloaded(i64 precedence, Associativity assoc, Args... args) {
        return f_overloaded(precedence, assoc, vector_of<rc<Form>>(args...));
    }

    // Constructs an overloaded form from a list of callables, instead of having to go through rc<Form>.
    rc<Form> f_overloaded(i64 precedence, Associativity assoc, const vector<rc<Callable>>& overloads);

    // Constructs a compound form from the provided subform mapping information.
    // Compound forms are not applicable, so no associativity or precedence is necessary.
    rc<Form> f_compound(const map<Value, rc<Form>>& members);
}

void write(stream& io, const basil::Param& param);
void write(stream& io, const rc<basil::Callable>& callable);
void write(stream& io, const rc<basil::Form>& form);
void write_with_self(stream& io, const basil::Value& self, const rc<basil::Callable>& callable);
void write(stream& io, basil::ParamKind fk);
void write(stream& io, basil::FormKind fk);

#endif