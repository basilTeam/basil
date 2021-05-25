#include "value.h"

namespace basil {
    Value::Data::Data(Kind kind) {
        switch (kind) {
            case K_INT: i = 0; break;
            case K_FLOAT: f32 = 0; break;
            case K_DOUBLE: f64 = 0; break;
            case K_SYMBOL: sym = S_NONE; break;
            case K_TYPE: type = T_VOID; break;
            case K_CHAR: ch = 0; break;
            case K_BOOL: b = false; break;
            case K_VOID: break;
            case K_ERROR: break;
            case K_STRING: new (&string) rc<String>(); break;
            case K_LIST: new (&list) rc<List>(); break;
            case K_NAMED: new (&named) rc<Named>(); break;
            case K_TUPLE: new (&tuple) rc<Tuple>(); break;
            case K_ARRAY: new (&array) rc<Array>(); break;
            case K_UNION: new (&u) rc<Union>(); break;
            case K_STRUCT: new (&str) rc<Struct>(); break;
            case K_DICT: new (&dict) rc<Dict>(); break;
            default:
                panic("Unsupported value kind!"); 
                break;
        }
    }

    Value::Data::Data(Kind kind, const Value::Data& other) {
        switch (kind) {
            case K_INT: i = other.i; break;
            case K_FLOAT: f32 = other.f32; break;
            case K_DOUBLE: f64 = other.f64; break;
            case K_SYMBOL: sym = other.sym; break;
            case K_TYPE: type = other.type; break;
            case K_CHAR: ch = other.ch; break;
            case K_BOOL: b = other.b; break;
            case K_VOID: break;
            case K_ERROR: break;
            case K_STRING: new (&string) rc<String>(other.string); break;
            case K_LIST: new (&list) rc<List>(other.list); break;
            case K_NAMED: new (&named) rc<Named>(other.named); break;
            case K_TUPLE: new (&tuple) rc<Tuple>(other.tuple); break;
            case K_ARRAY: new (&array) rc<Array>(other.array); break;
            case K_UNION: new (&u) rc<Union>(other.u); break;
            case K_STRUCT: new (&str) rc<Struct>(other.str); break;
            case K_DICT: new (&dict) rc<Dict>(other.dict); break;
            default:
                panic("Unsupported value kind!"); 
                break;
        }
    }

    Value::Data::~Data() {
        // do nothing...we handle this in Value's destructor.
    }

    Value::Value(): Value({}, T_VOID, nullptr) {}

    Value::~Value() {
        switch (type.kind()) {
            case K_STRING: data.string.~rc(); break;
            case K_LIST: data.list.~rc(); break;
            case K_NAMED: data.named.~rc(); break;
            case K_TUPLE: data.tuple.~rc(); break;
            case K_ARRAY: data.array.~rc(); break;
            case K_UNION: data.u.~rc(); break;
            case K_STRUCT: data.str.~rc(); break;
            case K_DICT: data.dict.~rc();
            default: break; // trivial destructor
        }
    }

    Value::Value(const Value& other):
        pos(other.pos), type(other.type), form(other.form), data(other.type.kind(), other.data) {}

    Value& Value::operator=(const Value& other) {
        if (this != &other) {
            switch (type.kind()) {
                case K_STRING: data.string.~rc(); break;
                case K_LIST: data.list.~rc(); break;
                case K_NAMED: data.named.~rc(); break;
                case K_TUPLE: data.tuple.~rc(); break;
                case K_ARRAY: data.array.~rc(); break;
                case K_UNION: data.u.~rc(); break;
                case K_STRUCT: data.str.~rc(); break;
                case K_DICT: data.dict.~rc();
                default: break; // trivial destructor
            }
            type = other.type;
            pos = other.pos;
            form = other.form;
            new (&data) Data(type.kind(), other.data); // copy over
        }
        return *this;
    }     
    
