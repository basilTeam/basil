/*
 * Copyright (c) 2021, the Basil authors
 * All rights reserved.
 *
 * This source code is licensed under the 3-Clause BSD License, the full text
 * of which can be found in the LICENSE file in the root directory
 * of this project.
 */

#ifndef BASIL_VALUE_H
#define BASIL_VALUE_H

#include "util/defs.h"
#include "util/utf8.h"
#include "util/ustr.h"
#include "util/rc.h"
#include "util/vec.h"
#include "util/hash.h"
#include "type.h"
#include "source.h"
#include "errors.h"

namespace basil {
    struct FormTuple;
    struct Value;
}

template<>
u64 hash(const basil::FormTuple& forms);

template<>
u64 hash(const basil::Value& value);

namespace basil {
    // Forward declaration.
    struct Builtin;
    struct Form;
    struct Env;
    struct AST;

    struct String;
    struct List;
    struct Tuple;
    struct Array;
    struct Union;
    struct Named;
    struct Struct;
    struct Dict;
    struct Intersect;
    struct Function;
    struct FormFn;
    struct FormIsect;
    struct Alias;
    struct Module;
    struct Runtime;

    // Represents a compile-time value. Values have a few fundamental
    // properties. A value's type describes what kind of data it holds.
    // A value's pos (position) corresponds to the location in the source
    // file that gave rise to that value.
    //
    // We use these values to represent code, where the position corresponds
    // directly to the region the value was read from. As evaluation progresses,
    // the code being evaluated imparts its location to any resulting values it
    // produces.
    struct Value {
        Source::Pos pos;
        Type type;
        rc<Form> form;

        union Data {
            i64 i;                  // A primitive int value, of any bit width.
            float f32;              // A 32-bit primitive float value.
            double f64;             // A 64-bit primitive float value.
            Symbol sym;             // A primitive symbol value.
            Type type;              // A primitive type value.
            rune ch;                // A primitive UTF-8 character value.
            bool b;                 // A primitive boolean value.
            rc<String> string;      // A string value.
            rc<List> list;          // A list value.
            rc<Tuple> tuple;        // A tuple value.
            rc<Array> array;        // An array value.
            rc<Union> u;            // A union value.
            rc<Named> named;        // A named value.
            rc<Struct> str;         // A struct value.
            rc<Dict> dict;          // A dictionary value.
            rc<Intersect> isect;    // An intersection value.
            rc<Module> mod;         // A module value.
            rc<Function> fn;        // A function value.
            rc<Alias> alias;        // An alias value.
            rc<Runtime> rt;         // A runtime value.

            Symbol undefined_sym; // Not used in operations. Stores the variable name associated
                                  // with an undefined value.
            rc<FormFn> fl_fn;
            rc<FormIsect> fl_isect;

            Data(Kind kind);
            Data(Kind kind, const Data& other);
            ~Data();
        } data;

        Value();
        ~Value();
        Value(const Value& other);
        Value(Value&& other);
        Value& operator=(const Value& other);
        Value& operator=(Value&& other);

        // Returns a hashcode for this value.
        u64 hash() const;

        // Writes this value to the provided stream.
        void format(stream& io) const;

        // Deep-copies this value.
        Value clone() const;

        bool operator==(const Value& other) const;
        bool operator!=(const Value& other) const;

        Value& with(rc<Form> form);
    private:
        // Constructs a value with the provided pos and type, but does not initialize
        // the data. Used within value-constructing functions internally, which
        // construct the data immediately after.
        Value(Source::Pos pos, Type type, rc<Form> form);

