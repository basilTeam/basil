#include "ast.h"
#include "value.h"
#include "eval.h"

namespace basil {
    AST::AST(Source::Pos pos_in, ASTKind kind_in, Type type_in): 
        pos(pos_in), t(t_runtime_base(type_in)), k(kind_in) {}

    AST::~AST() {
        //
    }

    u32 AST::children() const {
        return end() - begin();
    }

    Type AST::type(rc<Env> env) {
        if (!resolved) {
            resolved = true;
            if (children()) { // resolve children
                resolved = true;

                vector<Type> child_types;
                for (rc<AST> child : *this) {
                    child_types.push(child->type(env));
                }

                for (Type t : child_types) {
                    if (t.of(K_ERROR)) return cached_type = T_ERROR;
                }

                // expect function or intersect
                if (!t.of(K_FUNCTION) && (!t.of(K_INTERSECT) || !t_intersect_procedural(t))) {
                    panic("Expected procedural type in parent AST node!");
                }

                Type args_type = child_types.size() == 1 ? child_types[0] : t_tuple(child_types);

                if (t.of(K_INTERSECT)) {
                    vector<Type> valid;
                    for (Type t : t_intersect_members(t)) valid.push(t);
                    auto resolved = resolve_call(env, valid, args_type);
                    if (resolved.is_right() && resolved.right().ambiguous) {
                        t_unbind(args_type);
                        if (!t_is_concrete(args_type)) {
                            vector<Type> rets;

                            t_tvar_enable_isect();
                            for (const auto& mismatch : resolved.right().mismatches) {
                                args_type.coerces_to(t_arg(mismatch.first));
                                rets.push(t_ret(mismatch.first));
                            }
                            t_tvar_disable_isect();
                            
                            cached_type = t_var();
                            cached_type.coerces_to(t_intersect(rets));
                        }
                        else {
                            err(pos, "Ambiguous call to overloaded function.");
                            for (u32 i = 0; i < resolved.right().mismatches.size(); i ++) {
                                const auto& mismatch = resolved.right().mismatches[i];
                                note({}, "Candidate function found of type '", mismatch.first, "'.");
                            }
                            cached_type = T_ERROR;
                        }
                    }
                    else if (resolved.is_right()) {
                        err(pos, "Incompatible arguments '", args_type, "' for function.");
                        for (u32 i = 0; i < resolved.right().mismatches.size(); i ++) {
                            const auto& mismatch = resolved.right().mismatches[i];
                            note(begin()[mismatch.second]->pos, "Candidate function found of type '", 
                                mismatch.first, "', but given incompatible argument of type '", 
                                child_types[mismatch.second], "'.");
                        }
                        cached_type = T_ERROR;
                    }
                    else t = resolved.left(), cached_type = t_ret(resolved.left());
                }
                else cached_type = t_ret(t);
            }
            else cached_type = t; // leaf most likely
        }
        return cached_type;
    }

    ASTKind AST::kind() const {
        return k;
    }

    // Useful intermediate classes.

    // An AST node with no children.
    struct ASTLeaf : public AST {  
        ASTLeaf(Source::Pos pos, ASTKind kind, Type type):
            AST(pos, kind, type) {}
        
        const rc<AST>* begin() const override {
            return nullptr;
        }

        const rc<AST>* end() const override {
            return nullptr;
        }
    };

    // An AST node with one child.
    struct ASTUnary : public AST {
        rc<AST> child[1];

        ASTUnary(Source::Pos pos, ASTKind kind, Type type, rc<AST> child_in):
            AST(pos, kind, type) {
            child[0] = child_in;
        }

        const rc<AST>* begin() const override {
            return child;
        }
        
        const rc<AST>* end() const override {
            return begin() + 1;
        }

        rc<AST> operand() const {
            return child[0];
        }
    };

    // An AST node with two children.
    struct ASTBinary : public AST {
        rc<AST> lr[2];

        ASTBinary(Source::Pos pos, ASTKind kind, Type type, rc<AST> left, rc<AST> right):
            AST(pos, kind, type) {
            lr[0] = left;
            lr[1] = right;
        }

        const rc<AST>* begin() const override {
            return lr;
        }
        
        const rc<AST>* end() const override {
            return begin() + 2;
        }

        rc<AST> left() const {
            return lr[0];
        }

        rc<AST> right() const {
            return lr[1];
        }
    };

    // Constants

    struct ASTInt : public ASTLeaf {
        i64 val;
        ASTInt(Source::Pos pos, Type type, i64 value_in):
            ASTLeaf(pos, AST_INT, type), val(value_in) {}
        
        void format(stream& io) const override {
            write(io, val);
        }

        rc<AST> clone() const override {
            return ref<ASTInt>(pos, t, val);
        }

        IRParam gen_ssa(rc<Env> env, rc<IRFunction> func) override {
            return ir_int(val);
        }
    };

    struct ASTFloat : public ASTLeaf {
        float val;
        ASTFloat(Source::Pos pos, Type type, float value_in):
            ASTLeaf(pos, AST_FLOAT, type), val(value_in) {}
        
        void format(stream& io) const override {
            write(io, val);
        }

        rc<AST> clone() const override {
            return ref<ASTFloat>(pos, t, val);
        }

        IRParam gen_ssa(rc<Env> env, rc<IRFunction> func) override {
            return ir_float(val);
        }
    };

    struct ASTDouble : public ASTLeaf {
        double val;
        ASTDouble(Source::Pos pos, Type type, double value_in):
            ASTLeaf(pos, AST_DOUBLE, type), val(value_in) {}
        
