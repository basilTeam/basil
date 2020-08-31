#include "driver.h"
#include "lex.h"
#include "parse.h"
#include "errors.h"
#include "values.h"
#include "native.h"
#include "eval.h"
#include "ssa.h"
#include "ast.h"
#include "util/io.h"

namespace basil {
	static bool _print_tokens = false,
		_print_parsed = false,
		_print_ast = false,
		_print_ssa = false,
		_print_asm = false;

	void print_tokens(bool should) {
		_print_tokens = should;
	}

	void print_parsed(bool should) {
		_print_parsed = should;
	}

	void print_ast(bool should) {
		_print_ast = should;
	}

	void print_ssa(bool should) {
		_print_ssa = should;
	}

	void print_asm(bool should) {
		_print_asm = should;
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
		while (view.peek()) {
			Value line = parse_line(view, view.peek().column);
			if (!line.is_void()) results.push(line);
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
		Env global_env(root);
		return global_env;
	}

	void compile(Value value, Object& object, Function& fn) {
		fn.allocate();
		fn.emit(object);
		ssa_emit_constants(object);
		if (error_count()) return;

		if (_print_asm) {
			print(BOLDRED);
			byte_buffer code = object.code();
			while (code.size()) printf("%02x ", code.read());
			println(RESET, "\n");
		}

		add_native_functions(object);

		object.load();
	}

	void generate(Value value, Function& fn) {
		Location last = ssa_none();
		if (value.is_runtime()) last = value.get_runtime()->emit(fn);
		if (last.type != SSA_NONE) fn.add(new RetInsn(last));
		if (error_count()) return;

		if (_print_ssa) {
			print(BOLDMAGENTA);
			print(fn);
			println(RESET, "\n");
		}
	}

	void compile(Value value, Object& object) {
		Function main_fn("main");
		Location last = ssa_none();
		if (value.is_runtime()) last = value.get_runtime()->emit(main_fn);
		if (last.type != SSA_NONE) main_fn.add(new RetInsn(last));
		if (error_count()) return;

		if (_print_ssa) {
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
		auto main_jit = object.find(jasmine::global("main"));
		if (main_jit) return ((i64(*)())main_jit)();
		return 1;
	}

	Value repl(ref<Env> global, Source& src, Function& mainfn) {
    print("? ");
		auto view = src.expand(_stdin);
		auto tokens = lex(view);
		if (error_count()) return print_errors(_stdout), error();

		TokenView tview(tokens, src, true);
		Value program = parse(tview);
		if (error_count()) return print_errors(_stdout), error();

		prep(global, program);
		Value result = eval(global, program);
		if (error_count()) return print_errors(_stdout), error();

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
		if (error_count()) return print_errors(_stdout), error();

		print(BOLDBLUE);
		jit_print(result, object);
		println(RESET);
		return result;
	}

	ref<Env> load(Source& src) {
		auto view = src.begin();
		auto tokens = lex(view);
		if (error_count()) return print_errors(_stdout), ref<Env>::null();

		TokenView tview(tokens, src);
		Value program = parse(tview);
		if (error_count()) return print_errors(_stdout), ref<Env>::null();

		ref<Env> global = create_global_env();

		prep(global, program);
		Value result = eval(global, program);
		if (error_count()) return print_errors(_stdout), ref<Env>::null();

		return global;
	}

	int run(Source& src) {
		auto view = src.begin();
		auto tokens = lex(view);
		if (error_count()) return print_errors(_stdout), 1;

		TokenView tview(tokens, src);
		Value program = parse(tview);
		if (error_count()) return print_errors(_stdout), 1;

		ref<Env> global = create_global_env();

		prep(global, program);
		Value result = eval(global, program);
		if (error_count()) return print_errors(_stdout), 1;

		if (!result.is_runtime()) return 0;
		if (_print_ast) 
			println(BOLDCYAN, result.get_runtime(), RESET, "\n");

		jasmine::Object object;
		compile(result, object);
		if (error_count()) return print_errors(_stdout), 1;

		return execute(result, object);
	}
}