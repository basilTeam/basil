/*
 * Copyright (c) 2021, the Basil authors
 * All rights reserved.
 *
 * This source code is licensed under the 3-Clause BSD License, the full text
 * of which can be found in the LICENSE file in the root directory
 * of this project.
 */

#include "type.h"
#include "forms.h"

namespace basil {
    struct Class;
}

template<>
u64 hash(const rc<basil::Class>& tclass);

void write(stream& io, const rc<basil::Class>& tclass);

namespace basil {
    Symbol::Symbol(): id(0) {}

    bool Symbol::operator==(Symbol other) const {
        return id == other.id;
    }

    bool Symbol::operator!=(Symbol other) const {
        return id != other.id;
    }   

    Symbol::Symbol(u32 id_in): id(id_in) {}

    static map<ustring, u32> SYMBOL_MAP;
    static vector<ustring> SYMBOL_LIST;

    const ustring& string_from(Symbol sym) {
        return SYMBOL_LIST[sym.id];
    }

    Symbol symbol_from(const ustring& str) {
        auto it = SYMBOL_MAP.find(str);
        if (it != SYMBOL_MAP.end()) {
            return Symbol(it->second); // Return existing symbol
        }
        else {
            Symbol result(SYMBOL_LIST.size()); // Construct new symbol with next-highest id.
            SYMBOL_MAP[str] = result.id;
            SYMBOL_LIST.push(str);
            return result;
        }
    }

    Symbol S_NONE,
        S_LPAREN, S_RPAREN, S_LSQUARE, S_RSQUARE, S_LBRACE, S_RBRACE, S_NEWLINE, S_BACKSLASH,
        S_PLUS, S_MINUS, S_COLON, S_TIMES, S_QUOTE, S_ARRAY, S_DICT, S_SPLICE, S_AT, S_LIST,
        S_QUESTION, S_ELLIPSIS, S_COMMA, S_ASSIGN, S_PIPE, S_DO, S_CONS, S_WITH, S_CASE_ARROW, S_OF,
        S_EVAL, S_STREAM, S_WRITE;

    void init_symbols() {
        S_NONE = symbol_from("");
        S_LPAREN = symbol_from("(");
        S_RPAREN = symbol_from(")");
        S_LSQUARE = symbol_from("[");
        S_RSQUARE = symbol_from("]");
        S_LBRACE = symbol_from("{");
        S_RBRACE = symbol_from("}");
        S_NEWLINE = symbol_from("\n");
        S_BACKSLASH = symbol_from("\\");
        S_PLUS = symbol_from("+");
        S_MINUS = symbol_from("-"); 
        S_COLON = symbol_from(":");
        S_TIMES = symbol_from("*");
        S_QUOTE = symbol_from("quote");
        S_ARRAY = symbol_from("array");
        S_DICT = symbol_from("dict");
        S_SPLICE = symbol_from("splice");
        S_AT = symbol_from("at");
        S_LIST = symbol_from("list");
        S_QUESTION = symbol_from("?");
        S_ELLIPSIS = symbol_from("...");
        S_COMMA = symbol_from(",");
        S_ASSIGN = symbol_from("=");
        S_PIPE = symbol_from("|");
        S_DO = symbol_from("do");
        S_CONS = symbol_from("::");
        S_WITH = symbol_from("with");
        S_CASE_ARROW = symbol_from("=>");
        S_OF = symbol_from("of");
        S_EVAL = symbol_from("eval");
        S_STREAM = symbol_from("Stream");
        S_WRITE = symbol_from("write");
    }

    const char* KIND_NAMES[NUM_KINDS] = {
        "int",
        "float",
        "double",
        "symbol",
        "string",
        "char",
        "bool",
        "type",
        "void",
        "error",
        "undefined",
        "form-level function",
        "form-level intersect",
        "any",
        "named",
        "list",
        "tuple",
        "array",
        "union",
        "intersect",
        "function",
        "struct",
        "dict",
        "macro",
        "alias",
        "tvar", 
        "module",
        "runtime"
    };

    const u64 KIND_HASHES[NUM_KINDS] = {
        17611011710004237389ul,
        6730409401287790033ul,
        8749129017535518397ul,
        2347621762901089247ul,
        17740938897638896553ul,
        5426840037560560943ul,
        16755173331346678897ul,
        1718972122632748549ul,
        18010413155395840397ul,
        1390713874379805383ul,
        75251982808456021ul,
        2104018774235203543ul,
        18377143599403925159ul,
        11722889889822411841ul,
        14799943271302886699ul,
        11716327447522562003ul,
        4972894215258340103ul,
        5625416075860148053ul,
        7475917240723778177ul,
        2948583097529606413ul,
        14239964922572717219ul,
        14100517225124763857ul,
        3843382840898873837ul,
        9920235303098296457ul,
        7206390394945354127ul
    };

    struct Class {
        u64 cached_hash = 0;
        bool hashed = false;
        Kind _kind;
        u32 _id;

        Class(Kind kind): _kind(kind), _id(0) {}
        virtual ~Class() {}

        Kind kind() const {
            return _kind;
        }

        u64 hash() const {
            if (!hashed) { // terrifying const hax to preserve constness elsewhere
                ((Class*)this)->hashed = true;
                ((Class*)this)->cached_hash = lazy_hash();
            }
            return cached_hash;
        }

        virtual u64 lazy_hash() const = 0;
        virtual void format(stream& io) const = 0;

        virtual bool coerces_to(const Class& other) const;
        virtual bool coerces_to_generic(const Class& other) const;

        virtual bool operator==(const Class& other) const = 0;
        virtual void write_mangled(stream& io) const = 0;

        u32 id() const {
            return _id;
        }

        bool operator!=(const Class& other) const {
            return !operator==(other);
        }
    };

    static bool operator==(const rc<Class>& a, const rc<Class>& b) {
        return *a == *b;
    }

    static bool operator!=(const rc<Class>& a, const rc<Class>& b) {
        return *a != *b;
    }

    static map<rc<Class>, u32> TYPE_MAP;
    static vector<rc<Class>> TYPE_LIST;

    Type::Type(): id(0) {}

    Kind Type::kind() const {
        Kind k = TYPE_LIST[id]->kind();

        Type t = *this;
        while (k == K_TVAR) { // look at underlying type until we find a non-tvar
            t = t_tvar_concrete(t);
            k = TYPE_LIST[t.id]->kind();
        }
        return k;
    }

    Kind Type::true_kind() const {
        return TYPE_LIST[id]->kind();
    }

    bool Type::of(Kind kind) const {
        return this->kind() == kind;
    }

    bool Type::is_tvar() const {
        return TYPE_LIST[id]->kind() == K_TVAR;
    }

    void Type::format(stream& io) const {
        TYPE_LIST[id]->format(io);
    }

    bool Type::coerces_to(Type other) const {
        return TYPE_LIST[id]->coerces_to(*TYPE_LIST[other.id]);
    }

    bool Type::coerces_to_generic(Type other) const {
        return TYPE_LIST[id]->coerces_to_generic(*TYPE_LIST[other.id]);
    }

    void Type::write_mangled(stream& io) const {
        return TYPE_LIST[id]->write_mangled(io);
    }

    static u32 nonbinding = 0;

    // to avoid reimplementing coerces_to and coerces_to_generic (there's already
    // more than enough duplication between those two...) in nonbinding forms, we
    // instead track the depth of nonbinding calls using this static variable,
    // and refer to it within TVarClass::coerces_to.