        void format(stream& io) const override {
            write(io, val);
        }

        rc<AST> clone() const override {
            return ref<ASTDouble>(pos, t, val);
        }

        IRParam gen_ssa(rc<Env> env, rc<IRFunction> func) override {
            return ir_double(val);
        }
    };

    struct ASTVoid : public ASTLeaf {
        ASTVoid(Source::Pos pos):
            ASTLeaf(pos, AST_VOID, T_VOID) {}
        
        void format(stream& io) const override {
            write(io, "()");
        }

        rc<AST> clone() const override {
            return ref<ASTVoid>(pos);
        }

        IRParam gen_ssa(rc<Env> env, rc<IRFunction> func) override {
            return ir_int(0);
        }
    };

    struct ASTSymbol : public ASTLeaf {
        Symbol val;
        ASTSymbol(Source::Pos pos, Type type, Symbol value_in):
            ASTLeaf(pos, AST_SYMBOL, type), val(value_in) {}
        
        void format(stream& io) const override {
            write(io, ':', val);
        }

        rc<AST> clone() const override {
            return ref<ASTSymbol>(pos, t, val);
        }

        IRParam gen_ssa(rc<Env> env, rc<IRFunction> func) override {
            return ir_sym(val);
        }
    };

    struct ASTType : public ASTLeaf {
        Type val;
        ASTType(Source::Pos pos, Type type, Type value_in):
            ASTLeaf(pos, AST_TYPE, type), val(value_in) {}
        
        void format(stream& io) const override {
            write(io, '#', val);
        }

        rc<AST> clone() const override {
            return ref<ASTType>(pos, t, val);
        }

        IRParam gen_ssa(rc<Env> env, rc<IRFunction> func) override {
            return ir_type(val);
        }
    };

    struct ASTBool : public ASTLeaf {
        bool val;
        ASTBool(Source::Pos pos, Type type, bool value_in):
            ASTLeaf(pos, AST_BOOL, type), val(value_in) {}
        
        void format(stream& io) const override {
            write(io, val);
        }

        rc<AST> clone() const override {
            return ref<ASTBool>(pos, t, val);
        }

        IRParam gen_ssa(rc<Env> env, rc<IRFunction> func) override {
            return ir_bool(val);
        }
    };

    struct ASTString : public ASTLeaf {
        ustring s;
        ASTString(Source::Pos pos, Type type, const ustring& s_in):
            ASTLeaf(pos, AST_STRING, type), s(s_in) {}
        
        void format(stream& io) const override {
            write(io, '"', s, '"');
        }

        rc<AST> clone() const override {
            return ref<ASTString>(pos, t, s);
        }

        IRParam gen_ssa(rc<Env> env, rc<IRFunction> func) override {
            return ir_string(s);
        }
    };

    struct ASTChar : public ASTLeaf {
        rune ch;
        ASTChar(Source::Pos pos, Type type, rune ch_in):
            ASTLeaf(pos, AST_CHAR, type), ch(ch_in) {}
        
        void format(stream& io) const override {
            write(io, "'", ch, "'");
        }

        rc<AST> clone() const override {
            return ref<ASTChar>(pos, t, ch);
        }

        IRParam gen_ssa(rc<Env> env, rc<IRFunction> func) override {
            return ir_char(ch);
        }
    };
    
    rc<AST> ast_int(Source::Pos pos, Type type, i64 value) {
        return ref<ASTInt>(pos, type, value);
    }

    rc<AST> ast_float(Source::Pos pos, Type type, float value) {
        return ref<ASTFloat>(pos, type, value);
    }
    
    rc<AST> ast_double(Source::Pos pos, Type type, double value) {
        return ref<ASTDouble>(pos, type, value);
    }
    
    rc<AST> ast_symbol(Source::Pos pos, Type type, Symbol value) {
        return ref<ASTSymbol>(pos, type, value);
    }
    
    rc<AST> ast_string(Source::Pos pos, Type type, const ustring& value) {
        return ref<ASTString>(pos, type, value);
    }
    
    rc<AST> ast_char(Source::Pos pos, Type type, rune value) {
        return ref<ASTChar>(pos, type, value);
    }
    
    rc<AST> ast_void(Source::Pos pos) {
        return ref<ASTVoid>(pos);
    }
    
    rc<AST> ast_type(Source::Pos pos, Type type, Type value) {
        return ref<ASTType>(pos, type, value);
    }
    
    rc<AST> ast_bool(Source::Pos pos, Type type, bool value) {
        return ref<ASTBool>(pos, type, value);
    }

    struct ASTVar : public ASTLeaf {
        rc<Env> env;
        Symbol name;
        ASTVar(Source::Pos pos, rc<Env> env_in, Symbol name_in):
            ASTLeaf(pos, AST_VAR, T_VOID), env(env_in), name(name_in) {}
        
        void format(stream& io) const override {
            write(io, name);
        }

        rc<AST> clone() const override {
            return ref<ASTVar>(pos, env, name);
        }

        Type type(rc<Env> env) override {
            if (!resolved) {
                resolved = true;
                auto found = this->env->find(name);
                cached_type = found ? t_runtime_base(found->type) : T_ERROR;
            }
            return cached_type;
        }

        IRParam gen_ssa(rc<Env> env, rc<IRFunction> func) override {
            return ir_var(name);
        }
    };

    rc<AST> ast_var(Source::Pos pos, rc<Env> env, Symbol name) {
        return ref<ASTVar>(pos, env, name);
    }

