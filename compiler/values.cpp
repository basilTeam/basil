#include "values.h"
#include "ast.h"
#include "builtin.h"
#include "env.h"
#include "eval.h"
#include "native.h"
#include "util/vec.h"

namespace basil {
    static map<string, u64> symbol_table;
    static vector<string> symbol_array;

    u64 symbol_value(const string& symbol) {
        static u64 count = 0;
        auto it = symbol_table.find(symbol);
        if (it == symbol_table.end()) {
            symbol_table.put(symbol, count++);
            symbol_array.push(symbol);
            return count - 1;
        } else
            return it->second;
    }

    const string& symbol_for(u64 value) {
        return symbol_array[value];
    }

    Value::Value() : Value(VOID) {}

    Value::Value(const Type* type) : _type(type) {}

    Value::Value(i64 i, const Type* type) : _type(type) {
        is_bool() ? _data.b = i : _data.i = i;
    }

    Value::Value(const string& s, const Type* type) : _type(type) {
        if (type == SYMBOL) _data.u = symbol_value(s);
        else if (type == STRING)
            _data.rc = new StringValue(s);
    }

    Value::Value(const Type* type_value, const Type* type) : _type(type) {
        _data.t = type_value;
    }

    Value::Value(ListValue* l) : _type(find<ListType>(l->head().type())) {
        _data.rc = l;
    }

    Value::Value(SumValue* s, const Type* type) : _type(type) {
        _data.rc = s;
    }

    Value::Value(IntersectValue* i, const Type* type) : _type(type) {
        _data.rc = i;
    }

    Value::Value(ProductValue* p) {
        vector<const Type*> ts;
        for (const Value& v : *p) ts.push(v.type());
        _type = find<ProductType>(ts);
        _data.rc = p;
    }

    Value::Value(ArrayValue* a) {
        set<const Type*> ts;
        for (const Value& v : *a) ts.insert(v.type());
        if (ts.size() == 0) ts.insert(ANY);
        _type = find<ArrayType>(ts.size() > 1 ? find<SumType>(ts) : *ts.begin(), a->size());
        _data.rc = a;
    }

    Value::Value(ArrayValue* a, const Type* t) {
        _type = t;
        _data.rc = a;
    }

    Value::Value(DictValue* d) {
        const auto& pair = *d->begin();
        _type = find<DictType>(pair.first.type(), pair.second.type());
        _data.rc = d;
    }

    Value::Value(ref<Env> env, const Builtin& b) : _type(b.type()) {
        if (b.type()->kind() == KIND_FUNCTION) _data.rc = new FunctionValue(env, b);
        else
            _data.rc = new MacroValue(env, b);
    }

    Value::Value(ref<Env> env, const Builtin& b, const string& name) : Value(env, b) {
        set_name(name);
    }

    Value::Value(FunctionValue* f, const Type* ftype) : _type(ftype) {
        _data.rc = f;
    }

    Value::Value(AliasValue* a) : _type(ALIAS) {
        _data.rc = a;
    }

    Value::Value(MacroValue* m) : _type(find<MacroType>(m->arity())) {
        _data.rc = m;
    }

    Value::Value(NamedValue* n, const Type* t) : _type(t) {
        _data.rc = n;
    }

    Value::Value(ModuleValue* m) : _type(MODULE) {
        _data.rc = m;
    }

    Value::Value(ASTNode* n) : 
        _type(n->type()->kind() == KIND_RUNTIME ? n->type() : find<RuntimeType>(n->type())) {
        _data.rc = n;
    }

    Value::~Value() {
        if (_type->kind() & GC_KIND_FLAG) _data.rc->dec();
    }

    Value::Value(const Value& other) : _type(other._type), _loc(other._loc), _name(other._name) {
        _data.u = other._data.u; // copy over raw data
        if (_type->kind() & GC_KIND_FLAG) _data.rc->inc();
    }

    Value& Value::operator=(const Value& other) {
        if (other.type()->kind() & GC_KIND_FLAG) other._data.rc->inc();
        if (_type->kind() & GC_KIND_FLAG) _data.rc->dec();
        _type = other._type;
        _loc = other._loc;
        _name = other._name;
        _data.u = other._data.u; // copy over raw data
        return *this;
    }

    bool Value::is_int() const {
        return _type == INT;
    }

    i64 Value::get_int() const {
        return _data.i;
    }

    i64& Value::get_int() {
        return _data.i;
    }

    bool Value::is_symbol() const {
        return _type == SYMBOL;
    }

    u64 Value::get_symbol() const {
        return _data.u;
    }

    u64& Value::get_symbol() {
        return _data.u;
    }

    bool Value::is_string() const {
        return _type == STRING;
    }

    const string& Value::get_string() const {
        return ((const StringValue*)_data.rc)->value();
    }

    string& Value::get_string() {
        return ((StringValue*)_data.rc)->value();
    }

    bool Value::is_void() const {
        return _type == VOID;
    }

    bool Value::is_error() const {
        return _type == ERROR;
    }

    bool Value::is_type() const {
        return _type == TYPE;
    }

    const Type* Value::get_type() const {
        return _data.t;
    }

    const Type*& Value::get_type() {
        return _data.t;
    }

    bool Value::is_bool() const {
        return _type == BOOL;
    }

    bool Value::get_bool() const {
        return _data.b;
    }

    bool& Value::get_bool() {
        return _data.b;
    }

    bool Value::is_list() const {
        return _type->kind() == KIND_LIST;
    }

    const ListValue& Value::get_list() const {
        return *(const ListValue*)_data.rc;
    }

    ListValue& Value::get_list() {
        return *(ListValue*)_data.rc;
    }

    bool Value::is_array() const {
        return _type->kind() == KIND_ARRAY;
    }

    const ArrayValue& Value::get_array() const {
        return *(const ArrayValue*)_data.rc;
    }

    ArrayValue& Value::get_array() {
        return *(ArrayValue*)_data.rc;
    }

    bool Value::is_sum() const {
        return _type->kind() == KIND_SUM;
    }

    const SumValue& Value::get_sum() const {
        return *(const SumValue*)_data.rc;
    }

    SumValue& Value::get_sum() {
        return *(SumValue*)_data.rc;
    }

    bool Value::is_intersect() const {
        return _type->kind() == KIND_INTERSECT;
    }

    const IntersectValue& Value::get_intersect() const {
        return *(const IntersectValue*)_data.rc;
    }

    IntersectValue& Value::get_intersect() {
        return *(IntersectValue*)_data.rc;
    }

    bool Value::is_product() const {
        return _type->kind() == KIND_PRODUCT;
    }

    const ProductValue& Value::get_product() const {
        return *(const ProductValue*)_data.rc;
    }

    ProductValue& Value::get_product() {
        return *(ProductValue*)_data.rc;
    }

    bool Value::is_dict() const {
        return _type->kind() == KIND_DICT;
    }

    const DictValue& Value::get_dict() const {
        return *(const DictValue*)_data.rc;
    }

    DictValue& Value::get_dict() {
        return *(DictValue*)_data.rc;
    }

    bool Value::is_function() const {
        return _type->kind() == KIND_FUNCTION;
    }

    const FunctionValue& Value::get_function() const {
        return *(const FunctionValue*)_data.rc;
    }

    FunctionValue& Value::get_function() {
        return *(FunctionValue*)_data.rc;
    }

    bool Value::is_alias() const {
        return _type->kind() == KIND_ALIAS;
    }

    const AliasValue& Value::get_alias() const {
        return *(const AliasValue*)_data.rc;
    }

    AliasValue& Value::get_alias() {
        return *(AliasValue*)_data.rc;
    }

    bool Value::is_macro() const {
        return _type->kind() == KIND_MACRO;
    }

    const MacroValue& Value::get_macro() const {
        return *(const MacroValue*)_data.rc;
    }

    MacroValue& Value::get_macro() {
        return *(MacroValue*)_data.rc;
    }

    bool Value::is_runtime() const {
        return _type->kind() == KIND_RUNTIME;
    }

    ASTNode* Value::get_runtime() const {
        return (ASTNode*)_data.rc;
    }

    ASTNode*& Value::get_runtime() {
        return (ASTNode*&)_data.rc;
    }    
    
    bool Value::is_named() const {
        return _type->kind() == KIND_NAMED;
    }

