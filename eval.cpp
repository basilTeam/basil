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

  Value builtin_and(ref<Env> env, const Value& args) {
    return logical_and(args.get_product()[0], args.get_product()[1]);
  }

  Value builtin_or(ref<Env> env, const Value& args) {
    return logical_or(args.get_product()[0], args.get_product()[1]);
  }

  Value builtin_xor(ref<Env> env, const Value& args) {
    return logical_xor(args.get_product()[0], args.get_product()[1]);
  }

  Value builtin_not(ref<Env> env, const Value& args) {
    return logical_not(args.get_product()[0]);
  }

  Value builtin_equal(ref<Env> env, const Value& args) {
    return equal(args.get_product()[0], args.get_product()[1]);
  }

  Value builtin_inequal(ref<Env> env, const Value& args) {
    return inequal(args.get_product()[0], args.get_product()[1]);
  }

  Value builtin_less(ref<Env> env, const Value& args) {
    return less(args.get_product()[0], args.get_product()[1]);
  }

  Value builtin_greater(ref<Env> env, const Value& args) {
    return greater(args.get_product()[0], args.get_product()[1]);
  }

  Value builtin_less_equal(ref<Env> env, const Value& args) {
    return less_equal(args.get_product()[0], args.get_product()[1]);
  }

  Value builtin_greater_equal(ref<Env> env, const Value& args) {
    return greater_equal(args.get_product()[0], args.get_product()[1]);
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
    root->infix("+", Value(new FunctionValue(root, builtin_add, 2)), 2, 20);
    root->infix("-", Value(new FunctionValue(root, builtin_sub, 2)), 2, 20);
    root->infix("*", Value(new FunctionValue(root, builtin_mul, 2)), 2, 40);
    root->infix("/", Value(new FunctionValue(root, builtin_div, 2)), 2, 40);
    root->infix("%", Value(new FunctionValue(root, builtin_rem, 2)), 2, 40);
    root->infix("and", Value(new FunctionValue(root, builtin_and, 2)), 2, 5);
    root->infix("or", Value(new FunctionValue(root, builtin_or, 2)), 2, 5);
    root->infix("xor", Value(new FunctionValue(root, builtin_xor, 2)), 2, 5);
    root->def("not", Value(new FunctionValue(root, builtin_not, 1)), 1);
    root->infix("==", Value(new FunctionValue(root, builtin_equal, 2)), 2, 10);
    root->infix("!=", Value(new FunctionValue(root, builtin_inequal, 2)), 2, 10);
    root->infix("<", Value(new FunctionValue(root, builtin_less, 2)), 2, 10);
    root->infix(">", Value(new FunctionValue(root, builtin_greater, 2)), 2, 10);
    root->infix("<=", Value(new FunctionValue(root, builtin_less_equal, 2)), 2, 10);
    root->infix(">=", Value(new FunctionValue(root, builtin_greater_equal, 2)), 2, 10);
    root->infix("head", Value(new FunctionValue(root, builtin_head, 1)), 1, 80);
    root->infix("tail", Value(new FunctionValue(root, builtin_tail, 1)), 1, 80);
    root->infix("::", Value(new FunctionValue(root, builtin_cons, 2)), 2, 15);
    root->infix("cons", Value(new FunctionValue(root, builtin_cons, 2)), 2, 15);
    root->def("true", Value(true, BOOL));
    root->def("false", Value(false, BOOL));
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

  vector<const Value*> to_ptr_vector(const Value& list) {
    vector<const Value*> values;
    const Value* v = &list;
    while (v->is_list()) {
      values.push(&v->get_list().head());
      v = &v->get_list().tail();
    }
    return values;
  }

  vector<Value*> to_ptr_vector(Value& list) {
    vector<Value*> values;
    Value* v = &list;
    while (v->is_list()) {
      values.push(&v->get_list().head());
      v = &v->get_list().tail();
    }
    return values;
  }

  bool introduces_env(const Value& list) {
    if (!list.is_list()) return false;
    const Value& h = head(list);
    if (!h.is_symbol()) return false;
    const string& name = symbol_for(h.get_symbol());
    if (name == "def" || name == "macro") {
      const Value& second = tail(list);
      if (second.is_list()) {
        const Value& third = tail(second);

        // if list has more than three elements and third parameter
        // is a list (aka is a procedure-type definition)
        if (third.is_list() && tail(third).is_list() &&
          !head(third).is_symbol()) return true;
      }
    }
    else if (name == "lambda") return true;

    return false;
  }

  void traverse_list(ref<Env> env, const Value& list, 
    void(*fn)(ref<Env>, const Value&)) {
    vector<const Value*> vals = to_ptr_vector(list);
    u32 length = introduces_env(list) ? vals.size() - 1 : vals.size();
    for (u32 i = 0; i < length; i ++) fn(env, *vals[i]);
  }

  void traverse_list(ref<Env> env, Value& list, 
    void(*fn)(ref<Env>, Value&)) {
    vector<Value*> vals = to_ptr_vector(list);
    u32 length = introduces_env(list) ? vals.size() - 1 : vals.size();
    for (u32 i = 0; i < length; i ++) fn(env, *vals[i]);
  }

  void handle_splice(ref<Env> env, Value& item) {
    if (!item.is_list()) return;
    Value h = head(item);
    if (h.is_symbol() && h.get_symbol() == symbol_value("splice")) {
      Value t = tail(item);
      if (t.is_void()) item = Value(VOID);
      else item = eval(env, head(tail(item)));
    }
    else traverse_list(env, item, handle_splice);
  }

  void visit_macro_defs(ref<Env> env, const Value& item) {
    if (!item.is_list()) return;
    Value h = head(item);
    if (h.is_symbol() && h.get_symbol() == symbol_value("macro")) {
      vector<Value> values = to_vector(item);
      if (values.size() < 2 || !values[1].is_symbol())
        err(item.loc(), "Expected name in definition.");
      if (values.size() < 3)
        err(item.loc(), "Expected value in definition.");
      if (values.size() > 3) { // procedure
        env->def_macro(symbol_for(values[1].get_symbol()),
          to_vector(values[2]).size());
      }
      else env->def_macro(symbol_for(values[1].get_symbol()));
    }
    else traverse_list(env, item, visit_macro_defs);
  }

  void visit_defs(ref<Env> env, const Value& item) {
    if (!item.is_list()) return;
    Value h = head(item);
    if (h.is_symbol() && h.get_symbol() == symbol_value("def")) {
      vector<Value> values = to_vector(item);
      if (values.size() < 2 || !values[1].is_symbol())
        err(item.loc(), "Expected name in definition.");
      if (values.size() < 3)
        err(item.loc(), "Expected value in definition.");
      if (values.size() > 3) { // procedure
        env->def(symbol_for(values[1].get_symbol()),
          to_vector(values[2]).size());
      }
      else env->def(symbol_for(values[1].get_symbol()));
    }
    else traverse_list(env, item, visit_defs);
  }

  void handle_macro(ref<Env> env, Value& item);

  Value expand(const Value& macro, const Value& arg) {
    if (!macro.is_macro() && !macro.is_error()) {
      err(macro.loc(), "Expanded value is not a macro.");
      return error();
    }
    if (!arg.is_product() && !arg.is_error()) {
      err(arg.loc(), "Arguments not provided as a product.");
      return error();
    }
    if (macro.is_error() || arg.is_error()) return error();

    const MacroValue& fn = macro.get_macro();
    if (fn.is_builtin()) {
      return fn.get_builtin()(fn.get_env(), arg);
    }
    else {
      ref<Env> env = fn.get_env();
      u32 argc = arg.get_product().size(), arity = fn.args().size();
      if (argc != arity) {
        err(macro.loc(), "Procedure requires ", arity, " arguments, ",
          argc, " provided.");
        return error();
      }
      for (u32 i = 0; i < arity; i ++) {
        const string& argname = symbol_for(fn.args()[i]);
        env->find(argname)->value = arg.get_product()[i];
      }
      Value result = fn.body().clone();
      handle_splice(fn.get_env(), result);
      return result;
    }
  }

  void handle_macro(ref<Env> env, Value& item) {
    if (item.is_symbol()) {
      const Def* def = env->find(symbol_for(item.get_symbol()));
      if (def && def->is_macro_variable())
        item = def->value.get_alias().value();
    }
    else if (item.is_list()) {
      const Value& h = head(item);
      if (h.is_symbol()) {
        const Def* def = env->find(symbol_for(h.get_symbol()));
        if (def && def->is_macro_procedure())
          item = eval_list(env, item);
      }
    }
  }

  Value apply_op(const Value& op, const Value& lhs) {
    return list_of(op, lhs);
  }

  Value apply_op(const Value& op, const Value& lhs, const Value& rhs) {
    return list_of(op, lhs, rhs);
  }

  pair<Value, Value> infix_helper(ref<Env> env, const Value& lhs, 
    const Value& op, const Def* def, const Value& term) {
    Value iter = term;

    if (iter.is_void()) {
      err(term.loc(), "Expected term in binary expression.");
      return { error(), term };
    }
    Value rhs = head(iter);
    iter = tail(iter); // consume second term

    if (iter.is_void()) return { apply_op(op, lhs, rhs), iter };
    Value next_op = head(iter);
    if (!next_op.is_symbol()) return { apply_op(op, lhs, rhs), iter };
    const Def* next_def = env->find(symbol_for(next_op.get_symbol()));
    if (!next_def || !next_def->is_infix) return { apply_op(op, lhs, rhs), iter };
    iter = tail(iter); // consume op
    
    if (next_def->precedence > def->precedence) {
      auto p = infix_helper(env, rhs, next_op, next_def, iter);
      return { apply_op(op, lhs, p.first), p.second };
    }
    else return infix_helper(env, apply_op(op, lhs, rhs), next_op, next_def, iter);
  }

  pair<Value, Value> infix_transform(ref<Env> env, const Value& term) {
    Value iter = term;

    if (iter.is_void()) {
      err(term.loc(), "Expected term in binary expression.");
      return { error(), term };
    }
    Value lhs = head(iter); // 1 + 2 -> 1
    iter = tail(iter); // consume first term

    if (iter.is_void()) return { lhs, iter };
    Value op = head(iter);
    if (!op.is_symbol()) return { lhs, iter };
    const Def* def = env->find(symbol_for(op.get_symbol()));
    if (!def || !def->is_infix) return { lhs, iter };
    iter = tail(iter); // consume op

    // todo: unary op

    return infix_helper(env, lhs, op, def, iter);
  }

  Value handle_infix(ref<Env> env, const Value& term) {
    vector<Value> infix_exprs;
    Value iter = term;
    while (iter.is_list()) {
      auto p = infix_transform(env, iter);
      infix_exprs.push(p.first); // next s-expr
      iter = p.second; // move past it in source list
    }
    Value result = list_of(infix_exprs);
    return result;
  }

  // definition stuff

  Value define(ref<Env> env, const Value& term, bool is_macro) {
    vector<Value> values = to_vector(term);
    if (values.size() == 3) { // variable
      if (is_macro) 
        env->def_macro(symbol_for(values[1].get_symbol()), 
          Value(new AliasValue(values[2])));
      else 
        env->def(symbol_for(values[1].get_symbol()), 
          eval(env, values[2]));
      return Value(VOID);
    }
    else {
      if (!values[2].is_list()) {
        err(values[2].loc(), "Expected argument list in function.");
        return error();
      }
      Env env_descendant(env);
      ref<Env> function_env(env_descendant);
      vector<Value> args = to_vector(values[2]);
      vector<u64> argnames;
      for (const Value& v : args) {
        if (v.is_symbol()) {
          argnames.push(v.get_symbol());
          function_env->def(symbol_for(v.get_symbol()));
        }
      }
      vector<Value> body;
      for (u32 i = 3; i < values.size(); i ++) body.push(values[i]);
      if (is_macro) {
        Value mac(new MacroValue(function_env, 
          argnames, cons(Value("do"), list_of(body))));
        env->def_macro(symbol_for(values[1].get_symbol()), 
          mac, argnames.size());
      }
      else {
        Value func(new FunctionValue(function_env, 
          argnames, cons(Value("do"), handle_infix(env, list_of(body)))));
        env->def(symbol_for(values[1].get_symbol()), 
          func, argnames.size());
      }
      return Value(VOID);
    }
  }

  Value infix(ref<Env> env, const Value& term) {
    vector<Value> values = to_vector(term);

    u8 precedence = 0;

    u32 i = 1;
    if (values[i].is_int()) { // precedence
      precedence = (u8)values[i].get_int();
      i ++;
    }
    if (!values[i].is_symbol()) {
      err(values[i].loc(), "Expected name for infix definition.");
      return error();
    }
    const string& name = symbol_for(values[i].get_symbol());
    i ++;

    if (i + 1 >= values.size()) {
      err(values[0].loc(), "Expected argument list and body in ",
        "infix definition.");
      return error();
    }
    if (!values[i].is_list()) {
      err(values[i].loc(), "Expected argument list in infix ",
        "definition.");
      return error();
    }
    Env env_descendant(env);
    ref<Env> function_env(env_descendant);
    vector<Value> args = to_vector(values[i]);
    i ++;
    vector<u64> argnames;
    for (const Value& v : args) {
      if (v.is_symbol()) {
        argnames.push(v.get_symbol());
        function_env->def(symbol_for(v.get_symbol()));
      }
    }
    vector<Value> body;
    for (; i < values.size(); i ++) body.push(values[i]);
    Value func(new FunctionValue(function_env, 
      argnames, cons(Value("do"), handle_infix(env, list_of(body)))));
    env->infix(name, func, argnames.size(), precedence);
    return Value(VOID);
  }

  Value lambda(ref<Env> env, const Value& term) {
    vector<Value> values = to_vector(term);
    if (values.size() < 2) {
      err(values[0].loc(), "Expected argument list and body in ",
        "lambda expression.");
      return error();
    }
    if (!values[1].is_list()) {
      err(values[1].loc(), "Expected argument list in lambda ",
        "expression.");
      return error();
    }
    Env env_descendant(env);
    ref<Env> function_env(env_descendant);
    vector<Value> args = to_vector(values[1]);
    vector<u64> argnames;
    for (const Value& v : args) {
      if (v.is_symbol()) {
        argnames.push(v.get_symbol());
        function_env->def(symbol_for(v.get_symbol()));
      }
    }
    vector<Value> body;
    for (u32 i = 2; i < values.size(); i ++) body.push(values[i]);
    return Value(new FunctionValue(function_env, argnames, 
      cons(Value("do"), handle_infix(env, list_of(body)))));
  }

  Value do_block(ref<Env> env, const Value& term) {
    const Value& todo = tail(term);

    const Value* v = &todo;
    while (v->is_list()) {
      if (v->get_list().tail().is_void()) // last element
        return eval(env, v->get_list().head());
      eval(env, v->get_list().head());
      v = &v->get_list().tail();
    }
    return Value(VOID);
  }

  Value if_expr(ref<Env> env, const Value& term) {
    Value handled = cons(head(term), handle_infix(env, tail(term)));
    vector<Value> values = to_vector(handled);
    if (values.size() != 4) {
      err(term.loc(), "Incorrect number of arguments for if expression. ",
        "Expected condition, case if true, and case if false.");
      return error();
    }
    Value cond = eval(env, values[1]);
    if (!cond.is_bool()) {
      err(cond.loc(), "Expected boolean condition in if expression, given '",
        cond.type(), "'.");
      return error();
    }
    if (cond.get_bool()) return eval(env, values[2]);
    else return eval(env, values[3]);
  }

  Value eval_list(ref<Env> env, const Value& term) {
    Value h = head(term);
    if (h.is_symbol()) {
      const string& name = symbol_for(h.get_symbol());
      if (name == "quote") return head(tail(term));
      else if (name == "def") return define(env, term, false);
      else if (name == "infix") return infix(env, term);
      else if (name == "macro") return define(env, term, true);
      else if (name == "lambda") return lambda(env, term);
      else if (name == "do") return do_block(env, term);
      else if (name == "if") return if_expr(env, term);
    }

    Value first = eval(env, h);
    if (first.is_macro()) {
      vector<Value> args;
      const Value* v = &term.get_list().tail();
      while (v->is_list()) {
        args.push(v->get_list().head());
        v = &v->get_list().tail();
      }
      if (args.size() != first.get_macro().arity()) {
        err(term.loc(), "Macro procedure expects ", 
          first.get_macro().arity(), " arguments, ", 
          args.size(), " provided.");
        return error();
      }
      return expand(first, Value(new ProductValue(args)));
    }
    else if (first.is_function()) {
      vector<Value> args;
      Value args_term = handle_infix(env, tail(term));
      const Value* v = &args_term;
      while (v->is_list()) {
        args.push(eval(env, v->get_list().head()));
        v = &v->get_list().tail();
      }
      if (args.size() != first.get_function().arity()) {
        err(term.loc(), "Procedure expects ", 
          first.get_function().arity(), " arguments, ", 
          args.size(), " provided.");
        return error();
      }
      return call(first, Value(new ProductValue(args)));
    }

    if (tail(term).is_void()) return eval(env, h);

    Value infix_term = handle_infix(env, term);
    if (infix_term == term) {
      err(term.loc(), "Could not evaluate list.");
      return error();
    }
    else return eval_list(env, infix_term);
  }

  Value eval(ref<Env> env, Value term) {
    handle_splice(env, term);
    visit_macro_defs(env, term);
    handle_macro(env, term);
    visit_defs(env, term);
    if (term.is_list()) return eval_list(env, term);
    else if (term.is_int()) return term;
    else if (term.is_symbol()) {
      const string& name = symbol_for(term.get_symbol());
      const Def* def = env->find(name);
      if (def && def->is_macro_variable())
        return def->value.get_alias().value();
      else if (def) return def->value; 
      else {
        err(term.loc(), "Undefined variable '", name, "'.");
        return error();
      }
    }
    return error();
  }
}