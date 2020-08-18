#ifndef BASIL_VALUES_H
#define BASIL_VALUES_H

#include "defs.h"
#include "type.h"
#include "io.h"
#include "hash.h"
#include "errors.h"
#include "vec.h"
#include "rc.h"

namespace basil {
  u64 symbol_value(const string& symbol);
  const string& symbol_for(u64 value);

  class ListValue;
  class SumValue;
  class ProductValue;
  class FunctionValue;

  class Value {
    const Type* _type;
    union {
      i64 i;
      u64 u;
      const Type* t;
      RC* rc;
    } _data;
    SourceLocation _loc;
  public:
    Value();
    Value(const Type* type);
    Value(i64 i, const Type* type = INT);
    Value(const string& s, const Type* type = SYMBOL);
    Value(const Type* type_value, const Type* type);
    Value(ListValue* l);
    Value(SumValue* s, const Type* type);
    Value(ProductValue* p);
    Value(FunctionValue* f);
    ~Value();
    Value(const Value& other);
    Value& operator=(const Value& other);

    bool is_int() const;
    i64 get_int() const;
    i64& get_int();

    bool is_symbol() const;
    u64 get_symbol() const;
    u64& get_symbol();

    bool is_void() const;

    bool is_error() const;

    bool is_type() const;
    const Type* get_type() const;
    const Type*& get_type();

    bool is_list() const;
    const ListValue& get_list() const;
    ListValue& get_list();

    bool is_sum() const;
    const SumValue& get_sum() const;
    SumValue& get_sum();

    bool is_product() const;
    const ProductValue& get_product() const;
    ProductValue& get_product();

    bool is_function() const;
    const FunctionValue& get_function() const;
    FunctionValue& get_function(); 

    const Type* type() const;
    void format(stream& io) const;
    u64 hash() const;
    bool operator==(const Value& other) const;
    bool operator!=(const Value& other) const;

    void set_location(SourceLocation loc);
    SourceLocation loc() const;
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
  };

  using Builtin = Value (*)(ref<Env>, const Value& params);

  class FunctionValue : public RC {
    Value _code;
    Builtin _builtin;
    ref<Env> _env;
    vector<u64> _args;
  public:
    FunctionValue(ref<Env> env, const vector<u64>& args, 
      const Value& code);
    FunctionValue(ref<Env> env, Builtin builtin);

    const vector<u64>& args() const;
    const Value& body() const;
    bool is_builtin() const;
    Builtin get_builtin() const;
    ref<Env> get_env();
    const ref<Env> get_env() const;
  };  

  Value add(const Value& lhs, const Value& rhs);
  Value sub(const Value& lhs, const Value& rhs);
  Value mul(const Value& lhs, const Value& rhs);
  Value div(const Value& lhs, const Value& rhs);
  Value rem(const Value& lhs, const Value& rhs);

  Value head(const Value& v);
  Value tail(const Value& v);
  Value cons(const Value& head, const Value& tail);
  Value empty();
  Value list_of(const Value& element);

  template<typename ...Args>
  Value list_of(const Value& element, Args... args) {
    return cons(element, list_of(args...));
  }
  
  Value list_of(const vector<Value>& elements);

  Value error();
  Value type_of(const Value& v);

  Value call(const Value& function, const Value& arg);
}

template<>
u64 hash(const basil::Value& t);

void write(stream& io, const basil::Value& t);

#endif