    const NamedValue& Value::get_named() const {
        return *(const NamedValue*)_data.rc;
    }

    NamedValue& Value::get_named() {
        return *(NamedValue*)_data.rc;
    }

    bool Value::is_module() const {
        return _type == MODULE;
    }

    const ModuleValue& Value::get_module() const {
        return *(const ModuleValue*)_data.rc;
    }

    ModuleValue& Value::get_module() {
        return *(ModuleValue*)_data.rc;
    }

    const Type* Value::type() const {
        return _type;
    }

    void Value::format(stream& io) const {
        if (_name != -1) write(io, symbol_for(_name));
        else if (is_void()) write(io, "()");
        else if (is_error())
            write(io, "error");
        else if (is_int())
            write(io, get_int());
        else if (is_symbol())
            write(io, symbol_for(get_symbol()));
        else if (is_string())
            write(io, '"', get_string(), '"');
        else if (is_type())
            write(io, get_type());
        else if (is_bool())
            write(io, get_bool());
        else if (is_named())
            write(io, ((const NamedType*)type())->name(), "(", get_named().get(), ")");
        else if (is_list()) {
            bool first = true;
            write(io, "(");
            const Value* ptr = this;
            while (ptr->is_list()) {
                write(io, first ? "" : " ", ptr->get_list().head());
                ptr = &ptr->get_list().tail();
                first = false;
            }
            write(io, ")");
        } else if (is_array()) {
            bool first = true;
            write(io, "[");
            for (const Value& v : get_array()) {
                write(io, first ? "" : ", ", v);
                first = false;
            }
            write(io, "]");
        } else if (is_sum())
            write(io, get_sum().value());
        else if (is_intersect()) {
            bool first = true;
            write(io, "(");
            for (const auto& p : get_intersect()) {
                write(io, first ? "" : " & ", p.second);
                first = false;
            }
            write(io, ")");
        } else if (is_product()) {
            bool first = true;
            write(io, "(");
            for (const Value& v : get_product()) {
                write(io, first ? "" : ", ", v);
                first = false;
            }
            write(io, ")");
        } else if (is_dict()) {
            const DictType* dt = (const DictType*)type();
            bool first = true;
            write(io, "{");
            for (const auto& pair : get_dict()) {
                write(io, first ? "" : " ", pair.first);
                if (dt->value() != VOID) write(io, ": ", pair.second);
                first = false;
            }  
            write(io, "}");
        } else if (is_function())
            write(io, "<#", get_function().name() < 0 ? "procedure" : symbol_for(get_function().name()), ">");
        else if (is_alias())
            write(io, "<#alias>");
        else if (is_macro())
            write(io, "<#macro>");
        else if (is_runtime())
            write(io, "<#runtime ", ((const RuntimeType*)_type)->base(), ">");
        else if (is_module())
            write(io, "<#module>");
    }

    u64 Value::hash() const {
        if (is_void()) return 11103515024943898793ul;
        else if (is_error())
            return 14933118315469276343ul;
        else if (is_int())
            return ::hash(get_int()) ^ 6909969109598810741ul;
        else if (is_symbol())
            return ::hash(get_symbol()) ^ 1899430078708870091ul;
        else if (is_string())
            return ::hash(get_string()) ^ 1276873522146073541ul;
        else if (is_type())
            return get_type()->hash();
        else if (is_bool())
            return get_bool() ? 9269586835432337327ul : 18442604092978916717ul;
        else if (is_named())
            return get_named().get().hash() ^ (5789283014586986071ul * ::hash(((const NamedType*)type())->name()));
        else if (is_list()) {
            u64 h = 9572917161082946201ul;
            Value ptr = *this;
            while (ptr.is_list()) {
                h ^= ptr.get_list().head().hash();
                ptr = ptr.get_list().tail();
            }
            return h;
        } else if (is_sum()) {
            return get_sum().value().hash() ^ 7458465441398727979ul;
        } else if (is_intersect()) {
            u64 h = 1250849227517037781ul;
            for (const auto& p : get_intersect()) h ^= p.second.hash();
            return h;
        } else if (is_product()) {
            u64 h = 16629385277682082909ul;
            for (const Value& v : get_product()) h ^= v.hash();
            return h;
        } else if (is_array()) {
            u64 h = 7135911592309895053ul;
            for (const Value& v : get_array()) h ^= v.hash();
            return h;
        } else if (is_dict()) {
            u64 h = 13974436514101026401ul;  
            for (const auto& p : get_dict()) h ^= 14259444292234844953ul * p.first.hash() ^ p.second.hash();
            return h;
        } else if (is_function()) {
            u64 h = 10916307465547805281ul;
            if (get_function().is_builtin()) h ^= ::hash(&get_function().get_builtin());
            else {
                h ^= get_function().body().hash();
                for (u64 arg : get_function().args()) h ^= ::hash(arg);
            }
            return h;
        } else if (is_alias())
            return 6860110315984869641ul;
        else if (is_macro()) {
            u64 h = 16414641732770006573ul;
            if (get_macro().is_builtin()) h ^= ::hash(&get_macro().get_builtin());
            else {
                h ^= get_macro().body().hash();
                for (u64 arg : get_macro().args()) h ^= ::hash(arg);
            }
            return h;
        } else if (is_runtime()) {
            return _type->hash() ^ ::hash(_data.rc);
        } else if (is_module()) {
            u64 h = 6343561091602366673ul;
            for (auto& p : get_module().entries()) {
                h ^= 12407217216741519607ul * ::hash(p.first);
                h ^= p.second.hash();
            }
        }
        return 0;
    }

    bool Value::operator==(const Value& other) const {
        if (type() != other.type()) return false;
        else if (is_int())
            return get_int() == other.get_int();
        else if (is_symbol())
            return get_symbol() == other.get_symbol();
        else if (is_type())
            return get_type() == other.get_type();
        else if (is_bool())
            return get_bool() == other.get_bool();
        else if (is_string())
            return get_string() == other.get_string();
        else if (is_named())
            return get_named().get() == other.get_named().get();
        else if (is_sum())
            return get_sum().value() == other.get_sum().value();
        else if (is_intersect()) {
            if (get_intersect().size() != other.get_intersect().size()) return false;
            for (const auto& p : get_intersect()) {
                if (!other.get_intersect().has(p.first)) return false;
                if (p.second != other.get_intersect().values()[p.first]) return false;
            }
            return true;
        } else if (is_product()) {
            if (get_product().size() != other.get_product().size()) return false;
            for (u32 i = 0; i < get_product().size(); i++)
                if (get_product()[i] != other.get_product()[i]) return false;
            return true;
        } else if (is_array()) {
            if (get_array().size() != other.get_array().size()) return false;
            for (u32 i = 0; i < get_array().size(); i++)
                if (get_array()[i] != other.get_array()[i]) return false;
            return true;
        } else if (is_list()) {
            const Value *l = this, *o = &other;
            while (l->is_list() && o->is_list()) {
                if (l->get_list().head() != o->get_list().head()) return false;
                l = &l->get_list().tail(), o = &o->get_list().tail();
            }
            return l->is_void() && o->is_void();
        } else if (is_dict()) {
            if (get_dict().size() != other.get_dict().size()) return false;
            for (const auto& p : get_dict()) {
                auto it = other.get_dict().entries().find(p.first);
                if (it == other.get_dict().entries().end()) return false;
                if (it->second != p.second) return false;
            }
            return true;
        } else if (is_function()) {
            if (get_function().is_builtin())
                return &get_function().get_builtin() == &other.get_function().get_builtin();
            else {
                if (other.get_function().arity() != get_function().arity()) return false;
                for (u32 i = 0; i < get_function().arity(); i++) {
                    if (other.get_function().args()[i] != get_function().args()[i]) return false;
                }
                return get_function().body() == other.get_function().body();
            }
        } else if (is_macro()) {
            if (get_macro().is_builtin()) return &get_macro().get_builtin() == &other.get_macro().get_builtin();
            else {
                if (other.get_macro().arity() != get_macro().arity()) return false;
                for (u32 i = 0; i < get_macro().arity(); i++) {
                    if (other.get_macro().args()[i] != get_macro().args()[i]) return false;
                }
                return get_macro().body() == other.get_macro().body();
            }
        } else if (is_runtime()) {
            return _data.rc == other._data.rc;
        } else if (is_module()) {
            if (get_module().entries().size() != other.get_module().entries().size())
                return false;
            for (auto& p : get_module().entries()) {
                auto it = other.get_module().entries().find(p.first);
                if (it == other.get_module().entries().end()) return false;
                if (it->second != p.second) return false;
            }
            return true;
        }
        return type() == other.type();
    }

