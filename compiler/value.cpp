/*
 * Copyright (c) 2021, the Basil authors
 * All rights reserved.
 *
 * This source code is licensed under the 3-Clause BSD License, the full text
 * of which can be found in the LICENSE file in the root directory
 * of this project.
 */

#include "value.h"
#include "builtin.h"
#include "env.h"
#include "forms.h"
#include "type.h"
#include "ast.h"
#include "eval.h"

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
            case K_UNDEFINED: undefined_sym = S_NONE; break;
            case K_FORM_FN: new (&fl_fn) rc<FormFn>(); break;
            case K_FORM_ISECT: new (&fl_isect) rc<FormIsect>(); break;
            case K_STRING: new (&string) rc<String>(); break;
            case K_LIST: new (&list) rc<List>(); break;
            case K_NAMED: new (&named) rc<Named>(); break;
            case K_TUPLE: new (&tuple) rc<Tuple>(); break;
            case K_ARRAY: new (&array) rc<Array>(); break;
            case K_UNION: new (&u) rc<Union>(); break;
            case K_STRUCT: new (&str) rc<Struct>(); break;
            case K_DICT: new (&dict) rc<Dict>(); break;
            case K_INTERSECT: new (&isect) rc<Intersect>(); break;
            case K_MODULE: new (&mod) rc<Module>(); break;
            case K_FUNCTION: new (&fn) rc<Function>(); break;
            case K_RUNTIME: new (&rt) rc<Runtime>(); break;
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
            case K_UNDEFINED: undefined_sym = other.undefined_sym; break;
            case K_FORM_FN: new (&fl_fn) rc<FormFn>(other.fl_fn); break;
            case K_FORM_ISECT: new (&fl_isect) rc<FormIsect>(other.fl_isect); break;
            case K_STRING: new (&string) rc<String>(other.string); break;
            case K_LIST: new (&list) rc<List>(other.list); break;
            case K_NAMED: new (&named) rc<Named>(other.named); break;
            case K_TUPLE: new (&tuple) rc<Tuple>(other.tuple); break;
            case K_ARRAY: new (&array) rc<Array>(other.array); break;
            case K_UNION: new (&u) rc<Union>(other.u); break;
            case K_STRUCT: new (&str) rc<Struct>(other.str); break;
            case K_DICT: new (&dict) rc<Dict>(other.dict); break;
            case K_INTERSECT: new (&isect) rc<Intersect>(other.isect); break;
            case K_MODULE: new (&mod) rc<Module>(other.mod); break;
            case K_FUNCTION: new (&fn) rc<Function>(other.fn); break;
            case K_RUNTIME: new (&rt) rc<Function>(other.rt); break;
            default:
                panic("Unsupported value kind!"); 
                break;
        }
    }

    Value::Data::~Data() {
        // do nothing...we handle this in Value's destructor.
    }

    Value::Value(): Value({}, T_VOID, nullptr) {}

    void destruct_list(rc<List>& rc) {
        uint8_t* it = rc._data;
        while (it && *(u64*)it) { // while it is not null *and* still has a refcount
            u64 count = -- *(u64*)it; // decrement refcount
            if (!count) { // manually deallocate
                uint8_t* next = ((List*)(it + sizeof(u64)))->tail._data;
                ((List*)(it + sizeof(u64)))->head.~Value();
                delete[] it;
                it = next;
            }
            else break; // if we aren't deleting something, no need to decrement further cells
        }
    }

    Value::~Value() {
        switch (type.kind()) {
            case K_FORM_FN: data.fl_fn.~rc(); break;
            case K_FORM_ISECT: data.fl_isect.~rc(); break;
            case K_STRING: data.string.~rc(); break;
            case K_LIST: destruct_list(data.list); break;
            case K_NAMED: data.named.~rc(); break;
            case K_TUPLE: data.tuple.~rc(); break;
            case K_ARRAY: data.array.~rc(); break;
            case K_UNION: data.u.~rc(); break;
            case K_STRUCT: data.str.~rc(); break;
            case K_DICT: data.dict.~rc(); break;
            case K_INTERSECT: data.isect.~rc(); break;
            case K_MODULE: data.mod.~rc(); break;
            case K_FUNCTION: data.fn.~rc(); break;
            case K_RUNTIME: data.rt.~rc(); break;
            default: break; // trivial destructor
        }
    }

    Value::Value(const Value& other):
        pos(other.pos), type(other.type), form(other.form), data(other.type.kind(), other.data) {}

    Value::Value(Value&& other):
        pos(other.pos), type(other.type), form(other.form), data(K_VOID) {
        (u64&)data = (u64&)other.data; // direct byte-to-byte copy
        other.type = T_VOID; // force other value to do trivial destructor
        other.form = nullptr;
    }

    Value& Value::operator=(const Value& other) {
        if (this != &other) {
            switch (type.kind()) {
                case K_FORM_FN: data.fl_fn.~rc(); break;
                case K_FORM_ISECT: data.fl_isect.~rc(); break;
                case K_STRING: data.string.~rc(); break;
                case K_LIST: destruct_list(data.list); break;
                case K_NAMED: data.named.~rc(); break;
                case K_TUPLE: data.tuple.~rc(); break;
                case K_ARRAY: data.array.~rc(); break;
                case K_UNION: data.u.~rc(); break;
                case K_STRUCT: data.str.~rc(); break;
                case K_DICT: data.dict.~rc(); break;
                case K_INTERSECT: data.isect.~rc(); break;
                case K_MODULE: data.mod.~rc(); break;
                case K_FUNCTION: data.fn.~rc(); break;
                case K_RUNTIME: data.rt.~rc(); break;
                default: break; // trivial destructor
            }
            type = other.type;
            pos = other.pos;
            form = other.form;
            new (&data) Data(type.kind(), other.data); // copy over
        }
        return *this;
    }    

    Value& Value::operator=(Value&& other) {
        if (this != &other) {
            switch (type.kind()) {
                case K_FORM_FN: data.fl_fn.~rc(); break;
                case K_FORM_ISECT: data.fl_isect.~rc(); break;
                case K_STRING: data.string.~rc(); break;
                case K_LIST: destruct_list(data.list); break;
                case K_NAMED: data.named.~rc(); break;
                case K_TUPLE: data.tuple.~rc(); break;
                case K_ARRAY: data.array.~rc(); break;
                case K_UNION: data.u.~rc(); break;
                case K_STRUCT: data.str.~rc(); break;
                case K_DICT: data.dict.~rc(); break;
                case K_INTERSECT: data.isect.~rc(); break;
                case K_MODULE: data.mod.~rc(); break;
                case K_FUNCTION: data.fn.~rc(); break;
                case K_RUNTIME: data.rt.~rc(); break;
                default: break; // trivial destructor
            }
            type = other.type;
            pos = other.pos;
            form = other.form;
            (u64&)data = (u64&)other.data; // direct byte-to-byte copy
            other.type = T_VOID; // force other value to do trivial destructor
            other.form = nullptr; 
        }
        return *this;
    }
    
    u64 Value::hash() const {
        u64 kh = raw_hash(type);
        switch (type.kind()) {
            case K_INT: return kh ^ raw_hash(data.i);
            case K_FLOAT: return kh ^ raw_hash(data.f32);
            case K_DOUBLE: return kh ^ raw_hash(data.f64);
            case K_SYMBOL: return kh ^ raw_hash(data.sym);
            case K_CHAR: return kh ^ raw_hash(data.ch);
            case K_BOOL: return kh ^ raw_hash(data.b);
            case K_VOID: return kh;
            case K_ERROR: return kh;
            case K_UNDEFINED: return kh ^ raw_hash(data.undefined_sym);
            case K_STRING: return kh ^ raw_hash(data.string->data);
            case K_NAMED: return kh ^ raw_hash(t_get_name(type)) ^ data.named->value.hash();
            case K_UNION: return kh ^ raw_hash(data.u->value.hash());
            case K_RUNTIME: return kh ^ 5679053960214674339ul * ::hash(u64(data.rt->ast.raw())); // pointer hash
            case K_LIST: {
                rc<List> l = data.list;
                while (l) {
                    kh ^= 9078847634459849863ul * l->head.hash();
                    l = l->tail;
                }
                return kh;
            }
            case K_TUPLE: {
                for (const Value& v : data.tuple->members)
                    kh ^= 17506913336699353123ul * v.hash();
                return kh;
            }
            case K_ARRAY: {
                for (const Value& v : data.array->elements)
                    kh ^= 14514260704651213427ul * v.hash();
                return kh;
            }
            case K_STRUCT: {
                for (const auto& [s, v] : data.str->fields) {
                    kh ^= 3643764085211794813ul * ::hash(s);
                    kh ^= 4428768580518955441ul * v.hash();
                }
                return kh;
            }
            case K_DICT: {
                for (const auto& [k, v] : data.dict->elements) {
                    kh ^= 9153145680466808213ul * k.hash();
                    kh ^= 8665824272381522569ul * v.hash();
                }
                return kh;
            }
            case K_INTERSECT: {
                for (const auto& [t, v] : data.isect->values) {
                    kh ^= 200878521973963957ul * ::hash(t);
                    kh ^= 11923319286714586559ul * v.hash();
                }
                return kh;
            }
            case K_MODULE: {
                for (const auto& [k, v] : data.mod->env->values) {
                    kh ^= 14221613862391592843ul * ::hash(k);
                    kh ^= 12782913719168895739ul * v.hash();
                }
                return kh;
            }
            default:
                panic("Unsupported value kind ", type.kind(), "!"); 
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
            case K_UNDEFINED: write(io, "#undefined(", data.undefined_sym, ")"); break; // #undefined(x)
            case K_FORM_FN: write(io, "#form-level-function"); break;
            case K_FORM_ISECT: write(io, "#form-level-intersect"); break;
            case K_STRING: write(io, '"', data.string->data, '"'); break;
            case K_NAMED: write(io, t_get_name(type), " of ", data.named->value); break; // Name of value
            case K_UNION: write(io, data.u->value, " in ", type); break; // value in (type | type)
            case K_LIST: write_seq(io, iter_list(*this), "(", " ", ")"); break; // (1 2 3)
            case K_TUPLE: write_seq(io, data.tuple->members, "(", ", ", ")"); break; // (1, 2, 3)
            case K_ARRAY: write_seq(io, data.array->elements, "{", " ", "}"); break; // [1 2 3]
            case K_STRUCT: write_pairs(io, data.str->fields, "{", " : ", "; ", "}"); break; // {x : 1; y : 2}
            case K_DICT: {
                if (t_dict_value(type) == T_VOID) write_keys(io, data.dict->elements, "{", " ", "}"); // {1 2 3}
                else write_pairs(io, data.dict->elements, "{", " = ", "; ", "}"); // {"x" = 1; "y" = 2}
                break;
            }
            case K_INTERSECT: write_pairs(io, data.isect->values, "(", ": ", " & ", ")"); break; // (1 & 2 & 3)
            case K_MODULE: write(io, "#module"); break;
            case K_FUNCTION: {
                if (data.fn->name) write(io, *data.fn->name);
                else if (t_is_macro(type)) write(io, "#macro");
                else write(io, "#procedure");
                break;
            }
            case K_RUNTIME: write(io, "#runtime[", data.rt->ast, " : ", ((rc<AST>)data.rt->ast)->type(root_env()), "]"); break;
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
            case K_UNDEFINED: return data.undefined_sym == other.data.undefined_sym;
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
            case K_INTERSECT: {
                if (data.isect->values.size() != other.data.isect->values.size()) return false;
                auto copy_values = data.isect->values;
                for (const auto& [t, v] : other.data.isect->values) {
                    auto it = copy_values.find(t);
                    if (it == copy_values.end()) return false; // fail if the other intersect has a type we don't have
                    if (it->second != v) return false;         // fail if the other intersect's type has a different value than ours
                    copy_values.erase(t);                    // otherwise, make sure we don't consider this type again
                }
                return copy_values.size() == 0; // our intersect had a matching type for every type in the other intersect
            }
            case K_MODULE:
                return data.mod->env.is(other.data.mod->env); // must be same environment
            case K_FUNCTION: 
                return data.fn.is(other.data.fn); // only consider reference equality
            case K_FORM_FN: 
                return data.fl_fn.is(other.data.fl_fn); // only consider reference equality
            case K_FORM_ISECT: {
                if (data.fl_isect->overloads.size() != other.data.fl_isect->overloads.size()) return false;
                auto copy_values = data.fl_isect->overloads;
                for (const auto& [t, v] : other.data.fl_isect->overloads) {
                    auto it = copy_values.find(t);
                    if (it == copy_values.end()) return false; // fail if the other intersect has a type we don't have
                    if (it->second != v) return false;         // fail if the other intersect's type has a different value than ours
                    copy_values.erase(t);                    // otherwise, make sure we don't consider this type again
                }
                return copy_values.size() == 0; // our intersect had a matching type for every type in the other intersect
            }
            case K_RUNTIME:
                return data.rt.is(other.data.rt);
            default:
                panic("Unsupported value kind!"); 
                return true;
        }
    }

    Value Value::clone() const {
        switch (type.kind()) {
            case K_INT:
            case K_FLOAT:
            case K_DOUBLE:
            case K_SYMBOL:
            case K_TYPE:
            case K_CHAR:
            case K_BOOL:
            case K_VOID:
            case K_ERROR:
            case K_UNDEFINED:
            case K_FUNCTION:
            case K_FORM_FN:
            case K_MODULE:
                return *this; // shallow copy is sufficient for these kinds
            case K_STRING: return v_string(pos, data.string->data).with(form);
            case K_NAMED: return v_named(pos, type, data.named->value.clone()).with(form);
            case K_UNION: return v_union(pos, type, data.u->value.clone()).with(form);
            case K_LIST: {
                vector<Value> elts;
                rc<List> l = data.list;
                while (l) elts.push(l->head.clone()), l = l->tail;
                return v_list(pos, type, move(elts)).with(form);
            }
            case K_TUPLE: {
                vector<Value> elts;
                for (const Value& v : data.tuple->members) elts.push(v.clone());
                return v_tuple(pos, type, move(elts)).with(form);
            }
            case K_ARRAY: {
                vector<Value> elts;
                for (const Value& v : data.array->elements) elts.push(v.clone());
                return v_array(pos, type, move(elts)).with(form);
            }
            case K_STRUCT: {
                map<Symbol, Value> fields;
                for (const auto& [s, v] : data.str->fields) fields[s] = v.clone();
                return v_struct(pos, type, move(fields)).with(form);
            }
            case K_DICT: {
                map<Value, Value> elements;
                for (const auto& [k, v] : data.dict->elements) elements[k.clone()] = v.clone();
                return v_dict(pos, type, move(elements)).with(form);    
            }
            case K_INTERSECT: {
                map<Type, Value> values;
                for (const auto& [t, v] : data.isect->values) values[t] = v.clone();
                return v_intersect(pos, type, move(values)).with(form);    
            }
            case K_FORM_ISECT: {
                map<rc<Form>, Value> overloads;
                for (const auto& [t, v] : data.fl_isect->overloads) overloads[t] = v.clone();
                return v_form_isect(pos, type, form, move(overloads)).with(form);    
            }
            case K_RUNTIME: {
                return v_runtime(pos, type, data.rt->ast->clone());
            }
            default:
                panic("Unsupported value kind!"); 
                return *this;
        }
    }
    
    bool Value::operator!=(const Value& other) const {
        return !operator==(other);
    }

    Value& Value::with(rc<Form> form_in) {
        form = form_in;
        return *this;
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

    Intersect::Intersect(const map<Type, Value>& values_in): values(values_in) {}

    Module::Module(rc<Env> env_in): env(env_in) {}

    Runtime::Runtime(rc<AST> ast_in): ast(ast_in) {}

    bool FormTuple::operator==(const FormTuple& other) const {
        if (forms.size() != other.forms.size()) return false;
        for (u32 i = 0; i < forms.size(); i ++) if (*forms[i] != *other.forms[i]) return false;
        return true;
    }

    void FormTuple::compute_hash() {
        u64 h = 16267324476120324511ul;
        for (const auto& f : forms) {
            h *= 13332580176933800113ul;
            h ^= f->hash();
        }
        hash = h;
    }
    
    FnInst::FnInst(Type args_in, rc<Env> env_in, rc<AST> func_in):
        args(args_in), env(env_in), func(func_in) {}

    rc<FnInst> monomorphize(const Function& fn, InstTable& table, rc<Env> env, rc<Value> base, Type args_type_in) {
        Type args_type = t_lower(args_type_in);
        if (args_type == T_ERROR) {
            err(base->pos, "Could not compile function - provided arguments type '", args_type_in, "' cannot be compiled.");
            return nullptr;
        }
        // println("monomorphizing ", *fn.name, " on ", args_type);
        rc<Env> local = env->clone();
        for (u32 i = 0; i < fn.args.size(); i ++) {
            Type argt = (i == 0 && !args_type.of(K_TUPLE)) ? args_type : t_tuple_at(args_type, i);
            local->def(fn.args[i], v_runtime(base->pos, t_runtime(argt), ast_unknown(base->pos, argt)));
        }

        Type stub_type = t_func(args_type, t_var());
        if (fn.name) {
            Value stub = v_runtime(base->pos, t_runtime(stub_type), ast_func_stub(base->pos, stub_type, *fn.name, true));
            local->def(*fn.name, stub);
            // we want to make this available for mutually recursive calls, so we add it to the table
            // (it'll be replaced later, upon successful compilation of this function)
            table.insts.put(args_type, ref<FnInst>(args_type, local, stub.data.rt->ast)); 
        }
        rc<Value> cloned_base = base->clone();
        Value evaled_body = eval(local, *cloned_base);
        if (evaled_body.type == T_ERROR) return nullptr;
        Value lowered = lower(local, evaled_body);
        if (lowered.type == T_ERROR) return nullptr;

        rc<AST> body_ast = lowered.data.rt->ast;
        if (!t_ret(stub_type).coerces_to(body_ast->type(local))) { // if the body is incompatible with the type signature
            err(base->pos, "Incompatible function body: expected expression of type '", t_ret(stub_type), 
                "', but found '", body_ast->type(local), "' instead.");
            return nullptr;
        }
        Type fntype = fn.name ? t_func(args_type, t_concrete(t_ret(stub_type))) 
            : t_func(args_type, body_ast->type(local));

        return ref<FnInst>(
            args_type,
            local,
            ast_func(base->pos, fntype, local, fn.name, fn.args, body_ast)
        );
    }
    
    InstTable::InstTable(rc<Env> local, rc<Value> base_in):
        env(local), base(base_in) {}

    bool InstTable::is_instantiating(Type args_type) const {
        return is_inst.contains(args_type) && is_inst[args_type] > 0;
    }

    bool InstTable::is_resolving() const {
        return resolving;
    }

    rc<FnInst> InstTable::inst(const Function& fn, Type args_type) {
        auto it = insts.find(args_type);
        if (it != insts.end()) {
            return it->second;
        }
        else {
            // instantiating lets other invocations of this method know that they're
            // happening inside of an active compilation - so we don't compile them
            // within this method.
            // this avoids infinite loops where a function instantiates itself
            // within itself
            is_inst[args_type] ++;
            auto morph = monomorphize(fn, *this, env, base, args_type);
            if (!morph) return nullptr;
            insts.put(args_type, morph);
            is_inst[args_type] --;
            auto i = insts.find(args_type)->second;
            
            // println("instantiated ", ITALICBLUE, fn.name ? *fn.name : symbol_from("<lambda>"), args_type.of(K_TUPLE) ? "" : "(",
            //     args_type, args_type.of(K_TUPLE) ? "" : ")", RESET, " as ", ITALICBLUE, *i->func->begin(), RESET);
            return insts.find(args_type)->second;
        }
    }

    AbstractFunction::AbstractFunction(rc<Env> env_in, const vector<Symbol>& args_in, const Value& body_in):
        env(env_in), args(args_in), body(body_in) {}
    
    Function::Function(optional<Symbol> name_in, optional<const Builtin&> builtin_in, rc<Env> env_in, 
        const vector<Symbol>& args_in, const Value& body_in):
        AbstractFunction(env_in, args_in, body_in), builtin(builtin_in), name(name_in) {}

    rc<InstTable> v_resolve_body(AbstractFunction& fn, FormTuple tup) {
        tup.compute_hash();

        auto& table = fn.resolutions;
        auto it = table.find(tup);
        if (it == table.end()) {
            rc<InstTable> inst = ref<InstTable>(fn.env->clone(), fn.body.clone());
            // write_seq(_stdout, fn.args, "fn args = [", ", ", "]\n");
            // println("input forms = ", tup.forms.size());
            for (u32 i = 0; i < tup.forms.size(); i ++) {
                // println(" => ", tup.forms[i]);
                inst->env->def(fn.args[i], v_undefined({}, fn.args[i], tup.forms[i]));
            }
            table.put(tup, inst);
            inst->resolving ++;
            resolve_form(inst->env, *inst->base);
            inst->resolving --;
            return table.find(tup)->second;
        }
        else {
            return it->second;
        }
    }

    rc<FnInst> Function::inst(Type args_type, const Value& v) {
        auto inst_table = v_resolve_body(*this, v);
        return inst_table->inst(*this, args_type);
    }

    Alias::Alias(const Value& term_in): term(term_in) {}
    
    FormFn::FormFn(rc<Env> env_in, const vector<Symbol>& args_in, const Value& body_in):
        AbstractFunction(env_in, args_in, body_in) {}
    
    FormIsect::FormIsect(const map<rc<Form>, Value>& overloads_in):
        overloads(overloads_in) {}

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

    Value v_bool(Source::Pos pos, bool b) {
        Value v(pos, T_BOOL, nullptr);
        v.data.b = b;
        return v;
    }

    Value v_void(Source::Pos pos) {
        return Value(pos, T_VOID, nullptr);
    }
    
    Value v_error(Source::Pos pos) {
        return Value(pos, T_ERROR, nullptr);
    }

    Value v_undefined(Source::Pos pos, Symbol name, rc<Form> form) {
        Value v(pos, T_UNDEFINED, form);
        v.data.undefined_sym = name;
        return v;
    }

    Value v_form_fn(Source::Pos pos, Type type, rc<Env> env, rc<Form> form, 
        const vector<Symbol>& args, const Value& body) {
        if (!type.of(K_FORM_FN)) 
            panic("Attempted to construct form-level function with incompatible type '", type, "'!");
        if (t_form_fn_arity(type) != args.size()) 
            panic("Attempted to construct form-level function with incorrect number of arguments; ",
                "provided ", args.size(), " arguments, but provided type has arity ", t_form_fn_arity(type));
        Value v(pos, type, form);
        v.data.fl_fn = ref<FormFn>(env, args, body);
        return v;
    }

    Value v_form_isect(Source::Pos pos, Type type, rc<Form> form, map<rc<Form>, Value>&& overloads) {
        if (!type.of(K_FORM_ISECT)) 
            panic("Attempted to construct form-level intersect with incompatible type '", type, "'!");
        Value v(pos, type, form);
        v.data.fl_isect = ref<FormIsect>(overloads);
        return v;
    }

    Value v_string(Source::Pos pos, const ustring& str) {
        Value v(pos, T_STRING, nullptr);
        v.data.str = ref<String>(str);
        return v;
    }

    Value v_cons(Source::Pos pos, Type type, const Value& head, const Value& tail) {
        Value v(pos, type, nullptr);
        if (!type.of(K_LIST)) 
            panic("Attempted to construct list with non-list type '", type, "'!");
        if (!head.type.coerces_to(t_list_element(type)))
            v.type = t_list(T_ANY); // switch to [Any]
        if (!tail.type.of(K_LIST) && tail.type != T_VOID)
            panic("Attempted to construct list with non-list tail of type '", tail.type, "'!");
        if (!tail.type.coerces_to(type))
            v.type = t_list(T_ANY); // switch to [Any]
        v.data.list = ref<List>(head, tail.type.of(K_VOID) ? nullptr : tail.data.list);
        return v;
    }

    Value v_list(Source::Pos pos, Type type, vector<Value>&& values) {
        if (values.size() == 0) return v_void(pos);

        Value v(pos, type, nullptr);
        if (!type.of(K_LIST))
            panic("Attempted to construct list with non-list type '", type, "'!");
        for (const Value& v : values) 
            if (!v.type.coerces_to(t_list_element(type)))
                panic("Cannot construct list - found vector element '", v.type, 
                    "' incompatible with list type '", type, "'!");
        
        rc<List> l = nullptr;
        for (i64 i = i64(values.size()) - 1; i >= 0; i --) // larger signed index to avoid overflow
            l = ref<List>(values[i], l);
        v.data.list = l;
        return v;
    }

    Value v_tuple(Source::Pos pos, Type type, vector<Value>&& values) {
        Value v(pos, type, nullptr);
        // if (!type.of(K_TUPLE))
        //     panic("Attempted to construct tuple with non-tuple type '", type, "'!");
        // if (t_tuple_len(type) != values.size())
        //     panic("Cannot construct tuple - number of provided values (", values.size(), 
        //         ") differs from tuple type size (", t_tuple_len(type), ")!");
        // for (u32 i = 0; i < values.size(); i ++) {
        //     if (!values[i].type.coerces_to(t_tuple_at(type, i)))
        //         panic("Cannot construct tuple - at least one vector element incompatible with member type!");
        // }
        v.data.tuple = ref<Tuple>(move(values));
        return v;
    }
    
    Value v_array(Source::Pos pos, Type type, vector<Value>&& values) {
        Value v(pos, type, nullptr);
        if (!type.of(K_ARRAY))
            panic("Attempted to construct array with non-array type '", type, "'!");
        if (t_array_is_sized(type) && t_array_size(type) != values.size())
            panic("Cannot construct array - number of provided values differs from array type size!");
        for (u32 i = 0; i < values.size(); i ++) {
            if (!values[i].type.coerces_to(t_array_element(type)))
                panic("Cannot construct array - at least one vector element incompatible with element type!");
        }
        v.data.array = ref<Array>(move(values));
        return v;
    }
    
    Value v_union(Source::Pos pos, Type type, const Value& value) {
        Value v(pos, type, nullptr);
        if (!type.of(K_UNION))
            panic("Attempted to construct union with non-union type '", type, "'!");
        if (!value.type.coerces_to(type))
            panic("Cannot construct union - provided value of type '", value.type, 
                "' is not a member of union type '", type, "'!");
        v.data.u = ref<Union>(value);
        return v;
    }
    
    Value v_named(Source::Pos pos, Type type, const Value& value) {
        Value v(pos, type, nullptr);
        if (!type.of(K_NAMED))
            panic("Attempted to construct named value with non-named type '", type, "'!");
        if (!value.type.coerces_to(t_get_base(type))) {
            // println("value type = ", value.type);
            // println("base type = ", t_get_base(type));
            panic("Cannot construct named value - provided value is of an incompatible type!");
        }
        v.data.named = ref<Named>(value);
        return v;
    }
    
    Value v_struct(Source::Pos pos, Type type, map<Symbol, Value>&& fields) {
        Value v(pos, type, nullptr);
        if (!type.of(K_STRUCT))
            panic("Attempted to construct struct value with non-struct type!");
        if (t_struct_len(type) != fields.size())
            panic("Cannot construct struct - wrong number of field values provided!");
        Type t = infer_struct(fields);
        if (!t.coerces_to(type)) 
            panic("Cannot construct struct - inferred type from fields is incompatible with desired type!");
        v.data.str = ref<Struct>(move(fields));
        return v;
    }

    Value v_dict(Source::Pos pos, Type type, map<Value, Value>&& entries) {
        Value v(pos, type, nullptr);
        if (!type.of(K_DICT))
            panic("Attempted to construct dict value with non-dictionary type!");
        for (const auto& [k, v] : entries) {
            if (!k.type.coerces_to(t_dict_key(type)))
                panic("Cannot construct dict - at least one pair has an incompatible key type!");
            if (!v.type.coerces_to(t_dict_value(type)))
                panic("Cannot construct dict - at least one pair has an incompatible value type!");
        }
        v.data.dict = ref<Dict>(move(entries));
        return v;
    }

    Value v_intersect(Source::Pos pos, Type type, map<Type, Value>&& values) {
        Value v(pos, type, nullptr);
        if (!type.of(K_INTERSECT))
            panic("Attempted to construct intersection value with non-intersection type!");
        v.data.isect = ref<Intersect>(values);
        return v;
    }

    Value v_intersect(vector<Builtin*>&& builtins) {
        if (builtins.size() == 1) return v_func(*builtins[0]);
        map<rc<Form>, map<Type, Builtin*>> values;
        for (Builtin* b : builtins) values[b->form][b->type] = b;
       
        map<rc<Form>, Value> type_level;
        for (const auto& [k, v] : values) {
            if (v.size() == 1) type_level[k] = v_func(*v.begin()->second).with(v.begin()->second->form);
            else {
                vector<Type> types;
                map<Type, Value> values;
                rc<Form> form;
                for (auto& [_, b] : v) 
                    types.push(b->type), values[b->type] = v_func(*b), form = b->form;
                type_level[k] = v_intersect({}, t_intersect(types), move(values)).with(form);
            }
        }

        if (type_level.size() == 1) return type_level.begin()->second;
        else {
            map<rc<Form>, Type> types;
            vector<rc<Form>> forms;
            for (const auto& [k, v] : type_level) types[k] = v.type, forms.push(v.form);
            i64 prec = forms[0]->precedence;
            Associativity assoc = forms[0]->assoc;
            return v_form_isect({}, t_form_isect(types), f_overloaded(prec, assoc, forms), move(type_level));
        }
    }

    Value v_module(Source::Pos pos, rc<Env> env) {
        Value v(pos, T_MODULE, nullptr);
        v.data.mod = ref<Module>(env);
        return v;
    }

    Value v_func(const Builtin& builtin) {
        Value v({}, builtin.type, builtin.form);
        if (!builtin.type.of(K_FUNCTION))
            panic("Attempted to create function value with non-function builtin!");
        v.data.fn = ref<Function>(none<Symbol>(), some<const Builtin&>(builtin), nullptr, vector_of<Symbol>(), v_void({}));
        return v;
    }

    Value v_func(Source::Pos pos, Type type, rc<Env> env, const vector<Symbol>& args, const Value& body) {
        Value v(pos, type, nullptr);
        if (!type.of(K_FUNCTION))
            panic("Attempted to construct function value with non-function type!");
        v.data.fn = ref<Function>(none<Symbol>(), none<const Builtin&>(), env, args, body);
        return v;
    }

    Value v_func(Source::Pos pos, Symbol name, Type type, rc<Env> env, const vector<Symbol>& args, const Value& body) {
        Value v(pos, type, nullptr);
        if (!type.of(K_FUNCTION))
            panic("Attempted to construct function value with non-function type!");
        v.data.fn = ref<Function>(some<Symbol>(name), none<const Builtin&>(), env, args, body);
        return v;
    }

    Value v_alias(Source::Pos pos, const Value& term) {
        Value v(pos, T_ALIAS, nullptr);
        v.data.alias = ref<Alias>(term);
        return v;
    }

    Value v_runtime(Source::Pos pos, Type type, rc<AST> ast) {
        Value v(pos, type, nullptr);
        if (!type.of(K_RUNTIME))
            panic("Attempted to construct runtime value with non-runtime type!");
        v.data.rt = ref<Runtime>(ast);
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
        if (v.type.of(K_LIST)) return list_iterable(v.data.list);
        else if (!v.type.of(K_VOID)) {
            panic("Expected list value!");
        }
        return list_iterable(nullptr);
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
        static vector<Type> ts;
        ts.clear();
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
        static map<Symbol, Type> fieldts;
        fieldts.clear();
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

    Value& v_head(Value& list) {
        if (!list.type.of(K_LIST)) panic("Expected a list value!");
        if (!list.data.list) panic("Attempted to get head of empty list!");
        return list.data.list->head;
    }

    void v_set_head(Value& list, const Value& v) {
        if (!list.type.of(K_LIST)) panic("Expected a list value!");
        if (!list.data.list) panic("Attempted to set head of empty list!");
        if (!v.type.coerces_to_generic(t_list_element(list.type))) 
            panic("Attempted to set list head to value of incompatible type!");
        list.data.list->head = v;
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
        if (!v.type.coerces_to_generic(t_tuple_at(tuple.type, i))) 
            panic("Attempted to set tuple member to value of incompatible type!");
        tuple.data.tuple->members[i] = v;
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
        if (!v.type.coerces_to_generic(t_array_element(array.type))) 
            panic("Attempted to set array element to value of incompatible type!");
        array.data.array->elements[i] = v;
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
        if (!v.type.coerces_to_generic(t_struct_field(str.type, field)))
            panic("Attempted to set struct field to value of incompatible type!");
        auto it = str.data.str->fields.find(field);
        if (it == str.data.str->fields.end()) panic("Attempted to get nonexistent struct field!");
        it->second = v;
    }

    u32 v_struct_len(const Value& str) {
        if (!str.type.of(K_STRUCT)) panic("Expected a struct value!");
        return str.data.str->fields.size();
    }

    const map<Symbol, Value>& v_struct_fields(const Value& str) {
        if (!str.type.of(K_STRUCT)) panic("Expected a struct value!");
        return str.data.str->fields;
    }

    bool v_dict_has(const Value& dict, const Value& key) {
        if (!dict.type.of(K_DICT)) panic("Expected a dictionary value!");
        if (!key.type.coerces_to_generic(t_dict_key(dict.type))) 
            panic("Attempted to check whether dict contains key of incompatible type!");
        return dict.data.dict->elements.contains(key);
    }

    const Value& v_dict_at(const Value& dict, const Value& key) {
        if (!dict.type.of(K_DICT)) panic("Expected a dictionary value!");
        if (!key.type.coerces_to_generic(t_dict_key(dict.type)))
            panic("Attempted to access dict by key of incompatible type!");
        auto it = dict.data.dict->elements.find(key);
        if (it == dict.data.dict->elements.end()) 
            panic("Attempted to access dict by nonexistent key!");
        return it->second;
    }

    void v_dict_put(Value& dict, const Value& key, const Value& value) {
        if (!dict.type.of(K_DICT)) panic("Expected a dictionary value!");
        if (!key.type.coerces_to_generic(t_dict_key(dict.type)))
            panic("Attempted to put key of incompatible type into dictionary!");
        if (!value.type.coerces_to_generic(t_dict_value(dict.type)))
            panic("Attempted to put value of incompatible type into dictionary!");
        dict.data.dict->elements.put(
            key, 
            value
        );
    }

    void v_dict_erase(Value& dict, const Value& key) {
        if (!dict.type.of(K_DICT)) panic("Expected a dictionary value!");
        if (!key.type.coerces_to_generic(t_dict_key(dict.type)))
            panic("Attempted to access dict by key of incompatible type!");
        dict.data.dict->elements.erase(key);
    }

    u32 v_dict_len(const Value& dict) {
        if (!dict.type.of(K_DICT)) panic("Expected a dictionary value!");
        return dict.data.dict->elements.size();
    }

    const map<Value, Value>& v_dict_elements(const Value& dict) {
        if (!dict.type.of(K_DICT)) panic("Expected a dictionary value!");
        return dict.data.dict->elements;
    }
    
    u32 v_len(const Value& v) {
        switch (v.type.kind()) {
            case K_TUPLE: return v_tuple_len(v);
            case K_ARRAY: return v_array_len(v);
            case K_DICT: return v_dict_len(v);
            case K_STRUCT: return v_struct_len(v);
            case K_LIST:
                panic("List does not support the length operation due to performance reasons.");
                return 0;
            default:
                panic("Provided value does not support the length operation!");
                return 0;
        }
    }

    Value v_at(const Value& v, u32 i) {
        switch (v.type.kind()) {
            case K_TUPLE: 
                return v_tuple_at(v, i);
            case K_ARRAY:
                return v_array_at(v, i);
            case K_LIST:
                panic("List does not support indexing due to performance reasons.");
                return v_error({});
            default:
                panic("Provided value is not indexable!");
                return v_error({});
        }
    }

    Value v_at(const Value& v, const Value& key) {
        switch (v.type.kind()) {
            case K_TUPLE: 
                if (key.type != T_INT) panic("Expected integer key accessing tuple element!"); 
                return v_tuple_at(v, key.data.i);
            case K_ARRAY:
                if (key.type != T_INT) panic("Expected integer key accessing array element!"); 
                return v_array_at(v, key.data.i);
            case K_STRUCT:
                if (key.type != T_SYMBOL) panic("Expected symbol key accessing struct field!"); 
                return v_struct_at(v, key.data.sym);
            case K_DICT:
                return v_dict_at(v, key);
            case K_LIST:
                panic("List does not support indexing due to performance reasons.");
                return v_error({});
            default:
                panic("Provided value is not indexable!");
                return v_error({});
        }
    }

    Value lower(rc<Env> env, const Value& src) {
        Type t_lowered = t_lower(src.type);
        switch (src.type.kind()) {
            case K_INT: return v_runtime(src.pos, t_runtime(t_lowered), ast_int(src.pos, t_lowered, src.data.i));
            case K_FLOAT: return v_runtime(src.pos, t_runtime(t_lowered), ast_float(src.pos, t_lowered, src.data.f32));
            case K_DOUBLE: return v_runtime(src.pos, t_runtime(t_lowered), ast_double(src.pos, t_lowered, src.data.f64));
            case K_SYMBOL: return v_runtime(src.pos, t_runtime(t_lowered), ast_symbol(src.pos, t_lowered, src.data.sym));
            case K_CHAR: return v_runtime(src.pos, t_runtime(t_lowered), ast_char(src.pos, t_lowered, src.data.ch));
            case K_STRING: return v_runtime(src.pos, t_runtime(t_lowered), ast_string(src.pos, t_lowered, src.data.string->data));
            case K_TYPE: return v_runtime(src.pos, t_runtime(t_lowered), ast_type(src.pos, t_lowered, src.data.type));
            case K_VOID: return v_runtime(src.pos, t_runtime(t_lowered), ast_void(src.pos));
            case K_BOOL: return v_runtime(src.pos, t_runtime(t_lowered), ast_bool(src.pos, t_lowered, src.data.b));
            case K_NAMED: {
                Value inner = lower(env, src.data.named->value);
                if (inner.type == T_ERROR) return inner;
                if (!inner.type.of(K_RUNTIME)) panic("Expected runtime type after lowering, got '", inner.type, "'!");
                rc<AST> inner_ast = inner.data.rt->ast;
                inner_ast->t = t_lowered;
                return v_runtime(src.pos, t_runtime(t_lowered), inner_ast);
            }
            case K_ERROR:
            case K_RUNTIME: return src;
            default:   
                err(src.pos, "Attempted to lower compile-time-only value '", src, "' of type '", t_lowered, "'.");
                return v_error(src.pos);
        }
    }

    Value coerce(rc<Env> env, const Value& src, Type target) {
        if (src.type == target) return src; // no coercion needed

        if (src.type.coerces_to_generic(target)) return src; // generic conversions don't require representational changes

        if (target.of(K_RUNTIME)) {
            if (!src.type.of(K_RUNTIME)) {
                Value result = src.type.coerces_to_generic(t_runtime_base(target)) ?
                    src : coerce(env, src, t_runtime_base(target));
                return lower(env, src);
            }
            else {
                // println("src type = ", src.type, ", target = ", target);
                return v_runtime(src.pos, target, ast_coerce(src.pos, src.data.rt->ast, t_runtime_base(target)));
            }
        }

        if (target.of(K_TYPE)) switch (src.type.kind()) { // coercing to type
            case K_LIST:
                if (v_tail(src).type != T_VOID) 
                    panic("List '", src, "' being coerced to type has more than one element!");
                if (!v_head(src).type.coerces_to(T_TYPE)) 
                    panic("List being coerced to type has non-type element '", v_head(src), "'!");
                return v_type(src.pos, t_list(coerce(env, v_head(src), T_TYPE).data.type));
            case K_TUPLE: {
                vector<Type> elts;
                for (const Value& v : v_tuple_elements(src)) {
                    if (!v.type.coerces_to(T_TYPE)) 
                        panic("Tuple being coerced to type has non-type element '", v, "'!");
                    elts.push(coerce(env, v, T_TYPE).data.type);
                }
                return v_type(src.pos, t_tuple(elts));
            }
            case K_NAMED: {
                const Value& b = src.data.named->value;
                if (!b.type.coerces_to(T_TYPE)) panic("Named value being coerced to type did not contain type!");
                return v_type(src.pos, t_named(t_get_name(src.type), coerce(env, b, T_TYPE).data.type));
            }
            default:
                break;
        }

        if (target.of(K_TUPLE) && src.type.of(K_TUPLE)) { // tuple-to-tuple elementwise coercion
            vector<Value> new_elements;
            for (u32 i = 0; i < v_len(src); i ++) {
                new_elements.push(coerce(env, v_at(src, i), t_tuple_at(target, i)));
            }
            return v_tuple(src.pos, target, move(new_elements));
        }

        if (src.type.of(K_INT)) {
            if (target.of(K_FLOAT)) return v_float({}, float(src.data.i));
            if (target.of(K_DOUBLE)) return v_double({}, double(src.data.i));
        }
        if (src.type.of(K_FLOAT)) {
            if (target.of(K_DOUBLE)) return v_double({}, double(src.data.f32));
        }

        if (target.of(K_UNION) && t_union_has(target, src.type)) {
            return v_union(src.pos, target, src);
        }

        panic("Attempted to perform unimplemented type conversion from '", src, " : ", src.type, 
            "' to type '", target, "'!");
        return v_error({});
    }

    rc<InstTable> v_resolve_body(AbstractFunction& fn, const vector<rc<Form>>& args) {
        u32 num_args = fn.args.size();
        FormTuple tup;
        tup.forms = args;
        for (rc<Form>& form : tup.forms) if (!form) form = F_TERM; // default to term
        return v_resolve_body(fn, tup);
    }

    rc<InstTable> v_resolve_body(AbstractFunction& fn, const Value& args) {
        u32 num_args = fn.args.size();
        auto& table = fn.resolutions;
        FormTuple tup;
        if (num_args == 1) {
            tup.forms.push(args.form); // single arg
        }
        else for (u32 i = 0; i < num_args; i ++) {
            tup.forms.push(v_tuple_at(args, i).form); // get forms for each arg
        }
        for (rc<Form>& form : tup.forms) if (!form) form = F_TERM; // default to term

        return v_resolve_body(fn, tup);
    }
}

template<>
u64 hash(const basil::FormTuple& forms) {
    return forms.hash;
}

template<>
u64 hash(const basil::Value& value) {
    return value.hash();
}

void write(stream& io, const basil::Value& value) {
    value.format(io);
}