    struct ASTUnknown : public ASTLeaf {
        ASTUnknown(Source::Pos pos, Type type):
            ASTLeaf(pos, AST_UNKNOWN, type) {}
        
        void format(stream& io) const override {
            write(io, t, '?');
        }

        rc<AST> clone() const override {
            return ref<ASTUnknown>(pos, t);
        }

        IRParam gen_ssa(rc<Env> env, rc<IRFunction> func) override {
            return ir_none();
        }
    };

    rc<AST> ast_unknown(Source::Pos pos, Type type) {
        return ref<ASTUnknown>(pos, type);
    }

    // Arithmetic

    struct ASTAdd : public ASTBinary {
        ASTAdd(Source::Pos pos, Type type, rc<AST> left, rc<AST> right):
            ASTBinary(pos, AST_ADD, type, left, right) {}
        
        void format(stream& io) const override {
            write(io, "(+ ", left(), " ", right(), ")");
        }

        rc<AST> clone() const override {
            return ref<ASTAdd>(pos, t, left()->clone(), right()->clone());
        }

        IRParam gen_ssa(rc<Env> env, rc<IRFunction> func) override {
            return func->add_insn(ir_add(func, type(env), left()->gen_ssa(env, func), right()->gen_ssa(env, func)));
        }
    };

    rc<AST> ast_add(Source::Pos pos, Type type, rc<AST> left, rc<AST> right) {
        return ref<ASTAdd>(pos, type, left, right);
    }

    struct ASTSub : public ASTBinary {
        ASTSub(Source::Pos pos, Type type, rc<AST> left, rc<AST> right):
            ASTBinary(pos, AST_SUB, type, left, right) {}
        
        void format(stream& io) const override {
            write(io, "(- ", left(), " ", right(), ")");
        }

        rc<AST> clone() const override {
            return ref<ASTSub>(pos, t, left()->clone(), right()->clone());
        }

        IRParam gen_ssa(rc<Env> env, rc<IRFunction> func) override {
            return func->add_insn(ir_sub(func, type(env), left()->gen_ssa(env, func), right()->gen_ssa(env, func)));
        }
    };

    rc<AST> ast_sub(Source::Pos pos, Type type, rc<AST> left, rc<AST> right) {
        return ref<ASTSub>(pos, type, left, right);
    }

    struct ASTMul : public ASTBinary {
        ASTMul(Source::Pos pos, Type type, rc<AST> left, rc<AST> right):
            ASTBinary(pos, AST_SUB, type, left, right) {}
        
        void format(stream& io) const override {
            write(io, "(* ", left(), " ", right(), ")");
        }

        rc<AST> clone() const override {
            return ref<ASTMul>(pos, t, left()->clone(), right()->clone());
        }

        IRParam gen_ssa(rc<Env> env, rc<IRFunction> func) override {
            return func->add_insn(ir_mul(func, type(env), left()->gen_ssa(env, func), right()->gen_ssa(env, func)));
        }
    };

    rc<AST> ast_mul(Source::Pos pos, Type type, rc<AST> left, rc<AST> right) {
        return ref<ASTMul>(pos, type, left, right);
    }

    struct ASTDiv : public ASTBinary {
        ASTDiv(Source::Pos pos, Type type, rc<AST> left, rc<AST> right):
            ASTBinary(pos, AST_SUB, type, left, right) {}
        
        void format(stream& io) const override {
            write(io, "(/ ", left(), " ", right(), ")");
        }

        rc<AST> clone() const override {
            return ref<ASTDiv>(pos, t, left()->clone(), right()->clone());
        }

        IRParam gen_ssa(rc<Env> env, rc<IRFunction> func) override {
            return func->add_insn(ir_div(func, type(env), left()->gen_ssa(env, func), right()->gen_ssa(env, func)));
        }
    };

    rc<AST> ast_div(Source::Pos pos, Type type, rc<AST> left, rc<AST> right) {
        return ref<ASTDiv>(pos, type, left, right);
    }

    struct ASTRem : public ASTBinary {
        ASTRem(Source::Pos pos, Type type, rc<AST> left, rc<AST> right):
            ASTBinary(pos, AST_REM, type, left, right) {}
        
        void format(stream& io) const override {
            write(io, "(% ", left(), " ", right(), ")");
        }

        rc<AST> clone() const override {
            return ref<ASTRem>(pos, t, left()->clone(), right()->clone());
        }

        IRParam gen_ssa(rc<Env> env, rc<IRFunction> func) override {
            return func->add_insn(ir_rem(func, type(env), left()->gen_ssa(env, func), right()->gen_ssa(env, func)));
        }
    };

    rc<AST> ast_rem(Source::Pos pos, Type type, rc<AST> left, rc<AST> right) {
        return ref<ASTRem>(pos, type, left, right);
    }

    // Logic    

    struct ASTAnd : public ASTBinary {
        ASTAnd(Source::Pos pos, Type type, rc<AST> left, rc<AST> right):
            ASTBinary(pos, AST_AND, type, left, right) {}
        
        void format(stream& io) const override {
            write(io, "(and ", left(), " ", right(), ")");
        }

        rc<AST> clone() const override {
            return ref<ASTAnd>(pos, t, left()->clone(), right()->clone());
        }

        IRParam gen_ssa(rc<Env> env, rc<IRFunction> func) override { // short circuiting
            rc<IRBlock> branch = func->new_block(), end = func->new_block();
            func->add_block(branch);
            func->add_insn(ir_if(func, left()->gen_ssa(env, func), branch, end));

            func->set_active(branch);
            IRParam rhs = right()->gen_ssa(env, func);
            func->add_insn(ir_goto(func, end));

            func->set_active(end);
            return func->add_insn(ir_phi(func, T_BOOL, ir_bool(false), rhs));
        }
    };

