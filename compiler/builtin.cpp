#include "builtin.h"
#include "ast.h"
#include "env.h"
#include "eval.h"
#include "type.h"
#include "values.h"

namespace basil {
    Builtin::Builtin() {}

    Builtin::Builtin(const Type* type, Function eval, Function compile, BuiltinFlags flags)
        : _type(type), _eval(eval), _compile(compile), _flags(flags) {}

    const Type* Builtin::type() const {
        return _type;
    }

    Value Builtin::eval(ref<Env> env, const Value& args) const {
        return _eval(env, args);
    }

    Value Builtin::compile(ref<Env> env, const Value& args) const {
        return _compile(env, args);
    }

    bool Builtin::should_lower() const {
        return !(_flags & NO_AUTO_LOWER);
    }

    bool Builtin::runtime_only() const {
        return !_eval;
    }

#define ARG(n) args.get_product()[n]

    Builtin ADD_INT, ADD_SYMBOL, SUB, MUL, DIV, REM,
        AND, OR, XOR, NOT,
        EQUALS, NOT_EQUALS, LESS, GREATER, LESS_EQUAL, GREATER_EQUAL,
        IS_EMPTY, HEAD, TAIL, CONS, DISPLAY, READ_LINE, READ_WORD, READ_INT, LENGTH,
        AT_INT, AT_LIST, AT_ARRAY_TYPE, AT_DYNARRAY_TYPE, AT_MODULE, STRCAT, SUBSTR, ANNOTATE, TYPEOF, LIST_TYPE, OF_TYPE_MACRO,
        OF_TYPE, ASSIGN, IF;

