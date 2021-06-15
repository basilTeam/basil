#ifndef BASIL_FORM_H
#define BASIL_FORM_H

#include "util/vec.h"
#include "util/option.h"
#include "type.h"

namespace basil {
    struct Env;
    struct Value;

    enum FormKind {
        FK_TERM,
        FK_CALLABLE,
        FK_OVERLOADED,
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
        PK_QUOTED, // Parameter can bind to any single term, with grouping, but skips evaluation.
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

    // Since all variables and terms are the same, we use
    // these constants instead of constructing them.
    extern const Param P_VAR, // A variable parameter.
                       P_QUOTED, // A quoted parameter.
                       P_TERM, // A term parameter.
                       P_SELF; // The self parameter.

    // Constructs a keyword parameter.
    Param p_keyword(Symbol name);

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
    private:
        // internal state used in the state machine
        u32 index;
        bool stopped;
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
        
        rc<set<Symbol>> mangled; // stores mangled forms of the different overloads to detect duplicates
        optional<bool> has_prefix, has_infix;

        friend optional<rc<Form>> f_overloaded(i64, const vector<rc<Form>>&);
        friend optional<rc<Form>> f_add_overload(rc<Form>, rc<Form>);
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
    };

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
    // Returns a none optional if the resulting overloaded form would be
    // ambiguous. Panics if a non-invokable form is provided.
    optional<rc<Form>> f_overloaded(i64 precedence, Associativity assoc, const vector<rc<Form>>& overloads);
    template<typename ...Args>
    optional<rc<Form>> f_overloaded(i64 precedence, Associativity assoc, Args... args) {
        return f_overloaded(precedence, assoc, vector_of<rc<Form>>(args...));
    }

    // Adds the provided 'addend' form to an existing overloaded form. 
    // Returns a none optional if adding addend would result in ambiguity,
    // and panics if addend is non-invokable. Otherwise, modifies
    // the existing overloaded form and returns its refcell.
    optional<rc<Form>> f_add_overload(rc<Form> existing, rc<Form> addend);
}

void write(stream& io, basil::ParamKind fk);
void write(stream& io, basil::FormKind fk);

#endif