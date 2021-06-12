#include "builtin.h"
#include "env.h"
#include "eval.h"
#include "forms.h"

namespace basil {
    Builtin ADD, SUB, MUL;

    bool builtins_inited = false;

    void init_builtins() {
        ADD = {
            t_func(t_tuple(T_INT, T_INT), T_INT), // type
            f_callable(20, ASSOC_LEFT, P_VAR, P_SELF, P_VAR), // form
            [](rc<Env> env, const Value& args) -> EvalResult {
                return {
                    env,
                    v_int({}, v_tuple_at(args, 0).data.i + v_tuple_at(args, 1).data.i)
                };
            },
            nullptr
        };
        SUB = {
            t_func(t_tuple(T_INT, T_INT), T_INT), // type
            f_callable(20, ASSOC_LEFT, P_VAR, P_SELF, P_VAR), // form
            [](rc<Env> env, const Value& args) -> EvalResult {
                return {
                    env,
                    v_int({}, v_tuple_at(args, 0).data.i - v_tuple_at(args, 1).data.i)
                };
            },
            nullptr
        };
        MUL = {
            t_func(t_tuple(T_INT, T_INT), T_INT), // type
            f_callable(40, ASSOC_LEFT, P_VAR, P_SELF, P_VAR), // form
            [](rc<Env> env, const Value& args) -> EvalResult {
                return {
                    env,
                    v_int({}, v_tuple_at(args, 0).data.i * v_tuple_at(args, 1).data.i)
                };
            },
            nullptr
        };
    }

    void add_builtins(rc<Env> env) {
        if (!builtins_inited) builtins_inited = true, init_builtins();

        env->def(symbol_from("+"), v_func(ADD));
        env->def(symbol_from("-"), v_func(SUB));
        env->def(symbol_from("*"), v_func(MUL));
    }
}