    bool Type::nonbinding_coerces_to(Type other) const {
        nonbinding ++;
        bool result = coerces_to(other);
        nonbinding --;
        return result;
    }

    bool Type::nonbinding_coerces_to_generic(Type other) const {
        nonbinding ++;
        bool result = coerces_to_generic(other);
        nonbinding --;
        return result;
    }

    bool t_soft_eq(Type a, Type b) {
        if (a.is_tvar()) a = t_concrete(a);
        if (b.is_tvar()) b = t_concrete(b);
        return a.id == b.id; // we can do reference equality after eliminating tvars
    }

    static bool t_soft_eq(const Class& a, const Class& b) {
        return t_soft_eq(t_from(a.id()), t_from(b.id()));
    }

    static void t_dedup(vector<rc<Class>>& types) {
        for (u32 i = 0; i < types.size(); i ++) {
            for (u32 j = i + 1; j < types.size(); j ++) {
                if (t_soft_eq(*types[i], *types[j])) {
                    types[j --] = types.back(), types.pop();
                    continue;
                }
            }
        }
    }

    bool Type::operator==(Type other) const {
        return id == other.id;
    }

    bool Type::operator!=(Type other) const {
        return id != other.id;
    }

    Repr Type::repr(Context& ctx) const {
        return jasmine::I64;
    }

    Type::Type(u32 id_in): id(id_in) {}

    Type t_from(u32 id) {
        if (id > TYPE_LIST.size()) panic("Type id exceeds bounds of type table! This is probably bad news...");
        return Type(id);
    }

    Type t_create(rc<Class> newtype) {
        auto it = TYPE_MAP.find(newtype);
        if (it != TYPE_MAP.end()) {
            return t_from(it->second); // Return existing type id.
        }
        else {
            auto result = t_from(TYPE_LIST.size());
            TYPE_MAP[newtype] = result.id;
            TYPE_LIST.push(newtype);
            newtype->_id = result.id;
            return result;
        }
    }
    
    template<typename T>
    T& as(Class& tc) {
        return (T&)tc;
    }

    template<typename T>
    const T& as(const Class& tc) {
        return (const T&)tc;
    }

    // Represents unitary types that have no members and no special coercion rules.
    struct SingletonClass : public Class {
        const char *name, *mangle;

        SingletonClass(Kind kind, const char* name_in, const char* mangle_in): 
            Class(kind), name(name_in), mangle(mangle_in) {}

        u64 lazy_hash() const override {
            return raw_hash(kind());
        }

        void format(stream& io) const override {
            write(io, name);
        }

        void write_mangled(stream& io) const override {
            write(io, mangle);
        }

        bool operator==(const Class& other) const override {
            return kind() == other.kind();
        }
    };

    // Encompasses different sizes of int and floating-point types.
    struct NumberClass : public Class {
        const char *name, *mangle;
        bool floating;
        u32 size;

        NumberClass(Kind kind, const char* name_in, const char* mangle_in, bool floating_in, u32 size_in_bytes): 
            Class(kind), name(name_in), mangle(mangle_in), floating(floating_in), size(size_in_bytes) {}

        u64 lazy_hash() const override {
            return raw_hash(kind()) * 544921574335967683ul ^ raw_hash(size) * 4508536729671157399ul ^ raw_hash(floating);
        };

        void format(stream& io) const override {
            write(io, name);
        }

        void write_mangled(stream& io) const override {
            write(io, mangle);
        }

        bool coerces_to(const Class& other) const override {
            if (Class::coerces_to(other)) return true;
            if (other.kind() == K_INT || other.kind() == K_FLOAT || other.kind() == K_DOUBLE) {
                if (kind() == K_FLOAT && other.kind() == K_DOUBLE) return true; // floats can convert to double
                else if (!floating && as<NumberClass>(other).floating) return true; // ints can convert to floats
                else if (!floating && !as<NumberClass>(other).floating && size < as<NumberClass>(other).size) return true; // ints can convert to larger ints
                // otherwise fallthrough
            }
            return false;
        }

        bool operator==(const Class& other) const override {
            return (other.kind() == K_INT || other.kind() == K_FLOAT || other.kind() == K_DOUBLE)
                && as<NumberClass>(other).floating == floating
                && as<NumberClass>(other).size == size;
        }
    };

    // Represents the void type.
    struct VoidClass : public SingletonClass {
        VoidClass(Kind kind, const char* name): SingletonClass(kind, name, "v") {}

        bool coerces_to_generic(const Class& other) const override {
            return Class::coerces_to_generic(other) || other.kind() == K_LIST;
        }
    };

    // Represents the undefined type.
    struct UndefinedClass : public SingletonClass {
        UndefinedClass(Kind kind, const char* name): SingletonClass(kind, name, "u") {}

        bool coerces_to_generic(const Class& other) const override {
            return true; // undefined can coerce to all other types
        }
    };

    // Represents named types.
    struct NamedClass : public Class {
        Symbol name;
        rc<Class> base;

        NamedClass(Symbol name_in, Type base_in):
            Class(K_NAMED), name(name_in), base(TYPE_LIST[base_in.id]) {}

        u64 lazy_hash() const override {
            return base->hash() * 10762593286530943657ul ^ raw_hash(name) * 11858331803272376449ul ^ raw_hash(K_NAMED);
        }

        void format(stream& io) const override {
            write(io, name, " of ", base);
        }

        void write_mangled(stream& io) const override {
            write(io, 'N', string_from(name).size(), name);
            base->write_mangled(io);
        }

        bool coerces_to_generic(const Class& other) const override {
            if (Class::coerces_to_generic(other)) return true;

            if (other.kind() == K_NAMED) {
                return as<NamedClass>(other).name == name  
                    && base->coerces_to_generic(*as<NamedClass>(other).base);
            }

            return false;
        }

        bool coerces_to(const Class& other) const override {
            if (Class::coerces_to(other)) return true;

            if (other.kind() == K_TYPE) {
                return base->coerces_to(*TYPE_LIST[T_TYPE.id]);
            }

            return false;
        }

        bool operator==(const Class& other) const override {
            return other.kind() == K_NAMED 
                && name == as<NamedClass>(other).name 
                && *base == *as<NamedClass>(other).base;
        }
    };

    Type t_named(Symbol name, Type base) {
        return t_create(ref<NamedClass>(name, base));
    }

    Type t_named(Symbol name) {
        return t_named(name, T_VOID);
    }

    // Represents a list type.
    struct ListClass : public Class {
        rc<Class> element;

        ListClass(Type element_in): 
            Class(K_LIST), element(TYPE_LIST[element_in.id]) {}

        u64 lazy_hash() const override {
            return element->hash() * 6769132636657327813ul ^ raw_hash(kind());
        }

        void format(stream& io) const override {
            write(io, "[", element, "]");
        }

        void write_mangled(stream& io) const override {
            write(io, 'L');
            element->write_mangled(io);
        }

        bool coerces_to_generic(const Class& other) const override {
            if (Class::coerces_to_generic(other)) return true;

            if (other.kind() == K_LIST) {
                return element->coerces_to_generic(*as<ListClass>(other).element);
            }
            
            return false;
        }

        bool coerces_to(const Class& other) const override {
            if (Class::coerces_to(other)) return true;
            
            if (other.kind() == K_TYPE) {
                return element->coerces_to(*TYPE_LIST[T_TYPE.id]); // [type] can convert to type
            }
            
            return false;
        }

        bool operator==(const Class& other) const override {
            return other.kind() == K_LIST && *element == *as<ListClass>(other).element;
        }
    };

    Type t_list(Type element) {
        return t_create(ref<ListClass>(element));
    }

