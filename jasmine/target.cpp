/*
 * Copyright (c) 2021, the Basil authors
 * All rights reserved.
 *
 * This source code is licensed under the 3-Clause BSD License, the full text
 * of which can be found in the LICENSE file in the root directory
 * of this project.
 */

#include "target.h"
#include "bc.h"
#include "x64.h"

namespace jasmine {
    static const bitset EMPTY = {};

    namespace x64 {
        static const GenericRegister GP_REGS[] = {
            RAX, RCX, RDX, RBX, /* RSP, RBP, */ RSI, RDI,
            R8, R9, R10, R11, R12, R13, R14, R15
        };

        static const GenericRegister FP_REGS[] = {
            XMM0, XMM1, XMM2, XMM3, XMM4, XMM5, XMM6, XMM7,
            XMM8, XMM9, XMM10, XMM11, XMM12, XMM13, XMM14, XMM15
        };

        static const GenericRegister GP_ARGS_SYSV[] = {
            RDI, RSI, RDX, RCX, R8, R9
        };

        static const GenericRegister FP_ARGS_SYSV[] = {
            XMM0, XMM1, XMM2, XMM3, XMM4, XMM5, XMM6, XMM7
        };

        static bitset from_slice(const_slice<GenericRegister> regs) {
            bitset b;
            for (GenericRegister reg : regs) b.insert(reg);
            return b;
        }
        
        static const bitset GP_REGSET = from_slice({ 14, GP_REGS }),
                            FP_REGSET = from_slice({ 16, FP_REGS }),
                            GP_ARGSET_SYSV = from_slice({ 6, GP_ARGS_SYSV }),
                            FP_ARGSET_SYSV = from_slice({ 8, FP_ARGS_SYSV });

        const_slice<GenericRegister> registers(Kind kind) {
            switch (kind) {
                case K_F32:
                case K_F64:
                    return { 16, FP_REGS };
                case K_I8:
                case K_I16:
                case K_I32:
                case K_I64:
                case K_U8:
                case K_U16:
                case K_U32:
                case K_U64:
                case K_PTR:
                    return { 16, GP_REGS };
                case K_STRUCT:
                    return { 0, nullptr };
                default:
                    panic("Unimplemented kind!");
                    return { 0, nullptr };
            }
        }

        const bitset& register_set(Kind kind) {
            switch (kind) {
                case K_F32:
                case K_F64:
                    return FP_REGSET;
                case K_I8:
                case K_I16:
                case K_I32:
                case K_I64:
                case K_U8:
                case K_U16:
                case K_U32:
                case K_U64:
                case K_PTR:
                    return GP_REGSET;
                case K_STRUCT:
                    return EMPTY;
                default:
                    panic("Unimplemented kind!");
                    return EMPTY;
            }
        }

        const_slice<GenericRegister> parameter_registers(Kind kind, OS os) {
            switch (kind) {
                case K_F32:
                case K_F64: switch (os) {
                    case LINUX: 
                    case MACOS:
                        return { 8, FP_ARGS_SYSV };
                    default:
                        panic("Unimplemented OS!");
                        return { 0, nullptr };
                }
                case K_I8:
                case K_I16:
                case K_I32:
                case K_I64:
                case K_U8:
                case K_U16:
                case K_U32:
                case K_U64:
                case K_PTR: switch (os) {
                    case LINUX: 
                    case MACOS:
                        return { 6, GP_ARGS_SYSV };
                    default:
                        panic("Unimplemented OS!");
                        return { 0, nullptr };
                }
                case K_STRUCT:
                    return { 0, nullptr };
                default:
                    panic("Unimplemented kind!");
                    return { 0, nullptr };
            }
        }

