#ifndef BASIL_ENV_H
#define BASIL_ENV_H

#include "defs.h"
#include "hash.h"
#include "str.h"
#include "rc.h"
#include "values.h"

namespace basil {
  struct Def {
    bool is_macro; // is the definition a macro alias or procedure?
    bool is_infix; // is the definition for an infix procedure?
    u8 arity; // 0 is a variable or alias, 1 or higher is procedure.
    u8 precedence; // precedence of infix procedure
    Value value;

    Def(bool is_macro_in = false, bool is_infix_in = false, 
        u8 arity_in = 0, u8 precedence_in = 0);
    Def(Value value_in, bool is_macro_in = false, 
        bool is_infix_in = false, u8 arity_in = 0, u8 precedence_in = 0);
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

    void def(const string& name, u8 arity = 0);
    void def_macro(const string& name, u8 arity = 0);
    void infix(const string& name, u8 precedence, u8 arity = 2);
    void infix_macro(const string& name, u8 precedence, u8 arity = 2);
    void def(const string& name, const Value& value, u8 arity = 0);
    void infix(const string& name, const Value& value, u8 arity = 2, 
      u8 precedence = 0);
    bool lookup(const string& name) const;
    const Def* find(const string& name) const;
    Def* find(const string& name);
  };
}

#endif