    // Represents tuple types.
    struct TupleClass : public Class {
        vector<rc<Class>> members;
        bool incomplete;
        
        TupleClass(const vector<Type>& members_in, bool incomplete_in): 
            Class(K_TUPLE), incomplete(incomplete_in) {
            for (Type t : members_in) members.push(TYPE_LIST[t.id]);
        }

        u64 lazy_hash() const override {
            u64 base = raw_hash(kind());
            if (incomplete) base ^= 10347714113816317481ul;
            for (const auto& t : members) {
                base ^= t->hash();
                base *= 5448056203459931801ul;
            }
            return base;
        }

        void format(stream& io) const override {
            write(io, "(");
            bool first = true;
            for (auto t : members) {
                if (!first) write(io, ", ");
                first = false;
                t->format(io);
            }
            if (incomplete) {
                if (!first) write(io, ", ");
                write(io, "...");
            }
            write(io, ")");
        }

        void write_mangled(stream& io) const override {
            write(io, 'T', members.size());
            for (rc<Class> element : members) element->write_mangled(io);
        }

        bool coerces_to_generic(const Class& other) const override {
            if (Class::coerces_to_generic(other)) return true;

            if (other.kind() == K_TUPLE) {
                const TupleClass& tc = as<TupleClass>(other);
                if (incomplete && !tc.incomplete) return false; // Can't go from incomplete to complete tuple.
                for (u32 i = 0; i < members.size(); i ++) {
                    if (i >= tc.members.size()) 
                        return tc.incomplete; // If target is smaller, we can only coerce if it's incomplete.

                    if (!members[i]->coerces_to_generic(*tc.members[i]))
                        return false; // Fail if members aren't convertible to the target members.
                }
                return members.size() == tc.members.size(); // Can't convert to tuple with more complete members.
            }

            return false;
        }

        bool coerces_to(const Class& other) const override {
            if (Class::coerces_to(other)) return true;

            if (other.kind() == K_TUPLE) {
                const TupleClass& tc = as<TupleClass>(other);
                if (incomplete && !tc.incomplete) return false; // Can't go from incomplete to complete tuple.
                for (u32 i = 0; i < members.size(); i ++) {
                    if (i >= tc.members.size()) 
                        return tc.incomplete; // If target is smaller, we can only coerce if it's incomplete.

                    if (!members[i]->coerces_to(*tc.members[i]))
                        return false; // Fail if members aren't convertible to the target members.
                }
                return members.size() == tc.members.size(); // Can't convert to tuple with more complete members.
            }

            if (other.kind() == K_TYPE) {
                for (u32 i = 0; i < members.size(); i ++) {
                    if (!members[i]->coerces_to(*TYPE_LIST[T_TYPE.id])) return false;
                }

                return true; // A tuple of only type elements can convert to 'type'.
            }

            return false;
        }

        bool operator==(const Class& other) const override {
            if (other.kind() != K_TUPLE
                || as<TupleClass>(other).members.size() != members.size()
                || as<TupleClass>(other).incomplete != incomplete)
                return false;
            // At this point, we know that the other class is a tuple with the same number of
            // members.
            for (u32 i = 0; i < members.size(); i ++)
                if (*members[i] != *as<TupleClass>(other).members[i]) return false;

            return true; // All members must have been matched.
        }
    };

    Type t_tuple(const vector<Type>& elements) {
        if (elements.size() < 2)
            panic("Cannot create complete tuple type with less than two members!");
        return t_create(ref<TupleClass>(elements, false));
    }

    Type t_incomplete_tuple(const vector<Type>& elements) {
        return t_create(ref<TupleClass>(elements, true));
    }

    struct ArrayClass : public Class {
        rc<Class> element;
        u64 size;
        bool sized;

        ArrayClass(Type element_in):
            Class(K_ARRAY), element(TYPE_LIST[element_in.id]), size(0), sized(false) {}
        ArrayClass(Type element_in, u64 size_in):
            Class(K_ARRAY), element(TYPE_LIST[element_in.id]), size(size_in), sized(true) {}

        u64 lazy_hash() const override {
            return raw_hash(kind()) ^ element->hash() * 8773895335238318147ul
                ^ (sized ? raw_hash(size) * 8954908842287060251ul : 11485220905872292697ul);
        }

        void format(stream& io) const override {
            write(io, element, "[");
            if (sized) write(io, size);
            write(io, "]");
        }

        void write_mangled(stream& io) const override {
            write(io, 'A');
            if (sized) write(io, size);
            else write(io, 0);
            element->write_mangled(io);
        }

        bool coerces_to_generic(const Class& other) const override {
            if (Class::coerces_to_generic(other)) return true;

            if (other.kind() == K_ARRAY) {
                return element->coerces_to_generic(*as<ArrayClass>(other).element)
                    && (!as<ArrayClass>(other).sized || as<ArrayClass>(other).size == size);
            }

            return false;
        }

        bool coerces_to(const Class& other) const override {
            if (Class::coerces_to(other)) return true;

            if (other.kind() == K_ARRAY) {
                if (*element == *as<ArrayClass>(other).element) // Can convert to unsized array with same element type.
                    return !as<ArrayClass>(other).sized;
            }

            return false;
        }

        bool operator==(const Class& other) const override {
            return other.kind() == K_ARRAY
                && *element == *as<ArrayClass>(other).element
                && sized == as<ArrayClass>(other).sized
                && (!sized || size == as<ArrayClass>(other).size);
        }
    };

    Type t_array(Type element) {
        return t_create(ref<ArrayClass>(element));
    }

    Type t_array(Type element, u64 size) {
        return t_create(ref<ArrayClass>(element, size));
    }

    struct UnionClass : public Class {
        set<rc<Class>> members;

        UnionClass(const set<Type>& members_in):
            Class(K_UNION) {
            for (Type t : members_in) {
                if (t.of(K_UNION)) { // add all members of other unions, preventing nesting
                    for (rc<Class> m : as<UnionClass>(*TYPE_LIST[t.id]).members)
                        members.insert(m);
                }
                else members.insert(TYPE_LIST[t.id]);
            }
        }

        u64 lazy_hash() const override {
            u64 base = raw_hash(kind());
            for (const auto& m : members) {
                base ^= 3958225336639215437ul * m->hash();
            }
            return base;
        }

        void format(stream& io) const override {
            write_seq(io, members, "(", " | ", ")");
        }

        void write_mangled(stream& io) const override {
            write(io, 'U', members.size());
            for (rc<Class> element : members) element->write_mangled(io);
        }

        bool coerces_to_generic(const Class& other) const override {
            if (Class::coerces_to_generic(other)) return true;

            if (other.kind() == K_UNION) {
                auto copy_members = members, 
                    copy_other_members = as<UnionClass>(other).members;
                if (members.size() != copy_members.size()) return false;
                vector<rc<Class>> toRemove;
                for (const auto& m : copy_members) {
                    if (copy_other_members.contains(m)) {
                        copy_other_members.erase(m);
                        toRemove.push(m);
                    }
                }
                for (const auto& m : toRemove) copy_members.erase(m);
                
                // we permit type inference on one member, such as t?|int -> string|int
                // we can't currently infer more than one, given that unions are unordered structures
                if (copy_members.size() == 1 && copy_other_members.size() == 1)
                    return (*copy_members.begin())->coerces_to_generic(**copy_other_members.begin());
            }

            return false;
        }

