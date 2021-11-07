/*
 * Copyright (c) 2021, the Basil authors
 * All rights reserved.
 *
 * This source code is licensed under the 3-Clause BSD License, the full text
 * of which can be found in the LICENSE file in the root directory
 * of this project.
 */

#ifndef BASIL_TYPE_H
#define BASIL_TYPE_H

#include "util/defs.h"
#include "util/rc.h"
#include "util/option.h"
#include "util/hash.h"
#include "util/vec.h"
#include "jasmine/bc.h"

namespace basil {
    struct Symbol;
    struct Type;

    using Context = jasmine::Context;
    using Repr = jasmine::Type;

    // A Kind represents a "type of types" - the broader class in which a
    // family of types is classified. More fine-grained primitive types like
    // int or float are included as well, since they are usually quite
    // different in structure and operation just as tuples and unions would be.
    enum Kind : u8 {
        K_INT,
        K_FLOAT,
        K_DOUBLE,
        K_SYMBOL,
        K_STRING,
        K_CHAR,
        K_BOOL,
        K_TYPE,
        K_VOID,
        K_ERROR,
        K_UNDEFINED,
        K_FORM_FN,
        K_FORM_ISECT,
        K_ANY,
        K_NAMED,
        K_LIST,
        K_TUPLE,
        K_ARRAY,
        K_UNION,
        K_INTERSECT,
        K_FUNCTION,
        K_STRUCT,
        K_DICT,
        K_MACRO,
        K_ALIAS,
        K_TVAR, 
        K_MODULE,
        K_RUNTIME,
        NUM_KINDS
    };
}

template<>
u64 hash(const basil::Symbol& symbol);

template<>
u64 hash(const basil::Type& type);

template<>
u64 hash(const basil::Kind& kind);

namespace basil {
    // Represents a unique name or identifier. Any two names with the same
    // text data are enforced to have the same id.
    struct Symbol {
        u32 id;

        // Initializes this symbol to the none symbol.
        Symbol();

        bool operator==(Symbol other) const;
        bool operator!=(Symbol other) const;    

        static const Symbol NONE;
    private:
        // Constructs a symbol with the given id. Hidden to force use of
        // symbol_from and constants in lieu of constructing symbols on
        // the fly.
        explicit Symbol(u32 id);

        friend Symbol symbol_from(const ustring& str);
    };

    // DO NOT MODIFY - these aren't const so they can be initialized 
    // nonstatically.
    // These variables represent certain common symbols automatically
    // tracked by the compiler. They should be treated as constants.
    extern Symbol S_NONE,
        S_LPAREN, S_RPAREN, S_LSQUARE, S_RSQUARE, S_LBRACE, S_RBRACE, S_NEWLINE, S_BACKSLASH,
        S_PLUS, S_MINUS, S_COLON, S_TIMES, S_QUOTE, S_ARRAY, S_DICT, S_SPLICE, S_AT, S_LIST,
        S_QUESTION, S_ELLIPSIS, S_COMMA, S_ASSIGN, S_PIPE, S_DO, S_CONS, S_WITH, S_CASE_ARROW, S_OF,
        S_EVAL, S_STREAM, S_WRITE;

    // Returns the associated string for the provided symbol.
    const ustring& string_from(Symbol sym);

    // Returns the existing symbol bound to the given string if it exists,
    // or constructs a new symbol if not.
    Symbol symbol_from(const ustring& str);

    extern const u64 KIND_HASHES[NUM_KINDS];

    // Represents a unique type. Two equal types with the same structure
    // and properties must have the same id.
    struct Type {
        u32 id;

        // Initializes this type to the void type.
        Type();

        // Returns the kind of this type, in the above enum.
        // 
        // Note: this is the "concrete kind" of the type - if this type is a
        // type variable, it will return the kind of the currently bound
        // type to that type variable.
        // 
        // The motivation for this is to allow type variables to be seamlessly
        // used in place of their bound types wherever possible - it's only in
        // exceptional cases that we really need to know if a type is a type
        // variable or not.
        Kind kind() const;

        // Returns the kind of this type, in the above enum.
        Kind true_kind() const;
        
        // Returns whether the kind of this type equals the provided one.
        //
        // Note: like kind(), this is based on the concrete kind of the type.
        bool of(Kind kind) const;

        // Returns true if this type is a type variable.
        bool is_tvar() const;

        // Returns whether this type can coerce to the provided target type.
        bool coerces_to(Type other) const;

        // Returns whether this type can coerce to the provided target type,
        // constrained to JUST generic types - as in, type variables and 'any'.
        //
        // This must be strictly a subset of coerces_to - coerces_to_generic implies
        // coerces_to (and this is implemented in Class::coerces_to).
        //
        // Generally, we use this to implement elementwise subtyping rules that would
        // otherwise be inappropriate due to performance reasons.
        bool coerces_to_generic(Type other) const;

