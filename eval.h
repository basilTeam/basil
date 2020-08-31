#ifndef BASIL_EVAL_H
#define BASIL_EVAL_H

#include "util/defs.h"
#include "values.h"
#include "env.h"

namespace basil {
  ref<Env> create_root_env();

	bool introduces_env(const Value& list);

	void prep(ref<Env> env, Value& term);
  Value eval(ref<Env> env, Value term);
}

#endif