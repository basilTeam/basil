#include "type.h"

namespace basil {
    struct Class;
}

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
        S_QUESTION, S_ELLIPSIS, S_COMMA;

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
    }

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
        3843382840898873837ul
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

        virtual bool coerces_to(const Class& other) const; // Implemented below, just above T_INT and such.

        virtual bool operator==(const Class& other) const = 0;

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
        return TYPE_LIST[id]->kind();
    }

    bool Type::of(Kind kind) const {
        return this->kind() == kind;
    }

    void Type::format(stream& io) const {
        TYPE_LIST[id]->format(io);
    }

    bool Type::coerces_to(Type other) const {
        return TYPE_LIST[id]->coerces_to(*TYPE_LIST[other.id]);
    }

    bool Type::operator==(Type other) const {
        return id == other.id;
    }

    bool Type::operator!=(Type other) const {
        return id != other.id;
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
        const char* name;

        SingletonClass(Kind kind, const char* name_in): Class(kind), name(name_in) {}

        u64 lazy_hash() const override {
            return ::hash(kind());
        }

        void format(stream& io) const override {
            write(io, name);
        }

        bool operator==(const Class& other) const override {
            return kind() == other.kind();
        }
    };

    // Encompasses different sizes of int and floating-point types.
    struct NumberClass : public Class {
        const char* name;
        bool floating;
        u32 size;

        NumberClass(Kind kind, const char* name_in, bool floating_in, u32 size_in_bytes): 
            Class(kind), name(name_in), floating(floating_in), size(size_in_bytes) {}

        u64 lazy_hash() const override {
            return ::hash(kind()) * 544921574335967683ul ^ ::hash(size) * 4508536729671157399ul ^ ::hash(floating);
        };

        void format(stream& io) const override {
            write(io, name);
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
        VoidClass(Kind kind, const char* name): SingletonClass(kind, name) {}

        bool coerces_to(const Class& other) const override {
            return Class::coerces_to(other) || other.kind() == K_LIST;
        }
    };

    // Represents named types.
    struct NamedClass : public Class {
        Symbol name;
        rc<Class> base;

        NamedClass(Symbol name_in, Type base_in): 
            Class(K_NAMED), name(name_in), base(TYPE_LIST[base_in.id]) {}

        u64 lazy_hash() const override {
            return base->hash() * 10762593286530943657ul ^ ::hash(name) * 11858331803272376449ul ^ ::hash(K_NAMED);
        }

        void format(stream& io) const override {
            write(io, name, " of ", base);
        }

        bool coerces_to(const Class& other) const override {
            if (Class::coerces_to(other)) return true;

            if (other.kind() == K_NAMED
                && as<NamedClass>(other).name == name
                && as<NamedClass>(other).base->kind() == K_ANY)
                return true; // e.g. Foo of int can coerce to Foo of any
            
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
            return element->hash() * 6769132636657327813ul ^ ::hash(kind());
        }

        void format(stream& io) const override {
            write(io, "[", element, "]");
        }

        bool coerces_to(const Class& other) const override {
            if (Class::coerces_to(other)) return true;

            if (other.kind() == K_LIST && as<ListClass>(other).element->kind() == K_ANY)
                return true; // all lists can convert to [any]
            
            if (other.kind() == K_TYPE) 
                return element->kind() == K_TYPE; // [type] can convert to type
            
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
            u64 base = ::hash(kind());
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
                    if (members[i]->kind() != K_TYPE) return false;
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
            return ::hash(kind()) ^ element->hash() * 8773895335238318147ul
                ^ (sized ? ::hash(size) * 8954908842287060251ul : 11485220905872292697ul);
        }

        void format(stream& io) const override {
            write(io, element, "[");
            if (sized) write(io, size);
            write(io, "]");
        }

        bool coerces_to(const Class& other) const override {
            if (Class::coerces_to(other)) return true;

            if (other.kind() == K_ARRAY) {
                if (*element == *as<ArrayClass>(other).element) // Can convert to unsized array with same element type.
                    return !as<ArrayClass>(other).sized;
                else if (as<ArrayClass>(other).element->kind() == K_ANY) // Can convert to unsized or same-sized 'any' arrays.
                    return !as<ArrayClass>(other).sized
                        || as<ArrayClass>(other).size == size;
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
            for (Type t : members_in) members.insert(TYPE_LIST[t.id]);
        }

        u64 lazy_hash() const override {
            u64 base = ::hash(kind());
            for (const auto& m : members) {
                base ^= 3958225336639215437ul * m->hash();
            }
            return base;
        }

        void format(stream& io) const override {
            write_seq(io, members, "(", " | ", ")");
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
        set<rc<Class>> members;

        IntersectionClass(const set<Type>& members_in):
            Class(K_INTERSECT) {
            for (Type t : members_in) members.insert(TYPE_LIST[t.id]);
        }

        u64 lazy_hash() const override {
            u64 base = ::hash(kind());
            for (const auto& m : members) {
                base ^= 16873539230647500721ul * m->hash();
            }
            return base;
        }

        void format(stream& io) const override {
            write_seq(io, members, "(", " & ", ")");
        }

        bool coerces_to(const Class& other) const override {
            if (Class::coerces_to(other)) return true;

            if (other.kind() == K_INTERSECT) {
                auto copy_other_members = as<IntersectionClass>(other).members;
                for (const auto& m : members) {
                    copy_other_members.erase(m);
                }
                return copy_other_members.size() == 0; // Coercion possible if target's members are a subset of ours.
            }
            return members.find(TYPE_LIST[other._id]) != members.end(); // Can coerce to any member.
        }

        bool operator==(const Class& other) const override {
            if (other.kind() != K_INTERSECT
                || as<IntersectionClass>(other).members.size() != members.size())
                return false;

            // At this point we know the size of both types' member sets is the same.
            auto copy_members = members;
            for (const auto& m : as<IntersectionClass>(other).members) {
                copy_members.erase(m);
            }
            return copy_members.size() == 0; // Member sets must have had the same elements.
        }
    };

    Type t_intersect(const set<Type>& members) {
        if (members.size() < 2) panic("Cannot create intersection type with less than two members!");
        return t_create(ref<IntersectionClass>(members));
    }

    struct FunctionClass : public Class {
        rc<Class> arg, ret;

        FunctionClass(Type arg_in, Type ret_in):
            Class(K_FUNCTION), arg(TYPE_LIST[arg_in.id]), ret(TYPE_LIST[ret_in.id]) {}

        u64 lazy_hash() const override {
            return ::hash(kind()) ^ arg->hash() * 4858037243276500399ul ^ ret->hash() * 16668975004056768077ul;
        }

        void format(stream& io) const override {
            write(io, arg, " -> ", ret);
        }

        bool coerces_to(const Class& other) const override {
            if (Class::coerces_to(other)) return true;

            if (other.kind() == K_FUNCTION) {
                if (!arg->coerces_to(*as<FunctionClass>(other).arg)) // Argument type must be coercible to target arg type.
                    return false;
                if (!ret->coerces_to(*as<FunctionClass>(other).ret)) // Return type must be coercible to target return type.
                    return false;

                return (arg->kind() != K_ANY && as<FunctionClass>(other).arg->kind() == K_ANY) // Permit coercion to more generic function type.
                    || (ret->kind() != K_ANY && as<FunctionClass>(other).ret->kind() == K_ANY);
            }

            if (other.kind() == K_TYPE) {
                return ret->kind() == K_TYPE; // Functions that return type can be coerced to 'type'.
            }

            return false;
        }

        bool operator==(const Class& other) const override {
            return other.kind() == K_FUNCTION
                && *arg == *as<FunctionClass>(other).arg
                && *ret == *as<FunctionClass>(other).ret;
        }
    };

    Type t_func(Type arg, Type ret) {
        return t_create(ref<FunctionClass>(arg, ret));
    }

    struct StructClass : public Class {
        map<Symbol, rc<Class>> fields;
        bool incomplete;

        StructClass(const map<Symbol, Type>& fields_in, bool incomplete_in):
            Class(K_STRUCT), incomplete(incomplete_in) {
            for (auto [s, t] : fields_in) fields[s] = TYPE_LIST[t.id];
        }

        u64 lazy_hash() const override {
            u64 base = ::hash(kind());
            if (incomplete) base ^= 6659356980319522183ul;
            for (auto [s, t] : fields) {
                base ^= ::hash(s) * 515562480546324473ul;
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
            return ::hash(kind()) ^ 1785136365411115207ul * key->hash() 
                ^ 14219447378751898973ul * value->hash();
        }

        void format(stream& io) const override {
            write(io, key, "[", value, "]");
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
            return ::hash(kind()) * 5822540408738177351ul ^ ::hash(arity);
        }

        void format(stream& io) const override {
            write(io, "macro(", arity, ")");
        }

        bool operator==(const Class& other) const override {
            return other.kind() == K_MACRO 
                && as<MacroClass>(other).arity == arity;
        }
    };

    bool Class::coerces_to(const Class& other) const {
        bool result = *this == other
            || other.kind() == K_ANY // all types can convert to 'any'
            || other.kind() == K_ERROR // ...and 'error', although it's more of an implementation detail
            || (other.kind() == K_UNION // all types can convert to unions containing them
                && as<UnionClass>(other).members.contains(TYPE_LIST[_id])); // TYPE_LIST[_id] is an rc to self
        return result;
    }

    Type T_VOID, T_INT, T_FLOAT, T_DOUBLE, T_SYMBOL, 
        T_STRING, T_CHAR, T_BOOL, T_TYPE, T_ALIAS, T_ERROR, 
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

    bool t_intersect_has(Type intersect, Type member) {
        if (!intersect.of(K_INTERSECT)) panic("Expected intersection type!");
        return as<IntersectionClass>(*TYPE_LIST[intersect.id]).members.contains(TYPE_LIST[member.id]);
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
        return t_from(as<NamedClass>(*TYPE_LIST[named.id]).id());
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

    void init_types() {
        T_VOID = t_create(ref<VoidClass>(K_VOID, "void"));
        T_INT = t_create(ref<NumberClass>(K_INT, "int", false, 8)); // 8-byte integral type
        T_FLOAT = t_create(ref<NumberClass>(K_FLOAT, "float", true, 4)); // 4-byte floating-point type
        T_DOUBLE = t_create(ref<NumberClass>(K_DOUBLE, "double", true, 8)); // 8-byte floating-point type
        T_SYMBOL = t_create(ref<SingletonClass>(K_SYMBOL, "symbol"));
        T_STRING = t_create(ref<SingletonClass>(K_STRING, "string"));
        T_CHAR = t_create(ref<SingletonClass>(K_CHAR, "char"));
        T_BOOL = t_create(ref<SingletonClass>(K_BOOL, "bool"));
        T_TYPE = t_create(ref<SingletonClass>(K_TYPE, "type"));
        T_ALIAS = t_create(ref<SingletonClass>(K_ALIAS, "alias"));
        T_ERROR = t_create(ref<SingletonClass>(K_ERROR, "error"));
        T_ANY = t_create(ref<SingletonClass>(K_ANY, "any"));
        T_UNDEFINED = t_create(ref<SingletonClass>(K_UNDEFINED, "undefined"));
    }

    void init_types_and_symbols() {
        init_symbols();
        init_types();
    }

    void free_types() {
        for (rc<Class>& r : TYPE_LIST) r = nullptr;
    }
}

u64 hash(const basil::Symbol& symbol) {
    return hash<u32>(symbol.id);
}

u64 hash(const basil::Type& type) {
    return basil::TYPE_LIST[type.id]->hash();
}

u64 hash(const rc<basil::Class>& tclass) {
    return tclass->hash();
}

u64 hash(const basil::Kind& kind) {
    return basil::KIND_HASHES[kind];
}

void write(stream& io, const basil::Symbol& symbol) {
    write(io, basil::string_from(symbol));
}

void write(stream& io, const basil::Type& type) {
    type.format(io);
}

void write(stream& io, const rc<basil::Class>& tclass) {
    tclass->format(io);
}