    u64 Value::hash() const {
        u64 kh = ::hash(type);
        switch (type.kind()) {
            case K_INT: return kh ^ ::hash(data.i);
            case K_FLOAT: return kh ^ ::hash(data.f32);
            case K_DOUBLE: return kh ^ ::hash(data.f64);
            case K_SYMBOL: return kh ^ ::hash(data.sym);
            case K_CHAR: return kh ^ ::hash(data.ch);
            case K_BOOL: return kh ^ ::hash(data.b);
            case K_VOID: return kh;
            case K_ERROR: return kh;
            case K_STRING: return kh ^ ::hash(data.string->data);
            case K_NAMED: return kh ^ ::hash(t_get_name(type)) ^ ::hash(data.named->value);
            case K_UNION: return kh ^ ::hash(data.u->value.hash());
            case K_LIST: {
                rc<List> l = data.list;
                while (l) {
                    kh ^= 9078847634459849863ul * ::hash(l->head);
                    l = l->tail;
                }
                return kh;
            }
            case K_TUPLE: {
                for (const Value& v : data.tuple->members)
                    kh ^= 17506913336699353123ul * ::hash(v);
                return kh;
            }
            case K_ARRAY: {
                for (const Value& v : data.array->elements)
                    kh ^= 14514260704651213427ul * ::hash(v);
                return kh;
            }
            case K_STRUCT: {
                for (const auto& [s, v] : data.str->fields) {
                    kh ^= 3643764085211794813ul * ::hash(s);
                    kh ^= 4428768580518955441ul * ::hash(v);
                }
                return kh;
            }
            case K_DICT: {
                for (const auto& [k, v] : data.dict->elements) {
                    kh ^= 9153145680466808213ul * ::hash(k);
                    kh ^= 8665824272381522569ul * ::hash(v);
                }
                return kh;
            }
            default:
                panic("Unsupported value kind!"); 
                return kh;
        }
    }

    void Value::format(stream& io) const {
        switch (type.kind()) {
            case K_INT: write(io, data.i); break;
            case K_FLOAT: write(io, data.f32); break;
            case K_DOUBLE: write(io, data.f64); break;
            case K_SYMBOL: write(io, data.sym); break;
            case K_CHAR: write(io, data.ch); break;
            case K_TYPE: write(io, data.type); break;
            case K_BOOL: write(io, data.b); break;
            case K_VOID: write(io, "()"); break;
            case K_ERROR: write(io, "#error"); break;
            case K_STRING: write(io, '"', data.string->data, '"'); break;
            case K_NAMED: write(io, t_get_name(type), " of ", data.named->value); break; // Name of value
            case K_UNION: write(io, data.u->value, " in ", type); break; // value in (type | type)
            case K_LIST: write_seq(io, iter_list(*this), "(", " ", ")"); break; // (1 2 3)
            case K_TUPLE: write_seq(io, data.tuple->members, "(", ", ", ")"); break; // (1, 2, 3)
            case K_ARRAY: write_seq(io, data.array->elements, "[", " ", "]"); break; // [1 2 3]
            case K_STRUCT: write_pairs(io, data.str->fields, "{", " : ", "; ", "}"); break; // {x : 1; y : 2}
            case K_DICT: {
                if (t_dict_value(type) == T_VOID) write_keys(io, data.dict->elements, "{", " ", "}"); // {1 2 3}
                else write_pairs(io, data.dict->elements, "{", " = ", "; ", "}"); // {"x" = 1; "y" = 2}
                break;
            }
            default:
                panic("Unsupported value kind!"); 
                break;
        }
    }

