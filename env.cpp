#include "env.h"

namespace basil {
  Def::Def(bool is_macro_in, bool is_procedure_in, bool is_infix_in, 
    u8 arity_in, u8 precedence_in):
    Def(Value(), is_macro_in, is_procedure_in, is_infix_in, 
      arity_in, precedence_in) {}

  Def::Def(Value value_in, bool is_macro_in, bool is_procedure_in,
    bool is_infix_in, u8 arity_in, u8 precedence_in):
    value(value_in), is_macro(is_macro_in), 
    is_proc(is_procedure_in), is_infix(is_infix_in),
    arity(arity_in), precedence(precedence_in) {}

  bool Def::is_procedure() const {
    return is_proc && !is_macro;
  }

  bool Def::is_variable() const {
    return !is_proc && !is_macro;
  }

  bool Def::is_macro_procedure() const {
    return is_proc && is_macro;
  }
  
  bool Def::is_macro_variable() const {
    return !is_proc && is_macro;
  }

  Env::Env(const ref<Env>& parent):
    _parent(parent) {}

  void Env::def(const string& name) {
    _defs[name] = Def(false, false, false);
  }

  void Env::def_macro(const string& name) {
    _defs[name] = Def(true, false, false);
  }

  void Env::def(const string& name, u8 arity) {
    _defs[name] = Def(false, true, false, arity);
  }

  void Env::def_macro(const string& name, u8 arity) {
    _defs[name] = Def(true, true, false, arity);
  }

  void Env::def(const string& name, const Value& value) {
    _defs[name] = Def(value, false, false, false);
  }

  void Env::def(const string& name, const Value& value, u8 arity) {
    _defs[name] = Def(value, false, true, false, arity); 
  }

  void Env::def_macro(const string& name, const Value& value) {
    _defs[name] = Def(value, true, false, false);
  }

  void Env::def_macro(const string& name, const Value& value, u8 arity) {
    _defs[name] = Def(value, true, true, false, arity); 
  }

  void Env::infix(const string& name, u8 arity, u8 precedence) {
    _defs[name] = Def(false, true, true, arity, precedence);
  }

  void Env::infix(const string& name, const Value& value, u8 arity, u8 precedence) {
    _defs[name] = Def(value, false, true, true, arity, precedence);
  }

  void Env::infix_macro(const string& name, u8 arity, u8 precedence) {
    _defs[name] = Def(true, true, true, arity, precedence);
  }

  void Env::infix_macro(const string& name, const Value& value, 
    u8 arity, u8 precedence) {
    _defs[name] = Def(value, true, true, true, arity, precedence);
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
  
  void Env::format(stream& io) const {
    write(io, "{");
    bool first = true;
    for (auto& def : _defs) 
      write(io, first ? "" : " ", def.first), first = false;
    write(io, "}");
    if (_parent) write(io, " -> "), _parent->format(io);
  }
  
  ref<Env> Env::clone() const {
    Env new_env(_parent);
    ref<Env> new_ref(new_env);
    for (auto& p : _defs) new_ref->_defs.put(p.first, p.second);
    return new_env;
  }
}