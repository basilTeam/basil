#ifndef BASIL_VALUES_H
#define BASIL_VALUES_H

#include "util/defs.h"
#include "type.h"
#include "util/io.h"
#include "util/hash.h"
#include "errors.h"
#include "util/vec.h"
#include "util/rc.h"

namespace basil {
  class Def;
  class Env;
	class ASTNode;
  class Builtin;

  u64 symbol_value(const string& symbol);
  const string& symbol_for(u64 value);

  class ListValue;
  class SumValue;
  class IntersectValue;
  class ProductValue;
  class ArrayValue;
  class FunctionValue;
  class AliasValue;
  class MacroValue;
  class NamedValue;
  class ModuleValue;
  class DictValue;

  class Value {
    const Type* _type;
    union {
      i64 i;
      u64 u;
      double f;
      const Type* t;
      bool b;
      RC* rc;
    } _data;
    SourceLocation _loc;
    i32 _name = -1;
  public:
    Value();
    Value(const Type* type);
    Value(i64 i, const Type* type = INT);
    // Value(double d);
    Value(const string& s, const Type* type = SYMBOL);
    Value(const Type* type_value, const Type* type);
    Value(ListValue* l);
    Value(SumValue* s, const Type* type);
    Value(IntersectValue* i, const Type* type);
    Value(ProductValue* p);
    Value(ArrayValue* a);
    Value(ArrayValue* a, const Type* type);
    Value(DictValue* d);
    Value(ref<Env> env, const Builtin& b);
    Value(ref<Env> env, const Builtin& b, const string& name);
    Value(FunctionValue* f, const Type* ftype);
    Value(AliasValue* f);
    Value(MacroValue* f);
    Value(NamedValue* n, const Type* t);
    Value(ModuleValue* m);
		Value(ASTNode* n);
    ~Value();
    Value(const Value& other);
    Value& operator=(const Value& other);

    bool is_int() const;
    i64 get_int() const;
    i64& get_int();

    bool is_float() const;
    double get_float() const;
    double& get_float();

    bool is_symbol() const;
    u64 get_symbol() const;
    u64& get_symbol();

		bool is_string() const;
		const string& get_string() const;
		string& get_string();

    bool is_void() const;

    bool is_error() const;

    bool is_type() const;
    const Type* get_type() const;
    const Type*& get_type();

    bool is_bool() const;
    bool get_bool() const;
    bool& get_bool();

    bool is_list() const;
    const ListValue& get_list() const;
    ListValue& get_list();

    bool is_array() const;
    const ArrayValue& get_array() const;
    ArrayValue& get_array();

    bool is_sum() const;
    const SumValue& get_sum() const;
    SumValue& get_sum();

    bool is_intersect() const;
    const IntersectValue& get_intersect() const;
    IntersectValue& get_intersect();

    bool is_product() const;
    const ProductValue& get_product() const;
    ProductValue& get_product();

    bool is_dict() const;
    const DictValue& get_dict() const;
    DictValue& get_dict();

    bool is_function() const;
    const FunctionValue& get_function() const;
    FunctionValue& get_function(); 

    bool is_alias() const;
    const AliasValue& get_alias() const;
    AliasValue& get_alias(); 

    bool is_macro() const;
    const MacroValue& get_macro() const;
    MacroValue& get_macro(); 

		bool is_runtime() const;
		ASTNode* get_runtime() const;
		ASTNode*& get_runtime();

    bool is_named() const;
    const NamedValue& get_named() const;
    NamedValue& get_named();

    bool is_module() const;
    const ModuleValue& get_module() const;
    ModuleValue& get_module();

    const Type* type() const;
    void format(stream& io) const;
    u64 hash() const;
    bool operator==(const Value& other) const;
    bool operator!=(const Value& other) const;
    Value clone() const;

    void set_location(SourceLocation loc);
    void set_name(const string& name);
    SourceLocation loc() const;
  };

	class StringValue : public RC {
		string _value;
	public:
		StringValue(const string& value);

		string& value();
		const string& value() const;
	};

  class NamedValue : public RC {
    Value _inner;
  public:
    NamedValue(const Value& inner);

    Value& get();
    const Value& get() const;
  };

  class ListValue : public RC {
    Value _head, _tail;
  public:
    ListValue(const Value& head, const Value& tail);

    Value& head();
    const Value& head() const;
    Value& tail();
    const Value& tail() const;
  };

  class SumValue : public RC {
    Value _value;
  public:
    SumValue(const Value& value);
  
    Value& value();
    const Value& value() const;
  };

  class IntersectValue : public RC {
    map<const Type*, Value> _values;
  public:
    IntersectValue(const map<const Type*, Value>& values);

    u32 size() const;
    bool has(const Type* t) const;
    map<const Type*, Value>::const_iterator begin() const;
    map<const Type*, Value>::const_iterator end() const;
    map<const Type*, Value>::iterator begin();
    map<const Type*, Value>::iterator end();
    const map<const Type*, Value>& values() const;
    map<const Type*, Value>& values();
  };

  class ProductValue : public RC {
    vector<Value> _values;
  public:
    ProductValue(const vector<Value>& values);

    u32 size() const;
    Value& operator[](u32 i);
    const Value& operator[](u32 i) const;
    const Value* begin() const;
    const Value* end() const;
    Value* begin();
    Value* end();
    const vector<Value>& values() const;
  };

  class ArrayValue : public ProductValue {
  public:
    ArrayValue(const vector<Value>& values);
  };