        friend Value v_int(Source::Pos, i64);
        friend Value v_float(Source::Pos, float);
        friend Value v_double(Source::Pos, double);
        friend Value v_symbol(Source::Pos, Symbol);
        friend Value v_type(Source::Pos, Type);
        friend Value v_char(Source::Pos, rune);
        friend Value v_bool(Source::Pos, bool);
        friend Value v_void(Source::Pos);
        friend Value v_error(Source::Pos);
        friend Value v_undefined(Source::Pos, Symbol, rc<Form>);
        friend Value v_form_fn(Source::Pos, Type, rc<Env>, rc<Form>, const vector<Symbol>&, const Value&);
        friend Value v_form_isect(Source::Pos, Type, rc<Form>, map<rc<Form>, Value>&&);
        friend Value v_string(Source::Pos, const ustring&);
        friend Value v_cons(Source::Pos, Type, const Value&, const Value&);
        friend Value v_list(Source::Pos, Type, vector<Value>&&);
        friend Value v_tuple(Source::Pos, Type, vector<Value>&&);
        friend Value v_array(Source::Pos, Type, vector<Value>&&);
        friend Value v_union(Source::Pos, Type, const Value&);
        friend Value v_named(Source::Pos, Type, const Value&);
        friend Value v_struct(Source::Pos, Type, map<Symbol, Value>&&);
        friend Value v_dict(Source::Pos, Type, map<Value, Value>&&);
        friend Value v_tail(const Value&);
        friend Value v_intersect(Source::Pos, Type, map<Type, Value>&&);
        friend Value v_module(Source::Pos, rc<Env>);
        friend Value v_func(const Builtin&);
        friend Value v_func(Source::Pos, Type, rc<Env>, const vector<Symbol>&, const Value&);
        friend Value v_func(Source::Pos, Symbol, Type, rc<Env>, const vector<Symbol>&, const Value&);
        friend Value v_alias(Source::Pos, const Value&);
        friend Value v_runtime(Source::Pos, Type, rc<AST>);
    };

    // Represents the associated data for a compile-time string.
    struct String {
        ustring data;

        String(const ustring& data);
    };

    // Represents the associated data for a compile-time list.
    struct List {
        Value head;
        rc<List> tail;

        List(const Value& head, rc<List> tail);
    };

    // Represents the associated data for a compile-time tuple.
    struct Tuple {
        vector<Value> members;

        Tuple(const vector<Value>& members);
    };

    // Represents the associated data for a compile-time array.
    struct Array {
        vector<Value> elements;

        Array(const vector<Value>& elements);
    };

    // Represents the associated data for a compile-time union.
    struct Union {
        Value value;

        Union(const Value& value);
    };

    // Represents the associated data for a named value.
    struct Named {
        Value value;

        Named(const Value& value);
    };

    // Represents the associated data for a compile-time struct.
    struct Struct {
        map<Symbol, Value> fields;

        Struct(const map<Symbol, Value>& fields);
    };

    // Represents the associated data for a compile-time dictionary.
    struct Dict {
        map<Value, Value> elements;

        Dict(const map<Value, Value>& elements);
    };

    // Represents the associated data for a compile-time intersection.
    struct Intersect {
        map<Type, Value> values;

        Intersect(const map<Type, Value>& values);
    };

    // Represents the associated data for a compile-time module.
    struct Module {
        rc<Env> env;

        Module(rc<Env> env);
    };

    struct FormTuple {
        u64 hash = 0;
        vector<rc<Form>> forms;

        void compute_hash();
        bool operator==(const FormTuple& other) const;
    };

    // An instantiation of a function for a particular type.
    struct FnInst {
        Type args;
        rc<Env> env;
        rc<AST> func;

        FnInst(Type args, rc<Env> env, rc<AST> func);
    };

    struct InstTable;

    // Instantiates a runtime function body from the given base for the provided arguments.
    rc<FnInst> monomorphize(const Function& fn, InstTable& table, rc<Env> env, rc<Value> base, Type args_type);

    // Stores all the instantiations of a function for a particular
    // form tuple.
    struct InstTable {
        rc<Env> env;
        rc<Value> base;
        map<Type, rc<FnInst>> insts;
        map<Type, u32> is_inst;
        u32 resolving = 0;

        InstTable(rc<Env> local, rc<Value> base_in);