        // Returns whether this type can coerce to the provided target type,
        // but does *not* bind any type variables in the process - type variables
        // are exclusively treated like their base type.
        bool nonbinding_coerces_to(Type other) const;

        // Like 'nonbinding_coerces_to' but for strictly generic coercion.
        bool nonbinding_coerces_to_generic(Type other) const;

        // Formats the type associated with this id to the provided stream.
        void format(stream& io) const;

        // Soft type equality/inequality. Types must be structurally equal
        // for this to return true, but type variables are equal to their
        // bound type.
        // If you want to do strict equality, check is_tvar() and/or make the
        // type concrete first.
        bool operator==(Type other) const;
        bool operator!=(Type other) const;

        // Writes this type's mangled form to the provided stream.
        void write_mangled(stream& io) const;

        // Returns this type's native representation in the Jasmine IR.
        Repr repr(Context& ctx) const;
    private:
        // Similar to symbols, we have a private constructor to directly
        // assemble a type from an id. This is only used internally in
        // t_find.
        explicit Type(u32 id);

        friend Type t_from(u32);
    };

    // DO NOT CALL unless you know what you're doing; constructs a type
    // in an unchecked manner from an id, used internally when creating
    // types.
    Type t_from(u32 id);

    // In general, the following type construction methods will attempt to
    // find any existing type id that matches the given properties. Only if
    // no such existing type exists will a new type be created and new type
    // id be allocated.

    // Constructs a named type from the given name and type.
    Type t_named(Symbol name, Type base);

    // Constructs a named type from the given name and the void type.
    Type t_named(Symbol name);

    // Constructs a list type of the provided element type.
    Type t_list(Type element);

    // Constructs a complete tuple type from the provided element types.
    Type t_tuple(const vector<Type>& elements);
    template<typename ...Args>
    Type t_tuple(Args... args) {
        return basil::t_tuple(vector_of<Type>(args...));
    }

    // Constructs an incomplete tuple type from the provided element types.
    Type t_incomplete_tuple(const vector<Type>& elements);
    template<typename ...Args>
    Type t_incomplete_tuple(Args... args) {
        return basil::t_incomplete_tuple(vector_of<Type>(args...));
    }

    // Constructs an unsized array type for the given element type.
    Type t_array(Type element);

    // Constructs a sized array type from the provided element type and size.
    Type t_array(Type element, u64 size);

    // Constructs a union type with the provided member types.
    Type t_union(const set<Type>& members);
    template<typename ...Args>
    Type t_union(Args... args) {
        return basil::t_union(set_of<Type>(args...));
    }

    // Constructs an intersection type with the provided member types.
    Type t_intersect(const vector<Type>& members);
    template<typename ...Args>
    Type t_intersect(Args... args) {
        return basil::t_intersect(vector_of<Type>(args...));
    }

    // Constructs an undefined function type with the provided arity.
    Type t_form_fn(u32 arity);

    struct Form;

    // Constructs an overloaded type with the provided forms and type members.
    Type t_form_isect(const map<rc<Form>, Type>& overloads);
    template<typename ...Args>
    Type t_form_isect(Args... args) {
        return basil::t_form_isect(map_of<rc<Form>, Type>(args...));
    }

    // Constructs a function type with the given argument and return types.
    Type t_func(Type arg, Type ret);

    // Constructs a macro function type with the given argument and return types.
    Type t_macro(Type arg, Type ret);

    // Constructs a complete struct type with the given fields and associated field types.
    Type t_struct(const map<Symbol, Type>& fields);
    template<typename ...Args>
    Type t_struct(Args... args) {
        return basil::t_struct(map_of<Symbol, Type>(args...));
    }

    // Constructs an incomplete struct type with the given fields and associated field types.
    Type t_incomplete_struct(const map<Symbol, Type>& fields);
    template<typename ...Args>
    Type t_incomplete_struct(Args... args) {
        return basil::t_incomplete_struct(map_of<Symbol, Type>(args...));
    }

    // Constructs a dictionary type with the given key and value types.
    Type t_dict(Type key, Type value);

    // Constructs a dictionary type with the given key type and a void value type.
    Type t_dict(Type key);

    // Returns a fresh type variable.
    Type t_var();

    // Returns a fresh type variable, identified with the provided name.
    Type t_var(Symbol name);

    // Constructs a runtime type with the given base type.
    Type t_runtime(Type base);

    extern Type T_VOID, T_INT, T_FLOAT, T_DOUBLE, T_SYMBOL, 
        T_STRING, T_CHAR, T_BOOL, T_TYPE, T_ALIAS, T_ERROR, T_MODULE,
        T_ANY, T_UNDEFINED;

    // Returns the ith member type of the provided tuple type.
    Type t_tuple_at(Type tuple, u32 i);