        bool coerces_to(const Class& other) const override {
            if (Class::coerces_to(other)) return true;

            if (other.kind() == K_UNION) {
                auto copy_members = members;
                for (const auto& m : as<UnionClass>(other).members) {
                    copy_members.erase(m);
                }
                return copy_members.size() == 0; // Coercion possible if target's members are a superset of ours.
            }

            return false;
        }

        bool operator==(const Class& other) const override {
            if (other.kind() != K_UNION
                || as<UnionClass>(other).members.size() != members.size())
                return false;

            // At this point we know the size of both types' member sets is the same.
            auto copy_members = members;
            for (const auto& m : as<UnionClass>(other).members) {
                copy_members.erase(m);
            }
            return copy_members.size() == 0; // Member sets must have had the same elements.
        }
    };

    Type t_union(const set<Type>& members) {
        if (members.size() < 2) panic("Cannot create union type with less than two members!");
        return t_create(ref<UnionClass>(members));
    }

    struct IntersectionClass : public Class {
        vector<rc<Class>> members;

        IntersectionClass(const vector<Type>& members_in):
            Class(K_INTERSECT) {
            for (Type t : members_in) members.push(TYPE_LIST[t.id]);
            t_dedup(members);
        }

        IntersectionClass(const vector<rc<Class>>& members_in):
            Class(K_INTERSECT), members(members_in) {}

        u64 lazy_hash() const override {
            u64 base = raw_hash(kind());
            for (const auto& m : members) {
                base ^= 16873539230647500721ul * m->hash();
            }
            return base;
        }

        void format(stream& io) const override {
            write_seq(io, members, "(", " & ", ")");
        }

        void write_mangled(stream& io) const override {
            write(io, 'I', members.size());
            for (rc<Class> element : members) element->write_mangled(io);
        }

        bool coerces_to(const Class& other) const override {
            if (Class::coerces_to(other)) return true;

            if (other.kind() == K_INTERSECT) {
                for (const auto& m : as<IntersectionClass>(other).members) {
                    bool found = false;
                    for (const auto& n : members) {
                        if (t_soft_eq(*m, *n)) { // we use soft equality to support type vars
                            found = true;
                            break;
                        }
                    }
                    if (!found) return false; // we don't contain a member of the target
                }
                return true; // we contain every member of the target
            }
            else for (const auto& m : members) {
                if (t_soft_eq(*m, other)) return true; // we contain the target
            }
            return false;
        }

        bool operator==(const Class& other) const override {
            if (other.kind() != K_INTERSECT
                || as<IntersectionClass>(other).members.size() != members.size())
                return false;
            
            // at this point we know the size of both types' member sets is the same.
            for (const auto& m : members) {
                bool found = false;
                for (const auto& n : as<IntersectionClass>(other).members) {
                    if (t_soft_eq(*m, *n)) { // we use soft equality to support type vars
                        found = true;
                        break;
                    }
                }
                if (!found) return false;
            }
            return true;
        }
    };

    Type t_intersect(const vector<Type>& members) {
        if (members.size() < 1) panic("Cannot create intersection type with zero members!");
        return t_create(ref<IntersectionClass>(members));
    }

    struct FunctionClass : public Class {
        rc<Class> arg, ret;
        bool is_macro;

        FunctionClass(Type arg_in, Type ret_in, bool is_macro_in):
            Class(K_FUNCTION), arg(TYPE_LIST[arg_in.id]), ret(TYPE_LIST[ret_in.id]), is_macro(is_macro_in) {}

        u64 lazy_hash() const override {
            return raw_hash(kind()) ^ arg->hash() * 4858037243276500399ul 
                ^ ret->hash() * 16668975004056768077ul
                ^ raw_hash(is_macro);
        }

        void format(stream& io) const override {
            write(io, arg, " -", is_macro ? "macro" : "", "> ", ret);
        }

        void write_mangled(stream& io) const override {
            write(io, 'F');
            arg->write_mangled(io);
            ret->write_mangled(io);
        }

        bool coerces_to_generic(const Class& other) const override {
            if (Class::coerces_to_generic(other)) return true;

            if (other.kind() == K_FUNCTION) {
                return arg->coerces_to_generic(*as<FunctionClass>(other).arg)
                    && ret->coerces_to_generic(*as<FunctionClass>(other).ret)
                    && is_macro == as<FunctionClass>(other).is_macro;
            }

            return false;
        }

        // bool coerces_to(const Class& other) const override {
        //     if (Class::coerces_to(other)) return true;

        //     return false;
        // }

        bool operator==(const Class& other) const override {
            return other.kind() == K_FUNCTION
                && *arg == *as<FunctionClass>(other).arg
                && *ret == *as<FunctionClass>(other).ret
                && is_macro == as<FunctionClass>(other).is_macro;
        }
    };

    Type t_func(Type arg, Type ret) {
        return t_create(ref<FunctionClass>(arg, ret, false));
    }

    Type t_macro(Type arg, Type ret) {
        return t_create(ref<FunctionClass>(arg, ret, true));
    }

    struct StructClass : public Class {
        map<Symbol, rc<Class>> fields;
        bool incomplete;

        StructClass(const map<Symbol, Type>& fields_in, bool incomplete_in):
            Class(K_STRUCT), incomplete(incomplete_in) {
            for (auto [s, t] : fields_in) fields[s] = TYPE_LIST[t.id];
        }

        u64 lazy_hash() const override {
            u64 base = raw_hash(kind());
            if (incomplete) base ^= 6659356980319522183ul;
            for (auto [s, t] : fields) {
                base ^= raw_hash(s) * 515562480546324473ul;
                base ^= t->hash() * 16271366544726016991ul;
            }
            return base;
        }

        void format(stream& io) const override {
            write(io, "{");
            bool first = true;
            for (auto [s, t] : fields) {
                if (!first) write(io, "; ");
                write(io, s, " : ", t);
            }
            if (incomplete) {
                if (!first) write(io, "; ");
                write(io, "...");
            }
            write(io, "}");
        }

        void write_mangled(stream& io) const override {
            write(io, 'S', fields.size());
            for (const auto& [k, v] : fields) {
                write(io, string_from(k).size(), k);
                v->write_mangled(io);
            }
        }

        bool coerces_to(const Class& other) const override {
            if (Class::coerces_to(other)) return true;

            if (other.kind() == K_STRUCT) {
                const StructClass& sc = as<StructClass>(other);
                if (incomplete && !sc.incomplete) return false; // Can't go from incomplete to complete struct.
                if (!incomplete && !sc.incomplete && sc.fields.size() != fields.size())
                    return false; // Can't go from complete struct to smaller complete struct.
                
                for (auto [s, t] : sc.fields) {
                    auto it = fields.find(s);
                    if (it == fields.end() || (*it->second != *t && t->kind() != K_ANY))
                        return false; // Incompatible field in destination.
                }
                return sc.fields.size() < fields.size(); // Can't convert to struct with more complete members.
            }

            return false;
        }

        bool operator==(const Class& other) const override {
            if (other.kind() != K_STRUCT 
                || as<StructClass>(other).fields.size() != fields.size()
                || incomplete != as<StructClass>(other).incomplete)
                return false;
            
            // At this point we know the other struct has the same number of fields, and the same completeness.
            for (auto [s, t] : fields) {
                auto it = as<StructClass>(other).fields.find(s);
                if (it == as<StructClass>(other).fields.end()) return false; // Field not present in other type.
                if (*it->second != *t) return false; // Field present, but has different type.
            }
            return true;
        }
    };

    Type t_struct(const map<Symbol, Type>& fields) {
        return t_create(ref<StructClass>(fields, false));
    }

