#include "values.h"
#include "vec.h"
#include "env.h"
#include "eval.h"

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
    }
    else return it->second;
  }
  
  const string& symbol_for(u64 value) {
    return symbol_array[value];
  }

  Value::Value(): Value(VOID) {}

  Value::Value(const Type* type):
    _type(type) {}

  Value::Value(i64 i, const Type* type):
    _type(type) {
    _data.i = i;
  }

  Value::Value(const string& s, const Type* type):
    _type(type) {
    _data.u = symbol_value(s);
  }

  Value::Value(const Type* type_value, const Type* type):
    _type(type) {
    _data.t = type_value;
  }

  Value::Value(ListValue* l):
    _type(find<ListType>(l->head().type())) {
    _data.rc = l;
  }

  Value::Value(SumValue* s, const Type* type):
    _type(type) {
    _data.rc = s;
  }

  Value::Value(ProductValue* p) {
    vector<const Type*> ts;
    for (const Value& v : *p) ts.push(v.type());
    _type = find<ProductType>(ts);
    _data.rc = p;
  }

  Value::Value(FunctionValue* f):
    _type(find<FunctionType>(INT, INT)) {
    _data.rc = f;
  }

  Value::~Value() {
    if (_type->kind() & GC_KIND_FLAG) _data.rc->dec();
  }
  
  Value::Value(const Value& other):
    _type(other._type), _loc(other._loc) {
    _data.u = other._data.u; // copy over raw data
    if (_type->kind() & GC_KIND_FLAG) _data.rc->inc();
  }

  Value& Value::operator=(const Value& other) {
    if (other.type()->kind() & GC_KIND_FLAG) other._data.rc->inc();
    if (_type->kind() & GC_KIND_FLAG) _data.rc->dec();
    _type = other._type;
    _loc = other._loc;
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

  bool Value::is_list() const {
    return _type->kind() == KIND_LIST;
  }

  const ListValue& Value::get_list() const {
    return *(const ListValue*)_data.rc;
  }

  ListValue& Value::get_list() {
    return *(ListValue*)_data.rc;
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

  bool Value::is_product() const {
    return _type->kind() == KIND_PRODUCT;
  }

  const ProductValue& Value::get_product() const {
    return *(const ProductValue*)_data.rc;
  }

  ProductValue& Value::get_product() {
    return *(ProductValue*)_data.rc;
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

  const Type* Value::type() const {
    return _type;
  }

  void Value::format(stream& io) const {
    if (is_void()) write(io, "()");
    else if (is_error()) write(io, "error");
    else if (is_int()) write(io, get_int());
    else if (is_symbol()) write(io, symbol_for(get_symbol()));
    else if (is_type()) write(io, get_type());
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
    }
    else if (is_sum()) write(io, get_sum().value());
    else if (is_product()) {
      bool first = true;
      write(io, "(");
      const Value* ptr = this;
      while (ptr->is_list()) {
        write(io, first ? "" : ", ", ptr->get_list().head());
        ptr = &ptr->get_list().tail();
        first = false;
      }
      write(io, ")");
    }
    else if (is_function()) {
      write(io, "<procedure>");
    }
  }

  u64 Value::hash() const {
    if (is_void()) return 11103515024943898793ul;
    else if (is_error()) return 14933118315469276343ul;
    else if (is_int()) return ::hash(get_int()) ^ 6909969109598810741ul;
    else if (is_symbol()) return ::hash(get_symbol()) ^ 1899430078708870091ul;
    else if (is_type()) return get_type()->hash();
    else if (is_list()) {
      u64 h = 9572917161082946201ul;
      Value ptr = *this;
      while (ptr.is_list()) {
        h ^= ptr.get_list().head().hash();
        ptr = ptr.get_list().tail();
      }
      return h;
    }
    return 0;
  }
  
  bool Value::operator==(const Value& other) const {
    if (type() != other.type()) return false;
    else if (is_int()) return get_int() == other.get_int();
    else if (is_symbol()) return get_symbol() == other.get_symbol();
    else if (is_type()) return get_type() == other.get_type();
    else if (is_list()) {
      const ListValue* l = &get_list(), *o = &other.get_list();
      do {
        if (l->head() != o->head()) return false;
        l = &l->tail().get_list(), o = &o->tail().get_list();
      } while (l->tail().is_list() && o->tail().is_list());
      return l->head() == o->head() && l->tail().is_void() && o->tail().is_void();
    }
    return type() == other.type();
  }

  bool Value::operator!=(const Value& other) const {
    return !(*this == other);
  }

  void Value::set_location(SourceLocation loc) {
    _loc = loc;
  }

  SourceLocation Value::loc() const {
    return _loc;
  }

  ListValue::ListValue(const Value& head, const Value& tail) : 
    _head(head), _tail(tail) {}

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

  SumValue::SumValue(const Value& value):
    _value(value) {}

  Value& SumValue::value() {
    return _value;
  }

  const Value& SumValue::value() const {
    return _value;
  }

  ProductValue::ProductValue(const vector<Value>& values):
    _values(values) {}

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

  FunctionValue::FunctionValue(ref<Env> env, const vector<u64>& args,
    const Value& code):
    _code(code), _builtin(nullptr), _env(env), _args(args) {}

  FunctionValue::FunctionValue(ref<Env> env, Builtin builtin):
    _builtin(builtin), _env(env) {}

  const vector<u64>& FunctionValue::args() const {
    return _args;
  }

  bool FunctionValue::is_builtin() const {
    return _builtin;
  }

  Builtin FunctionValue::get_builtin() const {
    return _builtin;
  }

  ref<Env> FunctionValue::get_env() {
    return _env;
  }

  const ref<Env> FunctionValue::get_env() const {
    return _env;
  }

  const Value& FunctionValue::body() const {
    return _code;
  }

  Value binary_arithmetic(const Value& lhs, const Value& rhs, i64(*op)(i64, i64)) {
    if (!lhs.is_int() && !lhs.is_error()) {
      err(lhs.loc(), "Expected integer value in arithmetic expression, found '",
          lhs.type(), "'.");
      return error();
    }
    if (!rhs.is_int() && !lhs.is_error()) {
      err(rhs.loc(), "Expected integer value in arithmetic expression, found '",
          rhs.type(), "'.");
      return error();
    }
    if (lhs.is_error() || rhs.is_error()) return error();

    return Value(op(lhs.get_int(), rhs.get_int()));
  }
  
  Value add(const Value& lhs, const Value& rhs) {
    return binary_arithmetic(lhs, rhs, [](i64 a, i64 b) -> i64 { return a + b; });
  }

  Value sub(const Value& lhs, const Value& rhs) {
    return binary_arithmetic(lhs, rhs, [](i64 a, i64 b) -> i64 { return a - b; });
  }

  Value mul(const Value& lhs, const Value& rhs) {
    return binary_arithmetic(lhs, rhs, [](i64 a, i64 b) -> i64 { return a * b; });
  }

  Value div(const Value& lhs, const Value& rhs) {
    return binary_arithmetic(lhs, rhs, [](i64 a, i64 b) -> i64 { return a / b; });
  }

  Value rem(const Value& lhs, const Value& rhs) {
    return binary_arithmetic(lhs, rhs, [](i64 a, i64 b) -> i64 { return a % b; });
  }

  Value head(const Value& v) {
    if (!v.is_list() && !v.is_error()) {
      err(v.loc(), "Can only get head of value of list type, given '",
          v.type(), "'.");
      return error();
    }
    if (v.is_error()) return error();

    return v.get_list().head();
  }

  Value tail(const Value& v) {
    if (!v.is_list() && !v.is_error()) {
      err(v.loc(), "Can only get tail of value of list type, given '",
          v.type(), "'.");
      return error();
    }
    if (v.is_error()) return error();

    return v.get_list().tail();
  }

  Value cons(const Value& head, const Value& tail) {
    if (!tail.is_list() && !tail.is_void() && !tail.is_error()) {
      err(tail.loc(), "Tail of cons cell must be a list or void, given '",
          tail.type(), "'.");
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
    for (i64 i = elements.size() - 1; i >= 0; i --) {
      l = cons(elements[i], l);
    }
    return l;
  }

  Value error() {
    return Value(ERROR);
  }

  Value type_of(const Value& v) {
    return Value(v.type(), TYPE);
  }

  Value call(const Value& function, const Value& arg) {
    if (!function.is_function() && !function.is_error()) {
      err(function.loc(), "Called value is not a procedure.");
      return error();
    }
    if (!arg.is_product() && !arg.is_error()) {
      err(arg.loc(), "Arguments not provided as a product.");
      return error();
    }
    if (function.is_error() || arg.is_error()) return error();

    const FunctionValue& fn = function.get_function();
    if (fn.is_builtin()) {
      return fn.get_builtin()(fn.get_env(), arg);
    }
    else {
      ref<Env> env = fn.get_env();
      u32 argc = arg.get_product().size(), arity = fn.args().size();
      if (argc != arity) {
        err(function.loc(), "Procedure requires ", arity, " arguments, ",
          argc, " provided.");
        return error();
      }
      for (u32 i = 0; i < arity; i ++) {
        const string& argname = symbol_for(fn.args()[i]);
        env->find(argname)->value = arg.get_product()[i];
      }
      return eval(env, fn.body());
    }
  }
}

template<>
u64 hash(const basil::Value& t) {
  return t.hash();
}

void write(stream& io, const basil::Value& t) {
  t.format(io);
}