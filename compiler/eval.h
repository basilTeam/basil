#ifndef BASIL_EVAL_H
#define BASIL_EVAL_H

#include "util/rc.h"
#include "env.h"
#include "value.h"

namespace basil {
    // Represents the result of grouping a source code term.
    struct GroupResult {
        rc<Env> env;
        Value value;
        list_iterator rest;
    };

    // Returns the next single (ungrouped) term from the front of 'term'.
    GroupResult next_term(rc<Env> env, const Value& term, list_iterator rest, list_iterator end);

    // Returns the next grouped expression from the front of 'term'.
    GroupResult next_group(rc<Env> env, const Value& term, list_iterator rest, list_iterator end);

    // Represents the result of evaluating a source code term. All evaluations
    // produce both an environment and a value. The value is whatever value we
    // get from evaluating a source code term. The environment 'env' is 
    // the environment resulting from any environmental changes the evaluation
    // produced, in which the next term may be evaluated.
    struct EvalResult {
        rc<Env> env;
        Value value;
    };

    // Evaluates the code value 'term' within the environment 'env'.
    EvalResult eval(rc<Env> env, Value& term);
}

#endif