    Type t_incomplete_struct(const map<Symbol, Type>& fields) {
        return t_create(ref<StructClass>(fields, true));
    }

    struct DictClass : public Class {
        rc<Class> key, value;

        DictClass(Type key_in, Type value_in):
            Class(K_DICT), key(TYPE_LIST[key_in.id]), value(TYPE_LIST[value_in.id]) {}
        
        u64 lazy_hash() const override {
            return raw_hash(kind()) ^ 1785136365411115207ul * key->hash() 
                ^ 14219447378751898973ul * value->hash();
        }

        void format(stream& io) const override {
            write(io, key, "[", value, "]");
        }

        void write_mangled(stream& io) const override {
            write(io, 'D');
            key->write_mangled(io);
            value->write_mangled(io);
        }

        bool coerces_to_generic(const Class& other) const override {
            if (Class::coerces_to_generic(other)) return true;

            if (other.kind() == K_DICT) {
                return key->coerces_to_generic(*as<DictClass>(other).key)
                    && value->coerces_to_generic(*as<DictClass>(other).value);
            }

            return false;
        }

        bool coerces_to(const Class& other) const override {
            if (Class::coerces_to(other)) return true;

            if (other.kind() == K_DICT) {
                if (!key->coerces_to(*as<DictClass>(other).key)) // Key type must be coercible to target key type.
                    return false;
                if (!value->coerces_to(*as<DictClass>(other).value)) // Value type must be coercible to target value type.
                    return false;

                return (key->kind() != K_ANY && as<DictClass>(other).key->kind() == K_ANY) // Permit coercion to more generic dictionary type.
                    || (value->kind() != K_ANY && as<DictClass>(other).value->kind() == K_ANY);
            }

            return false;
        }

        bool operator==(const Class& other) const override {
            return other.kind() == K_DICT 
                && *as<DictClass>(other).key == *key
                && *as<DictClass>(other).value == *value;
        }
    };
    
    Type t_dict(Type key, Type value) {
        return t_create(ref<DictClass>(key, value));
    }
    
    Type t_dict(Type key) {
        return t_dict(key, T_VOID);
    }

    struct MacroClass : public Class {
        i64 arity;

        MacroClass(i64 arity_in): Class(K_MACRO), arity(arity_in) {}

        u64 lazy_hash() const override {
            return raw_hash(kind()) * 5822540408738177351ul ^ raw_hash(arity);
        }

        void format(stream& io) const override {
            write(io, "macro(", arity, ")");
        }

        void write_mangled(stream& io) const override {
            write(io, 'M', arity);
        }

        bool operator==(const Class& other) const override {
            return other.kind() == K_MACRO 
                && as<MacroClass>(other).arity == arity;
        }
    };

    struct FormFnClass : public Class {
        u32 arity;

        FormFnClass(u32 arity_in): Class(K_FORM_FN), arity(arity_in) {}

        u64 lazy_hash() const override {
            return raw_hash(kind()) * 5822540408738177351ul ^ raw_hash(arity);
        }

        void format(stream& io) const override {
            write(io, "form-function(", arity, ")");
        }

        void write_mangled(stream& io) const override {
            panic("Tried to mangle compile-time-only form-level function type!");
        }

        bool operator==(const Class& other) const override {
            return other.kind() == K_FORM_FN 
                && as<FormFnClass>(other).arity == arity;
        }
    };
    
    Type t_form_fn(u32 arity) {
        return t_create(ref<FormFnClass>(arity));
    }

    struct FormIsectClass : public Class {
        map<rc<Form>, rc<Class>> members;

        FormIsectClass(const map<rc<Form>, Type>& members_in):    
            Class(K_FORM_ISECT) {
            for (const auto& [f, t] : members_in) members[f] = TYPE_LIST[t.id];
        }

        u64 lazy_hash() const override {
            u64 base = raw_hash(kind());
            for (auto [s, t] : members) {
                base ^= s->hash() * 515562480546324473ul;
                base ^= t->hash() * 16271366544726016991ul;
            }
            return base;
        }

        void format(stream& io) const override {
            write_pairs(io, members, "overloaded(", ": ", " & ", ")");
        }

        void write_mangled(stream& io) const override {
            panic("Tried to mangle compile-time-only form-level overload type!");
        }

        bool coerces_to(const Class& other) const override {
            if (Class::coerces_to(other)) return true;

            if (other.kind() == K_FORM_ISECT) {
                for (const auto& [f, t] : as<FormIsectClass>(other).members) {
                    if (!members.contains(f)) return false;
                    if (*t != *members[f]) return false;
                }
                return true; // we contain every member of the target
            }
            return false;
        }

        bool operator==(const Class& other) const override {
            if (other.kind() != K_FORM_ISECT
                || as<FormIsectClass>(other).members.size() != members.size())
                return false;
            
            // at this point we know the size of both types' member sets is the same.
            for (const auto& [f, t] : members) {
                if (!as<FormIsectClass>(other).members.contains(f)) return false;
            }
            return true;
        }
    };

    Type t_form_isect(const map<rc<Form>, Type>& members) {
        return t_create(ref<FormIsectClass>(members));
    }

    static vector<Type> tvar_bindings;
    static vector<vector<Type>> tvar_isects;
    static set<u64> tvar_isecting;

    static u32 is_isect_mode = 0;

    void bind_tvar(u32 id, Type type);

    struct TVarClass : public Class {
        u32 id;
        Symbol name;

        TVarClass(): Class(K_TVAR), id(tvar_bindings.size()),
            name(symbol_from(::format<ustring>("#", id))) { // use #<id> for default name
            tvar_bindings.push(T_UNDEFINED); // start out as 'undefined'
            tvar_isects.push({}); // reserve an isect vector whenever we need it
        }

        TVarClass(Symbol name_in): TVarClass() { name = name_in; }

        u64 lazy_hash() const override {
            return raw_hash(kind()) * 3078465884631522967ul
                ^ raw_hash(id) * 8292421814661686869ul
                ^ raw_hash(name);
        }

        void format(stream& io) const override {
            write(io, name);
            if (tvar_bindings[id] != T_UNDEFINED) write(io, "(", tvar_bindings[id], ")");
        }

        void write_mangled(stream& io) const override {
            concrete()->write_mangled(io);
        }

        rc<Class> concrete() const {
            return TYPE_LIST[tvar_bindings[id].id];
        }

        bool coerces_to_generic(const Class& other) const override {
            bool result = TYPE_LIST[tvar_bindings[id].id]->coerces_to(other);
            if (!nonbinding // we refer to the nonbinding variable used in nonbinding Type member functions
                && result && other.kind() != K_ANY) {
                Type ot = t_from(other.id());
                if (is_isect_mode && tvar_bindings[id] != ot) { // build an intersection set instead of binding directly
                    tvar_isecting.insert(id);
                    tvar_isects[id].push(ot);
                }
                else if (!is_isect_mode) {
                    bind_tvar(id, ot); // bind this tvar to the other type
                }
            } 
            return result;
        }

        bool operator==(const Class& other) const override {
            return &other == this; // we can just do ref equality, since tvars don't really need to be deduplicated
        }
    };

    Type t_var() {
        return t_create(ref<TVarClass>());
    }

    Type t_var(Symbol name) {
        return t_create(ref<TVarClass>(name));
    }

    void t_tvar_enable_isect() {
        is_isect_mode ++;
    }

