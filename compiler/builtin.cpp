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

    Builtin::Builtin(const Type* type, Function eval, Function compile, const vector<u64>& args, BuiltinFlags flags)
        : _type(type), _eval(eval), _compile(compile), _flags(flags), _args(args) {}

    const Type* Builtin::type() const {
        return _type;
    }

    const vector<u64>& Builtin::args() const {
        return _args;
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

<<<<<<< HEAD
    Builtin ADD_INT, ADD_SYMBOL, SUB, MUL, DIV, REM,
        AND, OR, XOR, NOT,
        EQUALS, NOT_EQUALS, LESS, GREATER, LESS_EQUAL, GREATER_EQUAL,
        IS_EMPTY, HEAD, TAIL, CONS, DICT_IN, DICT_NOT_IN,  
        DISPLAY, READ_LINE, READ_WORD, READ_INT, LENGTH,
        AT_INT, AT_LIST, AT_ARRAY_TYPE, AT_DYNARRAY_TYPE, AT_MODULE, AT_DICT, AT_DICT_TYPE, AT_DICT_LIST,
        STRCAT, SUBSTR, ANNOTATE, TYPEOF, LIST_TYPE, OF_TYPE_MACRO,
        OF_TYPE, ASSIGN, IF,
        EVAL, FLAG;
=======
    Builtin ADD_INT, ADD_SYMBOL, SUB, MUL, DIV, REM, AND, OR, XOR, NOT, EQUALS, NOT_EQUALS, LESS, GREATER, LESS_EQUAL,
        GREATER_EQUAL, IS_EMPTY, HEAD, TAIL, CONS, DICT_IN, DICT_NOT_IN, DISPLAY, READ_LINE, READ_WORD, READ_INT,
        LENGTH, AT_INT, AT_LIST, AT_ARRAY_TYPE, AT_DYNARRAY_TYPE, AT_MODULE, AT_DICT, AT_DICT_TYPE, AT_DICT_LIST,
        STRCAT, SUBSTR, ANNOTATE, TYPEOF, TYPEDEF, LIST_TYPE, OF_TYPE_MACRO, OF_TYPE, ASSIGN, IF;
>>>>>>> f840479bc314e660171a3eb91c404f37c4be4a33

    static void init_builtins() {
        ADD_INT = { find<FunctionType>(find<ProductType>(INT, INT), INT),
                    [](ref<Env> env, const Value& args) -> Value { return Value(ARG(0).get_int() + ARG(1).get_int()); },
                    [](ref<Env> env, const Value& args) -> Value {
                        return Value(
                            new ASTBinaryMath(ARG(0).loc(), AST_ADD, ARG(0).get_runtime(), ARG(1).get_runtime()));
                    } };
        ADD_SYMBOL = { find<FunctionType>(find<ProductType>(SYMBOL, SYMBOL), SYMBOL),
                       [](ref<Env> env, const Value& args) -> Value {
                           return Value(symbol_for(ARG(0).get_symbol()) + symbol_for(ARG(1).get_symbol()), SYMBOL);
                       },
                       nullptr };
        SUB = { find<FunctionType>(find<ProductType>(INT, INT), INT),
                [](ref<Env> env, const Value& args) -> Value { return Value(ARG(0).get_int() - ARG(1).get_int()); },
                [](ref<Env> env, const Value& args) -> Value {
                    return Value(new ASTBinaryMath(ARG(0).loc(), AST_SUB, ARG(0).get_runtime(), ARG(1).get_runtime()));
                } };
        MUL = { find<FunctionType>(find<ProductType>(INT, INT), INT),
                [](ref<Env> env, const Value& args) -> Value { return Value(ARG(0).get_int() * ARG(1).get_int()); },
                [](ref<Env> env, const Value& args) -> Value {
                    return Value(new ASTBinaryMath(ARG(0).loc(), AST_MUL, ARG(0).get_runtime(), ARG(1).get_runtime()));
                } };
        DIV = { find<FunctionType>(find<ProductType>(INT, INT), INT),
                [](ref<Env> env, const Value& args) -> Value { return Value(ARG(0).get_int() / ARG(1).get_int()); },
                [](ref<Env> env, const Value& args) -> Value {
                    return Value(new ASTBinaryMath(ARG(0).loc(), AST_DIV, ARG(0).get_runtime(), ARG(1).get_runtime()));
                } };
        REM = { find<FunctionType>(find<ProductType>(INT, INT), INT),
                [](ref<Env> env, const Value& args) -> Value { return Value(ARG(0).get_int() % ARG(1).get_int()); },
                [](ref<Env> env, const Value& args) -> Value {
                    return Value(new ASTBinaryMath(ARG(0).loc(), AST_REM, ARG(0).get_runtime(), ARG(1).get_runtime()));
                } };
        AND = { find<FunctionType>(find<ProductType>(BOOL, BOOL), BOOL),
                [](ref<Env> env, const Value& args) -> Value {
                    return Value(ARG(0).get_bool() && ARG(1).get_bool(), BOOL);
                },
                [](ref<Env> env, const Value& args) -> Value {
                    return Value(new ASTBinaryLogic(ARG(0).loc(), AST_AND, ARG(0).get_runtime(), ARG(1).get_runtime()));
                } };
        OR = { find<FunctionType>(find<ProductType>(BOOL, BOOL), BOOL),
               [](ref<Env> env, const Value& args) -> Value {
                   return Value(ARG(0).get_bool() || ARG(1).get_bool(), BOOL);
               },
               [](ref<Env> env, const Value& args) -> Value {
                   return Value(new ASTBinaryLogic(ARG(0).loc(), AST_OR, ARG(0).get_runtime(), ARG(1).get_runtime()));
               } };
        XOR = { find<FunctionType>(find<ProductType>(BOOL, BOOL), BOOL),
                [](ref<Env> env, const Value& args) -> Value {
                    return Value(ARG(0).get_bool() ^ ARG(1).get_bool(), BOOL);
                },
                [](ref<Env> env, const Value& args) -> Value {
                    return Value(new ASTBinaryLogic(ARG(0).loc(), AST_XOR, ARG(0).get_runtime(), ARG(1).get_runtime()));
                } };
        NOT = { find<FunctionType>(find<ProductType>(BOOL), BOOL),
                [](ref<Env> env, const Value& args) -> Value { return Value(!ARG(0).get_bool(), BOOL); },
                [](ref<Env> env, const Value& args) -> Value {
                    return Value(new ASTNot(ARG(0).loc(), ARG(0).get_runtime()));
                } };
        EQUALS = { find<FunctionType>(find<ProductType>(ANY, ANY), BOOL),
                   [](ref<Env> env, const Value& args) -> Value { return Value(ARG(0) == ARG(1), BOOL); },
                   [](ref<Env> env, const Value& args) -> Value {
                       return Value(
                           new ASTBinaryEqual(ARG(0).loc(), AST_EQUAL, ARG(0).get_runtime(), ARG(1).get_runtime()));
                   } };
        NOT_EQUALS = { find<FunctionType>(find<ProductType>(ANY, ANY), BOOL),
                       [](ref<Env> env, const Value& args) -> Value { return Value(ARG(0) != ARG(1), BOOL); },
                       [](ref<Env> env, const Value& args) -> Value {
                           return Value(new ASTBinaryEqual(ARG(0).loc(), AST_INEQUAL, ARG(0).get_runtime(),
                                                           ARG(1).get_runtime()));
                       } };
        LESS = { find<FunctionType>(find<ProductType>(INT, INT), BOOL),
                 [](ref<Env> env, const Value& args) -> Value {
                     return Value(ARG(0).get_int() < ARG(1).get_int(), BOOL);
                 },
                 [](ref<Env> env, const Value& args) -> Value {
                     return Value(new ASTBinaryRel(ARG(0).loc(), AST_LESS, ARG(0).get_runtime(), ARG(1).get_runtime()));
                 } };
        LESS_EQUAL = { find<FunctionType>(find<ProductType>(INT, INT), BOOL),
                       [](ref<Env> env, const Value& args) -> Value {
                           return Value(ARG(0).get_int() <= ARG(1).get_int(), BOOL);
                       },
                       [](ref<Env> env, const Value& args) -> Value {
                           return Value(new ASTBinaryRel(ARG(0).loc(), AST_LESS_EQUAL, ARG(0).get_runtime(),
                                                         ARG(1).get_runtime()));
                       } };
        GREATER = {
            find<FunctionType>(find<ProductType>(INT, INT), BOOL),
            [](ref<Env> env, const Value& args) -> Value { return Value(ARG(0).get_int() > ARG(1).get_int(), BOOL); },
            [](ref<Env> env, const Value& args) -> Value {
                return Value(new ASTBinaryRel(ARG(0).loc(), AST_GREATER, ARG(0).get_runtime(), ARG(1).get_runtime()));
            }
        };
        GREATER_EQUAL = { find<FunctionType>(find<ProductType>(INT, INT), BOOL),
                          [](ref<Env> env, const Value& args) -> Value {
                              return Value(ARG(0).get_int() >= ARG(1).get_int(), BOOL);
                          },
                          [](ref<Env> env, const Value& args) -> Value {
                              return Value(new ASTBinaryRel(ARG(0).loc(), AST_GREATER_EQUAL, ARG(0).get_runtime(),
                                                            ARG(1).get_runtime()));
                          } };
        DICT_IN = { find<FunctionType>(find<ProductType>(ANY, find<DictType>(ANY, ANY)), BOOL),
                    [](ref<Env> env, const Value& args) -> Value {
                        return Value(ARG(1).get_dict()[ARG(0)] ? true : false, BOOL);
                    },
                    nullptr };
        DICT_NOT_IN = { find<FunctionType>(find<ProductType>(ANY, find<DictType>(ANY, ANY)), BOOL),
                        [](ref<Env> env, const Value& args) -> Value {
                            return Value(ARG(1).get_dict()[ARG(0)] ? false : true, BOOL);
                        },
                        nullptr };
        DISPLAY = { find<FunctionType>(find<ProductType>(ANY), VOID), nullptr,
                    [](ref<Env> env, const Value& args) -> Value {
                        return Value(new ASTDisplay(ARG(0).loc(), ARG(0).get_runtime()));
                    } };
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
<<<<<<< HEAD
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
        AT_DICT = {
            find<FunctionType>(find<ProductType>(find<DictType>(ANY, ANY), ANY), ANY),
            [](ref<Env> env, const Value& args) -> Value {
                return *(ARG(0).get_dict())[ARG(1)];
            },
            nullptr
        };
        AT_DICT_LIST = {
            find<FunctionType>(find<ProductType>(find<DictType>(ANY, ANY), find<ListType>(ANY)), ANY),
            [](ref<Env> env, const Value& args) -> Value {
                vector<Value> vals;
                for (const Value& key : to_vector(ARG(1))) 
                    vals.push(*ARG(0).get_dict()[key]);
                return Value(new ArrayValue(vals));
            },
            nullptr
        };
        AT_DICT_TYPE = {
            find<FunctionType>(find<ProductType>(TYPE, TYPE), TYPE),
            [](ref<Env> env, const Value& args) -> Value {
                return Value(find<DictType>(ARG(1).get_type(), ARG(0).get_type()), TYPE);
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
        EVAL = {
            find<FunctionType>(find<ProductType>(ANY), ANY),
            [](ref<Env> env, const Value& args) -> Value {
                Value term = ARG(0);
                prep(env, term);
                return eval(env, term);
            },
            nullptr
        };
        FLAG = {
            find<FunctionType>(find<ProductType>(ANY), VOID),
            [](ref<Env> env, const Value& args) -> Value { 
                err(ARG(0).loc(), "Flagged!");
                return empty();
            },
            nullptr
        };
=======
        AT_ARRAY_TYPE = { find<FunctionType>(find<ProductType>(TYPE, INT), TYPE),
                          [](ref<Env> env, const Value& args) -> Value {
                              return Value(find<ArrayType>(ARG(0).get_type(), ARG(1).get_int()), TYPE);
                          },
                          nullptr };
        AT_DYNARRAY_TYPE = { find<FunctionType>(find<ProductType>(TYPE, VOID), TYPE),
                             [](ref<Env> env, const Value& args) -> Value {
                                 return Value(find<ArrayType>(ARG(0).get_type()), TYPE);
                             },
                             nullptr };
        AT_DICT = { find<FunctionType>(find<ProductType>(find<DictType>(ANY, ANY), ANY), ANY),
                    [](ref<Env> env, const Value& args) -> Value { return *(ARG(0).get_dict())[ARG(1)]; }, nullptr };
        AT_DICT_LIST = { find<FunctionType>(find<ProductType>(find<DictType>(ANY, ANY), find<ListType>(ANY)), ANY),
                         [](ref<Env> env, const Value& args) -> Value {
                             vector<Value> vals;
                             for (const Value& key : to_vector(ARG(1))) vals.push(*ARG(0).get_dict()[key]);
                             return Value(new ArrayValue(vals));
                         },
                         nullptr };
        AT_DICT_TYPE = { find<FunctionType>(find<ProductType>(TYPE, TYPE), TYPE),
                         [](ref<Env> env, const Value& args) -> Value {
                             return Value(find<DictType>(ARG(1).get_type(), ARG(0).get_type()), TYPE);
                         },
                         nullptr };
        AT_MODULE = { find<FunctionType>(find<ProductType>(MODULE, SYMBOL), ANY),
                      [](ref<Env> env, const Value& args) -> Value {
                          if (!ARG(0).get_module().has(ARG(1).get_symbol())) {
                              err(ARG(1).loc(), "Module does not contain member '", symbol_for(ARG(1).get_symbol()),
                                  "'.");
                              return error();
                          }
                          return ARG(0).get_module().entry(ARG(1).get_symbol());
                      },
                      nullptr };
        ANNOTATE = { find<FunctionType>(find<ProductType>(ANY, TYPE), ANY),
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
                     NO_AUTO_LOWER };
        TYPEOF = { find<FunctionType>(find<ProductType>(ANY), TYPE),
                   [](ref<Env> env, const Value& args) -> Value { return Value(ARG(0).type(), TYPE); }, nullptr };
        TYPEDEF = { find<MacroType>(2),
                    [](ref<Env> env, const Value& args) -> Value {
                        return list_of(Value("def", SYMBOL), ARG(0),
                                       list_of(Value("#of", SYMBOL), list_of(Value("quote", SYMBOL), ARG(0)), ARG(1)));
                    },
                    nullptr };
        LIST_TYPE = { find<FunctionType>(find<ProductType>(TYPE), TYPE),
                      [](ref<Env> env, const Value& args) -> Value {
                          return Value(find<ListType>(ARG(0).get_type()), TYPE);
                      },
                      nullptr };
        OF_TYPE_MACRO = { find<MacroType>(2),
                          [](ref<Env> env, const Value& args) -> Value {
                              return list_of(Value("#of", SYMBOL), list_of(Value("quote", SYMBOL), ARG(0)), ARG(1));
                          },
                          nullptr };
        OF_TYPE = { find<FunctionType>(find<ProductType>(SYMBOL, TYPE), TYPE),
                    [](ref<Env> env, const Value& args) -> Value {
                        return Value(find<NamedType>(symbol_for(ARG(0).get_symbol()), ARG(1).get_type()), TYPE);
                    },
                    nullptr };
        IF = { find<FunctionType>(find<ProductType>(BOOL, ANY, ANY), ANY),
               [](ref<Env> env, const Value& args) -> Value { return eval(env, ARG(ARG(0).get_bool() ? 1 : 2)); },
               [](ref<Env> env, const Value& args) -> Value {
                   Value left = eval(env, ARG(1)), right = eval(env, ARG(2));
                   if (left.is_error() || right.is_error()) return error();
                   if (!left.is_runtime()) left = lower(left);
                   if (!right.is_runtime()) right = lower(right);
                   return new ASTIf(ARG(0).loc(), ARG(0).get_runtime(), left.get_runtime(), right.get_runtime());
               },
               NO_AUTO_LOWER };
>>>>>>> f840479bc314e660171a3eb91c404f37c4be4a33
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

    template <typename... Args>
    Value cases(ref<Env> env, const char* name, const Args... args) {
        vector<Builtin*> vals = vector_of<Builtin*>(args...);
        set<const Type*> ts;
        map<const Type*, Value> m;
        for (Builtin* v : vals) ts.insert(v->type()), m.put(v->type(), Value(env, *v));
        for (auto& e : m) e.second.set_name(name);
        Value result = Value(new IntersectValue(m), find<IntersectType>(ts));
        result.set_name(name);
        return result;
    }

    void define_builtins(ref<Env> env) {
        if (!inited) inited = true, init_builtins();
        env->func("+", Proto::overloaded(
                Proto::of(ARG_VARIABLE, "+", ARG_VARIABLE), 
                Proto::of("+", ARG_VARIABLE, ARG_VARIABLE)
            ),
            cases(env, "+", &ADD_INT, &ADD_SYMBOL), 20);
        env->func("*", Proto::of(ARG_VARIABLE, "*", ARG_VARIABLE), 
            Value(env, MUL, "*"), 40);
        env->func("-", Proto::of(ARG_VARIABLE, "-", ARG_VARIABLE), 
            Value(env, SUB, "-"), 20);
        env->func("/", Proto::of(ARG_VARIABLE, "/", ARG_VARIABLE), 
            Value(env, DIV, "/"), 40);
        env->func("%", Proto::of(ARG_VARIABLE, "%", ARG_VARIABLE), 
            Value(env, REM, "%"), 40);
        env->func("and", Proto::of(ARG_VARIABLE, "and", ARG_VARIABLE), 
            Value(env, AND, "and"), 5);
        env->func("or", Proto::of(ARG_VARIABLE, "or", ARG_VARIABLE), 
            Value(env, OR, "or"), 5);
        env->func("xor", Proto::of(ARG_VARIABLE, "xor", ARG_VARIABLE), 
            Value(env, XOR, "xor"), 5);
        env->func("not", Proto::of("not", ARG_VARIABLE), 
            Value(env, NOT, "not"), 0);
        env->func("==", Proto::of(ARG_VARIABLE, "==", ARG_VARIABLE), 
            Value(env, EQUALS, "=="), 10);
        env->func("!=", Proto::of(ARG_VARIABLE, "!=", ARG_VARIABLE), 
            Value(env, NOT_EQUALS, "!="), 10);
        env->func("<", Proto::of(ARG_VARIABLE, "<", ARG_VARIABLE), 
            Value(env, LESS, "<"), 10);
        env->func(">", Proto::of(ARG_VARIABLE, ">", ARG_VARIABLE), 
            Value(env, GREATER, ">"), 10);
        env->func("<=", Proto::of(ARG_VARIABLE, "<=", ARG_VARIABLE), 
            Value(env, LESS_EQUAL, "<="), 10);
        env->func(">=", Proto::of(ARG_VARIABLE, ">=", ARG_VARIABLE), 
            Value(env, GREATER_EQUAL, ">="), 10);
        env->func("in", Proto::of(ARG_VARIABLE, "in", ARG_VARIABLE), 
            Value(env, DICT_IN, "in"), 60);
        // env->infix("not", Value(env, DICT_NOT_IN), 2, 60);
<<<<<<< HEAD
        env->func("display", Proto::of("display", ARG_VARIABLE), 
            Value(env, DISPLAY, "display"), 0);
        env->func("at", Proto::of(ARG_VARIABLE, "at", ARG_VARIABLE), 
            cases(env, "at", &AT_INT, &AT_LIST, &AT_ARRAY_TYPE, &AT_DYNARRAY_TYPE, 
            &AT_MODULE, &AT_DICT, &AT_DICT_TYPE, &AT_DICT_LIST), 120);
        env->func("annotate", Proto::of("annotate", ARG_VARIABLE, ARG_VARIABLE), 
            Value(env, ANNOTATE, "annotate"), 0);
        env->func("typeof", Proto::of("typeof", ARG_VARIABLE), 
            Value(env, TYPEOF, "typeof"), 0);
        env->macro("of", Proto::of(ARG_VARIABLE, "of", ARG_VARIABLE), 
            Value(env, OF_TYPE_MACRO, "of"), 20);
        env->func("#of", Proto::of("#of", ARG_VARIABLE, ARG_VARIABLE), 
            Value(env, OF_TYPE, "of"), 0);
        env->func("list", Proto::of(ARG_VARIABLE, "list"), 
            Value(env, LIST_TYPE, "list"), 80);
        env->func("#?", Proto::of(ARG_VARIABLE, "#?", ARG_VARIABLE, ARG_VARIABLE), 
            Value(env, IF, "if"), 2);
        env->func("eval", Proto::of("eval", ARG_VARIABLE), 
            Value(env, EVAL, "eval"), 0);
        env->func("$flag", Proto::of("$flag", ARG_VARIABLE), 
            Value(env, FLAG, "flag"), 0);
=======
        env->def("display", Value(env, DISPLAY), 1);
        env->infix("at",
                   cases(env, &AT_INT, &AT_LIST, &AT_ARRAY_TYPE, &AT_DYNARRAY_TYPE, &AT_MODULE, &AT_DICT, &AT_DICT_TYPE,
                         &AT_DICT_LIST),
                   2, 120);
        env->def("annotate", Value(env, ANNOTATE), 2);
        env->def("typeof", Value(env, TYPEOF), 1);
        env->def_macro("typedef", Value(env, TYPEDEF), 2);
        env->infix_macro("of", Value(env, OF_TYPE_MACRO), 2, 20);
        env->def("#of", Value(env, OF_TYPE), 2);
        env->infix("list", Value(env, LIST_TYPE), 1, 80);
        env->infix("#?", Value(env, IF), 3, 2);
>>>>>>> f840479bc314e660171a3eb91c404f37c4be4a33
    }
} // namespace basil