    static void init_builtins() {
        ADD_INT = {find<FunctionType>(find<ProductType>(INT, INT), INT),
               [](ref<Env> env, const Value& args) -> Value { return Value(ARG(0).get_int() + ARG(1).get_int()); },
               [](ref<Env> env, const Value& args) -> Value {
                   return Value(new ASTBinaryMath(ARG(0).loc(), AST_ADD, ARG(0).get_runtime(), ARG(1).get_runtime()));
               }};
        ADD_SYMBOL = {find<FunctionType>(find<ProductType>(SYMBOL, SYMBOL), SYMBOL),
                [](ref<Env> env, const Value& args) -> Value {
                    return Value(symbol_for(ARG(0).get_symbol()) + symbol_for(ARG(1).get_symbol()), SYMBOL);
                },
                nullptr};
        SUB = {find<FunctionType>(find<ProductType>(INT, INT), INT),
               [](ref<Env> env, const Value& args) -> Value { return Value(ARG(0).get_int() - ARG(1).get_int()); },
               [](ref<Env> env, const Value& args) -> Value {
                   return Value(new ASTBinaryMath(ARG(0).loc(), AST_SUB, ARG(0).get_runtime(), ARG(1).get_runtime()));
               }};
        MUL = {find<FunctionType>(find<ProductType>(INT, INT), INT),
               [](ref<Env> env, const Value& args) -> Value { return Value(ARG(0).get_int() * ARG(1).get_int()); },
               [](ref<Env> env, const Value& args) -> Value {
                   return Value(new ASTBinaryMath(ARG(0).loc(), AST_MUL, ARG(0).get_runtime(), ARG(1).get_runtime()));
               }};
        DIV = {find<FunctionType>(find<ProductType>(INT, INT), INT),
               [](ref<Env> env, const Value& args) -> Value { return Value(ARG(0).get_int() / ARG(1).get_int()); },
               [](ref<Env> env, const Value& args) -> Value {
                   return Value(new ASTBinaryMath(ARG(0).loc(), AST_DIV, ARG(0).get_runtime(), ARG(1).get_runtime()));
               }};
        REM = {find<FunctionType>(find<ProductType>(INT, INT), INT),
               [](ref<Env> env, const Value& args) -> Value { return Value(ARG(0).get_int() % ARG(1).get_int()); },
               [](ref<Env> env, const Value& args) -> Value {
                   return Value(new ASTBinaryMath(ARG(0).loc(), AST_REM, ARG(0).get_runtime(), ARG(1).get_runtime()));
               }};
        AND = {find<FunctionType>(find<ProductType>(BOOL, BOOL), BOOL),
               [](ref<Env> env, const Value& args) -> Value {
                   return Value(ARG(0).get_bool() && ARG(1).get_bool(), BOOL);
               },
               [](ref<Env> env, const Value& args) -> Value {
                   return Value(new ASTBinaryLogic(ARG(0).loc(), AST_AND, ARG(0).get_runtime(), ARG(1).get_runtime()));
               }};
        OR = {find<FunctionType>(find<ProductType>(BOOL, BOOL), BOOL),
              [](ref<Env> env, const Value& args) -> Value {
                  return Value(ARG(0).get_bool() || ARG(1).get_bool(), BOOL);
              },
              [](ref<Env> env, const Value& args) -> Value {
                  return Value(new ASTBinaryLogic(ARG(0).loc(), AST_OR, ARG(0).get_runtime(), ARG(1).get_runtime()));
              }};
        XOR = {
            find<FunctionType>(find<ProductType>(BOOL, BOOL), BOOL),
            [](ref<Env> env, const Value& args) -> Value { return Value(ARG(0).get_bool() ^ ARG(1).get_bool(), BOOL); },
            [](ref<Env> env, const Value& args) -> Value {
                return Value(new ASTBinaryLogic(ARG(0).loc(), AST_XOR, ARG(0).get_runtime(), ARG(1).get_runtime()));
            }};
        NOT = {find<FunctionType>(find<ProductType>(BOOL), BOOL),
               [](ref<Env> env, const Value& args) -> Value { return Value(!ARG(0).get_bool(), BOOL); },
               [](ref<Env> env, const Value& args) -> Value {
                   return Value(new ASTNot(ARG(0).loc(), ARG(0).get_runtime()));
               }};
        EQUALS = {find<FunctionType>(find<ProductType>(ANY, ANY), BOOL),
                  [](ref<Env> env, const Value& args) -> Value { return Value(ARG(0) == ARG(1), BOOL); },
                  [](ref<Env> env, const Value& args) -> Value {
                      return Value(
                          new ASTBinaryEqual(ARG(0).loc(), AST_EQUAL, ARG(0).get_runtime(), ARG(1).get_runtime()));
                  }};
        NOT_EQUALS = {
            find<FunctionType>(find<ProductType>(ANY, ANY), BOOL),
            [](ref<Env> env, const Value& args) -> Value { return Value(ARG(0) != ARG(1), BOOL); },
            [](ref<Env> env, const Value& args) -> Value {
                return Value(new ASTBinaryEqual(ARG(0).loc(), AST_INEQUAL, ARG(0).get_runtime(), ARG(1).get_runtime()));
            }};
        LESS = {
            find<FunctionType>(find<ProductType>(INT, INT), BOOL),
            [](ref<Env> env, const Value& args) -> Value { return Value(ARG(0).get_int() < ARG(1).get_int(), BOOL); },
            [](ref<Env> env, const Value& args) -> Value {
                return Value(new ASTBinaryRel(ARG(0).loc(), AST_LESS, ARG(0).get_runtime(), ARG(1).get_runtime()));
            }
        };
        LESS_EQUAL = {
            find<FunctionType>(find<ProductType>(INT, INT), BOOL),
            [](ref<Env> env, const Value& args) -> Value { return Value(ARG(0).get_int() <= ARG(1).get_int(), BOOL); },
            [](ref<Env> env, const Value& args) -> Value {
                return Value(
                    new ASTBinaryRel(ARG(0).loc(), AST_LESS_EQUAL, ARG(0).get_runtime(), ARG(1).get_runtime()));
            }
        };
        GREATER = {
            find<FunctionType>(find<ProductType>(INT, INT), BOOL),
            [](ref<Env> env, const Value& args) -> Value { return Value(ARG(0).get_int() > ARG(1).get_int(), BOOL); },
            [](ref<Env> env, const Value& args) -> Value {
                return Value(new ASTBinaryRel(ARG(0).loc(), AST_GREATER, ARG(0).get_runtime(), ARG(1).get_runtime()));
            }
        };
        GREATER_EQUAL = {
            find<FunctionType>(find<ProductType>(INT, INT), BOOL),
            [](ref<Env> env, const Value& args) -> Value { return Value(ARG(0).get_int() >= ARG(1).get_int(), BOOL); },
            [](ref<Env> env, const Value& args) -> Value {
                return Value(
                    new ASTBinaryRel(ARG(0).loc(), AST_GREATER_EQUAL, ARG(0).get_runtime(), ARG(1).get_runtime()));
            }
        };
        DISPLAY = {
            find<FunctionType>(find<ProductType>(ANY), VOID),
            nullptr,
            [](ref<Env> env, const Value& args) -> Value {
                return Value(new ASTDisplay(ARG(0).loc(), ARG(0).get_runtime()));
            }
        };
        AT_INT = {
            find<FunctionType>(find<ProductType>(ANY, INT), ANY),
            [](ref<Env> env, const Value& args) -> Value {
                if (ARG(0).is_string()) return Value(ARG(0).get_string()[ARG(1).get_int()], INT);
                if (ARG(0).is_product()) return ARG(0).get_product()[ARG(1).get_int()];
                if (ARG(0).is_array()) return ARG(0).get_array()[ARG(1).get_int()];
                err(ARG(0).loc(), "Cannot index into value of type '", ARG(0).type(), "'.");
                return error();
            },
            nullptr // todo: runtime indexing
        };
        AT_LIST = {
            find<FunctionType>(find<ProductType>(ANY, find<ListType>(INT)), ANY),
            [](ref<Env> env, const Value& args) -> Value {
                if (ARG(0).is_string()) {
                    string result = "";
                    Value indexlist = ARG(1);
                    while (!indexlist.is_void()) {
                        result += ARG(0).get_string()[head(indexlist).get_int()];
                        indexlist = tail(indexlist);
                    }
                    return Value(result, STRING);
                }
                if (ARG(0).is_product()) {
                    vector<Value> values;
                    Value indexlist = ARG(1);
                    while (!indexlist.is_void()) {
                        values.push(ARG(0).get_product()[head(indexlist).get_int()]);
                        indexlist = tail(indexlist);
                    }
                    return Value(new ProductValue(values));
                }
                if (ARG(0).is_array()) {
                    vector<Value> values;
                    Value indexlist = ARG(1);
                    while (!indexlist.is_void()) {
                        values.push(ARG(0).get_array()[head(indexlist).get_int()]);
                        indexlist = tail(indexlist);
                    }
                    return Value(new ArrayValue(values));
                }
                err(ARG(0).loc(), "Cannot index into value of type '", ARG(0).type(), "'.");
                return error();
            },
            nullptr // todo: runtime indexing
        };
        AT_ARRAY_TYPE = {
            find<FunctionType>(find<ProductType>(TYPE, INT), TYPE),
            [](ref<Env> env, const Value& args) -> Value {
                return Value(find<ArrayType>(ARG(0).get_type(), ARG(1).get_int()), TYPE);
            },
            nullptr
        };
        AT_DYNARRAY_TYPE = {
            find<FunctionType>(find<ProductType>(TYPE, VOID), TYPE),
            [](ref<Env> env, const Value& args) -> Value {
                return Value(find<ArrayType>(ARG(0).get_type()), TYPE);
            },
            nullptr
        };
        AT_MODULE = {
            find<FunctionType>(find<ProductType>(MODULE, SYMBOL), ANY),
            [](ref<Env> env, const Value& args) -> Value {
                if (!ARG(0).get_module().has(ARG(1).get_symbol())) {
                    err(ARG(1).loc(), "Module does not contain member '",
                        symbol_for(ARG(1).get_symbol()), "'.");
                    return error();
                }
                return ARG(0).get_module().entry(ARG(1).get_symbol());
            },
            nullptr
        };
        ANNOTATE = {
            find<FunctionType>(find<ProductType>(ANY, TYPE), ANY),
            [](ref<Env> env, const Value& args) -> Value {
                if (!ARG(0).type()->coerces_to(ARG(1).get_type())) {
                    err(ARG(0).loc(), "Could not unify value of type '", ARG(0).type(), "' with type '",
                        ARG(1).get_type(), "'.");
                    return error();
                }
                return cast(ARG(0), ARG(1).get_type());
            },
            [](ref<Env> env, const Value& args) -> Value {
                return new ASTAnnotate(ARG(0).loc(), lower(ARG(0)).get_runtime(), ARG(1).get_type());
            },
            NO_AUTO_LOWER
        };
        TYPEOF = {find<FunctionType>(find<ProductType>(ANY), TYPE),
                  [](ref<Env> env, const Value& args) -> Value { return Value(ARG(0).type(), TYPE); }, nullptr};
        LIST_TYPE = {
            find<FunctionType>(find<ProductType>(TYPE), TYPE),
            [](ref<Env> env, const Value& args) -> Value { return Value(find<ListType>(ARG(0).get_type()), TYPE); },
            nullptr};
        OF_TYPE_MACRO = {
            find<MacroType>(2),
            [](ref<Env> env, const Value& args) -> Value {
                return list_of(Value("#of", SYMBOL), list_of(Value("quote", SYMBOL), ARG(0)), ARG(1));
            },
            nullptr
        };
        OF_TYPE = {
            find<FunctionType>(find<ProductType>(SYMBOL, TYPE), TYPE),
            [](ref<Env> env, const Value& args) -> Value {
                return Value(find<NamedType>(symbol_for(ARG(0).get_symbol()), ARG(1).get_type()), TYPE);
            },
            nullptr
        };
        IF = {
            find<FunctionType>(find<ProductType>(BOOL, ANY, ANY), ANY),
            [](ref<Env> env, const Value& args) -> Value { return eval(env, ARG(ARG(0).get_bool() ? 1 : 2)); },
            [](ref<Env> env, const Value& args) -> Value {
                Value left = eval(env, ARG(1)), right = eval(env, ARG(2));
                if (left.is_error() || right.is_error()) return error();
                if (!left.is_runtime()) left = lower(left);
                if (!right.is_runtime()) right = lower(right);
                return new ASTIf(ARG(0).loc(), ARG(0).get_runtime(), left.get_runtime(), right.get_runtime());
            },
            NO_AUTO_LOWER
        };
    }