    rc<AST> ast_and(Source::Pos pos, Type type, rc<AST> left, rc<AST> right) {
        return ref<ASTAnd>(pos, type, left, right);
    }

    struct ASTOr : public ASTBinary {
        ASTOr(Source::Pos pos, Type type, rc<AST> left, rc<AST> right):
            ASTBinary(pos, AST_OR, type, left, right) {}
        
        void format(stream& io) const override {
            write(io, "(or ", left(), " ", right(), ")");
        }

        rc<AST> clone() const override {
            return ref<ASTOr>(pos, t, left()->clone(), right()->clone());
        }

        IRParam gen_ssa(rc<Env> env, rc<IRFunction> func) override {
            rc<IRBlock> branch = func->new_block(), end = func->new_block();
            func->add_block(branch);
            func->add_insn(ir_if(func, left()->gen_ssa(env, func), end, branch));

            func->set_active(branch);
            IRParam rhs = right()->gen_ssa(env, func);
            func->add_insn(ir_goto(func, end));

            func->set_active(end);
            return func->add_insn(ir_phi(func, T_BOOL, ir_bool(true), rhs));
        }
    };

    rc<AST> ast_or(Source::Pos pos, Type type, rc<AST> left, rc<AST> right) {
        return ref<ASTOr>(pos, type, left, right);
    }

    struct ASTXor : public ASTBinary {
        ASTXor(Source::Pos pos, Type type, rc<AST> left, rc<AST> right):
            ASTBinary(pos, AST_XOR, type, left, right) {}
        
        void format(stream& io) const override {
            write(io, "(xor ", left(), " ", right(), ")");
        }

        rc<AST> clone() const override {
            return ref<ASTXor>(pos, t, left()->clone(), right()->clone());
        }

        IRParam gen_ssa(rc<Env> env, rc<IRFunction> func) override {
            return func->add_insn(ir_xor(func, T_BOOL, left()->gen_ssa(env, func), right()->gen_ssa(env, func)));
        }
    };

    rc<AST> ast_xor(Source::Pos pos, Type type, rc<AST> left, rc<AST> right) {
        return ref<ASTXor>(pos, type, left, right);
    }
    
    struct ASTNot : public ASTUnary {
        ASTNot(Source::Pos pos, Type type, rc<AST> operand):
            ASTUnary(pos, AST_XOR, type, operand) {}
        
        void format(stream& io) const override {
            write(io, "(not ", operand(), ")");
        }

        rc<AST> clone() const override {
            return ref<ASTNot>(pos, t, operand()->clone());
        }

        IRParam gen_ssa(rc<Env> env, rc<IRFunction> func) override {
            return func->add_insn(ir_not(func, T_BOOL, operand()->gen_ssa(env, func)));
        }
    };

    rc<AST> ast_not(Source::Pos pos, Type type, rc<AST> operand) {
        return ref<ASTNot>(pos, type, operand);
    }

    // Comparisons

    struct ASTLess : public ASTBinary {
        ASTLess(Source::Pos pos, Type type, rc<AST> left, rc<AST> right):
            ASTBinary(pos, AST_LESS, type, left, right) {}
        
        void format(stream& io) const override {
            write(io, "(< ", left(), " ", right(), ")");
        }

        rc<AST> clone() const override {
            return ref<ASTLess>(pos, t, left()->clone(), right()->clone());
        }

        IRParam gen_ssa(rc<Env> env, rc<IRFunction> func) override {
            return func->add_insn(ir_less(func, left()->type(env), 
                left()->gen_ssa(env, func), right()->gen_ssa(env, func)));
        }
    };

    rc<AST> ast_less(Source::Pos pos, Type type, rc<AST> left, rc<AST> right) {
        return ref<ASTLess>(pos, type, left, right);
    }

    struct ASTLessEqual : public ASTBinary {
        ASTLessEqual(Source::Pos pos, Type type, rc<AST> left, rc<AST> right):
            ASTBinary(pos, AST_LESS_EQUAL, type, left, right) {}
        
        void format(stream& io) const override {
            write(io, "(<= ", left(), " ", right(), ")");
        }

        rc<AST> clone() const override {
            return ref<ASTLessEqual>(pos, t, left()->clone(), right()->clone());
        }

        IRParam gen_ssa(rc<Env> env, rc<IRFunction> func) override {
            return func->add_insn(ir_less_eq(func, left()->type(env), 
                left()->gen_ssa(env, func), right()->gen_ssa(env, func)));
        }
    };

    rc<AST> ast_less_equal(Source::Pos pos, Type type, rc<AST> left, rc<AST> right) {
        return ref<ASTLessEqual>(pos, type, left, right);
    }

    struct ASTGreater : public ASTBinary {
        ASTGreater(Source::Pos pos, Type type, rc<AST> left, rc<AST> right):
            ASTBinary(pos, AST_GREATER, type, left, right) {}
        
        void format(stream& io) const override {
            write(io, "(> ", left(), " ", right(), ")");
        }

        rc<AST> clone() const override {
            return ref<ASTGreater>(pos, t, left()->clone(), right()->clone());
        }

        IRParam gen_ssa(rc<Env> env, rc<IRFunction> func) override {
            return func->add_insn(ir_greater(func, left()->type(env), 
                left()->gen_ssa(env, func), right()->gen_ssa(env, func)));
        }
    };