    void t_tvar_disable_isect() {
        if (is_isect_mode == 0) panic("Cannot disable isect mode - it is already disabled!");
        is_isect_mode --;
        if (is_isect_mode == 0) {
            vector<Type> types;
            for (u32 tvar : tvar_isecting) {
                if (tvar_isects[tvar].size() == 1)
                    bind_tvar(tvar, tvar_isects[tvar][0]);
                else {
                    types.clear();
                    for (Type t : tvar_isects[tvar]) types.push(t);
                    bind_tvar(tvar, t_intersect(types));
                    tvar_isects[tvar].clear();
                }
            }
            tvar_isecting.clear();
        }
    }

    void bind_tvar(u32 id, Type type) {
        Type it = type;
        while (it.is_tvar()) {
            if (as<TVarClass>(*TYPE_LIST[it.id]).id == id) return; // can't create cyclic graph
            it = tvar_bindings[as<TVarClass>(*TYPE_LIST[it.id]).id];
        }
        tvar_bindings[id] = type;
    }
    
    // Represents runtime types.
    struct RuntimeClass : public Class {
        rc<Class> base;

        RuntimeClass(Type base_in):
            Class(K_RUNTIME), base(TYPE_LIST[base_in.id]) {}

        u64 lazy_hash() const override {
            return base->hash() * 9757042299901199593ul ^ raw_hash(K_NAMED);
        }

        void format(stream& io) const override {
            write(io, "runtime(", base, ")");
        }

        void write_mangled(stream& io) const override {
            base->write_mangled(io);
        }

        bool coerces_to_generic(const Class& other) const override {
            if (Class::coerces_to_generic(other)) return true;

            if (other.kind() == K_RUNTIME) {
                return base->coerces_to_generic(*as<RuntimeClass>(other).base);
            }

            return false;
        }

        bool coerces_to(const Class& other) const override {
            if (Class::coerces_to(other)) return true;

            if (other.kind() == K_RUNTIME) {
                return base->coerces_to(*as<RuntimeClass>(other).base);
            }

            return false;
        }

        bool operator==(const Class& other) const override {
            return other.kind() == K_RUNTIME 
                && *base == *as<RuntimeClass>(other).base;
        }
    };

    Type t_runtime(Type base) {
        if (base.of(K_RUNTIME)) return base;
        return t_create(ref<RuntimeClass>(base));
    }

    bool Class::coerces_to_generic(const Class& other) const {
        if (other.kind() == K_TVAR) {
            if (!t_is_concrete(t_from(other.id())) && other.coerces_to_generic(*this)) 
                return true;
            else if (t_is_concrete(t_from(other.id())) && coerces_to_generic(*as<TVarClass>(other).concrete()))
                return true;
        }
        return *this == other 
            || other.kind() == K_ANY
            || other.kind() == K_ERROR;
    }

    bool Class::coerces_to(const Class& other) const {
        return coerces_to_generic(other)
            || (other.kind() == K_TVAR 
                && t_is_concrete(t_from(other.id())) 
                && coerces_to(*as<TVarClass>(other).concrete()))
            || (other.kind() == K_RUNTIME && TYPE_LIST[_id]->coerces_to(*as<RuntimeClass>(other).base))
            || (other.kind() == K_UNION && as<UnionClass>(other).members.contains(TYPE_LIST[_id]));
    }

    Type T_VOID, T_INT, T_FLOAT, T_DOUBLE, T_SYMBOL, 
        T_STRING, T_CHAR, T_BOOL, T_TYPE, T_ALIAS, T_ERROR, T_MODULE,
        T_ANY, T_UNDEFINED;

    Type t_tuple_at(Type tuple, u32 i) {
        if (!tuple.of(K_TUPLE)) panic("Expected tuple type!");
        return t_from(as<TupleClass>(*TYPE_LIST[tuple.id]).members[i]->id());
    }

    u32 t_tuple_len(Type tuple) {
        if (!tuple.of(K_TUPLE)) panic("Expected tuple type!");
        return as<TupleClass>(*TYPE_LIST[tuple.id]).members.size();
    }

    bool t_tuple_is_complete(Type tuple) {
        if (!tuple.of(K_TUPLE)) panic("Expected tuple type!");
        return !as<TupleClass>(*TYPE_LIST[tuple.id]).incomplete;
    }

    bool t_union_has(Type u, Type member) {
        if (!u.of(K_UNION)) panic("Expected union type!");
        return as<UnionClass>(*TYPE_LIST[u.id]).members.contains(TYPE_LIST[member.id]);
    }

    set<Type> t_union_members(Type u) {
        if (!u.of(K_UNION)) panic("Expected union type!");
        set<Type> members;
        for (const rc<Class>& c : as<UnionClass>(*TYPE_LIST[u.id]).members)
            members.insert(t_from(c->id()));
        return members;
    }

    Type t_intersect_with(Type intersect, Type other) {
        if (!intersect.of(K_INTERSECT)) panic("Expected intersection type!");
        vector<rc<Class>> members = as<IntersectionClass>(*TYPE_LIST[intersect.id]).members;
        bool found = false;
        for (const auto& m : members) if (t_soft_eq(t_from(m->id()), other)) found = true;
        if (!found) members.push(TYPE_LIST[other.id]);
        
        return t_create(ref<IntersectionClass>(members));
    }

    Type t_intersect_without(Type intersect, Type other) {
        if (!intersect.of(K_INTERSECT)) panic("Expected intersection type!");
        vector<rc<Class>> members = as<IntersectionClass>(*TYPE_LIST[intersect.id]).members;
        for (auto& m : members) if (t_soft_eq(t_from(m->id()), other)) {
            m = members.back();
            members.pop();
            break;
        }
        return t_create(ref<IntersectionClass>(members));
    }

    bool t_intersect_has(Type intersect, Type member) {
        if (!intersect.of(K_INTERSECT)) panic("Expected intersection type!");
        return intersect.nonbinding_coerces_to(member);
    }

    bool t_intersect_procedural(Type intersect) {
        if (!intersect.of(K_INTERSECT)) panic("Expected intersection type!");
        for (Type t : t_intersect_members(intersect))
            if (!t_runtime_base(t).of(K_FUNCTION)) return false;
        return true;
    }
    
    vector<Type> t_intersect_members(Type intersect) {
        if (!intersect.of(K_INTERSECT)) panic("Expected intersection type!");
        vector<Type> members;
        for (const rc<Class>& c : as<IntersectionClass>(*TYPE_LIST[intersect.id]).members)
            members.push(t_from(c->id()));
        return members;
    }

    Type t_list_element(Type list) {
        if (!list.of(K_LIST)) panic("Expected list type!");
        return t_from(as<ListClass>(*TYPE_LIST[list.id]).element->id());
    }

    Type t_array_element(Type array) {
        if (!array.of(K_ARRAY)) panic("Expected array type!");
        return t_from(as<ArrayClass>(*TYPE_LIST[array.id]).element->id());
    }

    u32 t_array_size(Type array) {
        if (!array.of(K_ARRAY)) panic("Expected array type!");
        if (!as<ArrayClass>(*TYPE_LIST[array.id]).sized) panic("Attempted to get size from unsized array type!");
        return as<ArrayClass>(*TYPE_LIST[array.id]).size;
    }

    bool t_array_is_sized(Type array) {
        if (!array.of(K_ARRAY)) panic("Expected array type!");
        return as<ArrayClass>(*TYPE_LIST[array.id]).sized;
    }

    Symbol t_get_name(Type named) {
        if (!named.of(K_NAMED)) panic("Expected named type!");
        return as<NamedClass>(*TYPE_LIST[named.id]).name;
    }

    Type t_get_base(Type named) {
        if (!named.of(K_NAMED)) panic("Expected named type!");
        return t_from(as<NamedClass>(*TYPE_LIST[named.id]).base->id());
    }