    bool Value::operator==(const Value& other) const {
        if (type != other.type) return false;
        switch (type.kind()) {
            case K_INT: return data.i == other.data.i;
            case K_FLOAT: return data.f32 == other.data.f32;
            case K_DOUBLE: return data.f64 == other.data.f64;
            case K_SYMBOL: return data.sym == other.data.sym;
            case K_TYPE: return data.type == other.data.type;
            case K_CHAR: return data.ch == other.data.ch;
            case K_BOOL: return data.b == other.data.b;
            case K_VOID: return true;
            case K_ERROR: return true;
            case K_STRING: return data.string->data == other.data.string->data;
            case K_NAMED: return data.named->value == other.data.named->value;
            case K_UNION: return data.u->value == other.data.u->value;
            case K_LIST: {
                rc<List> l = data.list, ol = other.data.list;
                while (l && ol) {
                    if (l->head != ol->head) return false; // elementwise comparison
                    l = l->tail;
                    ol = ol->tail;
                }
                return !l && !ol; // both lists were completely traversed
            }
            case K_TUPLE: {
                if (data.tuple->members.size() != other.data.tuple->members.size()) return false;
                for (u32 i = 0; i < data.tuple->members.size(); i ++)
                    if (data.tuple->members[i] != other.data.tuple->members[i]) return false;
                return true;
            }
            case K_ARRAY: {
                if (data.array->elements.size() != other.data.array->elements.size()) return false;
                for (u32 i = 0; i < data.array->elements.size(); i ++)
                    if (data.array->elements[i] != other.data.array->elements[i]) return false;
                return true;
            }
            case K_STRUCT: {
                if (data.str->fields.size() != other.data.str->fields.size()) return false;
                auto copy_fields = data.str->fields;
                for (const auto& [s, v] : other.data.str->fields) {
                    auto it = copy_fields.find(s);
                    if (it == copy_fields.end()) return false; // fail if the other struct has a field we don't have
                    if (it->second != v) return false;         // fail if the other struct's field has a different value than ours
                    copy_fields.erase(s);                      // otherwise, make sure we don't consider this field again
                }
                return copy_fields.size() == 0; // our struct had a matching field for every field in the other struct, and no extras
            }
            case K_DICT: {
                if (data.dict->elements.size() != other.data.dict->elements.size()) return false;
                auto copy_elements = data.dict->elements;
                for (const auto& [k, v] : other.data.dict->elements) {
                    auto it = copy_elements.find(k);
                    if (it == copy_elements.end()) return false; // fail if the other dict has a key we don't have
                    if (it->second != v) return false;           // fail if the other dict's key has a different value than ours
                    copy_elements.erase(k);                      // otherwise, make sure we don't consider this key again
                }
                return copy_elements.size() == 0; // our dict had a matching key for every key in the other dict, and no extras
            }
            default:
                panic("Unsupported value kind!"); 
                return true;
        }
    }
    
    bool Value::operator!=(const Value& other) const {
        return !operator==(other);
    }

    Value::Value(Source::Pos pos_in, Type type_in, rc<Form> form_in):
        pos(pos_in), type(type_in), form(form_in), data(type_in.kind()) {}

    String::String(const ustring& data_in): data(data_in) {}

    List::List(const Value& head_in, rc<List> tail_in): head(head_in), tail(tail_in) {}

    Tuple::Tuple(const vector<Value>& members_in): members(members_in) {}

    Array::Array(const vector<Value>& elements_in): elements(elements_in) {}

    Union::Union(const Value& value_in): value(value_in) {}

    Named::Named(const Value& value_in): value(value_in) {}

    Struct::Struct(const map<Symbol, Value>& fields_in): fields(fields_in) {}

    Dict::Dict(const map<Value, Value>& elements_in): elements(elements_in) {}

    Value v_int(Source::Pos pos, i64 i) {
        Value v(pos, T_INT, nullptr);
        v.data.i = i;
        return v;
    }

    Value v_float(Source::Pos pos, float f) {
        Value v(pos, T_FLOAT, nullptr);
        v.data.f32 = f;
        return v;
    }

    Value v_double(Source::Pos pos, double d) {
        Value v(pos, T_DOUBLE, nullptr);
        v.data.f64 = d;
        return v;
    }

    Value v_symbol(Source::Pos pos, Symbol symbol) {
        Value v(pos, T_SYMBOL, nullptr);
        v.data.sym = symbol;
        return v;
    }

    Value v_type(Source::Pos pos, Type type) {
        Value v(pos, T_TYPE, nullptr);
        v.data.type = type;
        return v;
    }

    Value v_char(Source::Pos pos, rune r) {
        Value v(pos, T_CHAR, nullptr);
        v.data.ch = r;
        return v;
    }

    Value v_void(Source::Pos pos) {
        return Value(pos, T_VOID, nullptr);
    }
    
    Value v_error(Source::Pos pos) {
        return Value(pos, T_ERROR, nullptr);
    }

    Value v_string(Source::Pos pos, const ustring& str) {
        Value v(pos, T_STRING, nullptr);
        v.data.str = ref<String>(str);
        return v;
    }