    // Returns the number of complete members in the provided tuple type.
    u32 t_tuple_len(Type tuple);

    // Returns whether the provided tuple type is complete.
    bool t_tuple_is_complete(Type tuple);

    // Returns whether the union type 'u' contains the provided member.
    bool t_union_has(Type u, Type member);
    
    // Returns the set of member types from the provided union type.
    set<Type> t_union_members(Type u);

    // Returns the intersection type resulting from adding 'other' to the members of.
    // intersection type 'intersect'.
    Type t_intersect_with(Type intersect, Type other);

    // Returns the intersection type resulting from removing 'other' from the members of.
    // intersection type 'intersect'.
    Type t_intersect_without(Type intersect, Type other);

    // Returns whether the intersect type 'intersect' contains the provided member.
    bool t_intersect_has(Type intersect, Type member);

    // Returns whether the provided intersect type is "procedural" - consisting only
    // of function types.
    bool t_intersect_procedural(Type intersect);

    // Returns the set of member types from the provided intersection type.
    vector<Type> t_intersect_members(Type intersect);

    // Returns the element type of the provided list type.
    Type t_list_element(Type list);

    // Returns the element type of the provided array type.
    Type t_array_element(Type array);

    // Returns the size of a sized array type.
    u32 t_array_size(Type array);

    // Returns whether or not the provided array type is sized.
    bool t_array_is_sized(Type array);

    // Returns the name of the provided named type.
    Symbol t_get_name(Type named);

    // Returns the base type of the provided named type.
    Type t_get_base(Type named);

    // Returns whether the provided struct type is complete.
    bool t_struct_is_complete(Type str);

    // Returns the type of the provided field name within the given struct type.
    Type t_struct_field(Type str, Symbol field);

    // Returns whether the provided field name exists within the given struct type.
    bool t_struct_has(Type str, Symbol field);

    // Returns the number of fields in the provided struct type.
    u32 t_struct_len(Type str);

    // Returns the fields in the provided struct type.
    map<Symbol, Type> t_struct_fields(Type str);

    // Returns the key type of the provided dictionary type.
    Type t_dict_key(Type dict);

    // Returns the value type of the provided dictionary type.
    Type t_dict_value(Type dict);

    // Returns the arity of the provided function or macro type.
    u32 t_arity(Type fn);

    // Returns the argument type of the provided function type.
    Type t_arg(Type fn);

    // Returns the return type of the provided function type.
    Type t_ret(Type fn);

    // Returns whether or not the provided function type is a macro function type.
    bool t_is_macro(Type fn);

    // Returns the best symbol representation of the provided type variable.
    Symbol t_tvar_name(Type tvar);

    // Returns the currently-bound type of the provided type variable.
    Type t_tvar_concrete(Type tvar);

    // Binds the provided tvar to the desired type.
    void t_tvar_bind(Type tvar, Type dest);

    // Returns the arity of the provided form-level function type.
    u32 t_form_fn_arity(Type func);

    // Returns the members of a form-level intersection type.
    map<rc<Form>, Type> t_form_isect_members(Type isect);

    // Returns the type associated with the provided form for the form-level
    // intersection type, or an empty optional if no such type exists.
    optional<Type> t_overload_for(Type f_isect, rc<Form> form);

    // Returns the concrete base of a type variable, or the provided type if
    // it's not a type variable.
    Type t_concrete(Type t);

    // Changes type variable coercion to "intersection mode". In this mode,
    // type variable coercion does not rebind the variable if it coerces.
    // Instead, it builds a set of available coercion targets for the variable.
    // This call and t_tvar_disable_isect can be called in a nested fashion!
    void t_tvar_enable_isect();

    // Turns off type variable "intersection mode" coercion. As a result,
    // any type variables with created sets are bound to intersections of
    // those sets.
    void t_tvar_disable_isect();

    // If the provided type is a runtime type, returns its base; otherwise
    // returns the original type.
    Type t_runtime_base(Type t);

    // Resets the provided type's or any of its type variable members' bindings
    // to undefined.
    void t_unbind(Type t);

    // Returns whether the provided type is concrete - lacking any undefined or
    // 'any' members.
    bool t_is_concrete(Type t);

    // Returns a runtime-compatible form of the provided type, substituting
    // unbound type variables for dynamic types.
    Type t_lower(Type t);

    // Returns whether type a is equal to type b, taking into account type
    // variables. For example, a type variable bound to Int will be equal to Int
    // with this method, but not using operator==.
    bool t_soft_eq(Type a, Type b);

    // Performs initialization for this source file. Sets up the symbol
    // and type tables, and initializes all static variables.
    void init_types_and_symbols();

    // Releases all internal memory being held.
    void free_types();
}

void write(stream& io, const basil::Symbol& symbol);
void write(stream& io, const basil::Kind& type);
void write(stream& io, const basil::Type& type);

#endif