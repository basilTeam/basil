#include "eval.h"
#include "source.h"
#include "ast.h"
#include "driver.h"

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

	Value builtin_is_empty(ref<Env> env, const Value& args) {
		return is_empty(args.get_product()[0]);
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

	Value builtin_display(ref<Env> env, const Value& args) {
		return display(args.get_product()[0]);
	}

	Value gen_assign(ref<Env> env, const Value& dest, const Value& src) {
		return list_of(Value("#="), list_of(Value("quote"), dest), src);
	}

	Value builtin_assign_macro(ref<Env> env, const Value& args) {
		return gen_assign(env, args.get_product()[0], 
			args.get_product()[1]);
	}

	Value builtin_assign(ref<Env> env, const Value& args) {
		return assign(env, args.get_product()[0], args.get_product()[1]);
	}

  Value builtin_read_line(ref <Env> env, const Value& args) {
    return read_line();
  }

	Value builtin_if_macro(ref<Env> env, const Value& args) {
		return list_of(Value("#?"), args.get_product()[0],
			list_of(Value("quote"), args.get_product()[1]),
			list_of(Value("quote"), args.get_product()[2]));
	}

	Value builtin_if(ref<Env> env, const Value& args) {
    Value cond = args.get_product()[0];

		if (cond.is_runtime()) {
			Value left = eval(env, args.get_product()[1]), 
				right = eval(env, args.get_product()[2]);
			if (left.is_error() || right.is_error()) return error();
			if (!left.is_runtime()) left = lower(left);
			if (!right.is_runtime()) right = lower(right);
			ASTNode* ln = left.get_runtime();
			ASTNode* rn = right.get_runtime();
			return new ASTIf(cond.loc(), cond.get_runtime(), ln, rn);
		}
		
    if (!cond.is_bool()) {
      err(cond.loc(), "Expected boolean condition in if ",
				"expression, given '", cond.type(), "'.");
      return error();
    }

    if (cond.get_bool()) return eval(env, args.get_product()[1]);
    else return eval(env, args.get_product()[2]);
	}

  ref<Env> create_root_env() {
    ref<Env> root;
    root->def("nil", Value(VOID));
    root->infix("+", new FunctionValue(root, builtin_add, 2), 2, 20);
    root->infix("-", new FunctionValue(root, builtin_sub, 2), 2, 20);
    root->infix("*", new FunctionValue(root, builtin_mul, 2), 2, 40);
    root->infix("/", new FunctionValue(root, builtin_div, 2), 2, 40);
    root->infix("%", new FunctionValue(root, builtin_rem, 2), 2, 40);
    root->infix("and", new FunctionValue(root, builtin_and, 2), 2, 5);
    root->infix("or", new FunctionValue(root, builtin_or, 2), 2, 5);
    root->infix("xor", new FunctionValue(root, builtin_xor, 2), 2, 5);
    root->def("not", new FunctionValue(root, builtin_not, 1), 1);
    root->infix("==", new FunctionValue(root, builtin_equal, 2), 2, 10);
    root->infix("!=", new FunctionValue(root, builtin_inequal, 2), 2, 10);
    root->infix("<", new FunctionValue(root, builtin_less, 2), 2, 10);
    root->infix(">", new FunctionValue(root, builtin_greater, 2), 2, 10);
    root->infix("<=", new FunctionValue(root, builtin_less_equal, 2), 2, 10);
    root->infix(">=", new FunctionValue(root, builtin_greater_equal, 2), 2, 10);
    root->infix("empty?", new FunctionValue(root, builtin_is_empty, 1), 1, 60);
    root->infix("head", new FunctionValue(root, builtin_head, 1), 1, 80);
    root->infix("tail", new FunctionValue(root, builtin_tail, 1), 1, 80);
    root->infix("::", new FunctionValue(root, builtin_cons, 2), 2, 15);
    root->infix("cons", new FunctionValue(root, builtin_cons, 2), 2, 15);
    root->def("display", new FunctionValue(root, builtin_display, 1), 1);
		root->infix_macro("=", new MacroValue(root, builtin_assign_macro, 2), 2, 0);
		root->infix("#=", new FunctionValue(root, builtin_assign, 2), 2, 0);
		root->infix_macro("?", new MacroValue(root, builtin_if_macro, 3), 3, 2);
		root->infix("#?", new FunctionValue(root, builtin_if, 3), 3, 2);
    root->def("read-line", new FunctionValue(root, builtin_read_line, 0), 0);
    root->def("true", Value(true, BOOL));
    root->def("false", Value(false, BOOL));
    return root;
  }

  // stubs

  Value eval_list(ref<Env> env, const Value& list);
  Value eval(ref<Env> env, Value term);
	Value infix(ref<Env> env, const Value& term, bool is_macro);
  Value define(ref<Env> env, const Value& term, bool is_macro);

  // utilities

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
    if (name == "def") {
      return tail(list).is_list() && 
				head(tail(list)).is_list(); // is procedure
    }
    else if (name == "lambda" || name == "infix" 
			|| name == "infix-macro" || name == "macro") return true;

    return false;
  }

	static i64 traverse_deep = 0;

	void enable_deep() {
		traverse_deep ++;
	}

	void disable_deep() {
		traverse_deep --;
		if (traverse_deep < 0) traverse_deep = 0;
	}

  void traverse_list(ref<Env> env, const Value& list, 
    void(*fn)(ref<Env>, const Value&)) {
    vector<const Value*> vals = to_ptr_vector(list);
		if (!introduces_env(list) || traverse_deep)
    	for (u32 i = 0; i < vals.size(); i ++) 
				fn(env, *vals[i]);
  }

  void traverse_list(ref<Env> env, Value& list, 
    void(*fn)(ref<Env>, Value&)) {
    vector<Value*> vals = to_ptr_vector(list);
		if (!introduces_env(list) || traverse_deep)
    	for (u32 i = 0; i < vals.size(); i ++) 
				fn(env, *vals[i]);
  }

  void handle_splice(ref<Env> env, Value& item) {
    if (!item.is_list()) return;
    Value h = head(item);
    if (h.is_symbol() && h.get_symbol() == symbol_value("splice")) {
      Value t = tail(item);
      if (t.is_void()) item = Value(VOID);
      else {
				Value t = tail(item);
				prep(env, t);
				item = eval(env, t);
			}
    }
    else traverse_list(env, item, handle_splice);
  }

	Value use(ref<Env> env, const Value& term);

	void handle_use(ref<Env> env, Value& item) {
		if (!item.is_list()) return;
		Value h = head(item);
		if (h.is_symbol() && h.get_symbol() == symbol_value("use")) {
			use(env, item);
			item = list_of(string("list-of"));
		}
		else traverse_list(env, item, handle_use);
	}

  void visit_macro_defs(ref<Env> env, const Value& item) {
    if (!item.is_list()) return;
    Value h = head(item);
    if (h.is_symbol() && 
			(h.get_symbol() == symbol_value("macro")
				|| h.get_symbol() == symbol_value("infix-macro"))) {
			bool infix = h.get_symbol() == symbol_value("infix-macro");
      u8 precedence = 0;
			vector<Value> values = to_vector(item);
			u32 i = 1;
			if (values.size() >= 2 && infix && values[i].is_int()) {
				precedence = u8(values[i].get_int());
				i ++;
			}
			if (i + 1 >= values.size() || 
				(!values[i].is_symbol() && 
					!(values[i].is_list() && head(values[i]).is_symbol()) &&
					!(values[i].is_list() && tail(values[i]).is_list() &&
						head(tail(values[i])).is_symbol()))) {
        err(item.loc(), "Expected variable or function name ",
					"in definition.");
				return;
			}
      if (values.size() < 3)
        err(item.loc(), "Expected value in definition.");
      if (values[i].is_list()) { // procedure
        if (infix) {
					Value rest = tail(values[i]);
					if (!rest.is_list()) {
						err(rest.loc(), "Infix function must take at least one ",
							"argument.");
						return;
					}
					basil::infix(env, item, true);
				}
				else define(env, item, true);
      }
      else define(env, item, true);
    }
    else traverse_list(env, item, visit_macro_defs);
  }

  void visit_defs(ref<Env> env, const Value& item) {
    if (!item.is_list()) return;
    Value h = head(item);
    if (h.is_symbol() && 
			(h.get_symbol() == symbol_value("def")
				|| h.get_symbol() == symbol_value("infix"))) {
      bool infix = h.get_symbol() == symbol_value("infix");
      u8 precedence = 0;
			vector<Value> values = to_vector(item);
			u32 i = 1;
			if (values.size() >= 2 && infix && values[i].is_int()) {
				precedence = u8(values[i].get_int());
				i ++;
			}
			if (i + 1 >= values.size() || 
				(!values[i].is_symbol() && 
					!(values[i].is_list() && head(values[i]).is_symbol()) &&
					!(values[i].is_list() && tail(values[i]).is_list() &&
						head(tail(values[i])).is_symbol()))) {
        err(item.loc(), "Expected variable or function name ",
					"in definition.");
				return;
			}
      if (values.size() < 3)
        err(item.loc(), "Expected value in definition.");
      if (values[i].is_list()) { // procedure
        if (infix) {
					Value rest = tail(values[i]);
					if (!rest.is_list()) {
						err(rest.loc(), "Infix function must take at least one ",
							"argument.");
						return;
					}
					const string& name = symbol_for(head(rest).get_symbol());
					// if (env->find(name)) {
					// 	err(values[i].loc(), "Redefinition of '", name, "'.");
					// 	return;
					// }
					env->infix(name, to_vector(tail(rest)).size() + 1, 
						precedence);
				}
				else {
					const string& name = symbol_for(head(values[i]).get_symbol());
					// if (env->find(name)) {
					// 	err(values[i].loc(), "Redefinition of '", name, "'.");
					// 	return;
					// }
					env->def(name, to_vector(tail(values[i])).size());
				}
      }
      else {
				const string& name = symbol_for(values[i].get_symbol());
				// if (env->find(name)) {
				// 	err(values[i].loc(), "Redefinition of '", name, "'.");
				// 	return;
				// }
				env->def(name);
			}
    }
    else traverse_list(env, item, visit_defs);
  }

  void handle_macro(ref<Env> env, Value& item);

  Value expand(ref<Env> env, const Value& macro, const Value& arg) {
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
      return fn.get_builtin()(env, arg);
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
				if (fn.args()[i] & KEYWORD_ARG_BIT) {
					// keyword arg
					Value argument = eval(env, arg.get_product()[i]);
					if (!argument.is_symbol() ||
							argument.get_symbol() != 
								(fn.args()[i] & ARG_NAME_MASK)) {
						err(arg.get_product()[i].loc(), "Expected keyword '",
							symbol_for(fn.args()[i] & ARG_NAME_MASK), "', got '",
							argument, "'.");
						return error();
					}
				}
        else {
					const string& argname = symbol_for(fn.args()[i] & ARG_NAME_MASK);
        	env->find(argname)->value = arg.get_product()[i];
				}
      }
      Value result = fn.body().clone();
			enable_deep();
			handle_splice(env, result);
			disable_deep();
			prep(env, result);
			return result;
    }
  }

  void handle_macro(ref<Env> env, Value& item) {
    if (item.is_symbol()) {
      const Def* def = env->find(symbol_for(item.get_symbol()));
      if (def && def->is_macro_variable()) {
        item = def->value.get_alias().value();
				return;
			}
    }
    else if (item.is_list()) {
      const Value& h = head(item);
      if (h.is_symbol()) {
        const Def* def = env->find(symbol_for(h.get_symbol()));
        if (def && def->is_macro_procedure()) {
          item = eval_list(env, item);
					return;
				}
      }
    }
    traverse_list(env, item, handle_macro);
  }

  Value apply_op(const Value& op, const Value& lhs) {
    return list_of(op, lhs);
  }

  Value apply_op(const Value& op, const Value& lhs, const Value& rhs) {
    return list_of(op, lhs, rhs);
  }

  Value apply_op(const Value& op, const Value& lhs, const vector<Value>& internals,
    const Value& rhs) {
    Value l = list_of(rhs);
    for (i64 i = i64(internals.size()) - 1; i >= 0; i --) {
      l = cons(internals[i], l);
    }
    return cons(op, cons(lhs, l));
  }

  pair<Value, Value> unary_helper(ref<Env> env, const Value& lhs,
    const Value& term);
  pair<Value, Value> infix_helper(ref<Env> env, const Value& lhs, 
    const Value& op, const Def* def, const Value& rhs, const Value& term,
    const vector<Value>& internals);
  pair<Value, Value> infix_helper(ref<Env> env, const Value& lhs, 
    const Value& op, const Def* def, const Value& term);
  pair<Value, Value> infix_transform(ref<Env> env, const Value& term);

  pair<Value, Value> infix_helper(ref<Env> env, const Value& lhs, 
    const Value& op, const Def* def, const Value& rhs, const Value& term,
    const vector<Value>& internals) {
    Value iter = term;
      
    if (iter.is_void()) 
      return { apply_op(op, lhs, internals, rhs), iter };
    Value next_op = head(iter);
    if (!next_op.is_symbol()) 
      return { apply_op(op, lhs, internals, rhs), iter };
    const Def* next_def = env->find(symbol_for(next_op.get_symbol()));
    if (!next_def || !next_def->is_infix) 
      return { apply_op(op, lhs, internals, rhs), iter };
    iter = tail(iter); // consume op
    
    if (next_def->precedence > def->precedence) {
      if (next_def->arity == 1) {
        return infix_helper(env, lhs, op, def, 
          apply_op(next_op, rhs), iter, internals);
      }
      auto p = infix_helper(env, rhs, next_op, next_def, iter);
      return { apply_op(op, lhs, internals, p.first), p.second };
    }
    else {
      Value result = apply_op(op, lhs, rhs);
      if (next_def->arity == 1) {
        return unary_helper(env, apply_op(next_op, result), iter);
      }
      return infix_helper(env, apply_op(op, lhs, internals, rhs), 
        next_op, next_def, iter);
    }
  }

  pair<Value, Value> infix_helper(ref<Env> env, const Value& lhs, 
    const Value& op, const Def* def, const Value& term) {
    Value iter = term;

    vector<Value> internals;
    if (def->arity > 2) for (u32 i = 0; i < def->arity - 2; i ++) {
      auto p = infix_transform(env, iter);
      internals.push(p.first);
      iter = p.second;
    }

    if (iter.is_void()) {
      err(term.loc(), "Expected term in binary expression.");
      return { error(), term };
    }
    Value rhs = head(iter);
    iter = tail(iter); // consume second term

    return infix_helper(env, lhs, op, def, rhs, iter, internals);
  }

  pair<Value, Value> unary_helper(ref<Env> env, const Value& lhs,
    const Value& term) {
    Value iter = term;

    if (iter.is_void()) return { lhs, iter };
    Value op = head(iter);
    if (!op.is_symbol()) return { lhs, iter };
    const Def* def = env->find(symbol_for(op.get_symbol()));
    if (!def || !def->is_infix) return { lhs, iter };
    iter = tail(iter); // consume op

    if (def->arity == 1) 
      return unary_helper(env, apply_op(op, lhs), iter);
    return infix_helper(env, lhs, op, def, iter);
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

    if (def->arity == 1) 
      return unary_helper(env, apply_op(op, lhs), iter);
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

	void apply_infix(ref<Env> env, Value& item);

	void apply_infix_at(ref<Env> env, Value& item, u32 depth) {
		Value* iter = &item;
		while (depth && iter->is_list()) {
			iter = &iter->get_list().tail();
			depth --;
		}
		*iter = handle_infix(env, *iter);
    traverse_list(env, *iter, apply_infix);
	}

	void apply_infix(ref<Env> env, Value& item) {
		Value orig = item.clone();
    if (!item.is_list()) return;
    Value h = head(item);
    if (h.is_symbol()) {
			const string& name = symbol_for(h.get_symbol());
			if (name == "macro" || name == "infix"
					|| name == "infix-macro" || name == "lambda"
					|| name == "quote" || name == "splice") return;
			else if (name == "def") { 
				if (tail(item).is_list() && head(tail(item)).is_symbol())
					apply_infix_at(env, item, 2);
			}
			else if (name == "if" || name == "do" || name == "list-of")
				apply_infix_at(env, item, 1);
			else {
				silence_errors();
				Value proc = eval(env, h);
				unsilence_errors();
				if (proc.is_function()) apply_infix_at(env, item, 1);
				else apply_infix_at(env, item, 0);
			}
		}
		else apply_infix_at(env, item, 0); 
  }

	void prep(ref<Env> env, Value& term) {
		handle_splice(env, term);
		handle_use(env, term);
		visit_macro_defs(env, term);
		handle_macro(env, term);
		visit_defs(env, term);
		apply_infix(env, term);
		handle_macro(env, term);
		visit_defs(env, term);
	}

  // definition stuff
  Value define(ref<Env> env, const Value& term, bool is_macro) {
    vector<Value> values = to_vector(term);
		
		// visit_defs already does some error-checking, so we
		// don't need to check the number of values or their types
		// exhaustively.

    if (values[1].is_symbol()) { // variable
			const string& name = symbol_for(values[1].get_symbol());
		
      if (is_macro) {
        env->def_macro(name, Value(new AliasValue(values[2])));
      	return Value(VOID);
			}
      else {
				Value init = eval(env, values[2]);
				if (env->is_runtime())
					init = lower(init);
        env->def(name, init);
				if (init.is_runtime()) 
					return new ASTDefine(values[0].loc(), env, 
						values[1].get_symbol(), init.get_runtime());
				return Value(VOID);
			}
    }
    else if (values[1].is_list()) { // procedure
			const string& name = symbol_for(head(values[1]).get_symbol());
			
			Env env_descendant(env);
      ref<Env> function_env(env_descendant);
      vector<Value> args = to_vector(tail(values[1]));
      vector<u64> argnames;
      for (const Value& v : args) {
        if (v.is_symbol()) {
          argnames.push(v.get_symbol());
          function_env->def(symbol_for(v.get_symbol()));
        }
				else if (v.is_list() && head(v).is_symbol()
					&& symbol_for(head(v).get_symbol()) == "quote") {
					argnames.push(eval(env, v).get_symbol() | KEYWORD_ARG_BIT);
				}
				else {
					err(v.loc(), "Only symbols and quoted symbols are permitted ",
						"within an argument list.");
					return error();
				}
      }
      vector<Value> body;
      for (u32 i = 2; i < values.size(); i ++) body.push(values[i]);
			Value body_term = cons(Value("do"), list_of(body));
      if (is_macro) {
        Value mac(new MacroValue(function_env, argnames, body_term));
        env->def_macro(name, mac, argnames.size());
      }
      else {
				prep(function_env, body_term);
        Value func(new FunctionValue(function_env, argnames, body_term, 
					symbol_value(name)));
        env->def(name, func, argnames.size());
      }
      return Value(VOID);
    }
    else {
			err(values[1].loc(), "First parameter to definition must be ",
			  "a symbol (for variable or alias) or list (for procedure ",
				"or macro).");
			return error();
		}
  }

  Value infix(ref<Env> env, const Value& term, bool is_macro) {
    vector<Value> values = to_vector(term);

    u8 precedence = 0;

    u32 i = 1;
    if (values[i].is_int()) { // precedence
      precedence = (u8)values[i].get_int();
      i ++;
    }
    if (i + 1 >= values.size()) {
      err(values[0].loc(), "Expected argument list and body in ",
        "infix definition.");
      return error();
    }
    if (!values[i].is_list()) {
      err(values[i].loc(), "Expected name and argument list for ",
				"infix definition.");
      return error();
    }
		if (!head(tail(values[i])).is_symbol()) {
			err(tail(values[i]).loc(), "Expected name for infix definition.");
			return error();
		}
		const string& name = symbol_for(head(tail(values[i])).get_symbol());

		vector<Value> args; 
		args.push(head(values[i]));
		Value v = tail(tail(values[i]));
		while (v.is_list()) {
			args.push(head(v));
			v = tail(v);
		}
    if (args.size() == 0) {
      err(values[i].loc(), "Expected argument list in infix ",
        "definition.");
      return error();
    }
    i ++;
    Env env_descendant(env);
    ref<Env> function_env(env_descendant);
    vector<u64> argnames;
    for (const Value& v : args) {
      if (v.is_symbol()) {
        argnames.push(v.get_symbol());
        function_env->def(symbol_for(v.get_symbol()));
      }
			else if (v.is_list() && head(v).is_symbol()
				&& symbol_for(head(v).get_symbol()) == "quote"
				&& head(tail(v)).is_symbol()) {
				argnames.push(eval(env, v).get_symbol() | KEYWORD_ARG_BIT);
			}
			else {
				err(v.loc(), "Only symbols and quoted symbols are permitted ",
					"within an argument list.");
				return error();
			}
    }
    vector<Value> body;
    for (; i < values.size(); i ++) body.push(values[i]);
		Value body_term = cons(Value("do"), list_of(body));
    if (is_macro) {
      Value mac(new MacroValue(function_env, argnames, body_term));
      env->infix_macro(name, mac, argnames.size(), precedence);
      return Value(VOID);
    }
    else {
			prep(function_env, body_term);
      Value func(new FunctionValue(function_env, argnames, body_term,
				symbol_value(name)));
      env->infix(name, func, argnames.size(), precedence);
      return Value(VOID);
    }
  }

  Value lambda(ref<Env> env, const Value& term) {
    vector<Value> values = to_vector(term);
    if (values.size() < 3) {
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
			else {
				err(v.loc(), "Only symbols are permitted ",
					"within a lambda's argument list.");
				return error();
			}
    }
    vector<Value> body;
    for (u32 i = 2; i < values.size(); i ++) body.push(values[i]);
		Value body_term = list_of(body);
		prep(function_env, body_term);
    return Value(new FunctionValue(function_env, argnames, 
      cons(Value("do"), body_term)));
  }

  Value do_block(ref<Env> env, const Value& term) {
    const Value& todo = tail(term);

    const Value* v = &todo;
		vector<Value> values;
		bool runtime = false;
    while (v->is_list()) {
      values.push(eval(env, v->get_list().head()));
			if (values.back().is_runtime()) runtime = true;
      v = &v->get_list().tail();
    }
		if (values.size() == 0) return Value(VOID);
		if (runtime) {
			vector<ASTNode*> nodes;
			for (Value& v : values) if (!v.is_runtime()) {
				v = lower(v);
				if (v.is_error()) return v;
			}
			for (Value& v : values) nodes.push(v.get_runtime());
			return new ASTBlock(term.loc(), nodes);
		}
		return values.back();
  }

	u64 get_keyword(const Value& v) {
		if (v.is_symbol()) return v.get_symbol();
		else if (v.is_list() && head(v).is_symbol() 
			&& head(v).get_symbol() == symbol_value("quote")
			&& tail(v).is_list() && head(tail(v)).is_symbol())
			return head(tail(v)).get_symbol();
		return 0;
	}

	bool is_keyword(const Value& v, const string& word) {
		if (v.is_symbol()) return v.get_symbol() == symbol_value(word);
		else if (v.is_list() && head(v).is_symbol() 
			&& head(v).get_symbol() == symbol_value("quote")
			&& tail(v).is_list() && head(tail(v)).is_symbol())
			return head(tail(v)).get_symbol() == symbol_value(word);
		return false;
	}

  Value if_expr(ref<Env> env, const Value& term) {
    Value params = tail(term);
		prep(env, params);
		Value cond, if_true, if_false;
		bool has_else = false;
		vector<Value> if_true_vals, if_false_vals;
		if (!params.is_list()) {
			err(term.loc(), "Expected condition in if expression.");
			return error();
		}
		cond = head(params);
		params = tail(params);
		while (params.is_list() && !(is_keyword(head(params), "else")
				|| is_keyword(head(params), "elif"))) {
			if_true_vals.push(head(params));
			params = tail(params);
		}
		if_true = cons(Value("do"), list_of(if_true_vals));
		if (!params.is_list()) {
			err(term.loc(), "If expression requires at least one else.");
			return error();
		}
		if (get_keyword(head(params)) == symbol_value("elif")) {
			if_false = if_expr(env, params);
			return list_of(Value("#?"),
				cond,
				list_of(Value("quote"), if_true),
				list_of(Value("quote"), if_false));
		}
		params = tail(params);
		while (params.is_list()) {
			if_false_vals.push(head(params));
			params = tail(params);
		}
		if_false = cons(Value("do"), list_of(if_false_vals));
		return list_of(Value("#?"),
			cond, 
			list_of(Value("quote"), if_true),
			list_of(Value("quote"), if_false));
  }

	bool is_quoted_symbol(const Value& val) {
		return val.is_list() && tail(val).is_list()
			&& head(val).is_symbol() && head(val).get_symbol() == symbol_value("quote")
			&& head(tail(val)).is_symbol();
	}

	void find_assigns(ref<Env> env, const Value& term,
		set<u64>& dests) {
		if (!term.is_list()) return;
		Value h = head(term);
		if (h.is_symbol() && h.get_symbol() == symbol_value("#=")) {
			if (tail(term).is_list() && is_quoted_symbol(head(tail(term)))) {
				u64 sym = eval(env, head(tail(term))).get_symbol();
				Def* def = env->find(symbol_for(sym));
				if (def) dests.insert(sym);
			}
		}
		if (!introduces_env(term)) {
			const Value* v = &term;
			while (v->is_list()) {
				find_assigns(env, v->get_list().head(), dests);
				v = &v->get_list().tail();
			}
		}
	}

  Value while_stmt(ref<Env> env, const Value& term) {
    vector<Value> values = to_vector(term);
    if (values.size() < 3) {
      err(term.loc(), "Incorrect number of arguments for while ",
				"statement. Expected condition and body.");
      return error();
    }

		Value body = cons(Value("do"), tail(tail(term)));

		set<u64> dests;
		find_assigns(env, head(tail(term)), dests);
		find_assigns(env, body, dests);

		vector<ASTNode*> nodes;
		for (u64 u : dests) {
			Def* def = env->find(symbol_for(u));
			if (!def->value.is_runtime()) {
				Value new_value = lower(def->value);
				nodes.push(
					new ASTDefine(new_value.loc(), env, u, new_value.get_runtime()));
				def->value = new_value;
			}
		}

    Value cond = eval(env, head(tail(term)));
		if (cond.is_error()) return error();
		if (!cond.is_runtime()) cond = lower(cond);

		body = eval(env, body);
		if (body.is_error()) return error();
		if (!body.is_runtime()) body = lower(body);
		nodes.push(new ASTWhile(term.loc(), cond.get_runtime(),
			body.get_runtime()));
		return nodes.size() == 1 ? nodes[0] : new ASTBlock(term.loc(), nodes);
  }

  Value list_of(ref<Env> env, const Value& term) {
    Value items = tail(term);

    const Value* v = &items;
    vector<Value> vals;
    while (v->is_list()) {
      vals.push(eval(env, v->get_list().head()));
      v = &v->get_list().tail();
    }
    return list_of(vals);
  }

	Value use(ref<Env> env, const Value& term) {
		if (!tail(term).is_list() || !head(tail(term)).is_symbol()) {
			err(tail(term).loc(), "Expected symbol in use expression, given '",
				tail(term), "'.");
			return error();
		}
		Value h = head(tail(term));
		string path = symbol_for(h.get_symbol());
		path += ".bl";

		Source src((const char*)path.raw());
		if (!src.begin().peek()) {
			err(h.loc(), "Could not load source file at path '", path, "'.");
			return error();
		}
		ref<Env> module = load(src);
		if (error_count()) return error();

		for (const auto& p : *module) {
			if (env->find(p.first)) {
				err(term.loc(), "Module '", symbol_for(h.get_symbol()), 
					"' redefines '", p.first, "' in the current environment.");
				return error();
			}
		}
		env->import(module);
		return Value(VOID);
	}

  Value eval_list(ref<Env> env, const Value& term) {
    Value h = head(term);
    if (h.is_symbol()) {
      const string& name = symbol_for(h.get_symbol());
      if (name == "quote") return head(tail(term)).clone();
      else if (name == "def") return define(env, term, false);
      else if (name == "infix") return infix(env, term, false);
      else if (name == "macro") return define(env, term, true);
      else if (name == "infix-macro") return infix(env, term, true);
      else if (name == "lambda") return lambda(env, term);
      else if (name == "do") return do_block(env, term);
      else if (name == "if") return eval(env, if_expr(env, term));
      else if (name == "list-of") return list_of(env, term);
			else if (name == "use") return use(env, term);
			else if (name == "while") return while_stmt(env, term);
    }

    Value first = eval(env, h);
    if (first.is_macro()) {
      vector<Value> args;
      const Value* v = &term.get_list().tail();
			u32 i = 0;
      while (v->is_list()) {
				if (i < first.get_macro().arity()
						&& first.get_macro().args()[i] & KEYWORD_ARG_BIT
						&& v->get_list().head().is_symbol())
					args.push(list_of(Value("quote"), v->get_list().head())); 
        else args.push(v->get_list().head());
        v = &v->get_list().tail();
				i ++;
      }
      if (args.size() != first.get_macro().arity()) {
        err(term.loc(), "Macro procedure expects ", 
          first.get_macro().arity(), " arguments, ", 
          args.size(), " provided.");
        return error();
      }
      return expand(env, first, Value(new ProductValue(args)));
    }
    else if (first.is_function()) {
      vector<Value> args;
      Value args_term = tail(term);
      const Value* v = &args_term;
			u32 i = 0;
      while (v->is_list()) {
				if (i < first.get_function().arity()
						&& first.get_function().args()[i] & KEYWORD_ARG_BIT
						&& v->get_list().head().is_symbol())
					args.push(v->get_list().head()); // leave keywords quoted
        else args.push(eval(env, v->get_list().head()));
        v = &v->get_list().tail();
				i ++;
      }
      if (args.size() != first.get_function().arity()) {
        err(term.loc(), "Procedure expects ", 
          first.get_function().arity(), " arguments, ", 
          args.size(), " provided.");
        return error();
      }
      return call(env, first, Value(new ProductValue(args)));
    }

    if (tail(term).is_void()) return first;

		err(term.loc(), "Could not evaluate list.");
		return error();
  }

  Value eval(ref<Env> env, Value term) {
    if (term.is_list()) return eval_list(env, term);
    else if (term.is_int()) return term;
		else if (term.is_string()) return term;
    else if (term.is_symbol()) {
      const string& name = symbol_for(term.get_symbol());
      const Def* def = env->find(name);
      if (def && def->is_macro_variable())
        return def->value.get_alias().value();
      else if (def) {
				if (def->value.is_runtime())
					return new ASTVar(term.loc(), env, term.get_symbol());
				return def->value;
			}
      else {
        err(term.loc(), "Undefined variable '", name, "'.");
        return error();
      }
    }
    err(term.loc(), "Could not evaluate term '", term, "'.");
    return error();
  }
}