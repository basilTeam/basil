#include "ops.h"

#include "jasmine/x64.h"

namespace basil {
    static Architecture _arch = DEFAULT_ARCH;

    Architecture arch() {
        return _arch;
    }

    void set_arch(Architecture arch) {
        _arch = arch;
    }

    u32 arg_register(u32 i) {
        switch (_arch) {
            case X86_64: {
                using namespace x64;
                static u32 ARG_REGISTERS[] = { RDI, RSI, RCX, RDX, R8, R9 };
                return ARG_REGISTERS[i];
            }
            default:
                return 0;
        }
    }

    u32 clobber_register(u32 i) {
        switch (_arch) {
            case X86_64: {
                using namespace x64;
                static u32 CLOBBER_REGISTERS[] = { RAX, RDX, RCX, RBX };
                return CLOBBER_REGISTERS[i];
            }
            default:
                return 0;
        }
    }

    vector<u32> allocatable_registers() {
        switch (_arch) {
            case X86_64: {
                using namespace x64;
                static Register ALLOCATABLE_REGISTERS[] = {
                    RBX, R8, R9, R10, R11, R12, R13, R14, R15
                };
                vector<u32> regs;
                for (i64 i = sizeof(ALLOCATABLE_REGISTERS) / sizeof(Register) - 1; i >= 0; i --)
                    regs.push(u32(ALLOCATABLE_REGISTERS[i]));
                return regs;
            }
            default:
                return {};
        }
    }

    x64::Arg x64_clobber(u32 i) {
        return x64::r64((x64::Register)clobber_register(i));
    }

    x64::Arg x64_param(u32 i) {
        return x64::r64((x64::Register)arg_register(i));
    }

    x64::Arg x64_arg(const Location& src) {
        using namespace x64;
        switch (src.type) {
            case LOC_NONE:
                return imm64(0);
            case LOC_REGISTER:
                return r64((Register)src.reg);
            case LOC_LABEL:
                return label64(global((const char*)label_of(src).raw()));
            case LOC_LOCAL:
                if (local_of(src).reg >= 0) return r64((Register)local_of(src).reg);
                else return m64(RBP, local_of(src).offset);
            case LOC_CONSTANT:
                return label64(global((const char*)constant_of(src).name.raw()));
            case LOC_IMMEDIATE:
                return imm64(src.immediate);
        }
    }

    const x64::Arg& x64_to_register(const x64::Arg& src, const x64::Arg& clobber) {
        using namespace x64;
        if (is_memory(src.type)) {
            mov(clobber, src);
            return clobber;
        }
        else return src;
    }  

    void x64_move(const x64::Arg& dest, const x64::Arg& src, const x64::Arg& clobber) {
        using namespace x64;
        if (dest == src) return;
        if (is_memory(dest.type) && is_memory(src.type)) {
            mov(clobber, src);
            mov(dest, clobber);
        }
        else mov(dest, src);
    }

    void x64_binary(const x64::Arg& dest, const x64::Arg& lhs, 
        const x64::Arg& rhs, const x64::Arg& clobber, void(*op)(const x64::Arg&, const x64::Arg&, x64::Size)) {
        using namespace x64;
        if (is_memory(dest.type) && (is_memory(lhs.type) || is_memory(rhs.type))) {
            x64_move(clobber, lhs, clobber);
            op(clobber, rhs, AUTO);
            x64_move(dest, clobber, clobber);
        }
        else {
            x64_move(dest, lhs, clobber);
            op(dest, rhs, AUTO);
        }
    }

    void x64_compare(const x64::Arg& lhs, const x64::Arg& rhs, const x64::Arg& clobber) {
        using namespace x64;
        if (is_memory(lhs.type) && is_memory(rhs.type)) {
            x64_move(clobber, lhs, clobber);
            cmp(clobber, rhs);
        }
        else cmp(lhs, rhs, AUTO);
    }