    bool t_struct_is_complete(Type str) {
        if (!str.of(K_STRUCT)) panic("Expected struct type!");
        return !as<StructClass>(*TYPE_LIST[str.id]).incomplete;
    }

    Type t_struct_field(Type str, Symbol field) {
        if (!str.of(K_STRUCT)) panic("Expected struct type!");
        auto it = as<StructClass>(*TYPE_LIST[str.id]).fields.find(field);
        if (it == as<StructClass>(*TYPE_LIST[str.id]).fields.end()) panic("Field not found in struct!");
        return t_from(it->second->id());
    }

    bool t_struct_has(Type str, Symbol field) {
        if (!str.of(K_STRUCT)) panic("Expected struct type!");
        return as<StructClass>(*TYPE_LIST[str.id]).fields.contains(field);
    }

    u32 t_struct_len(Type str) {
        if (!str.of(K_STRUCT)) panic("Expected struct type!");
        return as<StructClass>(*TYPE_LIST[str.id]).fields.size();
    }
    
    map<Symbol, Type> t_struct_fields(Type str) {
        if (!str.of(K_STRUCT)) panic("Expected struct type!");
        map<Symbol, Type> fields;
        for (const auto& [k, v] : as<StructClass>(*TYPE_LIST[str.id]).fields)
            fields.put(k, t_from(v->id()));
        return fields;
    }

    Type t_dict_key(Type dict) {
        if (!dict.of(K_DICT)) panic("Expected dictionary type!");
        return t_from(as<DictClass>(*TYPE_LIST[dict.id]).key->id());
    }

    Type t_dict_value(Type dict) {
        if (!dict.of(K_DICT)) panic("Expected dictionary type!");
        return t_from(as<DictClass>(*TYPE_LIST[dict.id]).value->id());
    }

    u32 t_arity(Type fn) {
        if (fn.of(K_FUNCTION)) {
            const FunctionClass& fnc = as<FunctionClass>(*TYPE_LIST[fn.id]);
            if (fnc.arg->kind() == K_TUPLE) return as<TupleClass>(*fnc.arg).members.size();
            else return 1;
        }
        else if (fn.of(K_MACRO)) {
            return as<MacroClass>(*TYPE_LIST[fn.id]).arity;
        }
        else {
            panic("Expected function or macro type!");
            return 0;
        }
    }

    Type t_arg(Type fn) {
        if (!fn.of(K_FUNCTION)) panic("Expected function type!");
        return t_from(as<FunctionClass>(*TYPE_LIST[fn.id]).arg->id());
    }

    Type t_ret(Type fn) {
        if (!fn.of(K_FUNCTION)) panic("Expected function type!");
        return t_from(as<FunctionClass>(*TYPE_LIST[fn.id]).ret->id());
    }

    bool t_is_macro(Type fn) {
        if (!fn.of(K_FUNCTION)) panic("Expected function type!");
        return as<FunctionClass>(*TYPE_LIST[fn.id]).is_macro;
    }

    Type t_tvar_concrete(Type tvar) {
        if (!tvar.is_tvar()) panic("Expected type variable!");
        Type t = tvar_bindings[as<TVarClass>(*TYPE_LIST[tvar.id]).id];
        if (t.is_tvar()) return t_tvar_concrete(t);
        else return t;
    }

    Type t_concrete(Type type) {
        return type.is_tvar() ? t_tvar_concrete(type) : type;
    }

    Symbol t_tvar_name(Type tvar) {
        if (!tvar.is_tvar()) panic("Expected type variable!");
        return as<TVarClass>(*TYPE_LIST[tvar.id]).name;
    }

    void t_tvar_unbind(Type tvar) {
        if (!tvar.is_tvar()) panic("Expected type variable!");
        bind_tvar(as<TVarClass>(*TYPE_LIST[tvar.id]).id, T_UNDEFINED);
    }
    
    void t_tvar_bind(Type tvar, Type dest) {
        if (!tvar.is_tvar()) panic("Expected type variable!");
        bind_tvar(as<TVarClass>(*TYPE_LIST[tvar.id]).id, dest);
    }

    Type t_runtime_base(Type t) {
        if (!t.of(K_RUNTIME)) return t;
        return t_from(as<RuntimeClass>(*TYPE_LIST[t.id]).base->id());
    }

    u32 t_form_fn_arity(Type func) {
        if (!func.of(K_FORM_FN)) panic("Expected form-level function type!");
        return as<FormFnClass>(*TYPE_LIST[func.id]).arity;
    }

    map<rc<Form>, Type> t_form_isect_members(Type isect) {
        if (!isect.of(K_FORM_ISECT)) panic("Expected form-level intersection type!");
        map<rc<Form>, Type> members;
        for (const auto& [k, v] : as<FormIsectClass>(*TYPE_LIST[isect.id]).members)
            members[k] = t_from(v->id());
        return members;
    }

    optional<Type> t_overload_for(Type overloaded, rc<Form> form) {
        if (!overloaded.of(K_FORM_ISECT)) panic("Expected form-level intersection type!");
        const auto& members = as<FormIsectClass>(*TYPE_LIST[overloaded.id]).members;
        auto it = members.find(form);
        if (it == members.end()) return none<Type>();
        else return some<Type>(t_from(it->second->id()));
    }

    void t_unbind(Type t) {
        switch (t.is_tvar() ? K_TVAR : t.kind()) {
            case K_TVAR:
                bind_tvar(as<TVarClass>(*TYPE_LIST[t.id]).id, T_UNDEFINED);
                return;
            case K_RUNTIME:
                return t_unbind(t_runtime_base(t));
            case K_LIST: return t_unbind(t_list_element(t));
            case K_FUNCTION: return t_unbind(t_arg(t)), t_unbind(t_ret(t));
            case K_DICT: return t_unbind(t_dict_key(t)), t_unbind(t_dict_value(t));
            case K_NAMED: return t_unbind(t_get_base(t));
            case K_UNION: {
                for (rc<Class> cl : as<UnionClass>(*TYPE_LIST[t.id]).members) 
                    t_unbind(t_from(cl->id()));
                return;
            }
            case K_INTERSECT: {
                for (rc<Class> cl : as<IntersectionClass>(*TYPE_LIST[t.id]).members) 
                    t_unbind(t_from(cl->id()));
                return;
            }
            case K_STRUCT: {
                for (const auto& [f, t] : as<StructClass>(*TYPE_LIST[t.id]).fields) 
                    t_unbind(t_from(t->id()));
                return;
            }
            case K_FORM_ISECT: {
                for (const auto& [f, t] : as<FormIsectClass>(*TYPE_LIST[t.id]).members) 
                    t_unbind(t_from(t->id()));
                return;
            }
            case K_TUPLE: {
                for (u32 i = 0; i < t_tuple_len(t); i ++) t_unbind(t_tuple_at(t, i));
                return;
            }
            case K_ARRAY: return t_unbind(t_array_element(t));
            default: return;
        }
    }
    