    Value v_cons(Source::Pos pos, Type type, const Value& head, const Value& tail) {
        Value v(pos, type, nullptr);
        if (!type.of(K_LIST)) 
            panic("Attempted to construct list with non-list type!");
        if (!head.type.coerces_to(t_list_element(type)))
            panic("Cannot construct list - provided head incompatible with chosen type!");
        if (!tail.type.coerces_to(type)) 
            panic("Cannot construct list - provided tail incompatible with chosen type!"); 
        v.data.list = ref<List>(coerce(head, t_list_element(type)), tail.type.of(K_VOID) ? nullptr : tail.data.list);
        return v;
    }

    Value v_list(Source::Pos pos, Type type, const vector<Value>& values) {
        if (values.size() == 0) return v_void(pos);

        Value v(pos, type, nullptr);
        if (!type.of(K_LIST))
            panic("Attempted to construct list with non-list type!");
        for (const Value& v : values) 
            if (!v.type.coerces_to(t_list_element(type)))
                panic("Cannot construct list - at least one vector element incompatible with list type!");
        
        rc<List> l = nullptr;
        for (i64 i = i64(values.size()) - 1; i >= 0; i --) // larger signed index to avoid overflow
            l = ref<List>(values[i], l);
        v.data.list = l;
        return v;
    }

    Value v_tuple(Source::Pos pos, Type type, const vector<Value>& values) {
        Value v(pos, type, nullptr);
        if (!type.of(K_TUPLE))
            panic("Attempted to construct tuple with non-tuple type!");
        if (t_tuple_len(type) != values.size())
            panic("Cannot construct tuple - number of provided values differs from tuple type size!");
        for (u32 i = 0; i < values.size(); i ++) {
            if (!values[i].type.coerces_to(t_tuple_at(type, i)))
                panic("Cannot construct tuple - at least one vector element incompatible with member type!");
        }
        v.data.tuple = ref<Tuple>(values);
        return v;
    }
    
    Value v_array(Source::Pos pos, Type type, const vector<Value>& values) {
        Value v(pos, type, nullptr);
        if (!type.of(K_ARRAY))
            panic("Attempted to construct array with non-array type!");
        if (t_array_is_sized(type) && t_array_size(type) != values.size())
            panic("Cannot construct array - number of provided values differs from array type size!");
        for (u32 i = 0; i < values.size(); i ++) {
            if (!values[i].type.coerces_to(t_array_element(type)))
                panic("Cannot construct array - at least one vector element incompatible with element type!");
        }
        v.data.array = ref<Array>(values);
        return v;
    }
    
    Value v_union(Source::Pos pos, Type type, const Value& value) {
        Value v(pos, type, nullptr);
        if (!type.of(K_UNION))
            panic("Attempted to construct union with non-union type!");
        if (!value.type.coerces_to(type))
            panic("Cannot construct union - provided value is not a valid union member!");
        v.data.u = ref<Union>(value);
        return v;
    }
    
    Value v_named(Source::Pos pos, Type type, const Value& value) {
        Value v(pos, type, nullptr);
        if (!type.of(K_NAMED))
            panic("Attempted to construct named value with non-named type!");
        if (!value.type.coerces_to(type))
            panic("Cannot construct named value - provided value is of an incompatible type!");
        v.data.named = ref<Named>(value);
        return v;
    }
    
    Value v_struct(Source::Pos pos, Type type, const map<Symbol, Value>& fields) {
        Value v(pos, type, nullptr);
        if (!type.of(K_STRUCT))
            panic("Attempted to construct struct value with non-struct type!");
        if (t_struct_len(type) != fields.size())
            panic("Cannot construct struct - wrong number of field values provided!");
        Type t = infer_struct(fields);
        if (!t.coerces_to(type)) 
            panic("Cannot construct struct - inferred type from fields is incompatible with desired type!");
        v.data.str = ref<Struct>(fields);
        return v;
    }

    Value v_dict(Source::Pos pos, Type type, const map<Value, Value>& entries) {
        Value v(pos, type, nullptr);
        if (!type.of(K_DICT))
            panic("Attempted to construct dict value with non-dictionary type!");
        for (const auto& [k, v] : entries) {
            if (!k.type.coerces_to(t_dict_key(type)))
                panic("Cannot construct dict - at least one pair has an incompatible key type!");
            if (!v.type.coerces_to(t_dict_value(type)))
                panic("Cannot construct dict - at least one pair has an incompatible value type!");
        }
        v.data.dict = ref<Dict>(entries);
        return v;
    }

