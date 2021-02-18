#include "eval.h"
#include "ast.h"
#include "builtin.h"
#include "driver.h"
#include "source.h"

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
        return gen_assign(env, args.get_product()[0], args.get_product()[1]);
    }

    Value builtin_assign(ref<Env> env, const Value& args) {
        return assign(env, args.get_product()[0], args.get_product()[1]);
    }

    Value builtin_read_line(ref<Env> env, const Value& args) {
        return new ASTNativeCall(NO_LOCATION, "_read_line", STRING);
    }

    Value builtin_read_word(ref<Env> env, const Value& args) {
        return new ASTNativeCall(NO_LOCATION, "_read_word", STRING);
    }

    Value builtin_read_int(ref<Env> env, const Value& args) {
        return new ASTNativeCall(NO_LOCATION, "_read_int", INT);
    }

    Value builtin_length(ref<Env> env, const Value& args) {
        return length(args.get_product()[0]);
    }

    Value builtin_at(ref<Env> env, const Value& args) {
        return at(args.get_product()[0], args.get_product()[1]);
    }

    Value builtin_strcat(ref<Env> env, const Value& args) {
        return strcat(args.get_product()[0], args.get_product()[1]);
    }

    Value builtin_substr(ref<Env> env, const Value& args) {
        return substr(args.get_product()[0], args.get_product()[1], args.get_product()[2]);
    }

    Value builtin_if_macro(ref<Env> env, const Value& args) {
        return list_of(Value("#?"), args.get_product()[0], list_of(Value("quote"), args.get_product()[1]),
                       list_of(Value("quote"), args.get_product()[2]));
    }

    Value builtin_if(ref<Env> env, const Value& args) {
        Value cond = args.get_product()[0];

        if (cond.is_runtime()) {
            Value left = eval(env, args.get_product()[1]), right = eval(env, args.get_product()[2]);
            if (left.is_error() || right.is_error()) return error();
            if (!left.is_runtime()) left = lower(left);
            if (!right.is_runtime()) right = lower(right);
            ASTNode* ln = left.get_runtime();
            ASTNode* rn = right.get_runtime();
            return new ASTIf(cond.loc(), cond.get_runtime(), ln, rn);
        }

        if (!cond.is_bool()) {
            err(cond.loc(), "Expected boolean condition in if ", "expression, given '", cond.type(), "'.");
            return error();
        }

        if (cond.get_bool()) return eval(env, args.get_product()[1]);
        else
            return eval(env, args.get_product()[2]);
    }

    Value builtin_annotate(ref<Env> env, const Value& args) {
        return annotate(args.get_product()[0], args.get_product()[1]);
    }

    Value builtin_typeof(ref<Env> env, const Value& args) {
        return type_of(args.get_product()[0]);
    }

    ref<Env> create_root_env() {
        ref<Env> root = newref<Env>();
        root->def("nil", Value(VOID));
        // root->infix("+", new FunctionValue(root, builtin_add, 2), 2, 20);
        // root->infix("-", new FunctionValue(root, builtin_sub, 2), 2, 20);
        // root->infix("*", new FunctionValue(root, builtin_mul, 2), 2, 40);
        // root->infix("/", new FunctionValue(root, builtin_div, 2), 2, 40);
        // root->infix("%", new FunctionValue(root, builtin_rem, 2), 2, 40);
        // root->infix("and", new FunctionValue(root, builtin_and, 2), 2, 5);
        // root->infix("or", new FunctionValue(root, builtin_or, 2), 2, 5);
        // root->infix("xor", new FunctionValue(root, builtin_xor, 2), 2, 5);
        // root->def("not", new FunctionValue(root, builtin_not, 1), 1);
        // root->infix("==", new FunctionValue(root, builtin_equal, 2), 2, 10);
        // root->infix("!=", new FunctionValue(root, builtin_inequal, 2), 2, 10);
        // root->infix("<", new FunctionValue(root, builtin_less, 2), 2, 10);
        // root->infix(">", new FunctionValue(root, builtin_greater, 2), 2, 10);
        // root->infix("<=", new FunctionValue(root, builtin_less_equal, 2), 2, 10);
        // root->infix(">=", new FunctionValue(root, builtin_greater_equal, 2), 2, 10);
        // root->infix("empty?", new FunctionValue(root, builtin_is_empty, 1), 1, 60);
        // root->infix("head", new FunctionValue(root, builtin_head, 1), 1, 80);
        // root->infix("tail", new FunctionValue(root, builtin_tail, 1), 1, 80);
        // root->infix("::", new FunctionValue(root, builtin_cons, 2), 2, 15);
        // root->infix("cons", new FunctionValue(root, builtin_cons, 2), 2, 15);
        // root->def("display", new FunctionValue(root, builtin_display, 1), 1);
        // root->infix_macro("=", new MacroValue(root, builtin_assign_macro, 2), 2, 0);
        // root->infix("#=", new FunctionValue(root, builtin_assign, 2), 2, 0);
        // root->infix_macro("?", new MacroValue(root, builtin_if_macro, 3), 3, 2);
        // root->infix("#?", new FunctionValue(root, builtin_if, 3), 3, 2);
        // root->def("read-line", new FunctionValue(root, builtin_read_line, 0), 0);
        // root->def("read-word", new FunctionValue(root, builtin_read_word, 0), 0);
        // root->def("read-int", new FunctionValue(root, builtin_read_int, 0), 0);
        // root->infix("length", new FunctionValue(root, builtin_length, 1), 1, 50);
        // root->infix("at", new FunctionValue(root, builtin_at, 2), 2, 90);
        // root->infix("^", new FunctionValue(root, builtin_strcat, 2), 2, 20);
        // root->infix("substr", new FunctionValue(root, builtin_substr, 3), 3, 90);
        // root->def("annotate", new FunctionValue(root, builtin_annotate, 2), 2);
        // root->def("typeof", new FunctionValue(root, builtin_typeof, 1), 1);
        define_builtins(root);

        root->def("true", Value(true, BOOL));
        root->def("false", Value(false, BOOL));
        root->def("int", Value(INT, TYPE));
        root->def("symbol", Value(SYMBOL, TYPE));
        root->def("string", Value(STRING, TYPE));
        root->def("type", Value(TYPE, TYPE));
        root->def("bool", Value(BOOL, TYPE));
        root->def("void", Value(VOID, TYPE));
        return root;
    }

    // stubs

    Value eval_list(ref<Env> env, const Value& list);
    Value eval(ref<Env> env, Value term);
    Value define(ref<Env> env, const Value& term, bool is_macro, bool only_vars = false, bool only_procs = false);

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
            return tail(list).is_list() && head(tail(list)).is_list(); // is procedure
        } else if (name == "lambda" || name == "infix" || name == "infix-macro" || name == "macro")
            return true;

        return false;
    }

    static i64 traverse_deep = 0;

    void enable_deep() {
        traverse_deep++;
    }

    void disable_deep() {
        traverse_deep--;
        if (traverse_deep < 0) traverse_deep = 0;
    }

    void traverse_list(ref<Env> env, const Value& list, void (*fn)(ref<Env>, const Value&)) {
        vector<const Value*> vals = to_ptr_vector(list);
        if (!introduces_env(list) || traverse_deep)
            for (u32 i = 0; i < vals.size(); i++) fn(env, *vals[i]);
    }

    void traverse_list(ref<Env> env, Value& list, void (*fn)(ref<Env>, Value&)) {
        vector<Value*> vals = to_ptr_vector(list);
        if (!introduces_env(list) || traverse_deep)
            for (u32 i = 0; i < vals.size(); i++) fn(env, *vals[i]);
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
        } else
            traverse_list(env, item, handle_splice);
    }

    Value use(ref<Env> env, const Value& term);

    void handle_use(ref<Env> env, Value& item) {
        if (!item.is_list()) return;
        Value h = head(item);
        if (h.is_symbol() && h.get_symbol() == symbol_value("use")) {
            use(env, item);
            item = list_of(string("list-of"));
        } else
            traverse_list(env, item, handle_use);
    }

    void visit_macro_defs(ref<Env> env, const Value& item) {
        if (!item.is_list()) return;
        Value h = head(item);
        if (h.is_symbol() &&
            (h.get_symbol() == symbol_value("macro") || h.get_symbol() == symbol_value("infix-macro"))) {
            bool infix = h.get_symbol() == symbol_value("infix-macro");
            u8 precedence = 0;
            vector<Value> values = to_vector(item);
            u32 i = 1;
            if (values.size() >= 2 && infix && values[i].is_int()) {
                precedence = u8(values[i].get_int());
                i++;
            }
            if (i + 1 >= values.size() ||
                (!values[i].is_symbol() && !(values[i].is_list() && head(values[i]).is_symbol()) &&
                 !(values[i].is_list() && tail(values[i]).is_list() && head(tail(values[i])).is_symbol()))) {
                err(item.loc(), "Expected variable or function name ", "in definition.");
                return;
            }
            if (values.size() < 3) err(item.loc(), "Expected value in definition.");
            if (values[i].is_list()) { // procedure
                if (infix) {
                    Value rest = tail(values[i]);
                    if (!rest.is_list()) {
                        err(rest.loc(), "Infix function must take at least one ", "argument.");
                        return;
                    }
                    // basil::infix(env, item, true);
                } else
                    define(env, item, true);
            } else
                define(env, item, true);
        } else
            traverse_list(env, item, visit_macro_defs);
    }

    bool symbol_matches(const Value& term, const string& sym) {
        return term.is_symbol() && term.get_symbol() == symbol_value(sym);
    }

    bool is_annotation(const Value& term) {
        return term.is_list() && symbol_matches(head(term), "annotate");
    }

    Value annotation_type(const Value& term) {
        return head(tail(tail(term)));
    }

    bool is_keyword(const Value& term) {
        return term.is_symbol() && !symbol_for(term.get_symbol()).endswith('?');
    }

    bool is_valid_variable(const Value& term) {
        return term.is_symbol() // name
               || is_annotation(term) && tail(term).is_list() && head(tail(term)).is_symbol();
    }

    bool is_valid_argument(const Value& term) {
        return term.is_symbol() && symbol_for(term.get_symbol()).endswith('?') // name
               || is_annotation(term) && tail(term).is_list() && head(tail(term)).is_symbol();
    }

    bool is_valid_def(const Value& term) {
        return is_valid_argument(term) || term.is_list() && is_keyword(head(term)) ||
               is_annotation(term) && tail(term).is_list() && is_valid_def(head(tail(term)));
    }

    bool is_valid_infix_def(const Value& term) {
        return term.is_list() && tail(term).is_list() && is_valid_argument(head(term)) &&
                   is_keyword(head(tail(term))) ||
               is_annotation(term) && tail(term).is_list() && is_valid_infix_def(head(tail(term)));
    }

    string get_variable_name(const Value& term) {
        return is_annotation(term) ? get_variable_name(head(tail(term))) : symbol_for(term.get_symbol());
    }

    string get_arg_name(const Value& term) {
        if (is_annotation(term)) return get_arg_name(head(tail(term)));
        else {
            const string& name = symbol_for(term.get_symbol());
            return name[{0, name.size() - 1}];
        }
    }

    string get_def_name(const Value& term) {
        if (term.is_symbol()) return symbol_for(term.get_symbol());
        else if (is_annotation(term))
            return get_def_name(head(tail(term)));
        else
            return symbol_for(head(term).get_symbol());
    }

    Value get_def_info(const Value& term) {
        if (is_annotation(term)) return get_def_info(head(tail(term)));
        return term;
    }

    string get_infix_name(const Value& term) {
        if (is_annotation(term)) return get_infix_name(head(tail(term)));
        return symbol_for(head(tail(term)).get_symbol());
    }

    u64 get_keyword(const Value& v) {
        if (v.is_symbol()) return v.get_symbol();
        else if (v.is_list() && head(v).is_symbol() && head(v).get_symbol() == symbol_value("quote") &&
                 tail(v).is_list() && head(tail(v)).is_symbol())
            return head(tail(v)).get_symbol();
        return 0;
    }

    bool is_keyword(const Value& v, const string& word) {
        if (v.is_symbol()) return v.get_symbol() == symbol_value(word);
        else if (v.is_list() && head(v).is_symbol() && head(v).get_symbol() == symbol_value("quote") &&
                 tail(v).is_list() && head(tail(v)).is_symbol())
            return head(tail(v)).get_symbol() == symbol_value(word);
        return false;
    }

    void visit_defs(ref<Env> env, const Value& item) {
        if (!item.is_list()) return;
        Value h = head(item);
        if (symbol_matches(h, "def")) {
            u8 precedence = 0;
            bool has_precedence = false;
            vector<Value> values = to_vector(item);
            u32 i = 1;
            if (values.size() >= 2 && values[i].is_int()) {
                precedence = u8(values[i].get_int()); // consume precedence
                has_precedence = true;
                i++;
            }
            if (is_valid_variable(values[i])) { // variable
                string name = get_variable_name(values[i]);
                if (values.size() < 3) {
                    err(item.loc(), "Expected initial value in variable definition.");
                    return;
                }
                // if (env->find(name)) {
                // 	err(values[i].loc(), "Redefinition of '", name, "'.");
                // 	return;
                // }
                if (!env->find(name)) env->def(name);
            } else if (is_valid_def(values[i])) { // procedure
                const string& name = get_def_name(values[i]);
                if (name.endswith('?')) {
                    err(values[i].loc(), "Invalid name for procedure: cannot end in '?'.");
                    return;
                }
                if (has_precedence) {
                    err(values[i - 1].loc(), "Precedence cannot be specified for non-infix procedures.");
                    return;
                }
                if (values.size() < 3) {
                    err(item.loc(), "Expected procedure body in procedure definition.");
                    return;
                }
                // if (env->find(name)) {
                // 	err(values[i].loc(), "Redefinition of '", name, "'.");
                // 	return;
                // }
                if (!env->find(name)) env->def(name, to_vector(tail(values[i])).size());
            } else if (is_valid_infix_def(values[i])) { // infix procedure
                Value rest = tail(values[i]);
                if (!rest.is_list()) {
                    err(rest.loc(), "Infix procedure must take at least one ", "argument.");
                    return;
                }
                string name = get_infix_name(values[i]);
                if (name.endswith('?')) {
                    err(values[i].loc(), "Invalid name for infix procedure: cannot end in '?'.");
                    return;
                }
                if (values.size() < 3) {
                    err(item.loc(), "Expected procedure body in infix procedure definition.");
                    return;
                }
                if (!env->find(name)) env->infix(name, to_vector(tail(rest)).size() + 1, precedence);
            } else {
                err(item.loc(), "Invalid definition.");
                return;
            }
        } else
            traverse_list(env, item, visit_defs);
    }

    void define_procedures(ref<Env> env, const Value& item) {
        if (!item.is_list()) return;
        Value h = head(item);
        if (symbol_matches(h, "def")) {
            define(env, item, false, false, true);
        } else
            traverse_list(env, item, define_procedures);
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
            return fn.get_builtin().eval(env, arg);
        } else {
            ref<Env> env = fn.get_env();
            u32 argc = arg.get_product().size(), arity = fn.args().size();
            if (argc != arity) {
                err(macro.loc(), "Procedure requires ", arity, " arguments, ", argc, " provided.");
                return error();
            }
            for (u32 i = 0; i < arity; i++) {
                if (fn.args()[i] & KEYWORD_ARG_BIT) {
                    // keyword arg
                    if (get_keyword(arg.get_product()[i]) != (fn.args()[i] & ARG_NAME_MASK)) {
                        err(arg.get_product()[i].loc(), "Expected keyword '", symbol_for(fn.args()[i] & ARG_NAME_MASK),
                            "', got '", arg.get_product()[i], "'.");
                        return error();
                    }
                } else {
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
        } else if (item.is_list()) {
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

    Value apply_op(const Value& op, const Value& lhs, const vector<Value>& internals, const Value& rhs) {
        Value l = list_of(rhs);
        for (i64 i = i64(internals.size()) - 1; i >= 0; i--) { l = cons(internals[i], l); }
        return cons(op, cons(lhs, l));
    }

    struct InfixResult {
        Value first, second;
        bool changed;
    };

    InfixResult unary_helper(ref<Env> env, const Value& lhs, const Value& term);
    InfixResult infix_helper(ref<Env> env, const Value& lhs, const Value& op, const Def* def, const Value& rhs,
                             const Value& term, const vector<Value>& internals);
    InfixResult infix_helper(ref<Env> env, const Value& lhs, const Value& op, const Def* def, const Value& term);
    InfixResult infix_transform(ref<Env> env, const Value& term);

    InfixResult infix_helper(ref<Env> env, const Value& lhs, const Value& op, const Def* def, const Value& rhs,
                             const Value& term, const vector<Value>& internals) {
        Value iter = term;

        if (iter.is_void()) return {apply_op(op, lhs, internals, rhs), iter, true};
        Value next_op = head(iter);
        if (!next_op.is_symbol()) return {apply_op(op, lhs, internals, rhs), iter, true};
        const Def* next_def = env->find(symbol_for(next_op.get_symbol()));
        if (!next_def || !next_def->is_infix) return {apply_op(op, lhs, internals, rhs), iter, true};
        iter = tail(iter); // consume op

        if (next_def->precedence > def->precedence) {
            if (next_def->arity == 1) {
                return infix_helper(env, lhs, op, def, apply_op(next_op, rhs), iter, internals);
            }
            auto p = infix_helper(env, rhs, next_op, next_def, iter);
            return {apply_op(op, lhs, internals, p.first), p.second, true};
        } else {
            Value result = apply_op(op, lhs, rhs);
            if (next_def->arity == 1) { return unary_helper(env, apply_op(next_op, result), iter); }
            return infix_helper(env, apply_op(op, lhs, internals, rhs), next_op, next_def, iter);
        }
    }

    InfixResult infix_helper(ref<Env> env, const Value& lhs, const Value& op, const Def* def, const Value& term) {
        Value iter = term;

        vector<Value> internals;
        if (def->arity > 2)
            for (u32 i = 0; i < def->arity - 2; i++) {
                auto p = infix_transform(env, iter);
                internals.push(p.first);
                iter = p.second;
            }

        if (iter.is_void()) {
            err(term.loc(), "Expected term in binary expression.");
            return {error(), term, false};
        }
        Value rhs = head(iter);
        iter = tail(iter); // consume second term

        return infix_helper(env, lhs, op, def, rhs, iter, internals);
    }

    InfixResult unary_helper(ref<Env> env, const Value& lhs, const Value& term) {
        Value iter = term;

        if (iter.is_void()) return {lhs, iter, false};
        Value op = head(iter);
        if (!op.is_symbol()) return {lhs, iter, false};
        const Def* def = env->find(symbol_for(op.get_symbol()));
        if (!def || !def->is_infix) return {lhs, iter, false};
        iter = tail(iter); // consume op

        if (def->arity == 1) return unary_helper(env, apply_op(op, lhs), iter);
        return infix_helper(env, lhs, op, def, iter);
    }

    InfixResult infix_transform(ref<Env> env, const Value& term) {
        Value iter = term;

        if (iter.is_void()) {
            err(term.loc(), "Expected term in binary expression.");
            return {error(), term};
        }
        Value lhs = head(iter); // 1 + 2 -> 1
        iter = tail(iter);      // consume first term

        if (iter.is_void()) return {lhs, iter, false};
        Value op = head(iter);
        if (!op.is_symbol()) return {lhs, iter, false};
        const Def* def = env->find(symbol_for(op.get_symbol()));
        if (!def || !def->is_infix) return {lhs, iter, false};
        iter = tail(iter); // consume op

        if (def->arity == 1) return unary_helper(env, apply_op(op, lhs), iter);
        return infix_helper(env, lhs, op, def, iter);
    }

    Value handle_infix(ref<Env> env, const Value& term) {
        vector<Value> infix_exprs;
        Value iter = term;
        bool changed = false;
        while (iter.is_list()) {
            auto p = infix_transform(env, iter);
            infix_exprs.push(p.first); // next s-expr
            iter = p.second;           // move past it in source list
            changed = changed || p.changed;
        }
        Value result = infix_exprs.size() == 1 && changed ? infix_exprs[0] : list_of(infix_exprs);
        return result;
    }

    void apply_infix(ref<Env> env, Value& item);

    void apply_infix_at(ref<Env> env, Value& item, u32 depth) {
        Value* iter = &item;
        while (depth && iter->is_list()) {
            iter = &iter->get_list().tail();
            depth--;
        }
        *iter = handle_infix(env, *iter);
        traverse_list(env, *iter, apply_infix);
    }

    void apply_infix(ref<Env> env, Value& item) {
        if (!item.is_list()) return;
        Value h = head(item);
        if (h.is_symbol()) {
            const string& name = symbol_for(h.get_symbol());
            if (name == "macro" || name == "lambda" || name == "splice") return;
            // else if (name == "def") {
            //     if (tail(item).is_list() && head(tail(item)).is_symbol()) apply_infix_at(env, item, 2);
            // }
            else if (name == "if" || name == "do" || name == "list-of")
                apply_infix_at(env, item, 1);
            else {
                silence_errors();
                Value proc = eval(env, h);
                unsilence_errors();
                if (proc.is_function()) apply_infix_at(env, item, 1);
                else
                    apply_infix_at(env, item, 0);
            }
        } else
            apply_infix_at(env, item, 0);
    }

    void prep(ref<Env> env, Value& term) {
        handle_splice(env, term);
        handle_use(env, term);
        visit_macro_defs(env, term);
        visit_defs(env, term);
        apply_infix(env, term);
        handle_macro(env, term);
        visit_defs(env, term);
        apply_infix(env, term);
        // define_procedures(env, term);
    }

    Value overload(ref<Env> env, Def* existing, const Value& func, const string& name) {
        const Type* argst = (const ProductType*)((const FunctionType*)func.type())->arg();
        if (existing->value.is_function()) { // create new intersect
            const Type* existing_args = ((const FunctionType*)existing->value.type())->arg();
            if (existing_args == argst) {
                err(func.loc(), "Duplicate implementation for function '", name,
                    "': overload already exists for arguments ", argst, ".");
                // todo: mention original definition's location
                return error();
            }
            map<const Type*, Value> values;
            values.put(existing->value.type(), existing->value);
            values.put(func.type(), func);
            existing->value =
                Value(new IntersectValue(values), find<IntersectType>(existing->value.type(), func.type()));
            return Value(VOID);
        } else if (existing->value.is_intersect()) { // add to existing intersect
            for (const auto& p : existing->value.get_intersect()) {
                if (p.first->kind() == KIND_FUNCTION) {
                    const Type* existing_args = ((const FunctionType*)p.second.type())->arg();
                    if (existing_args == argst) {
                        err(func.loc(), "Duplicate implementation for function '", name,
                            "': overload already exists for arguments ", argst, ".");
                        // todo: mention original definition's location
                        return error();
                    }
                }
            }
            map<const Type*, Value> values = existing->value.get_intersect().values();
            values.put(func.type(), func);
            existing->value =
                Value(new IntersectValue(values), find<IntersectType>(existing->value.type(), func.type()));
            return Value(VOID);
        } else {
            err(func.loc(), "Cannot redefine symbol '", name, "' of type '", existing->value.type(), "' as function.");
            return error();
        }
    }

    // definition stuff
    Value define(ref<Env> env, const Value& term, bool is_macro, bool only_vars, bool only_procs) {
        vector<Value> values = to_vector(term);

        // visit_defs already does some error-checking, so we
        // don't need to check the number of values or their types
        // exhaustively.
        u8 precedence = 0;

        u32 i = 1;
        if (values[i].is_int()) { // precedence
            precedence = (u8)values[i].get_int();
            i++;
        }

        if (is_valid_variable(values[i])) { // variable
            if (only_procs) return Value(VOID);
            string name = get_variable_name(values[i]);

            if (is_macro) {
                if (is_annotation(values[i])) {
                    err(values[i].loc(), "Type annotations are forbidden in macro definitions.");
                    return error();
                }
                env->def_macro(name, Value(new AliasValue(values[i + 1])));
                return Value(VOID);
            } else {
                Value init = eval(env, values[i + 1]);
                if (env->is_runtime()) init = lower(init);
                if (is_annotation(values[i])) init = annotate(init, eval(env, annotation_type(values[i])));
                env->def(name, init);
                if (init.is_runtime())
                    return new ASTDefine(values[0].loc(), env, values[i].get_symbol(), init.get_runtime());
                return Value(VOID);
            }
        } else if (values[i].is_list()) { // procedure
            if (only_vars) return Value(VOID);
            bool infix = !is_valid_def(values[i]);
            string name = infix ? get_infix_name(values[i]) : get_def_name(values[i]);

            ref<Env> function_env = newref<Env>(env);
            vector<Value> args;
            vector<Value> elts = to_vector(get_def_info(values[i]));
            for (u32 i = 0; i < elts.size(); i++)
                if (i != infix ? 1 : 0) args.push(elts[i]);
            vector<u64> argnames;
            vector<Value> body;
            vector<const Type*> argts;
            for (const Value& v : args) {
                if (is_valid_argument(v)) {
                    string name = get_arg_name(v);
                    argnames.push(symbol_value(name));
                    function_env->def(name);
                } else if (is_keyword(v)) {
                    argnames.push(v.get_symbol() | KEYWORD_ARG_BIT);
                } else {
                    err(v.loc(),
                        "Only argument names and keywords "
                        "are permitted within an argument list; given '",
                        v, "'.");
                    return error();
                }
                if (is_annotation(v)) {
                    if (is_macro) {
                        err(v.loc(), "Type annotations are forbidden in macro definitions.");
                        return error();
                    }
                    Value tval = eval(env, annotation_type(v));
                    if (!tval.is_type()) {
                        if (tval.type()->coerces_to(TYPE)) tval = cast(tval, TYPE);
                        else {
                            err(v.loc(), "Expected type value in annotation, given '", tval.type(), "'.");
                            return error();
                        }
                    }
                    // body.push(v);
                    // body.push(list_of(Value("list-of")));
                    argts.push(tval.get_type());
                } else if (!is_keyword(v))
                    argts.push(ANY);
            }
            for (u32 j = i + 1; j < values.size(); j++) body.push(values[j]);
            Value body_term = cons(Value("do"), list_of(body));
            if (is_macro) {
                if (is_annotation(values[i])) {
                    err(values[i].loc(), "Type annotations are forbidden in macro definitions.");
                    return error();
                }
                Value mac(new MacroValue(function_env, argnames, body_term));
                if (infix) env->infix_macro(name, mac, argnames.size(), precedence);
                else
                    env->def_macro(name, mac, argnames.size());
            } else {
                const Type* returntype = ANY;
                if (is_annotation(values[i])) {
                    Value tval = eval(env, annotation_type(values[i]));
                    if (!tval.is_type()) {
                        if (tval.type()->coerces_to(TYPE)) tval = cast(tval, TYPE);
                        else {
                            err(values[i].loc(), "Expected type value in annotation, given '", tval.type(), "'.");
                            return error();
                        }
                    }
                    returntype = tval.get_type();
                }

                bool external = false;
                if (body.size() == 1 && symbol_matches(body[0], "extern")) {
                    if (!returntype->concrete()) {
                        err(body[0].loc(), "Explicit return type required in extern function definition.");
                        return error();
                    }
                    external = true;
                }

                const Type* argst = find<ProductType>(argts);
                const FunctionType* ft = (const FunctionType*)find<FunctionType>(argst, returntype);
                u64 fname = symbol_value(name);
                Value func = external ? Value(new ASTFunction(term.loc(), function_env, argst, argnames,
                                                              new ASTExtern(term.loc(), returntype), fname))
                                      : Value(new FunctionValue(function_env, argnames, body_term, fname), ft);
                Def* existing = env->find(name);
                if (existing && !existing->value.is_void()) {
                    return overload(env, existing, func, name);
                } else if (infix) {
                    env->infix(name, func, argnames.size(), precedence);
                } else
                    env->def(name, func, argnames.size());
                if (func.is_runtime()) return new ASTDefine(values[0].loc(), env, fname, func.get_runtime());
                if (argst->concrete() && !external) {
                    func.get_function().instantiate(argst, new ASTIncompleteFn(func.loc(), argst, symbol_value(name)));
                    instantiate(func.loc(), func.get_function(), find<ProductType>(argts));
                }
            }
            return Value(VOID);
        } else {
            err(values[i].loc(), "First parameter to definition must be ",
                "a symbol (for variable or alias) or list (for procedure ", "or macro).");
            return error();
        }
    }

    Value infix(ref<Env> env, const Value& term, bool is_macro) {
        vector<Value> values = to_vector(term);

        u8 precedence = 0;

        u32 i = 1;
        if (values[i].is_int()) { // precedence
            precedence = (u8)values[i].get_int();
            i++;
        }

        Value def_info = get_def_info(values[i]);
        bool infix = false;
        if (!is_valid_def(def_info)) infix = false;
        const string& name = symbol_for(head(tail(def_info)).get_symbol());

        vector<Value> args;
        args.push(head(def_info));
        Value v = tail(tail(def_info));
        while (v.is_list()) {
            args.push(head(v));
            v = tail(v);
        }
        if (args.size() == 0) {
            err(values[i].loc(), "Expected argument list in infix ", "definition.");
            return error();
        }
        i++;
        ref<Env> function_env = newref<Env>(env);
        vector<u64> argnames;
        vector<const Type*> argts;
        for (const Value& v : args) {
            if (is_valid_argument(v)) {
                string name = get_arg_name(v);
                argnames.push(symbol_value(name));
                function_env->def(name);
            } else if (is_keyword(v)) {
                argnames.push(eval(env, v).get_symbol() | KEYWORD_ARG_BIT);
            } else {
                err(v.loc(),
                    "Only symbols, annotated symbols, and quoted symbols "
                    "are permitted within an argument list; given '",
                    v, "'.");
                return error();
            }
            if (is_annotation(v)) {
                if (is_macro) {
                    err(v.loc(), "Type annotations are forbidden in macro definitions.");
                    return error();
                }
                Value tval = eval(env, annotation_type(v));
                if (!tval.is_type()) {
                    if (tval.type()->coerces_to(TYPE)) tval = cast(tval, TYPE);
                    else {
                        err(v.loc(), "Expected type value in annotation, given '", tval.type(), "'.");
                        return error();
                    }
                }
                // body.push(v);
                // body.push(list_of(Value("list-of")));
                argts.push(tval.get_type());
            } else if (!is_keyword(v))
                argts.push(ANY);
        }
        vector<Value> body;
        for (int j = i; j < values.size(); j++) body.push(values[j]);
        Value body_term = cons(Value("do"), list_of(body));
        if (is_macro) {
            if (is_annotation(values[i])) {
                err(values[i].loc(), "Type annotations are forbidden in macro definitions.");
                return error();
            }
            Value mac(new MacroValue(function_env, argnames, body_term));
            env->infix_macro(name, mac, argnames.size(), precedence);
        } else {
            const Type* returntype = ANY;
            if (is_annotation(values[i])) {
                Value tval = eval(env, annotation_type(values[i]));
                if (!tval.is_type()) {
                    if (tval.type()->coerces_to(TYPE)) tval = cast(tval, TYPE);
                    else {
                        err(values[i].loc(), "Expected type value in annotation, given '", tval.type(), "'.");
                        return error();
                    }
                }
                returntype = tval.get_type();
            }
            const Type* argst = find<ProductType>(argts);
            Value func(new FunctionValue(function_env, argnames, body_term, symbol_value(name)),
                       find<FunctionType>(argst, returntype));
            Def* existing = env->find(name);
            if (existing && !existing->value.is_void()) {
                if (existing->precedence != precedence) {
                    err(func.loc(), "Cannot overload infix function '", name, "' with function of incompatible ",
                        "precedence ", (u32)precedence, " (original was ", (u32)existing->precedence, ").");
                    return error();
                }
                return overload(env, existing, func, name);
            } else
                env->infix(name, func, argnames.size(), precedence);
            // if (argst->concrete()) instantiate(func.loc(), func.get_function(), find<ProductType>(argts));
        }
        return Value(VOID);
    }

    Value lambda(ref<Env> env, const Value& term) {
        vector<Value> values = to_vector(term);
        if (values.size() < 3) {
            err(values[0].loc(), "Expected argument list and body in ", "lambda expression.");
            return error();
        }
        if (!values[1].is_list()) {
            err(values[1].loc(), "Expected argument list in lambda ", "expression.");
            return error();
        }
        ref<Env> function_env = newref<Env>(env);
        vector<Value> args = to_vector(values[1]);
        vector<u64> argnames;
        vector<const Type*> argts;
        for (const Value& v : args) {
            if (is_valid_argument(v)) {
                string name = get_arg_name(v);
                argnames.push(symbol_value(name));
                function_env->def(name);
            } else if (is_keyword(v)) {
                argnames.push(eval(env, v).get_symbol() | KEYWORD_ARG_BIT);
            } else {
                err(v.loc(),
                    "Only symbols, annotated symbols, and quoted symbols "
                    "are permitted within an argument list; given '",
                    v, "'.");
                return error();
            }
            if (is_annotation(v)) {
                Value tval = eval(env, annotation_type(v));
                if (!tval.is_type()) {
                    if (tval.type()->coerces_to(TYPE)) tval = cast(tval, TYPE);
                    else {
                        err(v.loc(), "Expected type value in annotation, given '", tval.type(), "'.");
                        return error();
                    }
                }
                // body.push(v);
                // body.push(list_of(Value("list-of")));
                argts.push(tval.get_type());
            } else if (!is_keyword(v))
                argts.push(ANY);
        }
        const Type* returntype = ANY;
        if (is_annotation(values[1])) {
            Value tval = eval(env, annotation_type(values[1]));
            if (!tval.is_type()) {
                if (tval.type()->coerces_to(TYPE)) tval = cast(tval, TYPE);
                else {
                    err(values[1].loc(), "Expected type value in annotation, given '", tval.type(), "'.");
                    return error();
                }
            }
            returntype = tval.get_type();
        }
        const Type* argst = find<ProductType>(argts);
        vector<Value> body;
        for (u32 i = 2; i < values.size(); i++) body.push(values[i]);
        Value body_term = list_of(body);
        prep(function_env, body_term);
        return Value(new FunctionValue(function_env, argnames, cons(Value("do"), body_term)),
                     find<FunctionType>(argst, returntype));
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
            for (Value& v : values)
                if (!v.is_runtime()) {
                    v = lower(v);
                    if (v.is_error()) return v;
                }
            for (Value& v : values) nodes.push(v.get_runtime());
            return new ASTBlock(term.loc(), nodes);
        }
        return values.back();
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
        while (params.is_list() && !(is_keyword(head(params), "else") || is_keyword(head(params), "elif"))) {
            if_true_vals.push(head(params));
            params = tail(params);
        }
        if_true = cons(Value("do"), list_of(if_true_vals));
        if (!params.is_list()) {
            if_false = list_of(Value("list-of"));
        } else if (get_keyword(head(params)) == symbol_value("elif")) {
            if_false = if_expr(env, params);
            return list_of(Value("#?"), cond, list_of(Value("quote"), if_true), list_of(Value("quote"), if_false));
        } else {
            params = tail(params);
            while (params.is_list()) {
                if_false_vals.push(head(params));
                params = tail(params);
            }
            if_false = cons(Value("do"), list_of(if_false_vals));
        }
        return list_of(Value("#?"), cond, list_of(Value("quote"), if_true), list_of(Value("quote"), if_false));
    }

    bool is_quoted_symbol(const Value& val) {
        return val.is_list() && tail(val).is_list() && head(val).is_symbol() &&
               head(val).get_symbol() == symbol_value("quote") && head(tail(val)).is_symbol();
    }

    void find_assigns(ref<Env> env, const Value& term, set<u64>& dests) {
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
            err(term.loc(), "Incorrect number of arguments for while ", "statement. Expected condition and body.");
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
                nodes.push(new ASTDefine(new_value.loc(), env, u, new_value.get_runtime()));
                def->value = new_value;
            }
        }

        Value cond = eval(env, head(tail(term)));
        if (cond.is_error()) return error();
        if (!cond.is_runtime()) cond = lower(cond);

        body = eval(env, body);
        if (body.is_error()) return error();
        if (!body.is_runtime()) body = lower(body);
        nodes.push(new ASTWhile(term.loc(), cond.get_runtime(), body.get_runtime()));
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

    Value tuple_of(ref<Env> env, const Value& term) {
        Value items = tail(term);

        const Value* v = &items;
        vector<Value> vals;
        while (v->is_list()) {
            vals.push(eval(env, v->get_list().head()));
            v = &v->get_list().tail();
        }
        return tuple_of(vals);
    }

    Value array_of(ref<Env> env, const Value& term) {
        Value items = tail(term);

        const Value* v = &items;
        vector<Value> vals;
        while (v->is_list()) {
            vals.push(eval(env, v->get_list().head()));
            v = &v->get_list().tail();
        }
        return array_of(vals);
    }

    Value set_of(ref<Env> env, const Value& term) {
        Value items = tail(term);

        map<Value, Value> pairs;
        const Value* v = &items;
        // dict
        if (is_annotation(head(items))) {
            while (v->is_list()) {
                const Value* term = &v->get_list().head();
                if (!is_annotation(*term)) {
                    err(term->loc(), "Found set-style initializer in dictionary literal expression");
                    return error();
                }
                pairs.put(term->get_list().tail().get_list().head(),
                          term->get_list().tail().get_list().tail().get_list().head());
                v = &v->get_list().tail();
            }
        } else { // set
            while (v->is_list()) {
                const Value* term = &v->get_list().head();
                if (is_annotation(*term)) {
                    err(term->loc(), "Found dictionary-style initializer pair in set literal expression");
                    return error();
                }
                pairs.put(*term, empty());
                v = &v->get_list().tail();
            }
        }

        return dict_of(pairs);
    }

    struct CompilationUnit {
        Source source;
        ref<Env> env;
    };

    static map<string, CompilationUnit> modules;

    Value use(ref<Env> env, const Value& term) {
        if (!tail(term).is_list() || tail(term).is_void()) {
            err(tail(term).loc(), "Expected body in use expression.");
            return error();
        }

        string path, module_name;
        set<string> names;
        Value h = head(tail(term));
        // use <module> -> introduce a modulevalue with name <module> into the env
        // use <module> as <name> -> introduce a modulevalue with name <name> into the env
        if (head(tail(term)).is_symbol()) {
            if (!h.is_symbol()) {
                err(h.loc(), "Expected module name in use expression, got '", h, "'");
                return error();
            }

            path = symbol_for(h.get_symbol());

            if (!tail(tail(term)).is_void() && tail(tail(term)).is_list()) { // use <module> as <name>
                Value as_exp = tail(tail(term));
                if (!head(as_exp).is_symbol() || symbol_for(head(as_exp).get_symbol()) != "as") {
                    err(head(as_exp).loc(), "Expected 'as' after module name in use expression, got '", head(as_exp),
                        "'");
                    return error();
                }

                if (tail(as_exp).is_void() || !head(tail(as_exp)).is_symbol()) {
                    err(tail(as_exp).loc(), "Expected symbol after 'as' in use expression");
                    return error();
                }
                module_name = symbol_for(head(tail(as_exp)).get_symbol());
            } else {
                module_name = path;
            }
        } else { // use <module>[ ... ] -> introduce names from [ ... ] into the env
            if (!head(h).is_symbol() || symbol_for(head(h).get_symbol()) != "at") {
                err(h.loc(), "Expected at-expression in use, got '", head(h), "'");
                return error();
            }
            if (!tail(h).is_list() || tail(h).is_void() || !head(tail(h)).is_symbol()) {
                err(tail(h).loc(), "Expected body in at-expression in use");
                return error();
            }
            Value nameslist = head(tail(tail(h))); // May be either a list of names or just a single symbol
            if (!nameslist.is_list()) {
                if (!nameslist.is_symbol()) {
                    err(term.loc(), "Expected name(s) inside of brackets in bracketed use expression");
                    return error();
                }
                names.insert(symbol_for(nameslist.get_symbol()));
            } else {
                nameslist = tail(nameslist);
                for (const Value& name : to_vector(nameslist)) {
                    if (!name.is_symbol()) {
                        err(name.loc(), "Expected symbol in use expression, given'", name, "'");
                        return error();
                    }
                    names.insert(symbol_for(name.get_symbol()));
                }
            }
            path = symbol_for(head(tail(h)).get_symbol());
        }

        path += ".bl";

        ref<Env> module;
        auto it = modules.find(path);
        if (it != modules.end()) {
            module = it->second.env;
        } else {
            CompilationUnit unit{Source((const char*)path.raw()), {}};
            if (!unit.source.begin().peek()) {
                err(h.loc(), "Could not load source file at path '", path, "'.");
                return error();
            }
            module = unit.env = load(unit.source);
            if (error_count()) return error();

            if (names.size()) { // use <module>[names...]
                for (const auto& p : *module) {
                    if (names.find(p.first) != names.end()) {
                        if (env->find(p.first)) {
                            err(term.loc(), "Module '", symbol_for(h.get_symbol()), "' redefines '", p.first,
                                "' in the current environment.");
                            return error();
                        }
                        env->import_single(p.first, p.second);
                    }
                }
            } else { // use <module> | use <module> as <name>
                map<u64, Value> values;
                for (const auto& p : *module) values.put(symbol_value(p.first), p.second.value);
                env->def(module_name, Value(new ModuleValue(values)));
            }

            modules.put(path, unit);
        }

        return Value(VOID);
    }

    Value eval_list(ref<Env> env, const Value& term) {
        Value h = head(term);
        if (h.is_symbol()) {
            const string& name = symbol_for(h.get_symbol());
            if (name == "quote") return head(tail(term)).clone();
            else if (name == "def")
                return define(env, term, false);
            else if (name == "macro")
                return define(env, term, true);
            else if (name == "lambda")
                return lambda(env, term);
            else if (name == "do")
                return do_block(env, term);
            else if (name == "if")
                return eval(env, if_expr(env, term));
            else if (name == "list-of")
                return list_of(env, term);
            else if (name == "tuple-of")
                return tuple_of(env, term);
            else if (name == "array-of")
                return array_of(env, term);
            else if (name == "set-of")
                return set_of(env, term);
            else if (name == "use")
                return use(env, term);
            else if (name == "while")
                return while_stmt(env, term);
        }

        Value first = eval(env, h);

        if (h.is_symbol() && tail(term).is_void()) {
            const Def* def = env->find(symbol_for(h.get_symbol()));
            if (def && def->is_infix) return first;
        }

        if (first.is_macro()) {
            vector<Value> args;
            const Value* v = &term.get_list().tail();
            u32 i = 0;
            while (v->is_list()) {
                args.push(v->get_list().head());
                v = &v->get_list().tail();
                i++;
            }
            if (args.size() != first.get_macro().arity()) {
                err(term.loc(), "Macro procedure expects ", first.get_macro().arity(), " arguments, ", args.size(),
                    " provided.");
                return error();
            }
            return expand(env, first, Value(new ProductValue(args)));
        } else if (first.is_function() ||
                   first.is_intersect() && ((const IntersectType*)first.type())->has_function()) {
            vector<Value> args;
            Value args_term = tail(term);
            const Value* v = &args_term;
            u32 i = 0;
            while (v->is_list()) {
                // if (i < first.get_function().arity() && first.get_function().args()[i] & KEYWORD_ARG_BIT &&
                //     v->get_list().head().is_symbol()) {
                //     args.push(v->get_list().head()); // leave keywords quoted
                // } else
                args.push(eval(env, v->get_list().head()));

                v = &v->get_list().tail();
                i++;
            }
            return call(env, first, Value(new ProductValue(args)));
        } else if (first.is_runtime() && first.get_runtime()->type()->kind() == KIND_FUNCTION) {
            vector<Value> args;
            Value args_term = tail(term);
            const Value* v = &args_term;
            u32 i = 0;
            while (v->is_list()) {
                args.push(eval(env, v->get_list().head()));

                v = &v->get_list().tail();
                i++;
            }
            const FunctionType* fntype = (const FunctionType*)first.get_runtime()->type();
            const ProductType* args_type = (const ProductType*)fntype->arg();
            if (args.size() != args_type->count()) {
                err(term.loc(), "Procedure expects ", args_type->count(), " arguments, ", args.size(), " provided.");
                return error();
            }
            return call(env, first, Value(new ProductValue(args)));
        }

        if (tail(term).is_void()) return first;

        err(term.loc(), "Could not evaluate list '", term, "'.");
        return error();
    }

    Value eval(ref<Env> env, Value term) {
        if (term.is_list()) return eval_list(env, term);
        else if (term.is_int())
            return term;
        else if (term.is_string())
            return term;
        else if (term.is_symbol()) {
            const string& name = symbol_for(term.get_symbol());
            const Def* def = env->find(name);
            if (def && def->is_macro_variable()) return def->value.get_alias().value();
            else if (def) {
                if (def->value.is_runtime()) return new ASTVar(term.loc(), env, term.get_symbol());
                return def->value;
            } else {
                err(term.loc(), "Undefined variable '", name, "'.");
                return error();
            }
        }
        err(term.loc(), "Could not evaluate term '", term, "'.");
        return error();
    }
} // namespace basil