        const bitset& parameter_register_set(Kind kind, OS os) {
            switch (kind) {
                case K_F32:
                case K_F64: switch (os) {
                    case LINUX: 
                    case MACOS:
                        return FP_ARGSET_SYSV;
                    default:
                        panic("Unimplemented OS!");
                        return EMPTY;
                }
                case K_I8:
                case K_I16:
                case K_I32:
                case K_I64:
                case K_U8:
                case K_U16:
                case K_U32:
                case K_U64:
                case K_PTR: switch (os) {
                    case LINUX: 
                    case MACOS:
                        return GP_ARGSET_SYSV;
                    default:
                        panic("Unimplemented OS!");
                        return EMPTY;
                }
                case K_STRUCT:
                    return EMPTY;
                default:
                    panic("Unimplemented kind!");
                    return EMPTY;
            }
        }

        bitset clobbers(const Insn& insn, const Target& target) {    
            bitset clobbers;    
            switch (insn.opcode) {
                case OP_DIV:
                case OP_REM:
                    clobbers.insert(RAX);
                    clobbers.insert(RDX);
                    if (insn.params[2].kind == PK_IMM) clobbers.insert(RCX); // we need a register to hold immediate divisors
                    break;
                case OP_MUL:    // these instructions don't permit memory destinations or immediates,
                case OP_ZXT:    // so we reserve rax just in case
                case OP_EXT:
                    clobbers.insert(RAX);
                    break;
                case OP_CALL: {
                    // clobber return value
                    if (auto reg = target.locate_return_value(insn.type.kind).reg) {
                        clobbers.insert(*reg);
                    }
                    else if (insn.type.kind == K_STRUCT) switch (target.os) {
                        case MACOS:
                        case LINUX:
                            clobbers.insert(RDI); // rdi stores address of returned structs
                            break;
                        default:
                            panic("Unimplemented OS!");
                            break;
                    }

                    // reserve rax just in case
                    clobbers.insert(RAX);

                    // we don't handle parameters here so we can do smarter parameter handling
                    // in the actual code generator
                    break;
                }
                default:
                    break;
            }
            return clobbers;
        }

        void hint(const Insn& insn, const vector<LiveRange*>& params, const Target& target) {  
            switch (insn.opcode) {
                case OP_DIV:
                case OP_REM:
                    if (params[0]) params[0]->hint = some<GenericRegister>(RAX);
                    break;
                case OP_CALL: {
                    // return value hint
                    if (params[0]) params[0]->hint = target.locate_return_value(params[0]->type.kind).reg;

                    const bitset& gp = target.parameter_register_set(K_PTR);
                    const bitset& fp = target.parameter_register_set(K_F64);
                    auto gp_it = gp.begin(), fp_it = fp.begin();
                    for (u32 i = 2; i < params.size(); i ++) {
                        if (params[i]) {
                            if ((params[i]->type.kind == K_F32 || params[i]->type.kind == K_F64) 
                                && fp_it != fp.end())
                                params[i]->hint = some<GenericRegister>(*fp_it), ++ fp_it;
                            else if ((params[i]->type.kind != K_F32 && params[i]->type.kind != K_F64) 
                                && gp_it != gp.end()) 
                                params[i]->hint = some<GenericRegister>(*gp_it), ++ gp_it;
                        }
                    }
                    break;
                }
                case OP_PARAM: {
                    // println((bool)params[0], " ", params[0] ? (bool)params[0]->param_idx : false);
                    if (params[0] && params[0]->param_idx) {
                        params[0]->hint = some<GenericRegister>(
                            target.parameter_registers(params[0]->type.kind)[*params[0]->param_idx]
                        );
                    }
                    break;
                }
                default:
                    break;
            }
        }
    }
    
    Location loc_reg(GenericRegister reg) {
        return { LT_REGISTER, some<GenericRegister>(reg), none<i64>() };
    }

    Location loc_stack(i64 offset) {
        return { LT_STACK_MEMORY, none<GenericRegister>(), some<i64>(offset) };
    }

    const_slice<GenericRegister> Target::registers(Kind kind) const {
        switch (arch) {
            case X86_64: return x64::registers(kind);
            default:
                panic("Unimplemented architecture!");
                return { 0, nullptr };
        }
    }
    