    bool t_is_concrete(Type t) {
        switch (t.is_tvar() ? K_TVAR : t.kind()) {
            case K_SYMBOL:
            case K_INT:
            case K_FLOAT:
            case K_STRING:
            case K_VOID:
            case K_CHAR:
            case K_BOOL:
            case K_TYPE:
            case K_DOUBLE:
            case K_MODULE:
            case K_ALIAS:
            case K_MACRO:
            case K_ERROR:
            case K_FORM_FN:      // despite the name, undefined functions should not be generically
            case K_FORM_ISECT:   // converted to/from
                return true;
            case K_ANY:
            case K_UNDEFINED:
                return false;
            case K_TVAR:
                return t_is_concrete(t_tvar_concrete(t));
            case K_RUNTIME:
                return t_is_concrete(t_runtime_base(t));
            case K_LIST: return t_is_concrete(t_list_element(t));
            case K_FUNCTION: return t_is_concrete(t_arg(t)) && t_is_concrete(t_ret(t));
            case K_DICT: return t_is_concrete(t_dict_key(t)) && t_is_concrete(t_dict_value(t));
            case K_NAMED: return t_is_concrete(t_get_base(t));
            case K_UNION: {
                bool result = true;
                for (rc<Class> cl : as<UnionClass>(*TYPE_LIST[t.id]).members) 
                    result = result && t_is_concrete(t_from(cl->id()));
                return result;
            }
            case K_INTERSECT: {
                bool result = true;
                for (rc<Class> cl : as<IntersectionClass>(*TYPE_LIST[t.id]).members) 
                    result = result && t_is_concrete(t_from(cl->id()));
                return result;
            }
            case K_STRUCT: {
                if (!t_struct_is_complete(t)) return false;
                bool result = true;
                for (const auto& [f, t] : as<StructClass>(*TYPE_LIST[t.id]).fields) 
                    result = result && t_is_concrete(t_from(t->id()));
                return result;
            }
            case K_TUPLE: {
                if (!t_tuple_is_complete(t)) return false;
                bool result = true;
                for (u32 i = 0; i < t_tuple_len(t); i ++) result = result && t_is_concrete(t_tuple_at(t, i));
                return result;
            }
            case K_ARRAY: return t_is_concrete(t_array_element(t));
            default:
                return false;
        }
    }

    Type t_lower(Type t) {
        switch (t.is_tvar() ? K_TVAR : t.kind()) {
            case K_SYMBOL:
            case K_INT:
            case K_FLOAT:
            case K_STRING:
            case K_VOID:
            case K_CHAR:
            case K_BOOL:
            case K_TVAR:
            case K_TYPE:
            case K_DOUBLE:
            case K_UNDEFINED:
                return t;
            case K_MODULE:
            case K_ALIAS:
            case K_MACRO:
            case K_ERROR:
            case K_FORM_FN:      // form-level types aside from undefined should
            case K_FORM_ISECT:   // not be lowered.
                return T_ERROR;
            case K_ANY:
                return t_var();
            case K_RUNTIME:
                return t_runtime_base(t);
            case K_LIST: {
                Type elt = t_lower(t_list_element(t));
                return elt == T_ERROR ? elt : t_list(elt);
            }
            case K_FUNCTION: {
                if (t_is_macro(t)) return T_ERROR; // can't lower macro types
                Type arg = t_lower(t_arg(t)), ret = t_lower(t_ret(t));
                return arg == T_ERROR || ret == T_ERROR ? T_ERROR : t_func(arg, ret);
            }
            case K_DICT: {
                Type key = t_lower(t_dict_key(t)), val = t_lower(t_dict_value(t));
                return key == T_ERROR || val == T_ERROR ? T_ERROR : t_dict(key, val);
            }
            case K_NAMED: {
                Type elt = t_lower(t_get_base(t));
                return elt == T_ERROR ? elt : t_named(t_get_name(t), elt);
            }
            case K_UNION: {
                set<Type> types;
                for (rc<Class> cl : as<UnionClass>(*TYPE_LIST[t.id]).members) {
                    types.insert(t_lower(t_from(cl->id())));
                }
                if (types.contains(T_ERROR)) return T_ERROR;
                return t_union(types);
            }
            case K_INTERSECT: {
                vector<Type> types;
                for (rc<Class> cl : as<IntersectionClass>(*TYPE_LIST[t.id]).members) {
                    types.push(t_lower(t_from(cl->id())));
                    if (types.back() == T_ERROR) return T_ERROR;
                }
                return t_intersect(types);
            }
            case K_STRUCT: {
                map<Symbol, Type> fields;
                for (const auto& [f, t] : as<StructClass>(*TYPE_LIST[t.id]).fields) {
                    Type l = t_lower(t_from(t->id()));
                    if (l == T_ERROR) return T_ERROR;
                    fields.put(f, l);
                }
                return t_struct_is_complete(t) ? t_struct(fields) : t_incomplete_struct(fields);
            }
            case K_TUPLE: {
                vector<Type> members;
                for (u32 i = 0; i < t_tuple_len(t); i ++) {
                    members.push(t_lower(t_tuple_at(t, i)));
                    if (members.back() == T_ERROR) return T_ERROR;
                }
                return t_tuple_is_complete(t) ? t_tuple(members) : t_incomplete_tuple(members);
            }
            case K_ARRAY: {
                Type elt = t_lower(t_array_element(t));
                if (elt == T_ERROR) return T_ERROR;
                return t_array_is_sized(t) ? t_array(elt, t_array_size(t)) : t_array(elt);
            }
            default:
                return T_ERROR;
        }
    }

    void init_types() {
        T_VOID = t_create(ref<VoidClass>(K_VOID, "Void"));
        T_INT = t_create(ref<NumberClass>(K_INT, "Int", "i", false, 8)); // 8-byte integral type
        T_FLOAT = t_create(ref<NumberClass>(K_FLOAT, "Float", "f", true, 4)); // 4-byte floating-point type
        T_DOUBLE = t_create(ref<NumberClass>(K_DOUBLE, "Double", "d", true, 8)); // 8-byte floating-point type
        T_SYMBOL = t_create(ref<SingletonClass>(K_SYMBOL, "Symbol", "n"));
        T_STRING = t_create(ref<SingletonClass>(K_STRING, "String", "s"));
        T_CHAR = t_create(ref<SingletonClass>(K_CHAR, "Char", "c"));
        T_BOOL = t_create(ref<SingletonClass>(K_BOOL, "Bool", "b"));
        T_TYPE = t_create(ref<SingletonClass>(K_TYPE, "Type", "t"));
        T_ALIAS = t_create(ref<SingletonClass>(K_ALIAS, "Alias", ""));
        T_ERROR = t_create(ref<SingletonClass>(K_ERROR, "Error", ""));
        T_MODULE = t_create(ref<SingletonClass>(K_MODULE, "Module", ""));
        T_ANY = t_create(ref<SingletonClass>(K_ANY, "Any", "w"));
        T_UNDEFINED = t_create(ref<UndefinedClass>(K_UNDEFINED, "Undefined"));
    }

    void init_types_and_symbols() {
        init_symbols();
        init_types();
    }

    void free_types() {
        for (rc<Class>& r : TYPE_LIST) r = nullptr;
    }
}

template<>
u64 hash(const basil::Symbol& symbol) {
    return raw_hash<u32>(symbol.id);
}

template<>
u64 hash(const basil::Type& type) {
    return basil::TYPE_LIST[type.id]->hash();
}

template<>
u64 hash(const rc<basil::Class>& tclass) {
    return tclass->hash();
}

template<>
u64 hash(const basil::Kind& kind) {
    return basil::KIND_HASHES[kind];
}

void write(stream& io, const basil::Symbol& symbol) {
    write(io, basil::string_from(symbol));
}

void write(stream& io, const basil::Kind& kind) {
    write(io, basil::KIND_NAMES[kind]);
}

void write(stream& io, const basil::Type& type) {
    type.format(io);
}

void write(stream& io, const rc<basil::Class>& tclass) {
    tclass->format(io);
}