    const_list_iterator::const_list_iterator(const rc<List> l): list(l.raw()) {}
    
    const_list_iterator::const_list_iterator(const List* l): list(l) {}

    const Value& const_list_iterator::operator*() const {
        return list->head;
    }

    const Value* const_list_iterator::operator->() const {
        return &list->head;
    }

    bool const_list_iterator::operator==(const const_list_iterator& other) const {
        return list == other.list;
    }

    bool const_list_iterator::operator!=(const const_list_iterator& other) const {
        return list != other.list;
    }

    const_list_iterator& const_list_iterator::operator++() {
        if (list) list = list->tail.raw();
        return *this;
    }

    const_list_iterator const_list_iterator::operator++(int) {
        const_list_iterator prev = *this;
        operator++();
        return prev;
    }

    list_iterator::list_iterator(rc<List> l): list(l.raw()) {}

    list_iterator::list_iterator(List* l): list(l) {}

    Value& list_iterator::operator*() {
        return list->head;
    }

    Value* list_iterator::operator->() {
        return &list->head;
    }

    bool list_iterator::operator==(const list_iterator& other) const {
        return list == other.list;
    }

    bool list_iterator::operator!=(const list_iterator& other) const {
        return list != other.list;
    }

    list_iterator& list_iterator::operator++() {
        if (list) list = list->tail.raw();
        return *this;
    }

    list_iterator list_iterator::operator++(int) {
        list_iterator prev = *this;
        operator++();
        return prev;
    }

    list_iterator::operator const_list_iterator() const {
        return const_list_iterator(list);
    }

    list_iterable::list_iterable(rc<List> l): list(l.raw()) {}

    list_iterator list_iterable::begin() {
        return list_iterator(list);
    }

    list_iterator list_iterable::end() {
        return list_iterator(nullptr);
    }

    const_list_iterator list_iterable::begin() const {
        return const_list_iterator(list);
    }

    const_list_iterator list_iterable::end() const {
        return const_list_iterator(nullptr);
    }

    list_iterable iter_list(const Value& v) {
        if (!v.type.of(K_LIST)) panic("Expected list value!");
        return list_iterable(v.data.list);
    }

    Type infer_list(const Value& head, const Value& tail) {
        if (!tail.type.of(K_LIST) && !tail.type.of(K_VOID)) 
            return T_ERROR; // can't form list of non-list tail
        if (tail.type.of(K_VOID)) 
            return t_list(head.type); // single-element list takes head as element type
        if (tail.type.of(K_LIST) && head.type == t_list_element(tail.type)) 
            return tail.type; // if head is compatible, keep the list going
        else 
            return t_list(T_ANY); // otherwise generify list
    }

    Type infer_list(const vector<Value>& values) {
        if (values.size() == 0) return t_list(T_ANY); // return generic list if we didn't get any elements
        Type t = values[0].type;
        for (const Value& v : values) {
            if (v.type != t) return t_list(T_ANY); // if we see heterogeneous types, generify list
        }
        return t_list(t); // return concrete list type if there wasn't a mismatch
    }

    Type infer_tuple(const vector<Value>& values) {
        vector<Type> ts;
        for (const Value& v : values) ts.push(v.type);
        return t_tuple(ts);
    }

    Type infer_array(const vector<Value>& values) {
        if (values.size() == 0) return t_array(T_ANY, values.size()); // return generic array if we didn't see any elements
        Type t = values[0].type;
        for (const Value& v : values) {
            if (v.type != t) return t_array(T_ANY, values.size()); // if we see heterogeneous types, generify array
        }
        return t_array(t, values.size()); // return concrete array if we didn't find a mismatch
    }

    Type infer_struct(const map<Symbol, Value>& fields) {
        map<Symbol, Type> fieldts;
        for (const auto& [s, v] : fields) fieldts.put(s, v.type);
        return t_struct(fieldts);
    }