    void store(const Location& dest, const Location& src, u32 offset) {
        switch (_arch) {
            case X86_64: {
                using namespace x64;
                auto d = x64_to_register(x64_arg(dest), x64_clobber(1)),
                    s = x64_to_register(x64_arg(src), x64_clobber(0));
                auto m = m64(d.data.reg, offset);                               
                x64_move(m, s, x64_clobber(0));                                                   
                return;
            }
            default:
                return;
        }
    }

    void load(const Location& dest, const Location& src, u32 offset) {
        switch (_arch) {
            case X86_64: {
                using namespace x64;
                auto d = x64_arg(dest), 
                    s = x64_to_register(x64_arg(src), x64_clobber(1)); 
                auto m = m64(s.data.reg, offset);                               
                x64_move(d, m, x64_clobber(0));                 
                return;
            }
            default:
                return;
        }
    }

    void move(const Location& dest, const Location& src) {
        switch (_arch) {
            case X86_64: {
                using namespace x64;
                auto d = x64_arg(dest), s = x64_arg(src);                    
                x64_move(d, s, x64_clobber(0));                                            
                return;
            }
            default:
                return;
        }
    }

    void add(const Location& dest, const Location& lhs, const Location& rhs) {
        switch (_arch) {
            case X86_64: {
                using namespace x64;
                auto d = x64_arg(dest), l = x64_arg(lhs), r = x64_arg(rhs);
                if (is_register(d.type) && !is_memory(l.type) && !is_memory(r.type)) {
                    if (d.data.reg == l.data.reg) // no need for two steps
                        if (is_immediate(r.type) && r.data.imm64 == 1)
                            return inc(d);
                        else if (is_immediate(r.type) && r.data.imm64 == -1)
                            return dec(d);
                        else return add(d, r);
                    else if (d.data.reg == r.data.reg) // no need for two steps
                        if (is_immediate(l.type) && l.data.imm64 == 1)
                            return inc(d);
                        else if (is_immediate(l.type) && l.data.imm64 == -1)
                            return dec(d);
                        else return add(d, l);
                    else if (is_register(l.type) && is_immediate(r.type))
                        return lea(d, m64(l.data.reg, r.data.imm64));
                    else if (is_immediate(l.type) && is_register(r.type))
                        return lea(d, m64(r.data.reg, l.data.imm64));
                    else if (is_register(l.type) && is_register(r.type))
                        return lea(d, m64(l.data.reg, r.data.reg, SCALE1, 0));
                }
                x64_binary(x64_arg(dest), x64_arg(lhs), x64_arg(rhs), x64_clobber(0), x64::add);                                          
                return;
            }
            default:
                return;
        }
    }

    void sub(const Location& dest, const Location& lhs, const Location& rhs) {
        switch (_arch) {
            case X86_64: {
                using namespace x64;
                auto d = x64_arg(dest), l = x64_arg(lhs), r = x64_arg(rhs);
                if (is_register(d.type) && !is_memory(l.type) && !is_memory(r.type)) {
                    if (d.data.reg == l.data.reg) // no need for two steps
                        if (is_immediate(r.type) && r.data.imm64 == 1)
                            return dec(d);
                        else if (is_immediate(r.type) && r.data.imm64 == -1)
                            return inc(d);
                        else return sub(d, r);
                    else if (is_register(l.type) && is_immediate(r.type))
                        return lea(d, m64(l.data.reg, -r.data.imm64));
                }
                x64_binary(x64_arg(dest), x64_arg(lhs), x64_arg(rhs), x64_clobber(0), x64::sub);                                          
                return;
            }
            default:
                return;
        }
    }

    void mul(const Location& dest, const Location& lhs, const Location& rhs) {
        switch (_arch) {
            case X86_64: {
                using namespace x64;
                auto d = x64_arg(dest);
                auto t = is_memory(d.type) ? x64_clobber(0) : d;
                x64_move(t, x64_arg(lhs), x64_clobber(0));
                auto r = x64_arg(rhs);
                if (is_immediate(r.type)) {
                    x64_move(x64_clobber(1), r, x64_clobber(1));
                    x64::imul(t, x64_clobber(1));
                }
                else x64::imul(t, r);
                if (is_memory(d.type)) mov(d, t);    
                return;
            }
            default:
                return;
        }
    }

