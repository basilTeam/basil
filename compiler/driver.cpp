#include "driver.h"
#include "ast.h"
#include "errors.h"
#include "eval.h"
#include "lex.h"
#include "native.h"
#include "parse.h"
#include "util/io.h"
#include "values.h"

namespace basil {
<<<<<<< HEAD
	static bool _print_tokens = false,
		_print_parsed = false,
		_print_ast = false,
		_print_ir = false,
		_print_asm = false,
		_compile_only = false;

	void print_tokens(bool should) {
		_print_tokens = should;
	}

	void print_parsed(bool should) {
		_print_parsed = should;
	}

	void print_ast(bool should) {
		_print_ast = should;
	}

	void print_ir(bool should) {
		_print_ir = should;
	}

	void print_asm(bool should) {
		_print_asm = should;
	}

	void compile_only(bool should) {
		_compile_only = should;
	}

	bool is_compile_only() {
		return _compile_only;
	}

	vector<Token> lex(Source::View& view) {
		vector<Token> tokens;
		while (view.peek()) tokens.push(scan(view));

		if (_print_tokens) {
			print(BOLDYELLOW);
			for (const Token& tok : tokens) print(tok, " ");
			println(RESET, "\n");
		}
		return tokens;
	}

	Value parse(TokenView& view) {
		vector<Value> results;
		TokenView end = view;
		while (end) end.read();
		while (view.peek()) {
			parse_line(results, view, view.peek().column);
		}
		if (_print_tokens) {
			print(BOLDYELLOW);
			while (end) print(end.read(), " ");
			println(RESET);
		}
		if (_print_parsed) {
			print(BOLDGREEN);
			for (const Value& v : results) print(v, " ");
			println(RESET);
		}
		return cons(string("do"), list_of(results));
	}

	ref<Env> create_global_env() {
		ref<Env> root = create_root_env();
		ref<Env> global_env = newref<Env>(root);
		return global_env;
	}

	void compile(Value value, Object& object, Function& fn) {
		fn.allocate();
		fn.emit(object);
		emit_constants(object);
		if (error_count()) return;

		if (_print_asm) {
			print(BOLDRED);
			byte_buffer code = object.code();
			while (code.size()) printf("%02x ", code.read());
			println(RESET, "\n");
		}
	}

	void generate(Value value, Function& fn) {
		Location last = loc_none();
		if (value.is_runtime()) last = value.get_runtime()->emit(fn);
		if (last.type != LOC_NONE) fn.add(new RetInsn(last));
		if (error_count()) return;

		if (_print_ir) {
			print(BOLDMAGENTA);
			print(fn);
			println(RESET, "\n");
		}
	}

	void compile(Value value, Object& object) {
		Function main_fn("_start");
		Location last = loc_none();
		if (value.is_runtime()) last = value.get_runtime()->emit(main_fn);
		if (last.type != LOC_NONE) main_fn.add(new RetInsn(last));
		if (error_count()) return;

		if (_print_ir) {
			print(BOLDMAGENTA);
			print(main_fn);
			println(RESET, "\n");
		}
		
		compile(value, object, main_fn);
	}

	void jit_print(Value value, const Object& object) {
		auto main_jit = object.find(jasmine::global("main"));
		if (main_jit) {
			u64 result = ((u64(*)())main_jit)();
			const Type* t = value.get_runtime()->type();
			if (t == VOID) return;
			print("= ");

			if (t == INT) print((i64)result);
			else if (t == SYMBOL) print(symbol_for(result));
			else if (t == BOOL) print((bool)result);
			else if (t == STRING) print('"', (const char*)result, '"');
			else if (t->kind() == KIND_LIST) display_native_list(t, (void*)result);
			println("");
		}
	}

	int execute(Value value, const Object& object) {
		auto main_jit = object.find(jasmine::global("_start"));
		if (main_jit) return ((i64(*)())main_jit)();
		return 1;
	}