    rc<AST> ast_greater(Source::Pos pos, Type type, rc<AST> left, rc<AST> right) {
        return ref<ASTGreater>(pos, type, left, right);
    }

    struct ASTGreaterEqual : public ASTBinary {
        ASTGreaterEqual(Source::Pos pos, Type type, rc<AST> left, rc<AST> right):
            ASTBinary(pos, AST_GREATER_EQUAL, type, left, right) {}
        
        void format(stream& io) const override {
            write(io, "(>= ", left(), " ", right(), ")");
        }

        rc<AST> clone() const override {
            return ref<ASTGreaterEqual>(pos, t, left()->clone(), right()->clone());
        }

        IRParam gen_ssa(rc<Env> env, rc<IRFunction> func) override {
            return func->add_insn(ir_greater_eq(func, left()->type(env), 
                left()->gen_ssa(env, func), right()->gen_ssa(env, func)));
        }
    };

    rc<AST> ast_greater_equal(Source::Pos pos, Type type, rc<AST> left, rc<AST> right) {
        return ref<ASTGreaterEqual>(pos, type, left, right);
    }

    struct ASTEqual : public ASTBinary {
        ASTEqual(Source::Pos pos, Type type, rc<AST> left, rc<AST> right):
            ASTBinary(pos, AST_EQUAL, type, left, right) {}
        
        void format(stream& io) const override {
            write(io, "(== ", left(), " ", right(), ")");
        }

        rc<AST> clone() const override {
            return ref<ASTEqual>(pos, t, left()->clone(), right()->clone());
        }

        IRParam gen_ssa(rc<Env> env, rc<IRFunction> func) override {
            return func->add_insn(ir_eq(func, left()->type(env), 
                left()->gen_ssa(env, func), right()->gen_ssa(env, func)));
        }
    };

    rc<AST> ast_equal(Source::Pos pos, Type type, rc<AST> left, rc<AST> right) {
        return ref<ASTEqual>(pos, type, left, right);
    }

    struct ASTNotEqual : public ASTBinary {
        ASTNotEqual(Source::Pos pos, Type type, rc<AST> left, rc<AST> right):
            ASTBinary(pos, AST_NOT_EQUAL, type, left, right) {}
        
        void format(stream& io) const override {
            write(io, "(!= ", left(), " ", right(), ")");
        }

        rc<AST> clone() const override {
            return ref<ASTNotEqual>(pos, t, left()->clone(), right()->clone());
        }

        IRParam gen_ssa(rc<Env> env, rc<IRFunction> func) override {
            return func->add_insn(ir_not_eq(func, left()->type(env), 
                left()->gen_ssa(env, func), right()->gen_ssa(env, func)));
        }
    };

    rc<AST> ast_not_equal(Source::Pos pos, Type type, rc<AST> left, rc<AST> right) {
        return ref<ASTNotEqual>(pos, type, left, right);
    }

    // Control 

    struct ASTIf : public AST {
        rc<AST> child[2];

        ASTIf(Source::Pos pos, rc<AST> cond, rc<AST> ifTrue):
            AST(pos, AST_IF, T_VOID) {
            child[0] = cond;
            child[1] = ifTrue;
        }

        const rc<AST>* begin() const override {
            return child;
        }

        const rc<AST>* end() const override {
            return begin() + 2;
        }

        rc<AST> cond() const {
            return child[0];
        }

        rc<AST> ifTrue() const {
            return child[1];
        }

        void format(stream& io) const override {
            write(io, "(if ", cond(), " ", ifTrue(), ")");
        }

        rc<AST> clone() const override {
            return ref<ASTIf>(pos, cond()->clone(), ifTrue()->clone());
        }

        Type type(rc<Env> env) override {
            if (!resolved) {
                resolved = true;

                Type cond_t = cond()->type(env);
                Type true_t = ifTrue()->type(env);

                if (cond_t == T_ERROR || true_t == T_ERROR) { // propagate errors
                    cached_type = T_ERROR;
                }
                else if (!cond_t.coerces_to(T_BOOL)) {
                    err(cond()->pos, "Expected condition of if-expression to have type 'Bool', ", 
                        "but given '", cond_t, "' instead.");
                    cached_type = T_ERROR;
                }
                else cached_type = T_VOID; 
            }
            return cached_type;
        }

        IRParam gen_ssa(rc<Env> env, rc<IRFunction> func) override {
            rc<IRBlock> ifTrue = func->new_block(), end = func->new_block();
            rc<IRBlock> active = func->active();
            func->add_block(ifTrue);
            func->add_insn(ir_if(func, cond()->gen_ssa(env, func), ifTrue, end));

            func->set_active(ifTrue);
            this->ifTrue()->gen_ssa(env, func);
            func->add_insn(ir_goto(func, end));

            func->add_block(end);
            func->set_active(end); // insert future instructions after the conditional
            return ir_none();
        }
    };

    rc<AST> ast_if(Source::Pos pos, rc<AST> cond, rc<AST> ifTrue) {
        return ref<ASTIf>(pos, cond, ifTrue);
    }

    struct ASTIfElse : public AST {
        rc<AST> child[3];

        ASTIfElse(Source::Pos pos, rc<AST> cond, rc<AST> ifTrue, rc<AST> ifFalse):
            AST(pos, AST_IF, T_VOID) {
            child[0] = cond;
            child[1] = ifTrue;
            child[2] = ifFalse;
        }