    Type infer_dict(const map<Value, Value>& entries) {
        if (entries.size() == 0) return t_dict(T_ANY, T_ANY); // return generic dictionary if we didn't get any entries
        Type kt = entries.begin()->first.type, vt = entries.begin()->second.type;
        for (const auto& [k, v] : entries) {
            if (!kt.of(K_ANY) && k.type != kt) kt = T_ANY; // generify if we see a key type mismatch
            if (!vt.of(K_ANY) && v.type != vt) vt = T_ANY; // generify if we see a value type mismatch
        }
        return t_dict(kt, vt);
    }

    const Value& v_head(const Value& list) {
        if (!list.type.of(K_LIST)) panic("Expected a list value!");
        if (!list.data.list) panic("Attempted to get head of empty list!");
        return list.data.list->head;
    }

    void v_set_head(Value& list, const Value& v) {
        if (!list.type.of(K_LIST)) panic("Expected a list value!");
        if (!list.data.list) panic("Attempted to set head of empty list!");
        if (!v.type.coerces_to(t_list_element(list.type))) 
            panic("Attempted to set list head to value of incompatible type!");
        list.data.list->head = coerce(v, t_list_element(list.type));
    }

    Value v_tail(const Value& list) {
        if (!list.type.of(K_LIST)) panic("Expected a list value!");
        if (!list.data.list) panic("Attempted to get tail of empty list!");
        if (!list.data.list->tail) return v_void(list.pos);
        else {
            Value v(list.pos, list.type, list.form);
            v.data.list = list.data.list->tail;
            return v;
        }
    }

    u32 v_list_len(const Value& list) {
        if (list.type.of(K_VOID)) return 0; // empty list
        if (!list.type.of(K_LIST)) panic("Expected a list value!");
        rc<List> l = list.data.list;
        u32 size = 0;
        while (l) {
            size ++;
            l = l->tail;
        }
        return size;
    }

    bool is_empty(const Value& v) {
        return v.type.of(K_VOID) || (v.type.of(K_LIST) && !v.data.list);
    }

    u32 v_tuple_len(const Value& tuple) {
        if (!tuple.type.of(K_TUPLE)) panic("Expected a tuple value!");
        return tuple.data.tuple->members.size();
    }

    const Value& v_tuple_at(const Value& tuple, u32 i) {
        if (!tuple.type.of(K_TUPLE)) panic("Expected a tuple value!");
        return tuple.data.tuple->members[i];
    }
    
    void v_tuple_set(Value& tuple, u32 i, const Value& v) {
        if (!tuple.type.of(K_TUPLE)) panic("Expected a tuple value!");
        if (!v.type.coerces_to(t_tuple_at(tuple.type, i))) 
            panic("Attempted to set tuple member to value of incompatible type!");
        tuple.data.tuple->members[i] = coerce(v, t_tuple_at(tuple.type, i));
    }

    const vector<Value>& v_tuple_elements(const Value& tuple) {
        if (!tuple.type.of(K_TUPLE)) panic("Expected a tuple value!");
        return tuple.data.tuple->members;
    }

    u32 v_array_len(const Value& array) {
        if (!array.type.of(K_ARRAY)) panic("Expected an array value!");
        return array.data.array->elements.size();
    }

    const Value& v_array_at(const Value& array, u32 i) {
        if (!array.type.of(K_ARRAY)) panic("Expected an array value!");
        return array.data.array->elements[i];
    }

    void v_array_set(Value& array, u32 i, const Value& v) {
        if (!array.type.of(K_ARRAY)) panic("Expected an array value!");
        if (!v.type.coerces_to(t_array_element(array.type))) 
            panic("Attempted to set array element to value of incompatible type!");
        array.data.array->elements[i] = coerce(v, t_array_element(array.type));
    }

    const vector<Value>& v_array_elements(const Value& array) {
        if (!array.type.of(K_ARRAY)) panic("Expected an array value!");
        return array.data.array->elements;
    }

    Type v_cur_type(const Value& u) {
        if (!u.type.of(K_UNION)) panic("Expected a union value!");
        return u.data.u->value.type;
    }

    const Value& v_current(const Value& u) {
        if (!u.type.of(K_UNION)) panic("Expected a union value!");
        return u.data.u->value;
    }

    void v_set_current(Value& u, const Value& v) {
        if (!u.type.of(K_UNION)) panic("Expected a union value!");
        if (!t_union_has(u.type, v.type)) 
            panic("Attempted to set current member of union to non-member type!");
        u.data.u->value = v;
    }

