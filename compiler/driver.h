#ifndef BASIL_DRIVER_H
#define BASIL_DRIVER_H

#include "source.h"
#include "env.h"
#include "ir.h"
#include "values.h"

namespace basil {
	void print_tokens(bool should);
	void print_parsed(bool should);
	void print_ast(bool should);
	void print_ir(bool should);
	void print_asm(bool should);
	void compile_only(bool should);
	bool is_compile_only();

	Value repl(ref<Env> global, Source& src, Function& mainfn);
	ref<Env> load(Source& src);
	int run(Source& src);
	int build(Source& src, const char* dest);
}

#endif