        const rc<AST>* begin() const override {
            return child;
        }

        const rc<AST>* end() const override {
            return begin() + 3;
        }

        rc<AST> cond() const {
            return child[0];
        }

        rc<AST> ifTrue() const {
            return child[1];
        }

        rc<AST> ifFalse() const {
            return child[2];
        }

        void format(stream& io) const override {
            write(io, "(if ", cond(), " ", ifTrue(), " ", ifFalse(), ")");
        }

        rc<AST> clone() const override {
            return ref<ASTIfElse>(pos, cond()->clone(), ifTrue()->clone(), ifFalse()->clone());
        }

        Type type(rc<Env> env) override {
            if (!resolved) {
                resolved = true;

                Type cond_t = cond()->type(env);
                Type true_t = ifTrue()->type(env);
                Type false_t = ifFalse()->type(env);
                Type unified = T_ERROR;

                if (t_soft_eq(true_t, false_t)) unified = true_t; // no coercion needed
                else if (true_t.coerces_to(false_t)) { // use false_t
                    unified = false_t; 
                    child[1] = ast_coerce(ifTrue()->pos, ifTrue(), false_t); // add coerce node
                }
                else if (false_t.coerces_to(true_t)) { // use true_t
                    unified = true_t; 
                    child[2] = ast_coerce(ifFalse()->pos, ifFalse(), true_t); // add coerce node
                }

                // println("true_t = ", true_t, ", false_t = ", false_t, ", unified to ", unified);

                if (cond_t == T_ERROR || true_t == T_ERROR || false_t == T_ERROR) { // propagate errors
                    cached_type = T_ERROR;
                }
                else if (!cond_t.coerces_to(T_BOOL)) {
                    err(cond()->pos, "Expected condition of if-expression to have type 'Bool', ", 
                        "but given '", cond_t, "' instead.");
                    cached_type = T_ERROR;
                }
                else if (unified == T_ERROR) {
                    err(span(ifTrue()->pos, ifFalse()->pos), "Could not unify branches of ",
                        "if-expression: 'then' branch has type '", true_t, "', which is ",
                        "incompatible with 'else' branch of type '", false_t, "'.");
                    cached_type = T_ERROR;
                }
                else cached_type = unified; 
            }
            return cached_type;
        }

        IRParam gen_ssa(rc<Env> env, rc<IRFunction> func) override {
            rc<IRBlock> ifTrue = func->new_block(), ifFalse = func->new_block(), end = func->new_block();
            rc<IRBlock> active = func->active();
            func->add_block(ifTrue);
            func->add_block(ifFalse);
            func->add_insn(ir_if(func, cond()->gen_ssa(env, func), ifTrue, ifFalse));

            func->set_active(ifTrue);
            IRParam ifTrue_result = this->ifTrue()->gen_ssa(env, func);
            rc<IRBlock> ifTrue_end = func->active();
            func->add_insn(ir_goto(func, end));

            func->set_active(ifFalse);
            IRParam ifFalse_result = this->ifFalse()->gen_ssa(env, func);
            rc<IRBlock> ifFalse_end = func->active();
            func->add_insn(ir_goto(func, end));

            func->set_active(ifTrue_end);
            func->add_block(end);
            end->add_entry(ifFalse_end); // add edge from other branch

            func->set_active(end); // insert future instructions after the conditional
            return func->add_insn(ir_phi(func, this->ifTrue()->type(env), ifTrue_result, ifFalse_result));
        }
    };

    rc<AST> ast_if_else(Source::Pos pos, rc<AST> cond, rc<AST> ifTrue, rc<AST> ifFalse) {
        return ref<ASTIfElse>(pos, cond, ifTrue, ifFalse);
    }

    struct ASTWhile : public AST {
        rc<AST> child[2];

        ASTWhile(Source::Pos pos, rc<AST> cond, rc<AST> body):
            AST(pos, AST_WHILE, T_VOID) {
            child[0] = cond;
            child[1] = body;
        }

        const rc<AST>* begin() const override {
            return child;
        }

        const rc<AST>* end() const override {
            return begin() + 2;
        }

        rc<AST> cond() const {
            return child[0];
        }

        rc<AST> body() const {
            return child[1];
        }

        void format(stream& io) const override {
            write(io, "(while ", cond(), " ", body(), ")");
        }

        rc<AST> clone() const override {
            return ref<ASTWhile>(pos, cond()->clone(), body()->clone());
        }

        Type type(rc<Env> env) override {
            if (!resolved) {
                resolved = true;
                Type cond_t = cond()->type(env);
                Type body_t = body()->type(env);

                if (cond_t == T_ERROR || body_t == T_ERROR) { // propagate errors
                    cached_type = T_ERROR;
                }
                else if (!cond_t.coerces_to(T_BOOL)) {
                    err(cond()->pos, "Expected condition of if-expression to have type 'Bool', ", 
                        "but given '", cond_t, "' instead.");
                    cached_type = T_ERROR;
                }
                else cached_type = T_VOID;
            }
            return cached_type;
        }

        IRParam gen_ssa(rc<Env> env, rc<IRFunction> func) override {
            rc<IRBlock> cond_block = func->new_block(), body_block = func->new_block(), end = func->new_block();
            func->add_block(cond_block);
            func->set_active(cond_block);
            func->add_insn(ir_if(func, cond()->gen_ssa(env, func), body_block, end));

            func->add_block(body_block);
            func->set_active(body_block);
            body()->gen_ssa(env, func);
            func->add_insn(ir_goto(func, cond_block));
            cond_block->add_entry(body_block);

            func->add_block(end);
            func->set_active(end); // insert future instructions after the loop
            return ir_none();
        }
    };