    Value Value::clone() const {
        if (is_list()) {
            ListValue* l = nullptr;
            const ListValue* i = &get_list();
            vector<const Value*> vals;
            while (i) vals.push(&i->head()), i = i->tail().is_void() ? nullptr : &i->tail().get_list();
            for (i64 i = i64(vals.size()) - 1; i >= 0; i --) l = new ListValue(vals[i]->clone(), l ? l : empty());
            Value result = Value(l);
            result.set_location(_loc);
            result._name = _name;
            return result;
        }
        else if (is_string()) {
            Value result =  Value(get_string(), STRING);
            result.set_location(_loc);
            result._name = _name;
            return result;
        }
        else if (is_named()) {
            Value result = Value(new NamedValue(get_named().get().clone()), type());
            result.set_location(_loc);
            return result;
        }
        else if (is_sum()) {
            Value result = Value(new SumValue(get_sum().value().clone()), type());
            result.set_location(_loc);
            result._name = _name;
            return result;
        }
        else if (is_intersect()) {
            map<const Type*, Value> values;
            for (const auto& p : get_intersect()) values.put(p.first, p.second.clone());
            Value result = Value(new IntersectValue(values), type());
            result.set_location(_loc);
            result._name = _name;
            return result;
        } else if (is_product()) {
            vector<Value> values;
            for (const Value& v : get_product()) values.push(v.clone());
            Value result = Value(new ProductValue(values));
            result.set_location(_loc);
            result._name = _name;
            return result;
        } else if (is_array()) {
            vector<Value> values;
            for (const Value& v : get_array()) values.push(v.clone());
            Value result = Value(new ArrayValue(values));
            result.set_location(_loc);
            result._name = _name;
            return result;
        } else if (is_dict()) {
            map<Value, Value> entries;
            for (const auto& p : get_dict()) entries.put(p.first.clone(), p.second.clone());
            Value result = Value(new DictValue(entries));
            result.set_location(_loc);
            result._name = _name;
            return result;
        } else if (is_function()) {
            if (get_function().is_builtin()) {
                Value result = Value(new FunctionValue(get_function().get_env()->clone(), get_function().get_builtin(),
                                               get_function().name()),
                             type());
                result.set_location(_loc);
                result._name = _name;
                return result;
            } else {
                Value result = Value(new FunctionValue(get_function().get_env()->clone(), get_function().args(),
                                               get_function().body().clone()),
                             type());
                result.set_location(_loc);
                result._name = _name;
                return result;
            }
        } else if (is_alias())
            return Value(new AliasValue(get_alias().value()));
        else if (is_macro()) {
            if (get_macro().is_builtin()) {
                Value result = Value(new MacroValue(get_macro().get_env()->clone(), get_macro().get_builtin()));
                result.set_location(_loc);
                result._name = _name;
                return result;
            } else {
                Value result = Value(
                    new MacroValue(get_macro().get_env()->clone(), get_macro().args(), get_macro().body().clone()));
                result.set_location(_loc);
                result._name = _name;
                return result;
            }
        } else if (is_runtime()) {
            // todo: ast cloning
        } else if (is_module()) {
            map<u64, Value> members;
            for (auto& p : get_module().entries()) members[p.first] = p.second.clone();
            Value result = new ModuleValue(members);
            result.set_location(_loc);
            result._name = _name;
            return result;
        }
        return *this;
    }

    bool Value::operator!=(const Value& other) const {
        return !(*this == other);
    }

    void Value::set_location(SourceLocation loc) {
        _loc = loc;
    }

    void Value::set_name(const string& name) {
        _name = symbol_value(name);
    }

    SourceLocation Value::loc() const {
        return _loc;
    }

    StringValue::StringValue(const string& value) : _value(value) {}

    string& StringValue::value() {
        return _value;
    }

    const string& StringValue::value() const {
        return _value;
    }

    NamedValue::NamedValue(const Value& inner):
        _inner(inner) {}

    Value& NamedValue::get() {
        return _inner;
    }

    const Value& NamedValue::get() const {
        return _inner;
    }

    ListValue::ListValue(const Value& head, const Value& tail) : _head(head), _tail(tail) {}

    Value& ListValue::head() {
        return _head;
    }

    const Value& ListValue::head() const {
        return _head;
    }

    Value& ListValue::tail() {
        return _tail;
    }

    const Value& ListValue::tail() const {
        return _tail;
    }

    SumValue::SumValue(const Value& value) : _value(value) {}

    Value& SumValue::value() {
        return _value;
    }

    const Value& SumValue::value() const {
        return _value;
    }

    IntersectValue::IntersectValue(const map<const Type*, Value>& values) : _values(values) {}

    u32 IntersectValue::size() const {
        return _values.size();
    }

    bool IntersectValue::has(const Type* t) const {
        return _values.find(t) != _values.end();
    }

    map<const Type*, Value>::const_iterator IntersectValue::begin() const {
        return _values.begin();
    }

    map<const Type*, Value>::const_iterator IntersectValue::end() const {
        return _values.end();
    }

    map<const Type*, Value>::iterator IntersectValue::begin() {
        return _values.begin();
    }

    map<const Type*, Value>::iterator IntersectValue::end() {
        return _values.end();
    }

    const map<const Type*, Value>& IntersectValue::values() const {
        return _values;
    }

    map<const Type*, Value>& IntersectValue::values() {
        return _values;
    }

    ProductValue::ProductValue(const vector<Value>& values) : _values(values) {}

    u32 ProductValue::size() const {
        return _values.size();
    }

    Value& ProductValue::operator[](u32 i) {
        return _values[i];
    }

    const Value& ProductValue::operator[](u32 i) const {
        return _values[i];
    }

    const Value* ProductValue::begin() const {
        return _values.begin();
    }

    const Value* ProductValue::end() const {
        return _values.end();
    }

    Value* ProductValue::begin() {
        return _values.begin();
    }

    Value* ProductValue::end() {
        return _values.end();
    }

    const vector<Value>& ProductValue::values() const {
        return _values;
    }

    ArrayValue::ArrayValue(const vector<Value>& values) : ProductValue(values) {}

    DictValue::DictValue(const map<Value, Value>& entries):
        _entries(entries) {}

    u32 DictValue::size() const {
        return _entries.size();
    }

    Value* DictValue::operator[](const Value& key) {
        auto it = _entries.find(key);
        if (it == _entries.end()) return nullptr;
        else return &it->second;
    }

    const Value* DictValue::operator[](const Value& key) const {
        auto it = _entries.find(key);
        if (it == _entries.end()) return nullptr;
        else return &it->second;
    }

    const map<Value, Value>::const_iterator DictValue::begin() const {
        return _entries.begin();
    }

    const map<Value, Value>::const_iterator DictValue::end() const {
        return _entries.end();
    }

    map<Value, Value>::iterator DictValue::begin() {
        return _entries.begin();
    }

    map<Value, Value>::iterator DictValue::end() {
        return _entries.end();
    }  

    const map<Value, Value>& DictValue::entries() const {
        return _entries;
    }

    const u64 KEYWORD_ARG_BIT = 1ul << 63;
    const u64 ARG_NAME_MASK = ~KEYWORD_ARG_BIT;

    FunctionValue::FunctionValue(ref<Env> env, const vector<u64>& args, const Value& code, i64 name)
        : _name(name), _code(code), _builtin(nullptr), _env(env), _args(args), _insts(nullptr), _calls(nullptr) {}

    FunctionValue::FunctionValue(ref<Env> env, const Builtin& builtin, i64 name)
        : _name(name), _builtin(&builtin), _env(env), _args(builtin.args()), _insts(nullptr), _calls(nullptr) {}

    FunctionValue::~FunctionValue() {
        if (_insts) {
            for (auto& p : *_insts) p.second->dec();
            delete _insts;
        }
        if (_calls) {
            for (auto& p : *_calls) ((FunctionValue*)p)->dec();
            delete _calls;
        }
    }