	Value repl(ref<Env> global, Source& src, Function& mainfn) {
    	print("? ");
		auto view = src.expand(_stdin);
		auto tokens = lex(view);
		if (error_count()) return print_errors(_stdout, src), error();

		TokenView tview(tokens, src, true);
		Value program = parse(tview);
		if (error_count()) return print_errors(_stdout, src), error();

		prep(global, program);
		Value result = eval(global, program);
		if (error_count()) return print_errors(_stdout, src), error();

		if (!result.is_runtime()) {
			if (!result.is_void()) 
				println(BOLDBLUE, "= ", result, RESET, "\n");
			return result;
		}
		if (_print_ast) 
			println(BOLDCYAN, result.get_runtime(), RESET, "\n");

		jasmine::Object object;
		generate(result, mainfn);
		compile(result, object, mainfn);
		add_native_functions(object);
		object.load();
		if (error_count()) return print_errors(_stdout, src), error();

		print(BOLDBLUE);
		jit_print(result, object);
		println(RESET);
		return result;
	}

	ref<Env> load(Source& src) {
		auto view = src.begin();
		auto tokens = lex(view);
		if (error_count()) return print_errors(_stdout, src), nullptr;

		TokenView tview(tokens, src);
		Value program = parse(tview);
		if (error_count()) return print_errors(_stdout, src), nullptr;

		ref<Env> global = create_global_env();

		prep(global, program);
		Value result = eval(global, program);
		if (error_count()) return print_errors(_stdout, src), nullptr;

		return global;
	}

	int run(Source& src) {
		auto view = src.begin();
		auto tokens = lex(view);
		if (error_count()) return print_errors(_stdout, src), 1;

		TokenView tview(tokens, src);
		Value program = parse(tview);
		if (error_count()) return print_errors(_stdout, src), 1;

		ref<Env> global = create_global_env();

		prep(global, program);
		Value result = eval(global, program);
		if (error_count()) return print_errors(_stdout, src), 1;

		if (!result.is_runtime()) return 0;
		if (_print_ast) 
			println(BOLDCYAN, result.get_runtime(), RESET, "\n");

		jasmine::Object object;
		compile(result, object);
		add_native_functions(object);
		object.load();
		if (error_count()) return print_errors(_stdout, src), 1;

		return execute(result, object);
	}

	string change_ending(string s, string e) {
		string d = "";
		int last = 0;
		for (int i = 0; i < s.size(); i ++) if (s[i] == '.') last = i;
		for (int i = 0; i < last; i ++) d += s[i];
		return d + e;
	}

	vector<Value> instantiations(ref<Env> env) {
		vector<Value> insts;
		for (const auto& p : *env) {
			if (p.second.value.is_function()) {
				const FunctionValue& fn = p.second.value.get_function();
				if (fn.instantiations()) for (auto& i : *fn.instantiations()) {
					insts.push(i.second);
				}
			}
		}
		return insts;
	}

