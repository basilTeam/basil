#ifndef BASIL_TYPE_H
#define BASIL_TYPE_H

#include "util/defs.h"
#include "util/rc.h"
#include "util/hash.h"
#include "util/vec.h"

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
        S_QUESTION, S_ELLIPSIS, S_COMMA;

    // Returns the associated string for the provided symbol.
    const ustring& string_from(Symbol sym);

    // Returns the existing symbol bound to the given string if it exists,
    // or constructs a new symbol if not.
    Symbol symbol_from(const ustring& str);

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
        NUM_KINDS
    };

    extern const u64 KIND_HASHES[NUM_KINDS];

    // Represents a unique type. Two equal types with the same structure
    // and properties must have the same id.
    struct Type {
        u32 id;

        // Initializes this type to the void type.
        Type();

        // Returns the kind of this type, in the above enum.
        Kind kind() const;
        
        // Returns whether the kind of this type equals the provided one.
        bool of(Kind kind) const;

        // Returns whether this type can coerce to the provided target type.
        bool coerces_to(Type other) const;

        // Formats the type associated with this id to the provided stream.
        void format(stream& io) const;

        bool operator==(Type other) const;
        bool operator!=(Type other) const;
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
    Type t_intersect(const set<Type>& members);
    template<typename ...Args>
    Type t_intersect(Args... args) {
        return basil::t_intersect(set_of<Type>(args...));
    }

    // Constructs a function type with the given argument and return types.
    Type t_func(Type arg, Type ret);

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

    // Constructs a macro type with the given arity.
    Type t_macro(int arity);

    extern Type T_VOID, T_INT, T_FLOAT, T_DOUBLE, T_SYMBOL, 
        T_STRING, T_CHAR, T_BOOL, T_TYPE, T_ALIAS, T_ERROR, 
        T_ANY, T_UNDEFINED;

    // Returns the ith member type of the provided tuple type.
    Type t_tuple_at(Type tuple, u32 i);

    // Returns the number of complete members in the provided tuple type.
    u32 t_tuple_len(Type tuple);

    // Returns whether the provided tuple type is complete.
    bool t_tuple_is_complete(Type tuple);

    // Returns whether the union type 'u' contains the provided member.
    bool t_union_has(Type u, Type member);

    // Returns whether the intersect type 'intersect' contains the provided member.
    bool t_intersect_has(Type intersect, Type member);

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
    
    // Performs initialization for this source file. Sets up the symbol
    // and type tables, and initializes all static variables.
    void init_types_and_symbols();

    // Releases all internal memory being held.
    void free_types();
}

u64 hash(const basil::Symbol& symbol);
u64 hash(const basil::Type& type);
u64 hash(const basil::Kind& kind);

void write(stream& io, const basil::Symbol& symbol);
void write(stream& io, const basil::Type& type);

#endif