    FunctionValue::FunctionValue(const FunctionValue& other)
        : _name(other._name), _code(other._code.clone()), _builtin(other._builtin), _env(other._env),
          _args(other._args), _insts(other._insts ? new map<const Type*, ASTNode*>(*other._insts) : nullptr),
          _calls(other._calls ? new set<const FunctionValue*>(*other._calls) : nullptr) {
        if (_insts)
            for (auto& p : *_insts) p.second->inc();
        if (_calls)
            for (auto p : *_calls) ((FunctionValue*)p)->inc();
    }

    FunctionValue& FunctionValue::operator=(const FunctionValue& other) {
        if (this != &other) {
            if (_insts) {
                for (auto& p : *_insts) p.second->dec();
                delete _insts;
            }
            if (_calls) {
                for (auto& p : *_calls) ((FunctionValue*)p)->dec();
                delete _calls;
            }
            _name = other._name;
            _code = other._code.clone();
            _builtin = other._builtin;
            _env = other._env;
            _args = other._args;
            _insts = other._insts ? new map<const Type*, ASTNode*>(*other._insts) : nullptr;
            _calls = other._calls ? new set<const FunctionValue*>(*other._calls) : nullptr;
            if (_insts)
                for (auto& p : *_insts) p.second->inc();
            if (_calls)
                for (auto& p : *_calls) ((FunctionValue*)p)->inc();
        }
        return *this;
    }

    const vector<u64>& FunctionValue::args() const {
        return _args;
    }

    bool FunctionValue::is_builtin() const {
        return _builtin;
    }

    const Builtin& FunctionValue::get_builtin() const {
        return *_builtin;
    }

    ref<Env> FunctionValue::get_env() {
        return _env;
    }

    const ref<Env> FunctionValue::get_env() const {
        return _env;
    }

    i64 FunctionValue::name() const {
        return _name;
    }

    bool FunctionValue::found_calls() const {
        return _calls;
    }

    bool FunctionValue::recursive() const {
        return _calls && _calls->find(this) != _calls->end();
    }

    void FunctionValue::add_call(const FunctionValue* other) {
        if (!_calls) _calls = new set<const FunctionValue*>();
        if (other != this && other->_calls)
            for (const FunctionValue* f : *other->_calls) {
                _calls->insert(f);
                ((FunctionValue*)f)->inc(); // evil
            }
        _calls->insert(other);
        ((FunctionValue*)other)->inc();
    }

    ASTNode* FunctionValue::instantiation(const Type* type) const {
        if (_insts) {
            auto it = _insts->find(type);
            if (it != _insts->end()) return it->second;
        }
        return nullptr;
    }

    const map<const Type*, ASTNode*>* FunctionValue::instantiations() const {
        return _insts;
    }

    void FunctionValue::instantiate(const Type* type, ASTNode* body) {
        if (!_insts) _insts = new map<const Type*, ASTNode*>();
        auto it = _insts->find(type);
        if (it == _insts->end()) {
            _insts->put(type, body);
        } else {
            it->second->dec();
            it->second = body;
        }
        body->inc();
    }

    u64 FunctionValue::arity() const {
        return _builtin ? ((const FunctionType*)_builtin->type())->arity() : _args.size();
    }

    const Value& FunctionValue::body() const {
        return _code;
    }

    AliasValue::AliasValue(const Value& value) : _value(value) {}

    Value& AliasValue::value() {
        return _value;
    }

    const Value& AliasValue::value() const {
        return _value;
    }

    MacroValue::MacroValue(ref<Env> env, const vector<u64>& args, const Value& code)
        : _code(code), _builtin(nullptr), _env(env), _args(args) {}

    MacroValue::MacroValue(ref<Env> env, const Builtin& builtin) : _builtin(&builtin), _env(env) {}

    const vector<u64>& MacroValue::args() const {
        return _args;
    }

    bool MacroValue::is_builtin() const {
        return _builtin;
    }

    const Builtin& MacroValue::get_builtin() const {
        return *_builtin;
    }

    ref<Env> MacroValue::get_env() {
        return _env;
    }

    const ref<Env> MacroValue::get_env() const {
        return _env;
    }

    u64 MacroValue::arity() const {
        return _builtin ? ((const MacroType*)_builtin->type())->arity() : _args.size();
    }

    const Value& MacroValue::body() const {
        return _code;
    }

    ModuleValue::ModuleValue(const map<u64, Value>& entries):
        _entries(entries) {}

    const map<u64, Value>& ModuleValue::entries() const {
        return _entries;
    }

    bool ModuleValue::has(u64 name) const {
        return _entries.find(name) != _entries.end();
    }

    const Value& ModuleValue::entry(u64 name) const {
        return _entries[name];
    }

    vector<Value> to_vector(const Value& list) {
        vector<Value> values;
        const Value* v = &list;
        while (v->is_list()) {
            values.push(v->get_list().head());
            v = &v->get_list().tail();
        }
        return values;
    }

    Value lower(const Value& v) {
        if (v.is_runtime()) return v;
        else if (v.is_void())
            return new ASTVoid(v.loc());
        else if (v.is_int())
            return new ASTInt(v.loc(), v.get_int());
        else if (v.is_symbol())
            return new ASTSymbol(v.loc(), v.get_symbol());
        else if (v.is_string())
            return new ASTString(v.loc(), v.get_string());
        else if (v.is_bool())
            return new ASTBool(v.loc(), v.get_bool());
        else if (v.is_list()) {
            vector<Value> vals = to_vector(v);
            ASTNode* acc = new ASTVoid(v.loc());
            for (i64 i = vals.size() - 1; i >= 0; i--) {
                Value l = lower(vals[i]);
                acc = new ASTCons(v.loc(), l.get_runtime(), acc);
            }
            return acc;
        } else if (v.is_error())
            return new ASTSingleton(ERROR);
        else {
            err(v.loc(), "Couldn't lower value '", v, "'.");
            return error();
        }
    }

    Value binary_arithmetic(const Value& lhs, const Value& rhs, i64 (*op)(i64, i64)) {
        if (!lhs.is_int() && !lhs.is_error()) {
            err(lhs.loc(), "Expected integer value in arithmetic expression, found '", lhs.type(), "'.");
            return error();
        }
        if (!rhs.is_int() && !rhs.is_error()) {
            err(rhs.loc(), "Expected integer value in arithmetic expression, found '", rhs.type(), "'.");
            return error();
        }
        if (lhs.is_error() || rhs.is_error()) return error();

        return Value(op(lhs.get_int(), rhs.get_int()));
    }

    bool is_runtime_binary(const Value& lhs, const Value& rhs) {
        return lhs.is_runtime() || rhs.is_runtime();
    }

    Value lower(ASTMathOp op, const Value& lhs, const Value& rhs) {
        return new ASTBinaryMath(lhs.loc(), op, lower(lhs).get_runtime(), lower(rhs).get_runtime());
    }

    Value add(const Value& lhs, const Value& rhs) {
        if (is_runtime_binary(lhs, rhs)) return lower(AST_ADD, lhs, rhs);
        return binary_arithmetic(lhs, rhs, [](i64 a, i64 b) -> i64 { return a + b; });
    }

    Value sub(const Value& lhs, const Value& rhs) {
        if (is_runtime_binary(lhs, rhs)) return lower(AST_SUB, lhs, rhs);
        return binary_arithmetic(lhs, rhs, [](i64 a, i64 b) -> i64 { return a - b; });
    }

    Value mul(const Value& lhs, const Value& rhs) {
        if (is_runtime_binary(lhs, rhs)) return lower(AST_MUL, lhs, rhs);
        return binary_arithmetic(lhs, rhs, [](i64 a, i64 b) -> i64 { return a * b; });
    }

    Value div(const Value& lhs, const Value& rhs) {
        if (is_runtime_binary(lhs, rhs)) return lower(AST_DIV, lhs, rhs);
        return binary_arithmetic(lhs, rhs, [](i64 a, i64 b) -> i64 { return a / b; });
    }