    void div(const Location& dest, const Location& lhs, const Location& rhs) {
        switch (_arch) {
            case X86_64: {
                using namespace x64;
                x64_move(x64_clobber(0), x64_arg(lhs), x64_clobber(0));
                cdq();
                auto r = x64_arg(rhs);
                if (is_immediate(r.type)) {
                    x64_move(x64_clobber(2), r, x64_clobber(2));
                    idiv(x64_clobber(2));
                }
                else idiv(r);
                x64_move(x64_arg(dest), x64_clobber(0), x64_clobber(0)); // dest <- rax   
                return;
            }
            default:
                return;
        }
    }

    void rem(const Location& dest, const Location& lhs, const Location& rhs) {
        switch (_arch) {
            case X86_64: {
                using namespace x64;
                x64_move(x64_clobber(0), x64_arg(lhs), x64_clobber(0));
                cdq();
                auto r = x64_arg(rhs);
                if (is_immediate(r.type)) {
                    x64_move(x64_clobber(2), r, x64_clobber(2));
                    idiv(x64_clobber(2));
                }
                else idiv(r);
                x64_move(x64_arg(dest), x64_clobber(1), x64_clobber(0)); // dest <- rdx 
                return;
            }
            default:
                return;
        }
    }

    void neg(const Location& dest, const Location& src) {
        //
    }

    void and_op(const Location& dest, const Location& lhs, const Location& rhs) {
        switch (_arch) {
            case X86_64:
                x64_binary(x64_arg(dest), x64_arg(lhs), x64_arg(rhs), x64_clobber(0), x64::and_);                                          
                return;
            default:
                return;
        }
    }

    void or_op(const Location& dest, const Location& lhs, const Location& rhs) {
        switch (_arch) {
            case X86_64:
                x64_binary(x64_arg(dest), x64_arg(lhs), x64_arg(rhs), x64_clobber(0), x64::or_);                                          
                return;
            default:
                return;
        }
    }

    void xor_op(const Location& dest, const Location& lhs, const Location& rhs) {
        switch (_arch) {
            case X86_64:
                x64_binary(x64_arg(dest), x64_arg(lhs), x64_arg(rhs), x64_clobber(0), x64::xor_);                                          
                return;
            default:
                return;
        }
    }

    void not_op(const Location& dest, const Location& src) {
        switch (_arch) {
            case X86_64: {
                using namespace x64;
                x64_binary(x64_arg(dest), x64_arg(src), imm64(0), x64_clobber(0), x64::cmp);                                          
                setcc(x64_arg(dest), EQUAL);
                return;
            }
            default:
                return;
        }
    }

    void equal(const Location& dest, const Location& lhs, const Location& rhs) {
        switch (_arch) {
            case X86_64:
                x64_compare(x64_arg(lhs), x64_arg(rhs), x64_clobber(0));  
                x64_move(x64_arg(dest), x64::imm64(0), x64_clobber(0));                                  
                x64::setcc(x64_arg(dest), x64::EQUAL);
                return;
            default:
                return;
        }
    }

    void not_equal(const Location& dest, const Location& lhs, const Location& rhs) {
        switch (_arch) {
            case X86_64:
                x64_compare(x64_arg(lhs), x64_arg(rhs), x64_clobber(0));  
                x64_move(x64_arg(dest), x64::imm64(0), x64_clobber(0));                                     
                x64::setcc(x64_arg(dest), x64::NOT_EQUAL);
                return;
            default:
                return;
        }
    }

    void less(const Location& dest, const Location& lhs, const Location& rhs) {
        switch (_arch) {
            case X86_64:
                x64_compare(x64_arg(lhs), x64_arg(rhs), x64_clobber(0));  
                x64_move(x64_arg(dest), x64::imm64(0), x64_clobber(0));                                  
                x64::setcc(x64_arg(dest), x64::LESS);
                return;
            default:
                return;
        }
    }

