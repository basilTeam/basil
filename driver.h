#ifndef BASIL_DRIVER_H
#define BASIL_DRIVER_H

#include "source.h"
#include "env.h"
#include "ssa.h"
#include "values.h"

namespace basil {
	void print_tokens(bool should);
	void print_parsed(bool should);
	void print_ast(bool should);
	void print_ssa(bool should);
	void print_asm(bool should);

	Value repl(ref<Env> global, Source& src, Function& mainfn);
	ref<Env> load(Source& src);
	int run(Source& src);
}

#endif