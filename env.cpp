#include "env.h"

namespace basil {
  Def::Def(bool is_macro_in, bool is_infix_in, 
    u8 arity_in, u8 precedence_in):
    Def(Value(), is_macro_in, is_infix_in, arity_in, precedence_in) {}

  Def::Def(Value value_in, bool is_macro_in, bool is_infix_in, 
    u8 arity_in, u8 precedence_in):
    is_macro(is_macro_in), is_infix(is_infix_in),
    arity(arity_in), precedence(precedence_in), value(value_in) {}

  bool Def::is_procedure() const {
    return arity > 0 && !is_macro;
  }

  bool Def::is_variable() const {
    return arity == 0 && !is_macro;
  }

  bool Def::is_macro_procedure() const {
    return arity > 0 && is_macro;
  }
  
  bool Def::is_macro_variable() const {
    return arity == 0 && is_macro;
  }

  Env::Env(const ref<Env>& parent):
    _parent(parent) {}

  void Env::def(const string& name, u8 arity) {
    _defs[name] = Def(false, false, arity);
  }

  void Env::def_macro(const string& name, u8 arity) {
    _defs[name] = Def(true, false, arity);
  }

  void Env::infix(const string& name, u8 precedence, u8 arity) {
    _defs[name] = Def(false, true, arity, precedence);
  }

  void Env::infix_macro(const string& name, u8 precedence, u8 arity) {
    _defs[name] = Def(true, true, arity, precedence);
  }

  void Env::def(const string& name, const Value& value, u8 arity) {
    _defs[name] = Def(value, false, false, arity); // todo: detect arity
  }
  
  void Env::infix(const string& name, const Value& value, 
    u8 arity, u8 precedence) {
    _defs[name] = Def(value, false, true, arity, precedence); 
    // todo: detect arity
  }

  const Def* Env::find(const string& name) const {
    auto it = _defs.find(name);
    if (it == _defs.end()) 
      return _parent ? _parent->find(name) : nullptr;
    else return &it->second;
  }

  Def* Env::find(const string& name) {
    auto it = _defs.find(name);
    if (it == _defs.end()) 
      return _parent ? _parent->find(name) : nullptr;
    else return &it->second;
  }
}