  class DictValue : public RC {
    map<Value, Value> _entries;
  public:
    DictValue(const map<Value, Value>& entries);

    u32 size() const;
    Value* operator[](const Value& key);
    const Value* operator[](const Value& key) const;
    const map<Value, Value>::const_iterator begin() const;
    const map<Value, Value>::const_iterator end() const;
    map<Value, Value>::iterator begin();
    map<Value, Value>::iterator end();  
    const map<Value, Value>& entries() const;
  };

  using BuiltinFn = Value (*)(ref<Env>, const Value& params);

	extern const u64 KEYWORD_ARG_BIT;
	extern const u64 ARG_NAME_MASK;

  class FunctionValue : public RC {
		i64 _name;
    Value _code;
    const Builtin* _builtin;
    ref<Env> _env;
    vector<u64> _args;
		set<const FunctionValue*>* _calls;
		map<const Type*, ASTNode*>* _insts;
  public:
    FunctionValue(ref<Env> env, const vector<u64>& args,
      const Value& code, i64 name = -1);
    FunctionValue(ref<Env> env, const Builtin& builtin, i64 name = -1);
		~FunctionValue();
		FunctionValue(const FunctionValue& other);
		FunctionValue& operator=(const FunctionValue& other);

    const vector<u64>& args() const;
    const Value& body() const;
    bool is_builtin() const;
    u64 arity() const;
    const Builtin& get_builtin() const;
    ref<Env> get_env();
    const ref<Env> get_env() const;
		i64 name() const;
		bool found_calls() const;
		bool recursive() const;
		void add_call(const FunctionValue* other);
		ASTNode* instantiation(const Type* args) const;
    const map<const Type*, ASTNode*>* instantiations() const;
		void instantiate(const Type* args, ASTNode* body);
  };  

  class AliasValue : public RC {
    Value _value;
  public:
    AliasValue(const Value& value);
  
    Value& value();
    const Value& value() const;
  };

  using BuiltinMacro = Value (*)(ref<Env>, const Value& params);

  class MacroValue : public RC {
    Value _code;
    const Builtin* _builtin;
    ref<Env> _env;
    vector<u64> _args;
  public:
    MacroValue(ref<Env> env, const vector<u64>& args, 
      const Value& code);
    MacroValue(ref<Env> env, const Builtin& builtin);

    const vector<u64>& args() const;
    const Value& body() const;
    bool is_builtin() const;
    u64 arity() const;
    const Builtin& get_builtin() const;
    ref<Env> get_env();
    const ref<Env> get_env() const;
  };

  class ModuleValue : public RC {
    map<u64, Value> _entries;
  public:
    ModuleValue(const map<u64, Value>& entries);

    const map<u64, Value>& entries() const;
    bool has(u64 name) const;
    const Value& entry(u64 name) const;
  };

	Value lower(const Value& v);

  Value add(const Value& lhs, const Value& rhs);
  Value sub(const Value& lhs, const Value& rhs);
  Value mul(const Value& lhs, const Value& rhs);
  Value div(const Value& lhs, const Value& rhs);
  Value rem(const Value& lhs, const Value& rhs);

  Value logical_and(const Value& lhs, const Value& rhs);
  Value logical_or(const Value& lhs, const Value& rhs);
  Value logical_xor(const Value& lhs, const Value& rhs);
  Value logical_not(const Value& v);

  Value equal(const Value& lhs, const Value& rhs);
  Value inequal(const Value& lhs, const Value& rhs);
  Value less(const Value& lhs, const Value& rhs);
  Value greater(const Value& lhs, const Value& rhs);
  Value less_equal(const Value& lhs, const Value& rhs);
  Value greater_equal(const Value& lhs, const Value& rhs);

  Value head(const Value& v);
  Value tail(const Value& v);
  Value cons(const Value& head, const Value& tail);
  Value empty();
  Value at(const Value& tuple, const Value& index);
	vector<Value> to_vector(const Value& list);
	Value is_empty(const Value& list);

  Value list_of(const Value& element);
  template<typename ...Args>
  Value list_of(const Value& element, Args... args) {
    return cons(element, list_of(args...));
  }
  Value list_of(const vector<Value>& elements);

  template<typename ...Args>
  Value tuple_of(const Value& element, Args... args) {
    return tuple_of(vector_of(args...));
  }
  Value tuple_of(const vector<Value>& elements);

  template<typename ...Args>
  Value array_of(const Value& element, Args... args) {
    return array_of(vector_of(args...));
  }
  Value array_of(const vector<Value>& elements);

  Value dict_of(const map<Value, Value> &elements);


  Value error();

  Value length(const Value& str);

  Value read_line();
  Value strcat(const Value& a, const Value& b);
  Value substr(const Value& str, const Value& start, const Value& end);

  ASTNode* instantiate(SourceLocation loc, FunctionValue& fn, 
		const Type* args_type);
  Value type_of(const Value& v);
  Value is(const Value& v, const Value& t);
  Value cast(const Value& val, const Type* type);
  Value annotate(const Value& val, const Value& type);
  Value as(const Value& v, const Value& t);
  Value call(ref<Env> env, Value& function, const Value& arg, SourceLocation callsite);
	Value display(const Value& arg);
	Value assign(ref<Env> env, const Value& dest, const Value& src);
}

template<>
u64 hash(const basil::Value& t);

void write(stream& io, const basil::Value& t);

#endif