    void less_equal(const Location& dest, const Location& lhs, const Location& rhs) {
        switch (_arch) {
            case X86_64:
                x64_compare(x64_arg(lhs), x64_arg(rhs), x64_clobber(0));  
                x64_move(x64_arg(dest), x64::imm64(0), x64_clobber(0));                            
                x64::setcc(x64_arg(dest), x64::LESS_OR_EQUAL);
                return;
            default:
                return;
        }
    }

    void greater(const Location& dest, const Location& lhs, const Location& rhs) {
        switch (_arch) {
            case X86_64:
                x64_compare(x64_arg(lhs), x64_arg(rhs), x64_clobber(0));  
                x64_move(x64_arg(dest), x64::imm64(0), x64_clobber(0));                               
                x64::setcc(x64_arg(dest), x64::GREATER);
                return;
            default:
                return;
        }
    }

    void greater_equal(const Location& dest, const Location& lhs, const Location& rhs) {
        switch (_arch) {
            case X86_64:
                x64_compare(x64_arg(lhs), x64_arg(rhs), x64_clobber(0));  
                x64_move(x64_arg(dest), x64::imm64(0), x64_clobber(0));                                       
                x64::setcc(x64_arg(dest), x64::GREATER_OR_EQUAL);
                return;
            default:
                return;
        }
    }

    void lea(const Location& dest, const Location& src) {
        switch (_arch) {
            case X86_64: {
                using namespace x64;
                auto d = x64_arg(dest);
                if (is_memory(d.type)) {
                    lea(x64_clobber(0), x64_arg(src));
                    x64_move(d, x64_clobber(0), x64_clobber(0));
                }
                else lea(d, x64_arg(src));
                return;
            }
            default:
                return;
        }
    }

    void jump(const Location& dest) {
        switch (_arch) {
            case X86_64:
                x64::jmp(x64_arg(dest));
                return;
            default:
                return;
        }
    }

    void jump_if_zero(const Location& dest, const Location& cond) {
        switch (_arch) {
            case X86_64:
                x64_compare(x64_arg(cond), x64::imm64(0), x64_clobber(0));
                x64::jcc(x64_arg(dest), x64::EQUAL);
                return;
            default:
                return;
        }
    }

    void set_arg(u32 i, const Location& src) {
        switch (_arch) {
            case X86_64:
                x64_move(x64_param(i), x64_arg(src), x64_clobber(0));
                return;
            default:
                return;
        }
    }

    void get_arg(const Location& dest, u32 i) {
        switch (_arch) {
            case X86_64:
                x64_move(x64_arg(dest), x64_param(i), x64_clobber(0));
                return;
            default:
                return;
        }
    }

    void call(const Location& dest, const Location& func) {
        switch (_arch) {
            case X86_64:
                x64::call(x64_arg(func));
                x64_move(x64_arg(dest), x64_clobber(0), x64_clobber(0));
                return;
            default:
                return;
        }
    }

    void global_label(const string& name) {
        switch (_arch) {
            case X86_64:
                x64::label(jasmine::global((const char*)name.raw()));
                return;
            default:
                return;
        }
    }

    void local_label(const string& name) {
        switch (_arch) {
            case X86_64:
                x64::label(jasmine::local((const char*)name.raw()));
                return;
            default:
                return;
        }
    }

    void push(const Location& src) {
        switch (_arch) {
            case X86_64:
                x64::push(x64_arg(src));
                return;
            default:
                return;
        }
    }

    void pop(const Location& dest) {
        switch (_arch) {
            case X86_64:
                x64::pop(x64_arg(dest));
                return;
            default:
                return;
        }
    }

    void open_frame(u32 size) {
        switch (_arch) {
            case X86_64: {
                using namespace x64;
                if (size) {
                    push(r64(RBP));
                    mov(r64(RBP), r64(RSP));
                    sub(r64(RSP), imm64(size));
                }
                return;
            }
            default:
                return;
        }
    }

    void close_frame(u32 size) {
        switch (_arch) {
            case X86_64: {
                using namespace x64;
                if (size) {
                    mov(r64(RSP), r64(RBP));
                    pop(r64(RBP));
                }
                ret();
                return;
            }
            default:
                return;
        }
    }
}