    Value rem(const Value& lhs, const Value& rhs) {
        if (is_runtime_binary(lhs, rhs)) return lower(AST_REM, lhs, rhs);
        return binary_arithmetic(lhs, rhs, [](i64 a, i64 b) -> i64 { return a % b; });
    }

    Value binary_logic(const Value& lhs, const Value& rhs, bool (*op)(bool, bool)) {
        if (!lhs.is_bool() && !lhs.is_error()) {
            err(lhs.loc(), "Expected boolean value in logical expression, found '", lhs.type(), "'.");
            return error();
        }
        if (!rhs.is_bool() && !rhs.is_error()) {
            err(rhs.loc(), "Expected boolean value in logical expression, found '", rhs.type(), "'.");
            return error();
        }
        if (lhs.is_error() || rhs.is_error()) return error();

        return Value(op(lhs.get_bool(), rhs.get_bool()), BOOL);
    }

    Value lower(ASTLogicOp op, const Value& lhs, const Value& rhs) {
        return new ASTBinaryLogic(lhs.loc(), op, lower(lhs).get_runtime(), lower(rhs).get_runtime());
    }

    Value logical_and(const Value& lhs, const Value& rhs) {
        if (is_runtime_binary(lhs, rhs)) return lower(AST_AND, lhs, rhs);
        return binary_logic(lhs, rhs, [](bool a, bool b) -> bool { return a && b; });
    }

    Value logical_or(const Value& lhs, const Value& rhs) {
        if (is_runtime_binary(lhs, rhs)) return lower(AST_OR, lhs, rhs);
        return binary_logic(lhs, rhs, [](bool a, bool b) -> bool { return a || b; });
    }

    Value logical_xor(const Value& lhs, const Value& rhs) {
        if (is_runtime_binary(lhs, rhs)) return lower(AST_XOR, lhs, rhs);
        return binary_logic(lhs, rhs, [](bool a, bool b) -> bool { return a ^ b; });
    }

    Value logical_not(const Value& v) {
        if (v.is_runtime()) return new ASTNot(v.loc(), lower(v).get_runtime());

        if (!v.is_bool() && !v.is_error()) {
            err(v.loc(), "Expected boolean value in logical expression, found '", v.type(), "'.");
            return error();
        }
        if (v.is_error()) return error();

        return Value(!v.get_bool(), BOOL);
    }

    Value lower(ASTEqualOp op, const Value& lhs, const Value& rhs) {
        return new ASTBinaryEqual(lhs.loc(), op, lower(lhs).get_runtime(), lower(rhs).get_runtime());
    }

    Value equal(const Value& lhs, const Value& rhs) {
        if (lhs.is_error() || rhs.is_error()) return error();
        if (is_runtime_binary(lhs, rhs)) return lower(AST_EQUAL, lhs, rhs);
        return Value(lhs == rhs, BOOL);
    }

    Value inequal(const Value& lhs, const Value& rhs) {
        if (lhs.is_error() || rhs.is_error()) return error();
        if (is_runtime_binary(lhs, rhs)) return lower(AST_INEQUAL, lhs, rhs);
        return Value(!equal(lhs, rhs).get_bool(), BOOL);
    }

    Value binary_relation(const Value& lhs, const Value& rhs, bool (*int_op)(i64, i64),
                          bool (*string_op)(string, string)) {
        if (!lhs.is_int() && !lhs.is_string() && !lhs.is_error()) {
            err(lhs.loc(), "Expected integer or string value in relational expression, found '", lhs.type(), "'.");
            return error();
        }
        if (!rhs.is_int() && !rhs.is_string() && !rhs.is_error()) {
            err(rhs.loc(), "Expected integer or string value in relational expression, found '", rhs.type(), "'.");
            return error();
        }
        if ((lhs.is_int() && !rhs.is_int()) || (lhs.is_string() && !rhs.is_string())) {
            err(rhs.loc(), "Invalid parameters to relational expression: '", lhs.type(), "' and '", rhs.type(), "'.");
            return error();
        }
        if (lhs.is_error() || rhs.is_error()) return error();

        if (lhs.is_string()) return Value(string_op(lhs.get_string(), rhs.get_string()), BOOL);
        return Value(int_op(lhs.get_int(), rhs.get_int()), BOOL);
    }

    Value lower(ASTRelOp op, const Value& lhs, const Value& rhs) {
        return new ASTBinaryRel(lhs.loc(), op, lower(lhs).get_runtime(), lower(rhs).get_runtime());
    }

    Value less(const Value& lhs, const Value& rhs) {
        if (is_runtime_binary(lhs, rhs)) return lower(AST_LESS, lhs, rhs);
        return binary_relation(
            lhs, rhs, [](i64 a, i64 b) -> bool { return a < b; }, [](string a, string b) -> bool { return a < b; });
    }

    Value greater(const Value& lhs, const Value& rhs) {
        if (is_runtime_binary(lhs, rhs)) return lower(AST_GREATER, lhs, rhs);
        return binary_relation(
            lhs, rhs, [](i64 a, i64 b) -> bool { return a > b; }, [](string a, string b) -> bool { return a > b; });
    }

    Value less_equal(const Value& lhs, const Value& rhs) {
        if (is_runtime_binary(lhs, rhs)) return lower(AST_LESS_EQUAL, lhs, rhs);
        return binary_relation(
            lhs, rhs, [](i64 a, i64 b) -> bool { return a <= b; }, [](string a, string b) -> bool { return a <= b; });
    }

    Value greater_equal(const Value& lhs, const Value& rhs) {
        if (is_runtime_binary(lhs, rhs)) return lower(AST_GREATER_EQUAL, lhs, rhs);
        return binary_relation(
            lhs, rhs, [](i64 a, i64 b) -> bool { return a >= b; }, [](string a, string b) -> bool { return a >= b; });
    }

    Value head(const Value& v) {
        if (v.is_runtime()) return new ASTHead(v.loc(), lower(v).get_runtime());
        if (!v.is_list() && !v.is_error()) {
            err(v.loc(), "Can only get head of value of list type, given '", v.type(), "'.");
            return error();
        }
        if (v.is_error()) return error();

        return v.get_list().head();
    }

    Value tail(const Value& v) {
        if (v.is_runtime()) return new ASTTail(v.loc(), lower(v).get_runtime());
        if (!v.is_list() && !v.is_error()) {
            err(v.loc(), "Can only get tail of value of list type, given '", v.type(), "'.");
            return error();
        }
        if (v.is_error()) return error();

        return v.get_list().tail();
    }

    Value cons(const Value& head, const Value& tail) {
        if (head.is_runtime() || tail.is_runtime()) {
            return new ASTCons(head.loc(), lower(head).get_runtime(), lower(tail).get_runtime());
        }
        if (!tail.is_list() && !tail.is_void() && !tail.is_error()) {
            err(tail.loc(), "Tail of cons cell must be a list or void, given '", tail.type(), "'.");
            return error();
        }
        if (head.is_error() || tail.is_error()) return error();

        return Value(new ListValue(head, tail));
    }

    Value empty() {
        return Value(VOID);
    }

    Value list_of(const Value& element) {
        if (element.is_error()) return error();
        return cons(element, empty());
    }

    Value list_of(const vector<Value>& elements) {
        Value l = empty();
        for (i64 i = i64(elements.size()) - 1; i >= 0; i--) { l = cons(elements[i], l); }
        return l;
    }

    Value is_empty(const Value& list) {
        if (list.is_runtime()) return new ASTIsEmpty(list.loc(), lower(list).get_runtime());
        if (!list.is_list() && !list.is_void() && !list.is_error()) {
            err(list.loc(), "Can only get tail of value of list type, given '", list.type(), "'.");
            return error();
        }
        return Value(list.is_void(), BOOL);
    }

    Value error() {
        return Value(ERROR);
    }

    Value length(const Value& val) {
        if (val.is_error()) return error();

        if (val.is_runtime()) return new ASTLength(val.loc(), lower(val).get_runtime());

        if (val.is_string()) return Value(i64(val.get_string().size()));
        else if (val.is_list())
            return Value(i64(to_vector(val).size()));
        else if (val.is_product())
            return Value(i64(val.get_product().size()));
        else if (val.is_array())
            return Value(i64(val.get_array().size()));
        else {
            err(val.loc(), "Cannot get length of value of type '", val.type(), "'.");
            return error();
        }
    }