	int build(Source& src, const char* filename) {
		string dest = change_ending(filename, ".o");
		auto view = src.begin();
		auto tokens = lex(view);
		if (error_count()) return print_errors(_stdout, src), 1;

		TokenView tview(tokens, src);
		Value program = parse(tview);
		if (error_count()) return print_errors(_stdout, src), 1;

		ref<Env> global = create_global_env();

		prep(global, program);
		Value result = eval(global, program);
		if (error_count()) return print_errors(_stdout, src), 1;

		auto insts = instantiations(global);
		if (result.is_runtime()) {
			if (_print_ast) 
				println(BOLDCYAN, result.get_runtime(), RESET, "\n");

			jasmine::Object object;
			compile(result, object);
			if (error_count()) return print_errors(_stdout, src), 1;
			object.writeELF((const char*)dest.raw());
		}
		else if (insts.size() > 0) {
			jasmine::Object object;
			Function container(string(".") + filename);
			for (Value f : insts) f.get_runtime()->emit(container);
			if (_print_ir) {
				print(BOLDMAGENTA);
				print(container);
				println(RESET, "\n");
			}
			container.allocate();
			container.emit(object, true);
			emit_constants(object);
			if (error_count()) return print_errors(_stdout, src), 1;
			object.writeELF((const char*)dest.raw());
		}

		return 0;
	}
}
=======
    static bool _print_tokens = false, _print_parsed = false, _print_ast = false, _print_ir = false, _print_asm = false,
                _compile_only = false;

    void print_tokens(bool should) {
        _print_tokens = should;
    }

    void print_parsed(bool should) {
        _print_parsed = should;
    }

    void print_ast(bool should) {
        _print_ast = should;
    }

    void print_ir(bool should) {
        _print_ir = should;
    }

    void print_asm(bool should) {
        _print_asm = should;
    }

    void compile_only(bool should) {
        _compile_only = should;
    }

    bool is_compile_only() {
        return _compile_only;
    }

    vector<Token> lex(Source::View& view) {
        vector<Token> tokens;
        while (view.peek()) tokens.push(scan(view));

        if (_print_tokens) {
            print(BOLDYELLOW);
            for (const Token& tok : tokens) print(tok, " ");
            println(RESET, "\n");
        }
        return tokens;
    }

    Value parse(TokenView& view) {
        vector<Value> results;
        TokenView end = view;
        while (end) end.read();
        while (view.peek()) {
            Value line = parse_line(view, view.peek().column);
            if (!line.is_void()) results.push(line);
        }
        if (_print_tokens) {
            print(BOLDYELLOW);
            while (end) print(end.read(), " ");
            println(RESET);
        }
        if (_print_parsed) {
            print(BOLDGREEN);
            for (const Value& v : results) println(v);
            println(RESET);
        }
        return cons(string("do"), list_of(results));
    }

    ref<Env> create_global_env() {
        ref<Env> root = create_root_env();
        ref<Env> global_env = newref<Env>(root);
        return global_env;
    }

    void compile(Value value, Object& object, Function& fn) {
        fn.allocate();
        fn.emit(object);
        emit_constants(object);
        if (error_count()) return;

        if (_print_asm) {
            print(BOLDRED);
            byte_buffer code = object.code();
            while (code.size()) printf("%02x ", code.read());
            println(RESET, "\n");
        }
    }

    void generate(Value value, Function& fn) {
        Location last = loc_none();
        if (value.is_runtime()) last = value.get_runtime()->emit(fn);
        if (last.type != LOC_NONE) fn.add(new RetInsn(last));
        if (error_count()) return;

        if (_print_ir) {
            print(BOLDMAGENTA);
            print(fn);
            println(RESET, "\n");
        }
    }

    void compile(Value value, Object& object) {
        Function main_fn("_start");
        Location last = loc_none();
        if (value.is_runtime()) last = value.get_runtime()->emit(main_fn);
        if (last.type != LOC_NONE) main_fn.add(new RetInsn(last));
        if (error_count()) return;

        if (_print_ir) {
            print(BOLDMAGENTA);
            print(main_fn);
            println(RESET, "\n");
        }

        compile(value, object, main_fn);
    }

    void jit_print(Value value, const Object& object) {
        auto main_jit = object.find(jasmine::global("main"));
        if (main_jit) {
            u64 result = ((u64(*)())main_jit)();
            const Type* t = value.get_runtime()->type();
            if (t == VOID) return;
            print("= ");

            if (t == INT) print((i64)result);
            else if (t == SYMBOL)
                print(symbol_for(result));
            else if (t == BOOL)
                print((bool)result);
            else if (t == STRING)
                print('"', (const char*)result, '"');
            else if (t->kind() == KIND_LIST)
                display_native_list(t, (void*)result);
            println("");
        }
    }

    int execute(Value value, const Object& object) {
        auto main_jit = object.find(jasmine::global("_start"));
        if (main_jit) return ((i64(*)())main_jit)();
        return 1;
    }

    Value repl(ref<Env> global, Source& src, Function& mainfn) {
        print("? ");
        auto view = src.expand(_stdin);
        auto tokens = lex(view);
        if (error_count()) return print_errors(_stdout, src), error();

        TokenView tview(tokens, src, true);
        Value program = parse(tview);
        if (error_count()) return print_errors(_stdout, src), error();

        prep(global, program);
        Value result = eval(global, program);
        if (error_count()) return print_errors(_stdout, src), error();

        if (!result.is_runtime()) {
            if (!result.is_void()) println(BOLDBLUE, "= ", result, RESET, "\n");
            return result;
        }
        if (_print_ast) println(BOLDCYAN, result.get_runtime(), RESET, "\n");

        jasmine::Object object;
        generate(result, mainfn);
        compile(result, object, mainfn);
        add_native_functions(object);
        object.load();
        if (error_count()) return print_errors(_stdout, src), error();

        print(BOLDBLUE);
        jit_print(result, object);
        println(RESET);
        return result;
    }

    ref<Env> load(Source& src) {
        auto view = src.begin();
        auto tokens = lex(view);
        if (error_count()) return print_errors(_stdout, src), nullptr;

        TokenView tview(tokens, src);
        Value program = parse(tview);
        if (error_count()) return print_errors(_stdout, src), nullptr;

        ref<Env> global = create_global_env();

        prep(global, program);
        Value result = eval(global, program);
        if (error_count()) return print_errors(_stdout, src), nullptr;

        return global;
    }

    int run(Source& src) {
        auto view = src.begin();
        auto tokens = lex(view);
        if (error_count()) return print_errors(_stdout, src), 1;

        TokenView tview(tokens, src);
        Value program = parse(tview);
        if (error_count()) return print_errors(_stdout, src), 1;

        ref<Env> global = create_global_env();

        prep(global, program);
        Value result = eval(global, program);
        if (error_count()) return print_errors(_stdout, src), 1;

        if (!result.is_runtime()) return 0;
        if (_print_ast) println(BOLDCYAN, result.get_runtime(), RESET, "\n");

        jasmine::Object object;
        compile(result, object);
        add_native_functions(object);
        object.load();
        if (error_count()) return print_errors(_stdout, src), 1;

        return execute(result, object);
    }

    string change_ending(string s, string e) {
        string d = "";
        int last = 0;
        for (int i = 0; i < s.size(); i++)
            if (s[i] == '.') last = i;
        for (int i = 0; i < last; i++) d += s[i];
        return d + e;
    }

    vector<Value> instantiations(ref<Env> env) {
        vector<Value> insts;
        for (const auto& p : *env) {
            if (p.second.value.is_function()) {
                const FunctionValue& fn = p.second.value.get_function();
                if (fn.instantiations())
                    for (auto& i : *fn.instantiations()) { insts.push(i.second); }
            }
        }
        return insts;
    }

    int build(Source& src, const char* filename) {
        string dest = change_ending(filename, ".o");
        auto view = src.begin();
        auto tokens = lex(view);
        if (error_count()) return print_errors(_stdout, src), 1;

        TokenView tview(tokens, src);
        Value program = parse(tview);
        if (error_count()) return print_errors(_stdout, src), 1;

        ref<Env> global = create_global_env();

        prep(global, program);
        Value result = eval(global, program);
        if (error_count()) return print_errors(_stdout, src), 1;

        auto insts = instantiations(global);
        if (result.is_runtime()) {
            if (_print_ast) println(BOLDCYAN, result.get_runtime(), RESET, "\n");

            jasmine::Object object;
            compile(result, object);
            if (error_count()) return print_errors(_stdout, src), 1;
            object.writeELF((const char*)dest.raw());
        } else if (insts.size() > 0) {
            jasmine::Object object;
            Function container(string(".") + filename);
            for (Value f : insts) f.get_runtime()->emit(container);
            if (_print_ir) {
                print(BOLDMAGENTA);
                print(container);
                println(RESET, "\n");
            }
            container.allocate();
            container.emit(object, true);
            emit_constants(object);
            if (error_count()) return print_errors(_stdout, src), 1;
            object.writeELF((const char*)dest.raw());
        }

        return 0;
    }
} // namespace basil
>>>>>>> f840479bc314e660171a3eb91c404f37c4be4a33