        // Returns true if this function resolution is currently instantiating a function for
        // the provided args_type. If this is the case, we've reached a recursive call within
        // the compiling function, and shouldn't eval it - we'll just do a runtime call to the 
        // function stub instead.
        bool is_instantiating(Type args_type) const;

        // Returns whether or not this function is currently resolving its body form.
        bool is_resolving() const;

        rc<FnInst> inst(const Function& fn, Type args_type);
    };

    struct AbstractFunction {
        rc<Env> env; // the local environment of the function
        vector<Symbol> args; // names of the non-keyword arguments of the function
        Value body; // body expression of the base function (as declared, prior to form resolution)
        map<FormTuple, rc<InstTable>> resolutions; // stores instantiations by form

        AbstractFunction(rc<Env> env, const vector<Symbol>& args, const Value& body);  
    };

    // Represents the associated data for a function.
    struct Function : public AbstractFunction {
        optional<const Builtin&> builtin; // builtin behavior, if necessary
        optional<Symbol> name; // the name of the function, if we can figure it out

        Function(optional<Symbol> name, optional<const Builtin&> builtin, rc<Env> env, const vector<Symbol>& args, const Value& body);
        rc<FnInst> inst(Type args_type, const Value& args);
    };

    // Represents the associated data for an undefined (form-level) function.
    struct FormFn : public AbstractFunction {
        FormFn(rc<Env> env, const vector<Symbol>& args, const Value& body);
    };

    // Represents the associated data for an overloaded value (form-level intersection).
    struct FormIsect {
        map<rc<Form>, Value> overloads;

        FormIsect(const map<rc<Form>, Value>& overloads);
    };

    // Represents the associated data for an alias.
    struct Alias {
        Value term;

        Alias(const Value& term);
    };

    // Represents the associated data for a runtime type.
    struct Runtime {
        rc<AST> ast;

        Runtime(rc<AST> ast);
    };

    // Constructs an integer value.
    Value v_int(Source::Pos pos, i64 i); 

    // Constructs a float value.
    Value v_float(Source::Pos pos, float f);

    // Constructs a double value.
    Value v_double(Source::Pos pos, double d);

    // Constructs a symbol value.
    Value v_symbol(Source::Pos pos, Symbol symbol);

    // Constructs a type value.
    Value v_type(Source::Pos pos, Type type);

    // Constructs a char value.
    Value v_char(Source::Pos pos, rune r);

    // Constructs a bool value.
    Value v_bool(Source::Pos pos, bool b);

    // Constructs a void value.
    Value v_void(Source::Pos pos);

    // Constructs an error value.
    Value v_error(Source::Pos pos);

    // Constructs an error value, also reporting an error at the given position,
    // using the variadic arguments as the message.
    template<typename ...Args>
    Value v_error(Source::Pos pos, const Args&... args) {
        err(pos, format<ustring>(args...));
        return v_error(pos);
    }

    // Constructs an undefined value with the provided form and variable name.
    //
    // Undefined values are used to represent variables that aren't known to have real
    // initial values yet, but have known forms during the form resolution phase.
    Value v_undefined(Source::Pos pos, Symbol name, rc<Form> form);

    // Constructs a form-level function value with the provided attributes.
    Value v_form_fn(Source::Pos pos, Type type, rc<Env> env, rc<Form> form, 
        const vector<Symbol>& args, const Value& body);

    // Constructs an form-level intersection value with the provided attributes.
    Value v_form_isect(Source::Pos pos, Type type, rc<Form> form, map<rc<Form>, Value>&& overloads);
    template<typename ...Args>
    Value v_form_isect(Source::Pos pos, Type type, rc<Form> form, const Args&... args) {
        return basil::v_form_isect(pos, type, form, map_of<rc<Form>, Value>(args...));
    }

    // Constructs a string value.
    Value v_string(Source::Pos pos, const ustring& str);