    rc<AST> ast_while(Source::Pos pos, rc<AST> cond, rc<AST> body) {
        return ref<ASTWhile>(pos, cond, body);
    }

    struct ASTDo : public AST {
        vector<rc<AST>> child;

        ASTDo(Source::Pos pos, const vector<rc<AST>>& subexprs):
            AST(pos, AST_DO, T_VOID), child(subexprs) {}

        const rc<AST>* begin() const override {
            return child.begin();
        }

        const rc<AST>* end() const override {
            return child.end();
        }

        void format(stream& io) const override {
            write(io, "(do");
            for (const rc<AST>& expr : child) write(io, " ", expr);
            write(io, ")");
        }

        rc<AST> clone() const override {
            vector<rc<AST>> cloned_subexprs;
            for (const rc<AST>& expr : child) cloned_subexprs.push(expr->clone());
            return ref<ASTDo>(pos, cloned_subexprs);
        }

        Type type(rc<Env> env) override {
            if (!resolved) {
                resolved = true;

                cached_type = T_VOID; // empty do is void
                for (rc<AST> expr : child) {
                    cached_type = expr->type(env); // use type of last expr
                    if (cached_type == T_ERROR) break; // exit early on error
                }
            }
            return cached_type;
        }

        IRParam gen_ssa(rc<Env> env, rc<IRFunction> func) override {
            IRParam result = ir_none();
            for (rc<AST> expr : child) result = expr->gen_ssa(env, func);
            return result;
        }
    };

    rc<AST> ast_do(Source::Pos pos, const vector<rc<AST>>& exprs) {
        return ref<ASTDo>(pos, exprs);
    }

    // Definitions

    struct ASTDefine : public ASTUnary {
        Symbol name;
        ASTDefine(Source::Pos pos, Symbol name_in, rc<AST> init):
            ASTUnary(pos, AST_DEF, T_VOID, init), name(name_in) {}

        void format(stream& io) const override {
            write(io, "(def ", name, " ", operand(), ")");
        }

        rc<AST> clone() const override {
            return ref<ASTDefine>(pos, name, operand()->clone());
        }

        IRParam gen_ssa(rc<Env> env, rc<IRFunction> func) override {
            func->add_insn(ir_assign(func, operand()->type(env), ir_var(name), operand()->gen_ssa(env, func)));
            return ir_none();
        }
    };

    rc<AST> ast_def(Source::Pos pos, Symbol name, rc<AST> value) {
        return ref<ASTDefine>(pos, name, value);
    }

    struct ASTFunctionStub : public ASTLeaf {
        Symbol name;
        
        ASTFunctionStub(Source::Pos pos, Type type, Symbol name_in):
            ASTLeaf(pos, AST_FUNCTION_STUB, type), name(name_in) {}

        void format(stream& io) const override {
            write(io, name);
        }

        rc<AST> clone() const override {
            return ref<ASTFunctionStub>(pos, t, name);
        }

        IRParam gen_ssa(rc<Env> env, rc<IRFunction> func) override {
            return ir_none(); // stubs don't have any code
        }
    };

    rc<AST> ast_func_stub(Source::Pos pos, Type type, Symbol name) {
        return ref<ASTFunctionStub>(pos, type, name);
    }

    static u32 anon_id = 0;

    static Symbol next_anon_fn() {
        return symbol_from(format<ustring>("#lambda_", anon_id ++));
    }

    struct ASTFunction : public ASTUnary {
        rc<Env> env;
        Symbol name;
        rc<IRFunction> ir_func = nullptr;

        ASTFunction(Source::Pos pos, Type type, rc<Env> local, optional<Symbol> name_in, rc<AST> body_in):
            ASTUnary(pos, AST_FUNCTION, type, body_in), env(local), name(name_in ? *name_in : next_anon_fn()) {}

        void format(stream& io) const override {
            write(io, name);
        }

        rc<AST> clone() const override {
            return ref<ASTFunction>(pos, t, env, some<Symbol>(name), operand()->clone());
        }

        Type type(rc<Env> outer_env) override {
            if (!resolved) {
                resolved = true;
                cached_type = t;
                operand()->type(env);
                operand() = resolve_overloads(env, operand());
            }
            return cached_type;
        }

        IRParam gen_ssa(rc<Env> outer_env, rc<IRFunction> func) override {
            if (!ir_func) {
                ir_func = ref<IRFunction>(name, type(outer_env));
                IRParam returned = operand()->gen_ssa(env, ir_func);
                ir_func->finish(operand()->type(env), returned);
            }
            return ir_var(name);
        }
    };

    rc<AST> ast_func(Source::Pos pos, Type type, rc<Env> fn_env, optional<Symbol> name, rc<AST> body) {
        return ref<ASTFunction>(pos, type, fn_env, name, body);
    }
    
    rc<IRFunction> get_ssa_function(rc<AST> function) {
        if (function->kind() != AST_FUNCTION) panic("Tried to get SSA function from non-function AST node!");
        return ((rc<ASTFunction>)function)->ir_func;
    }

    struct ASTCall : public AST {
        rc<AST> func;
        vector<rc<AST>> args;

        ASTCall(Source::Pos pos, rc<AST> func_in, const vector<rc<AST>>& args_in):
            AST(pos, AST_CALL, T_VOID), func(func_in), args(args_in) {}

        const rc<AST>* begin() const override {
            return &args[0];
        }