    static bool inited = false;

    template <typename... Args>
    Value cases(ref<Env> env, const Args... args) {
        vector<Builtin*> vals = vector_of<Builtin*>(args...);
        set<const Type*> ts;
        map<const Type*, Value> m;
        for (Builtin* v : vals) ts.insert(v->type()), m.put(v->type(), Value(env, *v));
        return Value(new IntersectValue(m), find<IntersectType>(ts));
    }

    void define_builtins(ref<Env> env) {
        if (!inited) inited = true, init_builtins();
        env->infix("+", cases(env, &ADD_INT, &ADD_SYMBOL), 2, 20);
        env->infix("-", Value(env, SUB), 2, 20);
        env->infix("*", Value(env, MUL), 2, 40);
        env->infix("/", Value(env, DIV), 2, 40);
        env->infix("%", Value(env, REM), 2, 40);
        env->infix("and", Value(env, AND), 2, 5);
        env->infix("or", Value(env, OR), 2, 5);
        env->infix("xor", Value(env, XOR), 2, 5);
        env->def("not", Value(env, NOT), 1);
        env->infix("==", Value(env, EQUALS), 2, 10);
        env->infix("!=", Value(env, NOT_EQUALS), 2, 10);
        env->infix("<", Value(env, LESS), 2, 10);
        env->infix(">", Value(env, GREATER), 2, 10);
        env->infix("<=", Value(env, LESS_EQUAL), 2, 10);
        env->infix(">=", Value(env, GREATER_EQUAL), 2, 10);
        env->def("display", Value(env, DISPLAY), 1);
        env->infix("at", cases(env, &AT_INT, &AT_LIST, &AT_ARRAY_TYPE, &AT_DYNARRAY_TYPE, &AT_MODULE), 2, 120);
        env->def("annotate", Value(env, ANNOTATE), 2);
        env->def("typeof", Value(env, TYPEOF), 1);
        env->infix_macro("of", Value(env, OF_TYPE_MACRO), 2, 20);
        env->def("#of", Value(env, OF_TYPE), 2);
        env->infix("list", Value(env, LIST_TYPE), 1, 80);
        env->infix("#?", Value(env, IF), 3, 2);
    }
} // namespace basil