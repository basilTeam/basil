#include "builtin.h"
#include "type.h"
#include "values.h"
#include "ast.h"
#include "env.h"

namespace basil {
    Builtin::Builtin() {}

    Builtin::Builtin(const Type* type, Function eval, Function compile):
        _type(type), _eval(eval), _compile(compile) {}

    const Type* Builtin::type() const {
        return _type;
    }

    Value Builtin::eval(ref<Env> env, const Value& args) const {
        return _eval(env, args);
    }

    Value Builtin::compile(ref<Env> env, const Value& args) const {
        return _compile(env, args);
    }

    #define ARG(n) args.get_product()[n]

    Builtin
        ADD, SUB, MUL, DIV, REM,
        AND, OR, XOR, NOT,
        EQUALS, NOT_EQUALS, LESS, GREATER, LESS_EQUAL, GREATER_EQUAL,
        IS_EMPTY, HEAD, TAIL, CONS,
        DISPLAY, READ_LINE, READ_WORD, READ_INT,
        LENGTH, AT, STRCAT, SUBSTR,
        ANNOTATE, TYPEOF, LIST_TYPE,
        ASSIGN, IF;

    static void init_builtins() {
        ADD = {
            find<FunctionType>(find<ProductType>(INT, INT), INT),
            [](ref<Env> env, const Value& args) -> Value {
                return Value(ARG(0).get_int() + ARG(1).get_int());
            },
            [](ref<Env> env, const Value& args) -> Value {
                return Value(new ASTBinaryMath(ARG(0).loc(), AST_ADD,
                    ARG(0).get_runtime(), ARG(1).get_runtime()));
            }
        };
        SUB = {
            find<FunctionType>(find<ProductType>(INT, INT), INT),
            [](ref<Env> env, const Value& args) -> Value {
                return Value(ARG(0).get_int() - ARG(1).get_int());
            },
            [](ref<Env> env, const Value& args) -> Value {
                return Value(new ASTBinaryMath(ARG(0).loc(), AST_SUB,
                    ARG(0).get_runtime(), ARG(1).get_runtime()));
            }
        };
        MUL = {
            find<FunctionType>(find<ProductType>(INT, INT), INT),
            [](ref<Env> env, const Value& args) -> Value {
                return Value(ARG(0).get_int() * ARG(1).get_int());
            },
            [](ref<Env> env, const Value& args) -> Value {
                return Value(new ASTBinaryMath(ARG(0).loc(), AST_MUL,
                    ARG(0).get_runtime(), ARG(1).get_runtime()));
            }
        };
        DIV = {
            find<FunctionType>(find<ProductType>(INT, INT), INT),
            [](ref<Env> env, const Value& args) -> Value {
                return Value(ARG(0).get_int() / ARG(1).get_int());
            },
            [](ref<Env> env, const Value& args) -> Value {
                return Value(new ASTBinaryMath(ARG(0).loc(), AST_DIV,
                    ARG(0).get_runtime(), ARG(1).get_runtime()));
            }
        };
        REM = {
            find<FunctionType>(find<ProductType>(INT, INT), INT),
            [](ref<Env> env, const Value& args) -> Value {
                return Value(ARG(0).get_int() % ARG(1).get_int());
            },
            [](ref<Env> env, const Value& args) -> Value {
                return Value(new ASTBinaryMath(ARG(0).loc(), AST_REM,
                    ARG(0).get_runtime(), ARG(1).get_runtime()));
            }
        };
        AND = {
            find<FunctionType>(find<ProductType>(BOOL, BOOL), BOOL),
            [](ref<Env> env, const Value& args) -> Value {
                return Value(ARG(0).get_bool() && ARG(1).get_bool(), BOOL);
            },
            [](ref<Env> env, const Value& args) -> Value {
                return Value(new ASTBinaryLogic(ARG(0).loc(), AST_AND,
                    ARG(0).get_runtime(), ARG(1).get_runtime()));
            }
        };
        OR = {
            find<FunctionType>(find<ProductType>(BOOL, BOOL), BOOL),
            [](ref<Env> env, const Value& args) -> Value {
                return Value(ARG(0).get_bool() || ARG(1).get_bool(), BOOL);
            },
            [](ref<Env> env, const Value& args) -> Value {
                return Value(new ASTBinaryLogic(ARG(0).loc(), AST_OR,
                    ARG(0).get_runtime(), ARG(1).get_runtime()));
            }
        };
        XOR = {
            find<FunctionType>(find<ProductType>(BOOL, BOOL), BOOL),
            [](ref<Env> env, const Value& args) -> Value {
                return Value(ARG(0).get_bool() ^ ARG(1).get_bool(), BOOL);
            },
            [](ref<Env> env, const Value& args) -> Value {
                return Value(new ASTBinaryLogic(ARG(0).loc(), AST_XOR,
                    ARG(0).get_runtime(), ARG(1).get_runtime()));
            }
        };
        NOT = {
            find<FunctionType>(find<ProductType>(BOOL), BOOL),
            [](ref<Env> env, const Value& args) -> Value {
                return Value(!ARG(0).get_bool(), BOOL);
            },
            [](ref<Env> env, const Value& args) -> Value {
                return Value(new ASTNot(ARG(0).loc(), ARG(0).get_runtime()));
            }
        };
        TYPEOF = {
            find<FunctionType>(find<ProductType>(ANY), TYPE),
            [](ref<Env> env, const Value& args) -> Value {
                return Value(ARG(0).type(), TYPE);
            },
            nullptr
        };
        LIST_TYPE = {
            find<FunctionType>(find<ProductType>(TYPE), TYPE),
            [](ref<Env> env, const Value& args) -> Value {
                return Value(find<ListType>(ARG(0).get_type()), TYPE);
            },
            nullptr
        };
    }

    static bool inited = false;

    void define_builtins(ref<Env> env) {
        if (!inited) inited = true, init_builtins();
        env->infix("+", Value(env, ADD), 2, 20);
        env->infix("-", Value(env, SUB), 2, 20);
        env->infix("*", Value(env, MUL), 2, 40);
        env->infix("/", Value(env, DIV), 2, 40);
        env->infix("%", Value(env, REM), 2, 40);
        env->infix("and", Value(env, AND), 2, 5);
        env->infix("or", Value(env, OR), 2, 5);
        env->infix("xor", Value(env, XOR), 2, 5);
        env->def("not", Value(env, NOT), 1);
        env->def("typeof", Value(env, TYPEOF), 1);
        env->infix("list", Value(env, LIST_TYPE), 1, 80);
    }
}