    Value tuple_of(const vector<Value>& elements) {
        for (const Value& v : elements) {
            if (v.is_runtime()) {
                err(v.loc(), "Cannot compile tuples yet.");
                return error();
            }
        }
        return Value(new ProductValue(elements));
    }

    Value array_of(const vector<Value>& elements) {
        for (const Value& v : elements) {
            if (v.is_runtime()) {
                err(v.loc(), "Cannot compile arrays yet.");
                return error();
            }
        }
        return Value(new ArrayValue(elements));
    }

    Value dict_of(const map<Value, Value> &elements) {
        for (const auto &p : elements) {
            if (p.first.is_runtime() || p.second.is_runtime()) {
                err(p.first.loc(), "Cannot compile arrays yet.");
                return error();
            }
        }
        return Value(new DictValue(elements));
    }

    Value at(const Value& val, const Value& idx) {
        if (val.is_error() || idx.is_error()) return error();
        if (val.is_runtime() || idx.is_runtime()) {
            vector<ASTNode*> args;
            Value s = lower(val), i = lower(idx);
            args.push(s.get_runtime());
            args.push(i.get_runtime());
            vector<const Type*> arg_types;
            arg_types.push(args[0]->type());
            arg_types.push(args[1]->type());
            if (args[0]->type() == STRING) {
                return new ASTNativeCall(val.loc(), "_char_at", INT, args, arg_types);
            } else {
                err(val.loc(), "Accesses not implemented in AST yet.");
                return error();
            }
        }
        if (!idx.is_int()) {
            err(idx.loc(), "Expected integer index in accessor, given '", val.type(), "'.");
            return error();
        }
        if (val.is_string()) return Value(i64(val.get_string()[idx.get_int()]));
        else if (val.is_product())
            return val.get_product()[idx.get_int()];
        else if (val.is_array())
            return val.get_array()[idx.get_int()];
        else {
            err(val.loc(), "Cannot index into value of type '", val.type(), "'.");
            return error();
        }
    }

    Value strcat(const Value& a, const Value& b) {
        if (a.is_error() || b.is_error()) return error();
        if (a.is_runtime() || b.is_runtime()) {
            vector<ASTNode*> args;
            Value al = lower(a), bl = lower(b);
            args.push(al.get_runtime());
            args.push(bl.get_runtime());
            vector<const Type*> arg_types;
            arg_types.push(STRING);
            arg_types.push(STRING);
            return new ASTNativeCall(a.loc(), "_strcat", STRING, args, arg_types);
        }
        if (!a.is_string() || !b.is_string()) {
            err(a.loc(), "Expected string and string, given '", a.type(), "' and '", b.type(), "'.");
            return error();
        }
        return Value(a.get_string() + b.get_string(), STRING);
    }

    Value substr(const Value& str, const Value& start, const Value& end) {
        if (str.is_error() || start.is_error() || end.is_error()) return error();
        if (str.is_runtime() || start.is_runtime() || end.is_runtime()) {
            vector<ASTNode*> args;
            Value strl = lower(str), startl = lower(start), endl = lower(end);
            args.push(strl.get_runtime());
            args.push(startl.get_runtime());
            args.push(endl.get_runtime());
            vector<const Type*> arg_types;
            arg_types.push(STRING);
            arg_types.push(INT);
            arg_types.push(INT);
            return new ASTNativeCall(str.loc(), "_substr", STRING, args, arg_types);
        }
        if (!str.is_string() || !start.is_int() || !end.is_int()) {
            err(str.loc(), "Expected string, integer, and integer, given '", str.type(), "' and '", start.type(),
                "' and '", end.type(), "'.");
            return error();
        }

        return end.get_int() < start.get_int()
                   ? Value("", STRING)
                   : Value(string(str.get_string()[{start.get_int(), end.get_int()}]), STRING);
    }

    Value type_of(const Value& v) {
        return Value(v.type(), TYPE);
    }

    // Assuming cast is possible, converts val to the right representation for the new type.
    Value cast(const Value& val, const Type* type) {
        if (val.type() == type || type == ANY) return val;

        if (val.type()->kind() == KIND_TYPEVAR) { 
            unify(val.type(), type);
            return val;
        }

        if (type->kind() == KIND_RUNTIME) {
            // we don't lower for ANY, so that generic builtins can do custom things
            return ((const RuntimeType*)type)->base() == ANY ? val : lower(val);
        }

        if (val.type()->kind() == type->kind() && !type->concrete()) {
            return val;
        }

        if (type->kind() == KIND_NAMED) {
            return Value(new NamedValue(val), type);
        }
        else if (val.type()->kind() == KIND_NAMED && type == ((const NamedType*)val.type())->base()) {
            return val.get_named().get();
        }

        if (val.is_product()) {
            if (type == TYPE) {
                vector<const Type*> ts;
                for (const Value& v : val.get_product()) ts.push(v.get_type());
                return Value(find<ProductType>(ts), TYPE);
            }
        }

        if (val.type()->kind() == KIND_ARRAY && type->kind() == KIND_ARRAY
            && ((const ArrayType*)val.type())->fixed() && !((const ArrayType*)type)->fixed()) {
            return Value(new ArrayValue(val.get_array()), type);
        }

        if (val.is_list() && type == TYPE) {
            if (length(val) != Value(1l)) {
                err(val.loc(), "Only single-element lists can be treated as types.");
                return error();
            }
            return find<ListType>(head(val).get_type());
        }

        if (val.is_sum() && val.get_sum().value().type() == type) return val.get_sum().value();
        else if (val.is_sum()) {
            err(val.loc(), "Sum value does not currently contain value of type '", type, "'.");
            return error();
        }

        if (type->kind() == KIND_SUM) { return Value(new SumValue(val), val.type()); }

        err(val.loc(), "Could not convert value of type '", val.type(), "' to type '", type, "'.");
        return error();
    }

    Value is(const Value& val, const Value& type) {
        if (!type.is_type()) {
            err(type.loc(), "Expected type value in is-expression, given '", type.type(), "'.");
            return error();
        }
        if (val.type() == type.get_type()) return Value(true, BOOL);
        else if (val.is_sum() && val.get_sum().value().type() == type.get_type())
            return Value(true, BOOL);
        else
            return Value(false, BOOL);
    }

    Value as(const Value& val, const Value& type) {
        if (!type.is_type()) {
            err(type.loc(), "Expected type value in explicit cast, given '", type.type(), "'.");
            return error();
        }
        // if (val.is_runtime()) {
        //   return Value(new ASTAnnotate(val.loc(), val.get_runtime(), type.get_type()));
        // }
        // else if (val.type()->coerces_to(*type.get_type())) {
        //   err(val.loc(), "Could not unify value of type '", val.type(), "' with type '",
        //       type.get_type(), "'.");
        //   return error();
        // }
        return cast(val, type.get_type());
    }

    Value annotate(const Value& val, const Value& type_in) {
        Value type = type_in.is_type() ? type_in : annotate(type_in, Value(TYPE, TYPE));
        if (!type.is_type()) {
            err(type.loc(), "Expected type value in annotation, given '", type.type(), "'.");
            return error();
        }
        if (val.is_runtime()) {
            return Value(new ASTAnnotate(val.loc(), val.get_runtime(), type.get_type()));
        } else if (!val.type()->coerces_to(type.get_type())) {
            err(val.loc(), "Could not unify value of type '", val.type(), "' with type '", type.get_type(), "'.");
            return error();
        }
        return cast(val, type.get_type());
    }

    ASTNode* instantiate(SourceLocation loc, FunctionValue& fn, const Type* args_type) {
        ref<Env> new_env = fn.get_env()->clone();
        new_env->make_runtime();
        u32 j = 0;
        vector<u64> new_args;
        for (u32 i = 0; i < fn.arity(); i++) {
            if (!(fn.args()[i] & KEYWORD_ARG_BIT)) {
                auto it = new_env->find(symbol_for(fn.args()[i] & ARG_NAME_MASK));
                const Type* argt = ((const ProductType*)args_type)->member(j);
                it->value = new ASTSingleton(argt);
                j++;
                new_args.push(fn.args()[i]);
            }
        }
        Value cloned = fn.body().clone();
        prep(new_env, cloned);
        Value v = eval(new_env, cloned);
        if (v.is_error()) return nullptr;
        if (!v.is_runtime()) v = lower(v);
        ASTNode* result = new ASTFunction(loc, new_env, args_type, new_args, v.get_runtime(), fn.name());
        fn.instantiate(args_type, result);
        return result;
    }

