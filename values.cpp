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
    is_bool() ? _data.b = i : _data.i = i;
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

  Value::Value(AliasValue* a):
    _type(ALIAS) {
    _data.rc = a;
  }

  Value::Value(MacroValue* m):
    _type(find<MacroType>(m->arity())) {
    _data.rc = m;
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

  const Type* Value::type() const {
    return _type;
  }

  void Value::format(stream& io) const {
    if (is_void()) write(io, "()");
    else if (is_error()) write(io, "error");
    else if (is_int()) write(io, get_int());
    else if (is_symbol()) write(io, symbol_for(get_symbol()));
    else if (is_type()) write(io, get_type());
    else if (is_bool()) write(io, get_bool());
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
      for (const Value& v : get_product()) {
        write(io, first ? "" : ", ", v);
        first = false;
      }
      write(io, ")");
    }
    else if (is_function()) write(io, "<procedure>");
    else if (is_alias()) write(io, "<alias>");
    else if (is_macro()) write(io, "<macro>");
  }

  u64 Value::hash() const {
    if (is_void()) return 11103515024943898793ul;
    else if (is_error()) return 14933118315469276343ul;
    else if (is_int()) return ::hash(get_int()) ^ 6909969109598810741ul;
    else if (is_symbol()) return ::hash(get_symbol()) ^ 1899430078708870091ul;
    else if (is_type()) return get_type()->hash();
    else if (is_bool()) 
      return get_bool() ? 9269586835432337327ul
        : 18442604092978916717ul;
    else if (is_list()) {
      u64 h = 9572917161082946201ul;
      Value ptr = *this;
      while (ptr.is_list()) {
        h ^= ptr.get_list().head().hash();
        ptr = ptr.get_list().tail();
      }
      return h;
    }
    else if (is_sum()) {
      return get_sum().value().hash() ^ 7458465441398727979ul;
    }
    else if (is_product()) {
      u64 h = 16629385277682082909ul;
      for (const Value& v : get_product()) h ^= v.hash();
      return h;
    }
    else if (is_function()) {
      u64 h = 10916307465547805281ul;
      if (get_function().is_builtin())
        h ^= ::hash(get_function().get_builtin());
      else {
        h ^= get_function().body().hash();
        for (u64 arg : get_function().args())
          h ^= ::hash(arg);
      }
      return h;
    }
    else if (is_alias()) return 6860110315984869641ul;
    else if (is_macro()) {
      u64 h = 16414641732770006573ul;
      if (get_macro().is_builtin())
        h ^= ::hash(get_macro().get_builtin());
      else {
        h ^= get_macro().body().hash();
        for (u64 arg : get_macro().args())
          h ^= ::hash(arg);
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
    else if (is_bool()) return get_bool() == other.get_bool();
    else if (is_list()) {
      const ListValue* l = &get_list(), *o = &other.get_list();
      do {
        if (l->head() != o->head()) return false;
        l = &l->tail().get_list(), o = &o->tail().get_list();
      } while (l->tail().is_list() && o->tail().is_list());
      return l->head() == o->head() && l->tail().is_void() && o->tail().is_void();
    }
    else if (is_function()) {
      if (get_function().is_builtin())
        return get_function().get_builtin() == 
          other.get_function().get_builtin();
      else {
        if (other.get_function().arity() != get_function().arity())
          return false;
        for (u32 i = 0; i < get_function().arity(); i ++) {
          if (other.get_function().args()[i] !=
              get_function().args()[i]) return false;
        }
        return get_function().body() == other.get_function().body();
      }
    }
    else if (is_macro()) {
      if (get_macro().is_builtin())
        return get_macro().get_builtin() == 
          other.get_macro().get_builtin();
      else {
        if (other.get_macro().arity() != get_macro().arity())
          return false;
        for (u32 i = 0; i < get_macro().arity(); i ++) {
          if (other.get_macro().args()[i] !=
              get_macro().args()[i]) return false;
        }
        return get_macro().body() == other.get_macro().body();
      }
    }
    return type() == other.type();
  }

  Value Value::clone() const {
    if (is_list())
      return Value(new ListValue(get_list().head().clone(),
        get_list().tail().clone()));
    else if (is_sum())
      return Value(new SumValue(get_sum().value()), type());
    else if (is_product()) {
      vector<Value> values;
      for (const Value& v : get_product()) values.push(v);
      return Value(new ProductValue(values));
    }
    else if (is_function()) {
      if (get_function().is_builtin()) {
        return Value(new FunctionValue(get_function().get_env()->clone(),
          get_function().get_builtin(), get_function().arity()));
      }
      else {
        return Value(new FunctionValue(get_function().get_env()->clone(),
          get_function().args(), get_function().body().clone()));
      }
    }
    else if (is_alias())
      return Value(new AliasValue(get_alias().value()));
    else if (is_macro()) {
      if (get_macro().is_builtin()) {
        return Value(new MacroValue(get_macro().get_env()->clone(),
          get_macro().get_builtin(), get_macro().arity()));
      }
      else {
        return Value(new MacroValue(get_macro().get_env()->clone(),
          get_macro().args(), get_macro().body().clone()));
      }
    }
    return *this;
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

  FunctionValue::FunctionValue(ref<Env> env, BuiltinFn builtin, 
    u64 arity):
    _builtin(builtin), _env(env), _builtin_arity(arity) {}

  const vector<u64>& FunctionValue::args() const {
    return _args;
  }

  bool FunctionValue::is_builtin() const {
    return _builtin;
  }

  BuiltinFn FunctionValue::get_builtin() const {
    return _builtin;
  }

  ref<Env> FunctionValue::get_env() {
    return _env;
  }

  const ref<Env> FunctionValue::get_env() const {
    return _env;
  }

  u64 FunctionValue::arity() const {
    return _builtin ? _builtin_arity : _args.size();
  }

  const Value& FunctionValue::body() const {
    return _code;
  }

  AliasValue::AliasValue(const Value& value):
    _value(value) {}

  Value& AliasValue::value() {
    return _value;
  }

  const Value& AliasValue::value() const {
    return _value;
  }

  MacroValue::MacroValue(ref<Env> env, const vector<u64>& args,
    const Value& code):
    _code(code), _builtin(nullptr), _env(env), _args(args) {}

  MacroValue::MacroValue(ref<Env> env, BuiltinMacro builtin, 
    u64 arity):
    _builtin(builtin), _env(env), _builtin_arity(arity) {}

  const vector<u64>& MacroValue::args() const {
    return _args;
  }

  bool MacroValue::is_builtin() const {
    return _builtin;
  }

  BuiltinMacro MacroValue::get_builtin() const {
    return _builtin;
  }

  ref<Env> MacroValue::get_env() {
    return _env;
  }

  const ref<Env> MacroValue::get_env() const {
    return _env;
  }

  u64 MacroValue::arity() const {
    return _builtin ? _builtin_arity : _args.size();
  }

  const Value& MacroValue::body() const {
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

  Value binary_logic(const Value& lhs, const Value& rhs, bool(*op)(bool, bool)) {
    if (!lhs.is_bool() && !lhs.is_error()) {
      err(lhs.loc(), "Expected boolean value in logical expression, found '",
          lhs.type(), "'.");
      return error();
    }
    if (!rhs.is_bool() && !lhs.is_error()) {
      err(rhs.loc(), "Expected boolean value in logical expression, found '",
          rhs.type(), "'.");
      return error();
    }
    if (lhs.is_error() || rhs.is_error()) return error();

    return Value(op(lhs.get_bool(), rhs.get_bool()), BOOL);
  }

  Value logical_and(const Value& lhs, const Value& rhs) {
    return binary_logic(lhs, rhs, [](bool a, bool b) -> bool { return a && b; });
  }
  
  Value logical_or(const Value& lhs, const Value& rhs) {
    return binary_logic(lhs, rhs, [](bool a, bool b) -> bool { return a || b; });
  }
  
  Value logical_xor(const Value& lhs, const Value& rhs) {
    return binary_logic(lhs, rhs, [](bool a, bool b) -> bool { return a ^ b; });
  }

  Value logical_not(const Value& v) {
    if (!v.is_bool() && !v.is_error()) {
      err(v.loc(), "Expected boolean value in logical expression, found '",
        v.type(), "'.");
      return error();
    }
    if (v.is_error()) return error();

    return Value(!v.get_bool(), BOOL);
  }

  Value equal(const Value& lhs, const Value& rhs) {
    if (lhs.is_error() || rhs.is_error()) return error();
    return Value(lhs == rhs, BOOL);
  }
  
  Value inequal(const Value& lhs, const Value& rhs) {
    return Value(!equal(lhs, rhs).get_bool(), BOOL);
  }

  Value binary_relation(const Value& lhs, const Value& rhs, bool(*op)(i64, i64)) {
    if (!lhs.is_int() && !lhs.is_error()) {
      err(lhs.loc(), "Expected integer value in relational expression, found '",
          lhs.type(), "'.");
      return error();
    }
    if (!rhs.is_int() && !lhs.is_error()) {
      err(rhs.loc(), "Expected integer value in relational expression, found '",
          rhs.type(), "'.");
      return error();
    }
    if (lhs.is_error() || rhs.is_error()) return error();

    return Value(op(lhs.get_int(), rhs.get_int()), BOOL);
  }

  Value less(const Value& lhs, const Value& rhs) {
    return binary_relation(lhs, rhs, [](i64 a, i64 b) -> bool { return a < b; });
  }

  Value greater(const Value& lhs, const Value& rhs) {
    return binary_relation(lhs, rhs, [](i64 a, i64 b) -> bool { return a > b; });
  }

  Value less_equal(const Value& lhs, const Value& rhs) {
    return binary_relation(lhs, rhs, [](i64 a, i64 b) -> bool { return a <= b; });
  }

  Value greater_equal(const Value& lhs, const Value& rhs) {
    return binary_relation(lhs, rhs, [](i64 a, i64 b) -> bool { return a >= b; });
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
    for (i64 i = i64(elements.size()) - 1; i >= 0; i --) {
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