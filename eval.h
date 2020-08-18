#ifndef BASIL_EVAL_H
#define BASIL_EVAL_H

#include "defs.h"
#include "values.h"
#include "env.h"

namespace basil {
  ref<Env> create_root_env();

  Value eval(ref<Env> env, Value term);
}

#endif