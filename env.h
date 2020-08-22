#ifndef BASIL_ENV_H
#define BASIL_ENV_H

#include "defs.h"
#include "hash.h"
#include "str.h"
#include "rc.h"
#include "values.h"

namespace basil {
  struct Def {
    Value value;
    bool is_macro; // is the definition a macro alias or procedure?
    bool is_infix; // is the definition for an infix procedure?
    bool is_proc; // is the definition a scalar or procedure?
    u8 arity; // number of arguments taken by a procedure.
    u8 precedence; // precedence of infix procedure

    Def(bool is_macro_in = false, bool is_procedure_in = false, 
        bool is_infix_in = false, u8 arity_in = 0, u8 precedence_in = 0);
    Def(Value value_in, bool is_procedure_in = false, 
        bool is_macro_in = false, bool is_infix_in = false, 
        u8 arity_in = 0, u8 precedence_in = 0);
    bool is_procedure() const;
    bool is_variable() const;
    bool is_macro_procedure() const;
    bool is_macro_variable() const;
  };

  class Env {
    map<string, Def> _defs;
    ref<Env> _parent;
  public:
    Env(const ref<Env>& parent = ref<Env>::null());

    void def(const string& name);
    void def(const string& name, u8 arity);
    void def_macro(const string& name);
    void def_macro(const string& name, u8 arity);
    void def(const string& name, const Value& value);
    void def(const string& name, const Value& value, u8 arity);
    void def_macro(const string& name, const Value& value);
    void def_macro(const string& name, const Value& value, u8 arity);
    void infix(const string& name, u8 arity, u8 precedence);
    void infix(const string& name, const Value& value, u8 arity, u8 precedence);
    void infix_macro(const string& name, u8 arity, u8 precedence);
    void infix_macro(const string& name, const Value& value, u8 arity, u8 precedence);
    const Def* find(const string& name) const;
    Def* find(const string& name);
    void format(stream& io) const;
    ref<Env> clone() const;
  };
}

#endif