    void find_calls(FunctionValue& fn, ref<Env> env, const Value& term, set<const FunctionValue*>& visited) {
        if (!term.is_list()) return;
        Value h = head(term);
        if (h.is_symbol()) {
            Def* def = env->find(symbol_for(h.get_symbol()));
            if (def && def->value.is_function()) {
                FunctionValue* f = &def->value.get_function();
                if (visited.find(f) == visited.end()) {
                    visited.insert(f);
                    if (f != &fn) find_calls(*f, f->get_env(), f->body(), visited);
                    fn.add_call(f);
                }
            }
        }
        if (!introduces_env(term)) {
            const Value* v = &term;
            while (v->is_list()) {
                find_calls(fn, env, v->get_list().head(), visited);
                v = &v->get_list().tail();
            }
        }
    }

    Value call(ref<Env> env, Value& callable, const Value& args, SourceLocation callsite) {
        if (!args.is_product()) {
            err(args.loc(), "Expected product value for arguments.");
            return error();
        }
        for (u32 i = 0; i < args.get_product().size(); i ++) {
            if (args.get_product()[i].is_error()) return error();
        }
        Value function = callable;
        if (function.is_intersect()) {
            map<const FunctionType*, i64> ftypes;
            i64 coerced_priority = args.get_product().size() + 1,
                exact_priority = coerced_priority * coerced_priority;
            // println("args = ", args.type());
            // for (const auto& p : function.get_intersect()) {
            //     if (p.first->kind() != KIND_FUNCTION) continue;
            //     println(p.first);
            // }
            // println("---");
            for (const auto& p : function.get_intersect()) {
                if (p.first->kind() != KIND_FUNCTION) continue;
                const ProductType* fnargst = (const ProductType*)((const FunctionType*)p.first)->arg();
                if (fnargst->count() != args.get_product().size()) continue;
                i64 priority = 0;
                const ProductType* argst = (const ProductType*)args.type();
                for (u32 i = 0; i < argst->count(); i ++) {
                    if (argst->member(i) == fnargst->member(i)) 
                        priority += exact_priority;
                    else if (fnargst->member(i) != ANY && argst->member(i)->coerces_to(fnargst->member(i))) 
                        priority += coerced_priority;
                    else if (fnargst->member(i) != ANY) // implies argst[i] cannot coerce to desired type, incompatible
                        goto end;
                }
                ftypes.put((const FunctionType*)p.first, priority);
            end:;
            }

            if (ftypes.size() == 0) {
                err(function.loc(), "No overload of '", function, "' matches argument type", 
                    args.get_product().size() == 1 ? " " : "s ", commalist(Value(args.type(), TYPE), true), ".");
                return error();
            }
            if (ftypes.size() > 1) {
                i64 max = 0;
                for (const auto& p : ftypes) if (p.second > max) max = p.second;
                set<const FunctionType*> to_remove;
                for (const auto& p : ftypes) if (p.second < max) to_remove.insert(p.first);
                for (const FunctionType* t : to_remove) ftypes.erase(t);
                
                if (ftypes.size() > 1) {
                    err(function.loc(), "Call to '", function, "' is ambiguous; multiple overloads match arguments ",
                        args.type(), ".");
                    return error();
                }
            }

            function = function.get_intersect().values()[ftypes.begin()->first];
        }

        if (function.is_runtime()) {
            if (((const RuntimeType*)function.type())->base()->kind() != KIND_FUNCTION) {
                err(function.loc(), "Cannot call non-function value '", function, "'.");
                return error();
            }
        }
        else if (!function.is_function()) {
            err(function.loc(), "Cannot call non-function value '", function, "'.");
            return error();
        }
        if (!args.is_product()) {
            err(function.loc(), "Cannot call function with non-product argument '", args, "'.");
            return error();
        }

        const FunctionType* ft = function.is_runtime() ? 
            ((const FunctionType*)((const RuntimeType*)function.type())->base()) 
            : (const FunctionType*)function.type();
        const ProductType* argst = (const ProductType*)ft->arg();

        if (args.get_product().size() != argst->count()) {
            err(function.loc(), "Wrong number of arguments for function '", function, "': expected ", argst->count(),
                " arguments, given ", args.get_product().size(), ".");
            return error();
        }

        bool has_runtime = false;
        for (u32 i = 0; i < argst->count(); i ++) {
            if (args.get_product()[i].is_runtime()) {
                has_runtime = true;
                break;
            }
        }
        
        if (function.is_runtime()) has_runtime = true;
        if (function.is_function()) {
            FunctionValue& fn = function.get_function();
            if (fn.is_builtin()) {
                if (fn.get_builtin().runtime_only()) has_runtime = true;
            }
            else {
                if (ft->ret()->kind() == KIND_RUNTIME) has_runtime = true;
                if (!fn.found_calls()) {
                    set<const FunctionValue*> visited;
                    find_calls(fn, env, fn.body(), visited);
                }
                // if (fn.recursive()) has_runtime = true;
            }
        }

        const ProductType* rtargst = argst;
        if (has_runtime) {
            vector<const Type*> argts;
            for (u32 i = 0; i < argst->count(); i ++) {
                if (argst->member(i)->kind() != KIND_RUNTIME) argts.push(find<RuntimeType>(argst->member(i)));
                else argts.push(argst->member(i));
            }
            rtargst = (const ProductType*)find<ProductType>(argts);
        }

        Value args_copy = args;
        for (u32 i = 0; i < argst->count(); i++) {
            const Value& arg = args.get_product()[i];
            const Type* argt = arg.type();
            if (!argt->coerces_to(rtargst->member(i))) {
                err(arg.loc(), "Incorrectly typed argument for function '", function, "' at position ", i,
                    ": expected '", rtargst->member(i), "', given '", argt, "'.");
                return error();
            }
            if (argt != rtargst->member(i)) args_copy.get_product()[i] = cast(args.get_product()[i], rtargst->member(i));
        }
        for (u32 i = 0; i < args_copy.get_product().size(); i ++) {
            if (args_copy.get_product()[i].is_error()) return error();
        }

        if (function.is_runtime()) {
            vector<ASTNode*> rtargs;
            for (u32 i = 0; i < ft->arity(); i++) {
                Value v = lower(args_copy.get_product()[i]);
                ASTNode* n = v.get_runtime();
                n->inc();
                rtargs.push(n);
            }
            return new ASTCall(function.loc(), function.get_runtime(), rtargs);
        }

        FunctionValue& fn = function.get_function();
        if (fn.is_builtin()) {
            if (has_runtime && fn.get_builtin().should_lower()) 
                for (Value& v : args_copy.get_product()) v = lower(v);
            Value result = has_runtime ? fn.get_builtin().compile(env, args_copy) 
                : fn.get_builtin().eval(env, args_copy);
            result.set_location(callsite);
            return result;
        } else {
            vector<ASTNode*> rtargs;
            ref<Env> fnenv = fn.get_env();
            map<string, Value> bindings;
            for (u32 i = 0; i < fn.arity(); i++) {
                if (fn.args()[i] & KEYWORD_ARG_BIT) {
                    // keyword arg
                    if (!args_copy.get_product()[i].is_symbol() ||
                        args_copy.get_product()[i].get_symbol() != (fn.args()[i] & ARG_NAME_MASK)) {
                        err(args_copy.get_product()[i].loc(), "Expected keyword '",
                            symbol_for(fn.args()[i] & ARG_NAME_MASK), "'.");
                        return error();
                    }
                } else {
                    const string& argname = symbol_for(fn.args()[i] & ARG_NAME_MASK);
                    if (has_runtime) {
                        Value v = lower(args_copy.get_product()[i]);
                        ASTNode* n = v.get_runtime();
                        n->inc();
                        rtargs.push(n);
                    }
                    else {
                        Def* def = fnenv->find(argname);
                        bindings[argname] = def->value;
                        def->value = args_copy.get_product()[i];
                    }
                }
            }
            if (has_runtime) {
                ASTNode* body = fn.instantiation(argst);
                if (!body) {
                    fn.instantiate(argst, new ASTIncompleteFn(function.loc(), argst, fn.name()));
                    body = instantiate(function.loc(), fn, argst);
                }
                if (!body) return error();
                return new ASTCall(callsite, body, rtargs);
            }
            else {
                Value result = eval(fnenv, fn.body());
                for (auto& p : bindings) fnenv->find(p.first)->value = p.second;
                result.set_location(callsite);
                return result;
            }
        }
    }