    const bitset& Target::register_set(Kind kind) const {
        switch (arch) {
            case X86_64: return x64::register_set(kind);
            default:
                panic("Unimplemented architecture!");
                return EMPTY;
        }
    }
    
    const_slice<GenericRegister> Target::parameter_registers(Kind kind) const {
        switch (arch) {
            case X86_64: return x64::parameter_registers(kind, os);
            default:
                panic("Unimplemented architecture!");
                return { 0, nullptr };
        }
    }

    const bitset& Target::parameter_register_set(Kind kind) const {
        switch (arch) {
            case X86_64: return x64::parameter_register_set(kind, os);
            default:
                panic("Unimplemented architecture!");
                return EMPTY;
        }
    }

    vector<Location> Target::place_parameters(const vector<Kind>& param_kinds) const {
        vector<Location> param_locs;
        u32 fp_idx = 0, gp_idx = 0;
        for (Kind k : param_kinds) {
            switch (k) {
                case K_F32:
                case K_F64: {
                    auto regs = parameter_registers(k);
                    if (fp_idx < regs.size()) param_locs.push(loc_reg(regs[fp_idx ++]));
                    else switch (os) {
                        case MACOS:
                        case LINUX:
                            param_locs.push({ LT_PUSHED_R2L, none<GenericRegister>(), none<i64>() });
                            break;
                        default:
                            panic("Unimplemented OS!");
                            break;
                    }
                }
                case K_I8:
                case K_I16:
                case K_I32:
                case K_I64:
                case K_U8:
                case K_U16:
                case K_U32:
                case K_U64:
                case K_PTR: {
                    auto regs = parameter_registers(k);
                    if (gp_idx < regs.size()) param_locs.push(loc_reg(regs[gp_idx ++]));
                    else switch (os) {
                        case MACOS:
                        case LINUX:
                            param_locs.push({ LT_PUSHED_R2L, none<GenericRegister>(), none<i64>() });
                            break;
                        default:
                            panic("Unimplemented OS!");
                            break;
                    }
                }
                case K_STRUCT:
                    param_locs.push({ LT_PUSHED_R2L, none<GenericRegister>(), none<i64>() });
                    break;
                default:
                    panic("Unimplemented kind!");
                    break;
            }
        }
        return param_locs;
    }

    Location Target::locate_return_value(Kind kind) const {
        switch (arch) {
            case X86_64: switch (os) {
                case MACOS:
                case LINUX: switch (kind) {
                    case K_F32:
                    case K_F64:
                        return { LT_REGISTER, some<GenericRegister>(x64::XMM0), none<i64>() };
                    case K_I8:
                    case K_I16:
                    case K_I32:
                    case K_I64:
                    case K_U8:
                    case K_U16:
                    case K_U32:
                    case K_U64:
                    case K_PTR:
                        return { LT_REGISTER, some<GenericRegister>(x64::RAX), none<i64>() };
                    case K_STRUCT:
                        return { LT_STACK_MEMORY, none<GenericRegister>(), none<i64>() };
                    default:
                        panic("Unimplemented kind!");
                        return { LT_NONE, none<GenericRegister>(), none<i64>() };
                }
                default:
                    panic("Unimplemented OS!");
                    return { LT_NONE, none<GenericRegister>(), none<i64>() };
            }
            default:
                panic("Unimplemented architecture!");
                return { LT_NONE, none<GenericRegister>(), none<i64>() };
        }
    }

    bitset Target::clobbers(const Insn& insn) const {
        switch (arch) {
            case X86_64: return x64::clobbers(insn, *this);
            default:
                panic("Unimplemented architecture!");
                return EMPTY;
        }
    }
    
    void Target::hint(const Insn& insn, const vector<LiveRange*>& params) const {
        switch (arch) {
            case X86_64: return x64::hint(insn, params, *this);
            default:
                panic("Unimplemented architecture!");
                return;
        }
    }
}