    Symbol v_get_name(const Value& named) {
        if (!named.type.of(K_NAMED)) panic("Expected a named value!");
        return t_get_name(named.type);
    }

    const Value& v_get_base(const Value& named) {
        if (!named.type.of(K_NAMED)) panic("Expected a named value!");
        return named.data.named->value;
    }

    bool v_struct_has(const Value& str, Symbol field) {
        if (!str.type.of(K_STRUCT)) panic("Expected a struct value!");
        return t_struct_has(str.type, field);
    }

    const Value& v_struct_at(const Value& str, Symbol field) {
        if (!str.type.of(K_STRUCT)) panic("Expected a struct value!");
        if (!t_struct_has(str.type, field)) panic("Attempted to get nonexistent struct field!");
        auto it = str.data.str->fields.find(field);
        if (it == str.data.str->fields.end()) panic("Attempted to get nonexistent struct field!");
        return it->second;
    }

    void v_struct_set(Value& str, Symbol field, const Value& v) {
        if (!str.type.of(K_STRUCT)) panic("Expected a struct value!");
        if (!t_struct_has(str.type, field)) panic("Attempted to set nonexistent struct field!");
        if (!v.type.coerces_to(t_struct_field(str.type, field)))
            panic("Attempted to set struct field to value of incompatible type!");
        auto it = str.data.str->fields.find(field);
        if (it == str.data.str->fields.end()) panic("Attempted to get nonexistent struct field!");
        it->second = coerce(v, t_struct_field(str.type, field));
    }

    const map<Symbol, Value>& v_struct_fields(const Value& str) {
        if (!str.type.of(K_STRUCT)) panic("Expected a struct value!");
        return str.data.str->fields;
    }

    bool v_dict_has(const Value& dict, const Value& key) {
        if (!dict.type.of(K_DICT)) panic("Expected a dictionary value!");
        if (!key.type.coerces_to(t_dict_key(dict.type))) 
            panic("Attempted to check whether dict contains key of incompatible type!");
        return dict.data.dict->elements.contains(coerce(key, t_dict_key(dict.type)));
    }

    const Value& v_dict_at(const Value& dict, const Value& key) {
        if (!dict.type.of(K_DICT)) panic("Expected a dictionary value!");
        if (!key.type.coerces_to(t_dict_key(dict.type)))
            panic("Attempted to access dict by key of incompatible type!");
        auto it = dict.data.dict->elements.find(coerce(key, t_dict_key(dict.type)));
        if (it == dict.data.dict->elements.end()) 
            panic("Attempted to access dict by nonexistent key!");
        return it->second;
    }

    void v_dict_put(Value& dict, const Value& key, const Value& value) {
        if (!dict.type.of(K_DICT)) panic("Expected a dictionary value!");
        if (!key.type.coerces_to(t_dict_key(dict.type)))
            panic("Attempted to put key of incompatible type into dictionary!");
        if (!value.type.coerces_to(t_dict_value(dict.type)))
            panic("Attempted to put value of incompatible type into dictionary!");
        dict.data.dict->elements.put(
            coerce(key, t_dict_key(dict.type)), 
            coerce(value, t_dict_value(dict.type))
        );
    }

    void v_dict_erase(Value& dict, const Value& key) {
        if (!dict.type.of(K_DICT)) panic("Expected a dictionary value!");
        if (!key.type.coerces_to(t_dict_key(dict.type)))
            panic("Attempted to access dict by key of incompatible type!");
        dict.data.dict->elements.erase(coerce(key, t_dict_key(dict.type)));
    }

    const map<Value, Value>& v_dict_elements(const Value& dict) {
        if (!dict.type.of(K_DICT)) panic("Expected a dictionary value!");
        return dict.data.dict->elements;
    }

    optional<map<Symbol, Value>> v_match(const Value& pattern, const Value& v) {
        using Bindings = map<Symbol, Value>;
        return none<Bindings>(); // no match found
    }

    Value coerce(const Value& src, Type target) {
        if (target.of(K_ANY)) return src; // anything can freely convert to the 'any' type
        if (src.type != target) panic("Implicit conversion rules are unimplemented!");
        return src;
    }
}

void write(stream& io, const basil::Value& value) {
    value.format(io);
}