    Value call_old(ref<Env> env, Value& function, const Value& arg) {
        if (function.is_runtime()) {
            u32 argc = arg.get_product().size();
            vector<const Type*> argts;
            vector<Value> lowered_args;
            for (u32 i = 0; i < argc; i++) {
                if (arg.get_product()[i].is_function()) {
                    vector<const Type*> inner_argts;
                    for (u32 j = 0; j < arg.get_product()[i].get_function().arity(); j++)
                        inner_argts.push(find<TypeVariable>());
                    argts.push(find<FunctionType>(find<ProductType>(inner_argts), find<TypeVariable>()));
                    lowered_args.push(arg.get_product()[i]); // we'll lower this later
                } else {
                    Value lowered = lower(arg.get_product()[i]);
                    argts.push(((const RuntimeType*)lowered.type())->base());
                    lowered_args.push(lowered);
                }
            }
            const Type* argt = find<ProductType>(argts);
            vector<ASTNode*> arg_nodes;
            for (u32 i = 0; i < lowered_args.size(); i++) {
                if (lowered_args[i].is_function()) {
                    const Type* t = ((const ProductType*)argt)->member(i);
                    if (!t->concrete() || t->kind() != KIND_FUNCTION) {
                        err(lowered_args[i].loc(), "Could not deduce type for function ", "parameter, resolved to '", t,
                            "'.");
                        return error();
                    }
                    const Type* fnarg = ((const FunctionType*)t)->arg();
                    FunctionValue& fn = lowered_args[i].get_function();
                    ASTNode* argbody = fn.instantiation(fnarg);
                    if (!argbody) {
                        fn.instantiate(fnarg, new ASTIncompleteFn(lowered_args[i].loc(), fnarg, fn.name()));
                        argbody = instantiate(lowered_args[i].loc(), fn, fnarg);
                    }
                    if (!argbody) return error();
                    arg_nodes.push(argbody);
                } else
                    arg_nodes.push(lowered_args[i].get_runtime());
            }
            return new ASTCall(function.loc(), function.get_runtime(), arg_nodes);
        }

        if (!function.is_function() && !function.is_error()) {
            err(function.loc(), "Called value is not a procedure.");
            return error();
        }
        if (!arg.is_product() && !arg.is_error()) {
            err(arg.loc(), "Arguments not provided as a product.");
            return error();
        }
        if (function.is_error() || arg.is_error()) return error();

        FunctionValue& fn = function.get_function();
        if (fn.is_builtin()) {
            return fn.get_builtin().eval(env, arg);
        } else {
            ref<Env> env = fn.get_env();
            u32 argc = arg.get_product().size(), arity = fn.args().size();
            if (argc != arity) {
                err(function.loc(), "Procedure requires ", arity, " arguments, ", argc, " provided.");
                return error();
            }

            bool runtime_call = false;
            for (u32 i = 0; i < argc; i++) {
                if (arg.get_product()[i].is_runtime()) runtime_call = true;
            }
            if (!fn.found_calls()) {
                set<const FunctionValue*> visited;
                find_calls(fn, env, fn.body(), visited);
            }
            if (fn.recursive()) runtime_call = true;

            if (runtime_call) {
                vector<const Type*> argts;
                vector<Value> lowered_args;
                for (u32 i = 0; i < argc; i++) {
                    if (fn.args()[i] & KEYWORD_ARG_BIT) {
                        // keyword arg
                        if (!arg.get_product()[i].is_symbol() ||
                            arg.get_product()[i].get_symbol() != (fn.args()[i] & ARG_NAME_MASK)) {
                            err(arg.get_product()[i].loc(), "Expected keyword '",
                                symbol_for(fn.args()[i] & ARG_NAME_MASK), "'.");
                            return error();
                        }
                    } else {
                        if (arg.get_product()[i].is_function()) {
                            vector<const Type*> inner_argts;
                            for (u32 j = 0; j < arg.get_product()[i].get_function().arity(); j++)
                                inner_argts.push(find<TypeVariable>());
                            argts.push(find<FunctionType>(find<ProductType>(inner_argts), find<TypeVariable>()));
                            lowered_args.push(arg.get_product()[i]); // we'll lower this later
                        } else {
                            Value lowered = lower(arg.get_product()[i]);
                            argts.push(((const RuntimeType*)lowered.type())->base());
                            lowered_args.push(lowered);
                        }
                    }
                }

                const Type* argt = find<ProductType>(argts);
                ASTNode* body = fn.instantiation(argt);
                if (!body) {
                    fn.instantiate(argt, new ASTIncompleteFn(function.loc(), argt, fn.name()));
                    body = instantiate(function.loc(), fn, argt);
                }
                if (!body) return error();
                vector<ASTNode*> arg_nodes;
                for (u32 i = 0; i < lowered_args.size(); i++) {
                    if (lowered_args[i].is_function()) {
                        const Type* t = ((const ProductType*)argt)->member(i);
                        if (t->kind() != KIND_FUNCTION || !((const FunctionType*)t)->arg()->concrete()) {
                            err(lowered_args[i].loc(), "Could not deduce type for function ",
                                "parameter, resolved to '", t, "'.");
                            return error();
                        }
                        const Type* fnarg = ((const FunctionType*)t)->arg();
                        while (fnarg->kind() == KIND_TYPEVAR) fnarg = ((const TypeVariable*)fnarg)->actual();
                        FunctionValue& fn = lowered_args[i].get_function();
                        ASTNode* argbody = fn.instantiation(fnarg);
                        if (!argbody) {
                            fn.instantiate(fnarg, new ASTIncompleteFn(lowered_args[i].loc(), fnarg, fn.name()));
                            argbody = instantiate(lowered_args[i].loc(), fn, fnarg);
                        }
                        if (!argbody) return error();
                        arg_nodes.push(argbody);
                    } else
                        arg_nodes.push(lowered_args[i].get_runtime());
                }
                return new ASTCall(function.loc(), body, arg_nodes);
            }

            for (u32 i = 0; i < arity; i++) {
                if (fn.args()[i] & KEYWORD_ARG_BIT) {
                    // keyword arg
                    if (!arg.get_product()[i].is_symbol() ||
                        arg.get_product()[i].get_symbol() != (fn.args()[i] & ARG_NAME_MASK)) {
                        err(arg.get_product()[i].loc(), "Expected keyword '", symbol_for(fn.args()[i] & ARG_NAME_MASK),
                            "'.");
                        return error();
                    }
                } else {
                    const string& argname = symbol_for(fn.args()[i] & ARG_NAME_MASK);
                    env->find(argname)->value = arg.get_product()[i];
                }
            }
            Value prepped = fn.body().clone();
            prep(env, prepped);
            return eval(env, prepped);
        }
    }

    Value display(const Value& arg) {
        return new ASTDisplay(arg.loc(), lower(arg).get_runtime());
    }

    Value assign(ref<Env> env, const Value& dest, const Value& src) {
        if (!dest.is_symbol()) {
            err(dest.loc(), "Invalid destination in assignment '", dest, "'.");
            return error();
        }

        Def* def = env->find(symbol_for(dest.get_symbol()));
        if (!def) {
            err(dest.loc(), "Undefined variable '", symbol_for(dest.get_symbol()), "'.");
            return error();
        }
        Value lowered = src;
        if (!lowered.is_runtime()) lowered = lower(src);
        if (def->value.is_runtime()) return new ASTAssign(dest.loc(), env, dest.get_symbol(), lowered.get_runtime());
        else {
            def->value = lower(def->value);
            return new ASTDefine(dest.loc(), env, dest.get_symbol(), lowered.get_runtime());
        }
    }
} // namespace basil

template <>
u64 hash(const basil::Value& t) {
    return t.hash();
}

void write(stream& io, const basil::Value& t) {
    t.format(io);
}