#ifndef BASIL_ENV_H
#define BASIL_ENV_H

#include "ir.h"
#include "ssa.h"
#include "util/defs.h"
#include "util/hash.h"
#include "util/rc.h"
#include "util/vec.h"
#include "values.h"

namespace basil {
  enum ArgType {
    ARG_VARIABLE,
    ARG_KEYWORD,
    ARG_VARIADIC
  };

  struct Arg {
    ArgType type;
    u64 name;

    bool matches(const Value& term) const;
  };

  class Proto {
    static void add_arg(vector<Arg>& proto, const ArgType& arg);
    static void add_arg(vector<Arg>& proto, const string& arg);
    static void add_args(vector<Arg>& proto);

    template<typename T, typename ...Args>
    static void add_args(vector<Arg>& proto, const T& arg, const Args&... rest) {
      Proto::add_arg(proto, arg);
      Proto::add_args(proto, rest...);
    }
    Proto();
  public:
    const static Proto VARIABLE;
    vector<vector<Arg>> overloads;

    Proto(const vector<Arg>& args);

    template<typename ...Args>
    static Proto of(const Args&... args) {
      vector<Arg> proto;
      add_args(proto, args...);
      return Proto(proto);
    }

    template<typename ...Args>
    static Proto overloaded(const Args&... args) {
      vector<Proto> protos = vector_of<Proto>(args...);
      Proto proto;
      for (const Proto& other : protos) proto.overload(other);
      return proto;
    }

    void overload(const Proto& other);
  };

  struct Def {
    Value value;
    Proto proto;
    bool is_macro; // is the definition a macro alias or procedure?
    u8 precedence; // precedence of infix procedure
    SSAIdent ident;
		Location location;

    Def();
    Def(const Proto& proto_in, bool is_macro_in, u8 precedence_in);
    Def(const Proto& proto_in, const Value& value_in, bool is_macro_in, u8 precedence_in);
    bool is_procedure() const;
    bool is_variable() const;
    bool is_macro_procedure() const;
    bool is_macro_variable() const;
    bool is_infix() const;
  };

  class Env {
    map<string, Def> _defs;
    ref<Env> _parent;
		bool _runtime;
  public:
    Env(const ref<Env>& parent = nullptr);

    void var(const string& name);
    void func(const string& name, const Proto& proto, u8 precedence);
    void var(const string& name, const Value& value);
    void func(const string& name, const Proto& proto, const Value& value, u8 precedence);
    void alias(const string& name);
    void macro(const string& name, const Proto& proto, u8 precedence);
    void alias(const string& name, const Value& value);
    void macro(const string& name, const Proto& proto, const Value& value, u8 precedence);
    const Def* find(const string& name) const;
    Def* find(const string& name);
    void format(stream& io) const;
    ref<Env> clone() const;
		map<string, Def>::const_iterator begin() const;
		map<string, Def>::const_iterator end() const;
		void import(ref<Env> env);
    void import_single(const string &name, const Def &d);
		void make_runtime();
		bool is_runtime() const;
  };
}

#endif