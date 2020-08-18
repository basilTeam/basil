#include "eval.h"

namespace basil {
  Value builtin_add(ref<Env> env, const Value& args) {
    return add(args.get_product()[0], args.get_product()[1]);
  }

  Value builtin_sub(ref<Env> env, const Value& args) {
    return sub(args.get_product()[0], args.get_product()[1]);
  }

  Value builtin_mul(ref<Env> env, const Value& args) {
    return mul(args.get_product()[0], args.get_product()[1]);
  }

  Value builtin_div(ref<Env> env, const Value& args) {
    return div(args.get_product()[0], args.get_product()[1]);
  }

  Value builtin_rem(ref<Env> env, const Value& args) {
    return rem(args.get_product()[0], args.get_product()[1]);
  }

  Value builtin_head(ref<Env> env, const Value& args) {
    return head(args.get_product()[0]);
  }

  Value builtin_tail(ref<Env> env, const Value& args) {
    return tail(args.get_product()[0]);
  }

  Value builtin_cons(ref<Env> env, const Value& args) {
    return cons(args.get_product()[0], args.get_product()[1]);
  }

  ref<Env> create_root_env() {
    ref<Env> root;
    root->def("nil", Value(VOID));
    root->def("+", Value(new FunctionValue(root, builtin_add)), 2);
    root->def("-", Value(new FunctionValue(root, builtin_sub)), 2);
    root->def("*", Value(new FunctionValue(root, builtin_mul)), 2);
    root->def("/", Value(new FunctionValue(root, builtin_div)), 2);
    root->def("%", Value(new FunctionValue(root, builtin_rem)), 2);
    root->def("head", Value(new FunctionValue(root, builtin_head)), 1);
    root->def("tail", Value(new FunctionValue(root, builtin_tail)), 1);
    root->def("::", Value(new FunctionValue(root, builtin_cons)), 2);
    root->def("cons", Value(new FunctionValue(root, builtin_cons)), 2);
    return root;
  }

  // stubs

  Value eval_list(ref<Env> env, const Value& list);
  Value eval(ref<Env> env, Value term);

  // utilities

  vector<Value> to_vector(const Value& list) {
    vector<Value> values;
    const Value* v = &list;
    while (v->is_list()) {
      values.push(v->get_list().head());
      v = &v->get_list().tail();
    }
    return values;
  }

  void traverse_list(ref<Env> env, const Value& list, 
    void(*fn)(ref<Env>, const Value&)) {
    const Value* v = &list;
    while (v->is_list()) {
      fn(env, v->get_list().head());
      v = &v->get_list().tail();
    }
  }

  void traverse_list(ref<Env> env, Value& list, 
    void(*fn)(ref<Env>, Value&)) {
    Value* v = &list;
    while (v->is_list()) {
      fn(env, v->get_list().head());
      v = &v->get_list().tail();
    }
  }

  void handle_splice(ref<Env> env, Value& item) {
    if (!item.is_list()) return;
    Value h = head(item);
    if (h.is_symbol() && h.get_symbol() == symbol_value("splice")) {
      Value t = tail(item);
      if (t.is_void()) item = Value(VOID);
      else item = eval(env, head(tail(item)));
    }
  }

  // definition stuff

  Value eval_list(ref<Env> env, const Value& term) {
    Value h = head(term);
    if (h.is_symbol()) {
      const string& name = symbol_for(h.get_symbol());
      if (name == "quote") {
        return head(tail(term));
      }
      else if (name == "def") {
        vector<Value> values = to_vector(term);
        if (values.size() < 2 || !values[1].is_symbol()) {
          err(term.loc(), "Expected name in definition.");
          return error();
        }
        if (values.size() < 3) {
          err(term.loc(), "Expected value in definition.");
          return error();
        }
        if (values.size() == 3) { // variable
          env->def(symbol_for(values[1].get_symbol()), 
            eval(env, values[2]));
          return Value(VOID);
        }
        else {
          if (!values[2].is_list()) {
            err(values[2].loc(), "Expected argument list in function.");
            return error();
          }
          vector<Value> args = to_vector(values[2]);
          vector<u64> argnames;
          for (Value v : args)
            if (v.is_symbol()) argnames.push(v.get_symbol());
          ref<Env> function_env(env);
          Value func(new FunctionValue(function_env, 
            argnames, values[3]));
          env->def(symbol_for(values[1].get_symbol()), 
            func, args.size());
          return Value(VOID);
        }
      }
      else if (env->find(name) && env->find(name)->is_procedure()) {
        vector<Value> args;
        const Value* v = &term.get_list().tail();
        while (v->is_list()) {
          args.push(eval(env, v->get_list().head()));
          v = &v->get_list().tail();
        }
        if (args.size() != env->find(name)->arity) {
          err(term.loc(), "Procedure expects ", env->find(name)->arity,
            " arguments, ", args.size(), " provided.");
          return error();
        }
        return call(eval(env, h), Value(new ProductValue(args)));
      }
    }
    err(term.loc(), "Could not evaluate list.");
    return error();
  }

  Value eval(ref<Env> env, Value term) {
    if (term.is_list()) {
      traverse_list(env, term, handle_splice);
      return eval_list(env, term);
    }
    else if (term.is_int()) return term;
    else if (term.is_symbol()) {
      const string& name = symbol_for(term.get_symbol());
      const Def* def = env->find(name);
      if (def) return def->value;
      else {
        err(term.loc(), "Undefined variable '", name, "'.");
        return error();
      }
    }
  }
}