#include "env.h"

namespace basil {

  // Arg

  bool Arg::matches(const Value& term) const {
    if (type == ARG_KEYWORD) return term.is_symbol() && term.get_symbol() == name;
    else return true;
  }

  // Proto

  const Proto Proto::VARIABLE;

  Proto::Proto() {}

  Proto::Proto(const vector<Arg>& args) {
    overloads.push(args);
  }

  void Proto::add_arg(vector<Arg>& proto, const ArgType& arg) {
    proto.push({ arg, 0 }); // name is irrelevant so we pick 0
  }

  void Proto::add_arg(vector<Arg>& proto, const string& arg) {
    proto.push({ ARG_VARIABLE, symbol_value(arg) });
  }

  void Proto::add_args(vector<Arg>& proto) {}

  bool args_equal(const vector<Arg>& a, const vector<Arg>& b) {
    if (a.size() != b.size()) return false;
    for (u32 i = 0; i < a.size(); i ++) {
      if (a[i].type != b[i].type) return false;
      if (a[i].name != b[i].name) return false;
    }
    return true;
  }

  void Proto::overload(const Proto& other) {
    for (const vector<Arg>& v : other.overloads) {
      bool duplicate = false;
      for (const vector<Arg>& w : other.overloads) {
        if (args_equal(v, w)) {
          duplicate = true;
          break;
        }
      }
      if (!duplicate) overloads.push(v);
    }
  }

  // Def

  Def::Def():
    Def(Proto(vector<Arg>()), Value(VOID), false, 0) {}

  Def::Def(const Proto& proto_in, bool is_macro_in, u8 precedence_in):
    Def(proto_in, Value(VOID), is_macro_in, precedence_in) {}

  Def::Def(const Proto& proto_in, const Value& value_in, bool is_macro_in, u8 precedence_in):
    proto(proto_in), value(value_in), is_macro(is_macro_in), precedence(precedence_in) {}

  bool Def::is_procedure() const {
    return !is_macro && proto.overloads.size() > 0;
  }

  bool Def::is_variable() const {
    return !is_macro && proto.overloads.size() == 0;
  }

  bool Def::is_macro_procedure() const {
    return is_macro && proto.overloads.size() > 0;
  }
  
  bool Def::is_macro_variable() const {
    return is_macro && proto.overloads.size() == 0;
  }
  
  bool Def::is_infix() const {
    if (proto.overloads.size() == 0) return false;
    for (const vector<Arg>& v : proto.overloads)
      if (v.size() == 0 || v[0].type == ARG_KEYWORD) return false;
    return true;
  }

  Env::Env(const ref<Env>& parent):
    _parent(parent), _runtime(false) {}

  void Env::var(const string& name) {
    _defs[name] = Def(Proto::VARIABLE, false, 0);
  }

  void Env::func(const string& name, const Proto& proto, u8 precedence) {
    _defs[name] = Def(proto, false, precedence);
  }

  void Env::var(const string& name, const Value& value) {
    _defs[name] = Def(Proto::VARIABLE, value, false, 0);
  }

  void Env::func(const string& name, const Proto& proto, const Value& value, u8 precedence) {
    _defs[name] = Def(proto, value, false, precedence);
  }

  void Env::alias(const string& name) {
    _defs[name] = Def(Proto::VARIABLE, true, 0);
  }

  void Env::macro(const string& name, const Proto& proto, u8 precedence) {
    _defs[name] = Def(proto, true, precedence);
  }

  void Env::alias(const string& name, const Value& value) {
    _defs[name] = Def(Proto::VARIABLE, value, true, 0);
  }

  void Env::macro(const string& name, const Proto& proto, const Value& value, u8 precedence) {
    _defs[name] = Def(proto, value, true, precedence);
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
    ref<Env> new_ref = newref<Env>(_parent);
    for (auto& p : _defs) new_ref->_defs.put(p.first, p.second);
		new_ref->_runtime = _runtime;
    return new_ref;
  }

	map<string, Def>::const_iterator Env::begin() const {
		return _defs.begin();
	}

	map<string, Def>::const_iterator Env::end() const {
		return _defs.end();
	}

	void Env::import(ref<Env> env) {
		for (auto& p : env->_defs) _defs.put(p.first, p.second);
	}

  void Env::import_single(const string &name, const Def &d) {
    _defs.put(name, d);
  }

	void Env::make_runtime() {
		_runtime = true;
	}

	bool Env::is_runtime() const {
		return _runtime;
	}
}