    // In the following constructors, since we're dealing with composite types, it's
    // possible for type errors to occur. For instance, initializing an int[] with 
    // string values. These constructors check that the provided types and values are
    // being used correctly, asserting coercion rules when necessary.
    //
    // HOWEVER: if any of these type errors occur, the compiler will PANIC! This is
    // because since these are internal methods, we should be taking care of type
    // errors elsewhere in the compiler. If anything goes wrong here, it's assumed
    // that something is wrong internally - that is to say, type errors in the
    // compiled code should be detected before reaching these functions.

    // Constructs a list cell from the given head and tail.
    Value v_cons(Source::Pos pos, Type type, const Value& head, const Value& tail);

    // Constructs a list of the provided values.
    Value v_list(Source::Pos pos, Type type, vector<Value>&& values);
    template<typename ...Args>
    Value v_list(Source::Pos pos, Type type, Args... args) {
        return v_list(pos, type, vector_of<Value>(args...));
    }

    // Constructs a tuple of the provided values.
    Value v_tuple(Source::Pos pos, Type type, vector<Value>&& values);
    template<typename ...Args>
    Value v_tuple(Source::Pos pos, Type type, Args... args) {
        return v_tuple(pos, type, vector_of<Value>(args...));
    }

    // Constructs an array of the provided values.
    Value v_array(Source::Pos pos, Type type, vector<Value>&& values);
    template<typename ...Args>
    Value v_array(Source::Pos pos, Type type, const Args&... args) {
        return v_array(pos, type, vector_of<Value>(args...));
    }

    // Constructs a value of a union type.
    Value v_union(Source::Pos pos, Type type, const Value& value);
    
    // Constructs a value of a named type.
    Value v_named(Source::Pos pos, Type type, const Value& value);

    // Constructs a value of a struct type.
    Value v_struct(Source::Pos pos, Type type, map<Symbol, Value>&& fields);
    template<typename ...Args>
    Value v_struct(Source::Pos pos, Type type, const Args&... args) {
        return v_struct(pos, type, map_of<Symbol, Value>(args...));
    }

    // Constructs a value of a dictionary type.
    Value v_dict(Source::Pos pos, Type type, map<Value, Value>&& entries);
    template<typename ...Args>
    Value v_dict(Source::Pos pos, Type type, const Args&... args) {
        return v_dict(pos, type, map_of<Value, Value>(args...));
    }

    // Constructs a value of an intersection type.
    Value v_intersect(Source::Pos pos, Type type, map<Type, Value>&& values);
    template<typename ...Args>
    Value v_intersect(Source::Pos pos, Type type, const Args&... args) {
        return v_intersect(pos, type, map_of<Type, Value>(args...));
    }

    // Constructs an intersection value from a number of builtins.
    Value v_intersect(vector<Builtin*>&& builtins);
    template<typename ...Args>
    Value v_intersect(const Args&... args) {
        return v_intersect(vector_of<Builtin*>(args...));
    }

    // Constructs a module from an environment.
    Value v_module(Source::Pos pos, rc<Env> env);

    // Constructs a function value from a builtin.
    Value v_func(const Builtin& builtin);

    // Constructs a function value from a body and env.
    Value v_func(Source::Pos pos, Type type, rc<Env> env, const vector<Symbol>& args, const Value& body);

    // Constructs a named function value from a body and env.
    Value v_func(Source::Pos pos, Symbol name, Type type, rc<Env> env, const vector<Symbol>& args, const Value& body);

    // Constructs an alias value from a term.
    Value v_alias(Source::Pos pos, const Value& term);

    // Constructs a runtime value from an AST node.
    Value v_runtime(Source::Pos pos, Type type, rc<AST> ast);

    // Constant iterator for traversing list values.
    struct const_list_iterator {
        const List* list;

        const_list_iterator(const List* l);
        const_list_iterator(const rc<List> l);

        const Value& operator*() const;
        const Value* operator->() const;
        bool operator==(const const_list_iterator& other) const;
        bool operator!=(const const_list_iterator& other) const;
        const_list_iterator& operator++();
        const_list_iterator operator++(int);
    };