        const rc<AST>* end() const override {
            return begin() + args.size();
        }

        void format(stream& io) const override {
            write(io, "(call ", func, ":");
            for (const rc<AST>& arg : args) write(io, " ", arg);
            write(io, ")");
        }

        rc<AST> clone() const override {
            vector<rc<AST>> cloned_args;
            for (const rc<AST>& arg : args) cloned_args.push(arg->clone());
            return ref<ASTCall>(pos, func->clone(), cloned_args);
        }

        Type type(rc<Env> env) override {
            if (!resolved) {
                t = func->type(env); // set type of this op to the function's type
                return AST::type(env);
            }
            else return cached_type;
        }

        IRParam gen_ssa(rc<Env> env, rc<IRFunction> func) override {
            IRParam f = this->func->gen_ssa(env, func);
            vector<IRParam> params;
            for (rc<AST> arg : args) params.push(arg->gen_ssa(env, func));
            return func->add_insn(ir_call(func, this->func->type(env), f, params));
        }
    };

    rc<AST> ast_call(Source::Pos pos, rc<AST> func, const vector<rc<AST>>& args) {
        return ref<ASTCall>(pos, func, args);
    }

    struct ASTOverload : public ASTLeaf {
        map<Type, either<Builtin, rc<InstTable>>> cases;

        ASTOverload(Source::Pos pos, Type type, map<Type, either<Builtin, rc<InstTable>>> cases_in):
            ASTLeaf(pos, AST_OVERLOAD, type), cases(cases_in) {}

        void format(stream& io) const override {
            write(io, "#overload");
        }

        rc<AST> clone() const override {
            return ref<ASTOverload>(pos, t, cases);
        }

        IRParam gen_ssa(rc<Env> env, rc<IRFunction> func) override {
            resolved = false;
            println("resolved overload call to ", type(env), " with type ", t);
            return ir_none();
        }
    };

    rc<AST> ast_overload(Source::Pos pos, Type type, const map<Type, either<Builtin, rc<InstTable>>>& cases) {
        return ref<ASTOverload>(pos, type, cases);
    }

    // Type stuff

    struct ASTCoerce : public ASTUnary {
        ASTCoerce(Source::Pos pos, rc<AST> value, Type dest):
            ASTUnary(pos, AST_COERCE, dest, value) {}
        
        void format(stream& io) const override {
            write(io, "(: ", operand(), " ", t, ")");
        }
        
        rc<AST> clone() const override {
            return ref<ASTCoerce>(pos, operand(), t);
        }

        Type type(rc<Env> env) override {
            if (!resolved) {
                resolved = true;

                if (!operand()->type(env).coerces_to(t)) {
                    err(pos, "Could not coerce value of type '", operand()->type(env), "' to '", t, "'.");
                    cached_type = T_ERROR;
                }
                else return cached_type = t;
            }
            return cached_type;
        }

        IRParam gen_ssa(rc<Env> env, rc<IRFunction> func) override {
            return operand()->gen_ssa(env, func);
        }
    };

    rc<AST> ast_coerce(Source::Pos pos, rc<AST> value, Type dest) {
        return ref<ASTCoerce>(pos, value, dest);
    }

    rc<AST> resolve_overloads(rc<Env> env, rc<AST> node) {
        // first, we recursively fix up overloads in any of this node's children
        for (const rc<AST>& child : *node) *(rc<AST>*)&child = resolve_overloads(env, child);

        // next, we see if this is a call to an unresolved overloaded function...
        if (node->kind() == AST_CALL && ((rc<ASTCall>)node)->func->kind() == AST_OVERLOAD) {
            rc<ASTOverload> overload = ((rc<ASTCall>)node)->func;

            // first, we assemble a list of arguments, so we know what we are resolving the overload
            // based on
            vector<Value> vals;
            vector<Type> arg_types, overloads;
            for (rc<AST> child : ((rc<ASTCall>)node)->args) {
                vals.push(v_runtime(child->pos, t_runtime(child->type(env)), child)); 
                arg_types.push(t_concrete(child->type(env))); // we elide type vars here
            }

            // next, we collect all the overloaded function types we're going to consider
            for (const auto& overload : t_intersect_members(overload->t))
                overloads.push(overload);

            Type args_type = arg_types.size() == 1 ? arg_types[0] : t_tuple(arg_types);
            auto call = resolve_call(env, overloads, args_type);
            if (call.is_left()) { // if the call was resolved
                const auto& cases = ((rc<ASTOverload>)overload)->cases;
                auto it = cases.find(call.left());
                if (it == cases.end()) 
                    panic("Somehow resolved overload to type '", call.left(), "' for which no overload exists!");
                
                auto fn = it->second;
                if (fn.is_left()) { // if it's a builtin function, we invoke it and replace the current
                    Value args = vals.size() == 1 ? vals[0] 
                        : v_tuple(span(vals[0].pos, vals[vals.size() - 1].pos), infer_tuple(vals), move(vals));
                    return fn.left().runtime(env, v_void(overload->pos), args);
                }
                else // otherwise, we replace the overload node with a runtime function
                    ((rc<ASTCall>)node)->func = fn.right()->insts[t_arg(call.left())]->func;
            }
            else { // we shouldn't reach here if the program typechecked correctly.
                if (call.right().ambiguous) panic("Ambiguous overloaded function call after typechecking!");
                else panic("Mismatched arguments to overloaded function call after typechecking!");
            }
        }
        return node;
    }
}

void write(stream& io, const rc<basil::AST>& param) {
    param->format(io);
}