    // Iterator for traversing list values.
    struct list_iterator {
        List* list;

        list_iterator(List* l);
        list_iterator(rc<List> l);

        Value& operator*();
        Value* operator->();
        bool operator==(const list_iterator& other) const;
        bool operator!=(const list_iterator& other) const;
        list_iterator& operator++();
        list_iterator operator++(int);
        operator const_list_iterator() const;
    };

    // A non-owning iterable object that allows traversal of a list.
    struct list_iterable {
        List* list;

        list_iterable(rc<List> l);

        list_iterator begin();
        list_iterator end();
        const_list_iterator begin() const;
        const_list_iterator end() const;
    };

    // Produces an iterable object that allows traversal of a list value.
    // Provided value must be a list!
    list_iterable iter_list(const Value& v);

    // The following functions allow compile-time type inference, determining a reasonable
    // type that can contain all of the provided values. In general, these are relatively
    // safe to call - the only error case is in the binary infer_list overload. Since they
    // are meant to be used at compile-time, incompatible element types simply return a
    // generic any-typed result - essentially deferring to dynamic typing.
    //
    // Don't worry about type errors in compiled code though - each instance of the any type
    // becomes a type variable when lowered, so heterogeneous elements in a homogeneous
    // container like an array will result in conflicting type variable bindings as we
    // transition to code generation.

    // Infers a list type that can represent the provided head and tail. Returns
    // error type if inference is impossible (i.e. when tail is not list or void).
    Type infer_list(const Value& head, const Value& tail);

    // Infers a list type that can represent all of the provided values.
    Type infer_list(const vector<Value>& values);

    // Infers the most precise tuple type that can represent all of the provided values.
    Type infer_tuple(const vector<Value>& values);

    // Infers the most precise array type that can represent all of the provided values.
    Type infer_array(const vector<Value>& values);

    // Infers the most precise struct type that can represent all of the provided fields.
    Type infer_struct(const map<Symbol, Value>& fields);

    // Infers the most precise dictionary type that can represent all of the provided entries.
    Type infer_dict(const map<Value, Value>& entries);

    // Finally, the following functions permit some basic operations over values, generally
    // for convenience in implementing the compiler itself. We don't include things like
    // arithmetic or logic or other primitives here, since they are generally only useful
    // as builtins. But things like list manipulation, comparison, and pattern matching
    // that would be handy for parsing and evaluation purposes are welcome.
    //
    // As in the constructor functions, since these are intended for internal compiler use,
    // any type errors will result in a PANIC! Please check your types BEFORE calling any
    // of these.

    // Returns the head of a list value.
    const Value& v_head(const Value& list);
    Value& v_head(Value& list);

    // Updates the head of a list value.
    void v_set_head(Value& list, const Value& v);

    // Returns the tail of a list value.
    Value v_tail(const Value& list);

    // Returns the length of a list value. Be careful - this requires linear traversal!
    u32 v_list_len(const Value& list);

    // Returns whether a value is the empty list.
    // Note: this is essentially equivalent to v.type.of(K_VOID). But it's more expressive.
    bool is_empty(const Value& v);

    // Maps a function over the provided list value, returning a new list.
    template<typename Func>
    Value v_map_list(const Func& func, const Value& list) {
        vector<Value> acc;
        for (const Value& v : iter_list(list)) acc.push(func(v));
        return v_list(list.pos, infer_list(acc), acc);
    }

    // Filters the provided list through a predicate, returning a new list.
    template<typename Pred>
    Value v_filter_list(const Pred& pred, const Value& list) {
        vector<Value> acc;
        for (const Value& v : iter_list(list)) if (pred(v)) acc.push(v);
        return v_list(list.pos, infer_list(acc), acc);
    }

    // Folds the provided list left with the provided initial value and function.
    template<typename Func>
    Value v_fold_list(const Func& func, const Value& initial, const Value& list) {
        Value acc = initial;
        for (const Value& v : iter_list(list)) acc = func(acc, v);
        return acc;
    }

    // Returns the length of the provided tuple value.
    u32 v_tuple_len(const Value& tuple);

    // Returns the ith element of the provided tuple value.
    const Value& v_tuple_at(const Value& tuple, u32 i);

    // Sets the ith element of the provided tuple value.
    void v_tuple_set(Value& tuple, u32 i, const Value& v);

    // Returns the element vector of the provided tuple value.
    const vector<Value>& v_tuple_elements(const Value& tuple);

    // Returns the length of the provided array value.
    u32 v_array_len(const Value& array);

    // Returns the ith element of the provided array value.
    const Value& v_array_at(const Value& array, u32 i);

    // Sets the ith element of the provided array value.
    void v_array_set(Value& array, u32 i, const Value& v);

    // Returns the element vector of the provided array value.
    const vector<Value>& v_array_elements(const Value& array);

    // Returns the current member type of the provided union value.
    Type v_cur_type(const Value& u);

    // Returns the current member of the provided union value.
    const Value& v_current(const Value& u);

    // Sets the current member of the provided union value.
    void v_set_current(Value& u, const Value& v);

    // Returns the name of the provided named value.
    Symbol v_get_name(const Value& named);

    // Returns the base value of the provided named value.
    const Value& v_get_base(const Value& named);

    // Returns whether the provided struct value contains the given field.
    bool v_struct_has(const Value& str, Symbol field);

    // Returns the value associated with the given field in the provided struct.
    const Value& v_struct_at(const Value& str, Symbol field);

    // Sets the value associated with the given field in the provided struct.
    void v_struct_set(Value& str, Symbol field, const Value& v);

    // Returns the number of fields in the provided struct.
    u32 v_struct_len(const Value& str);

    // Returns the underlying symbol -> value map of the provided struct.
    const map<Symbol, Value>& v_struct_fields(const Value& str);

    // Returns whether the provided dictionary value contains the given key.
    bool v_dict_has(const Value& dict, const Value& key);

    // Returns the value associated with the given key in the provided dict.
    const Value& v_dict_at(const Value& dict, const Value& key);

    // Associates the provided key and value together in the given dict.
    void v_dict_put(Value& dict, const Value& key, const Value& value);

    // Removes the key and associated value from the provided dict.
    void v_dict_erase(Value& dict, const Value& key);

    // Returns the number of elements in the provided dict.
    u32 v_dict_len(const Value& dict);

    // Returns the underlying value -> value map of the provided dict.
    const map<Value, Value>& v_dict_elements(const Value& dict);

    // Delegates to v_tuple_len, v_dict_len, v_array_len, or v_struct_len
    // depending on the type of 'v'.
    u32 v_len(const Value& v);

    // Delegates to v_tuple_at, v_dict_at, v_struct_at, or v_array_at 
    // depending on the type of 'v'.
    Value v_at(const Value& v, const Value& key);

    // Delegates to v_tuple_at or v_array_at depending on the type of 'v'.
    Value v_at(const Value& v, u32 i);

    // Attempts to lower a value known at compile-time to a runtime type, or returns an
    // error value if the value is invalid at runtime.
    // Will not do any type coercion, so ensure that the 'src' provided is already of the
    // exact correct type.
    Value lower(rc<Env> env, const Value& src);

    // Coerces a value to the given target type, or returns an error value if coercion
    // is impossible.
    // Should not return an error if src.type.coerces_to(target) returns true.
    Value coerce(rc<Env> env, const Value& src, Type target);

    // Returns the most appropriate body term for the function, given the provided
    // arguments. Specifically, this handles things like avoiding duplicate form
    // resolution.
    rc<InstTable> v_resolve_body(AbstractFunction& fn, const Value& args);
    rc<InstTable> v_resolve_body(AbstractFunction& fn, const vector<rc<Form>>& args);
}

void write(stream& io, const basil::Value& value);

#endif