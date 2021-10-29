/*
 * Copyright (c) 2021, the Basil authors
 * All rights reserved.
 *
 * This source code is licensed under the 3-Clause BSD License, the full text
 * of which can be found in the LICENSE file in the root directory
 * of this project.
 */

#include "x64.h"
#include "stdio.h"
#include "stdlib.h"

namespace jasmine::x64 {
    const char* REGISTER_NAMES[] = {
        "rax", "rcx", "rdx", "rbx", "rsp", "rbp", "rsi", "rdi",
        "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15"
    };

    static const char* size_names[] = {
        "byte", "word", "dword", "qword", "auto"
    };

    // the object machine code is written to
    static jasmine::Object* target = nullptr;

    void writeto(jasmine::Object& buf) {
        target = &buf;
    }

    ScaledRegister::ScaledRegister(Register reg_in, Scale scale_in):
        reg(reg_in), scale(scale_in) {
        //
    }

    ScaledRegister operator*(Register reg, int num) {
        Scale scale = SCALE1;
        switch (num) {
            case 1:
                scale = SCALE1;
                break;
            case 2:
                scale = SCALE2;
                break;
            case 4:
                scale = SCALE4;
                break;
            case 8:
                scale = SCALE8;
                break;
            default:
                fprintf(stderr, "[ERROR] Unknown scale factor '%d'.\n", num);
                exit(1);
        }
        return ScaledRegister{ reg, scale };
    }

    bool is_register(ArgType type) {
        return type >= REGISTER8 && type <= REGISTER64;
    }

    bool is_register_offset(ArgType type) {
        return (type >= REGISTER_OFFSET8 && type <= REGISTER_OFFSET64) || type == REGISTER_OFFSET_AUTO;
    }

    bool is_immediate(ArgType type) {
        return (type >= IMM8 && type <= IMM64) || type == IMM_AUTO;
    }

    bool is_memory(ArgType type) {
        return type >= REGISTER_LABEL8 && type < IMM_AUTO;
    }

    bool is_label(ArgType type) {
        return (type >= LABEL8 && type <= LABEL64) || type == LABEL_AUTO;
    }

    bool is_absolute(ArgType type) {
        return (type >= ABSOLUTE8 && type <= ABSOLUTE64) || type == ABSOLUTE_AUTO;
    }

    bool is_rip_relative(ArgType type) {
        return (type >= RIPRELATIVE8 && type <= RIPRELATIVE64) || type == RIPRELATIVE_AUTO;
    }

    bool is_displacement_only(ArgType type) {
        return is_absolute(type) || is_rip_relative(type);
    }

    Size operand_size(ArgType type) {
        if (type >= IMM_AUTO) return AUTO;
        return (Size)((u8)type & 3);
    }

    Register base_register(const Arg& arg) {
        switch (arg.type) {
            case REGISTER_LABEL8:
            case REGISTER_LABEL16:
            case REGISTER_LABEL32:
            case REGISTER_LABEL64:
            case REGISTER_LABEL_AUTO:
                return arg.data.register_label.base;
            case REGISTER_OFFSET8:
            case REGISTER_OFFSET16:
            case REGISTER_OFFSET32:
            case REGISTER_OFFSET64:
            case REGISTER_OFFSET_AUTO:
                return arg.data.register_offset.base;
            case REGISTER8:
            case REGISTER16:
            case REGISTER32:
            case REGISTER64:
                return arg.data.reg;
            case SCALED_INDEX8:
            case SCALED_INDEX16:
            case SCALED_INDEX32:
            case SCALED_INDEX64:
            case SCALED_INDEX_AUTO:
                return arg.data.scaled_index.base;
            default:
                return INVALID;
        }
    }

    bool is_64bit_register(Register r) {
        return r >= R8 && r <= R15;
    }

    i64 memory_displacement(const Arg& arg) {
        switch (arg.type) {
            case REGISTER_OFFSET8:
            case REGISTER_OFFSET16:
            case REGISTER_OFFSET32:
            case REGISTER_OFFSET64:
            case REGISTER_OFFSET_AUTO:
                return arg.data.register_offset.offset;
            case SCALED_INDEX8:
            case SCALED_INDEX16:
            case SCALED_INDEX32:
            case SCALED_INDEX64:
            case SCALED_INDEX_AUTO:
                return arg.data.scaled_index.offset;
                    case ABSOLUTE8:
                    case ABSOLUTE16:
                    case ABSOLUTE32:
                    case ABSOLUTE64:
                    case ABSOLUTE_AUTO:
                        return arg.data.absolute;
                    case RIPRELATIVE8:
                    case RIPRELATIVE16:
                    case RIPRELATIVE32:
                    case RIPRELATIVE64:
                    case RIPRELATIVE_AUTO:
                        return arg.data.rip_relative;
            default: 
                return 0;
        }
    }

    i64 immediate_value(const Arg& arg) {
        switch (arg.type) {
            case IMM8:
                return arg.data.imm8;
            case IMM16:
                return arg.data.imm16;
            case IMM32:
                return arg.data.imm32;
            case IMM64:
            case IMM_AUTO:
                return arg.data.imm64;
            default:
                return 0;
        }
    }

    bool is_scaled_addressing(ArgType type) {
        return (type >= SCALED_INDEX8 && type <= SCALED_INDEX64)
            || type == SCALED_INDEX_AUTO;
    }

    bool operator==(const Arg& lhs, const Arg& rhs) {
        if (lhs.type != rhs.type) return false;
        if (is_register(lhs.type)) return lhs.data.reg == rhs.data.reg;
        if (is_immediate(lhs.type)) return lhs.data.imm64 == rhs.data.imm64;
        if (is_label(lhs.type)) return lhs.data.label.id == rhs.data.label.id;
        if (is_absolute(lhs.type)) return lhs.data.absolute == rhs.data.absolute;
        if (is_rip_relative(lhs.type)) return lhs.data.rip_relative == rhs.data.rip_relative;
        if (is_scaled_addressing(lhs.type)) return lhs.data.scaled_index.base == rhs.data.scaled_index.base
            && lhs.data.scaled_index.index == rhs.data.scaled_index.index
            && lhs.data.scaled_index.scale == rhs.data.scaled_index.scale
            && lhs.data.scaled_index.offset == rhs.data.scaled_index.offset;
        if (is_memory(lhs.type)) return lhs.data.register_offset.base == rhs.data.register_offset.base
            && lhs.data.register_offset.offset == rhs.data.register_offset.offset;
        return false;
    }
    
    bool operator!=(const Arg& lhs, const Arg& rhs) {
        return !(lhs == rhs);
    }

    jasmine::RefType relative(Size size) {
        switch (size) {
            case BYTE: return jasmine::REL8;
            case WORD: return jasmine::REL16_LE;
            case DWORD: return jasmine::REL32_LE;
            case QWORD: return jasmine::REL64_LE;
            default: {
                fprintf(stderr, "[ERROR] Invalid size for relative reference.\n");
                exit(1);
            }
        }
    }

		jasmine::RefType absolute(Size size) {
        switch (size) {
            case BYTE: return jasmine::ABS8;
            case WORD: return jasmine::ABS16_LE;
            case DWORD: return jasmine::ABS32_LE;
            case QWORD: return jasmine::ABS64_LE;
            default: {
                fprintf(stderr, "[ERROR] Invalid size for absolute reference.\n");
                exit(1);
            }
        }
    }

    Arg imm8(i8 value) {
        Arg arg;
        arg.data.imm8 = value;
        arg.type = IMM8;
        return arg;
    }

    Arg imm16(i16 value) {
        Arg arg;
        arg.data.imm16 = value;
        arg.type = IMM16;
        return arg;
    }

    Arg imm32(i32 value) {
        Arg arg;
        arg.data.imm32 = value;
        arg.type = IMM32;
        return arg;
    }

    Arg imm64(i64 value) {
        Arg arg;
        arg.data.imm64 = value;
        arg.type = IMM64;
        return arg;
    }

    Arg imm(i64 value) {
        Arg arg;
        arg.data.imm64 = value;
        arg.type = IMM_AUTO;
        return arg;
    }

    Arg r8(Register reg) {
        Arg arg;
        arg.data.reg = reg;
        arg.type = REGISTER8;
        return arg;
    }

    Arg r16(Register reg) {
        Arg arg;
        arg.data.reg = reg;
        arg.type = REGISTER16;
        return arg;
    }

    Arg r32(Register reg) {
        Arg arg;
        arg.data.reg = reg;
        arg.type = REGISTER32;
        return arg;
    }

    Arg r64(Register reg) {
        Arg arg;
        arg.data.reg = reg;
        arg.type = REGISTER64;
        return arg;
    }

    Arg m8(Register reg, i64 offset) {
        Arg arg;
        arg.data.register_offset = { reg, offset };
        arg.type = REGISTER_OFFSET8;
        return arg;
    }

    Arg m16(Register reg, i64 offset) {
        Arg arg;
        arg.data.register_offset = { reg, offset };
        arg.type = REGISTER_OFFSET16;
        return arg;
    }

    Arg m32(Register reg, i64 offset) {
        Arg arg;
        arg.data.register_offset = { reg, offset };
        arg.type = REGISTER_OFFSET32;
        return arg;
    }

    Arg m64(Register reg, i64 offset) {
        Arg arg;
        arg.data.register_offset = { reg, offset };
        arg.type = REGISTER_OFFSET64;
        return arg;
    }

    Arg mem(Register reg, i64 offset) {
        Arg arg;
        arg.data.register_offset = { reg, offset };
        arg.type = REGISTER_OFFSET_AUTO;
        return arg;
    }

    Arg m8(Register base, Register index, Scale scale, i64 offset) {
        Arg arg;
        arg.data.scaled_index = { base, index, scale, offset };
        arg.type = SCALED_INDEX8;
        return arg;
    }

    Arg m16(Register base, Register index, Scale scale, i64 offset) {
        Arg arg;
        arg.data.scaled_index = { base, index, scale, offset };
        arg.type = SCALED_INDEX16;
        return arg;
    }

    Arg m32(Register base, Register index, Scale scale, i64 offset) {
        Arg arg;
        arg.data.scaled_index = { base, index, scale, offset };
        arg.type = SCALED_INDEX32;
        return arg;
    }

    Arg m64(Register base, Register index, Scale scale, i64 offset) {
        Arg arg;
        arg.data.scaled_index = { base, index, scale, offset };
        arg.type = SCALED_INDEX64;
        return arg;
    }

    Arg mem(Register base, Register index, Scale scale, i64 offset) {
        Arg arg;
        arg.data.scaled_index = { base, index, scale, offset };
        arg.type = SCALED_INDEX_AUTO;
        return arg;
    }

    Arg m8(Register base, ScaledRegister index, i64 offset) {
        return m8(base, index.reg, index.scale, offset);
    }

    Arg m16(Register base, ScaledRegister index, i64 offset) {
        return m16(base, index.reg, index.scale, offset);
    }
    
    Arg m32(Register base, ScaledRegister index, i64 offset) {
        return m32(base, index.reg, index.scale, offset);
    }
    
    Arg m64(Register base, ScaledRegister index, i64 offset) {
        return m64(base, index.reg, index.scale, offset);
    }
    
    Arg mem(Register base, ScaledRegister index, i64 offset) {
        return mem(base, index.reg, index.scale, offset);
    }

		Arg abs8(i64 offset) {
			Arg arg;
			arg.data.absolute = offset;
			arg.type = ABSOLUTE8;
			return arg;
		}

		Arg abs16(i64 offset) {
			Arg arg;
			arg.data.absolute = offset;
			arg.type = ABSOLUTE16;
			return arg;
		}

		Arg abs32(i64 offset) {
			Arg arg;
			arg.data.absolute = offset;
			arg.type = ABSOLUTE32;
			return arg;
		}

		Arg abs64(i64 offset) {
			Arg arg;
			arg.data.absolute = offset;
			arg.type = ABSOLUTE64;
			return arg;
		}

		Arg abs(i64 offset) {
			Arg arg;
			arg.data.absolute = offset;
			arg.type = ABSOLUTE_AUTO;
			return arg;
		}

		Arg riprel8(i64 offset) {
			Arg arg;
			arg.data.rip_relative = offset;
			arg.type = RIPRELATIVE8;
			return arg;
		}

		Arg riprel16(i64 offset) {
			Arg arg;
			arg.data.rip_relative = offset;
			arg.type = RIPRELATIVE16;
			return arg;
		}

		Arg riprel32(i64 offset) {
			Arg arg;
			arg.data.rip_relative = offset;
			arg.type = RIPRELATIVE32;
			return arg;
		}

		Arg riprel64(i64 offset) {
			Arg arg;
			arg.data.rip_relative = offset;
			arg.type = RIPRELATIVE64;
			return arg;
		}

		Arg riprel(i64 offset) {
			Arg arg;
			arg.data.rip_relative = offset;
			arg.type = RIPRELATIVE_AUTO;
			return arg;
		}

    Arg label8(jasmine::Symbol symbol) {
        Arg arg;
        arg.data.label = symbol;
        arg.type = LABEL8;
        return arg;
    }

    Arg label16(jasmine::Symbol symbol) {
        Arg arg;
        arg.data.label = symbol;
        arg.type = LABEL16;
        return arg;
    }
    
    Arg label32(jasmine::Symbol symbol) {
        Arg arg;
        arg.data.label = symbol;
        arg.type = LABEL32;
        return arg;
    }
    
    Arg label64(jasmine::Symbol symbol) {
        Arg arg;
        arg.data.label = symbol;
        arg.type = LABEL64;
        return arg;
    }

    void verify_buffer() {
        if (!target) { // requires that a buffer exist to write to
            fprintf(stderr, "[ERROR] Cannot assemble; no target buffer set.\n");
            exit(1);
        }
        if (target->get_target().arch != jasmine::X86_64) {
            fprintf(stderr, "[ERROR] Target buffer is not targeting the x86_64 architecture.");
            exit(1);
        }
    }

    void verify_args(const Arg& dest, const Arg& src) {
        if (is_memory(dest.type) && is_memory(src.type)) { 
            // multiple memory parameters is illegal
            fprintf(stderr, "[ERROR] More than one memory parameter in instruction.\n");
            exit(1);
        }
        if (is_immediate(dest.type)) {
            // cannot write to immediate
            fprintf(stderr, "[ERROR] Destination parameter cannot be immediate.\n");
            exit(1);
        }
    }

    Size resolve_size(const Arg& src, Size target) {
        Size src_size = operand_size(src.type);
        if (target == AUTO) {
            if (src_size == AUTO) {
                fprintf(stderr, "[ERROR] Ambiguous size for instruction.\n");
                exit(1);
            }
            return src_size;
        }
        else if (src_size != target && src_size != AUTO) {
            // src is incompatible with explicit instruction size
            fprintf(stderr, "[ERROR] Incompatible size; source has size '%s'"
                ", but instruction has size '%s'.", size_names[src_size], 
                size_names[target]);
            exit(1);
        }

        return target; // return explicit size if no inference occurred
    }

    Size resolve_size(const Arg& dest, const Arg& src, Size target) {
        Size dest_size = operand_size(dest.type);
        Size src_size = operand_size(src.type);

        if (target == AUTO) {
            if (dest_size == AUTO && src_size == AUTO) {
                fprintf(stderr, "[ERROR] Ambiguous size for instruction.\n");
                exit(1);
            }
            else if (dest_size == AUTO) { // infer size from source
                return src_size;
            }
            else if (src_size == AUTO) { // infer size from destination
                return dest_size;
            }
            else if (dest_size != src_size) { // incompatible sizes
                fprintf(stderr, "[ERROR] Incompatible operand sizes; destination has "
                    "size '%s', but source has size '%s'.\n", 
                    size_names[dest_size], size_names[src_size]);
                exit(1);
            }
            return dest_size; // dest and source size must be equal and non-auto
        }
        else if (dest_size != target && dest_size != AUTO) {
            // dest is incompatible with explicit instruction size
            fprintf(stderr, "[ERROR] Incompatible size; destination has size '%s'"
                ", but instruction has size '%s'.\n", size_names[dest_size], 
                size_names[target]);
            exit(1);
        }
        else if (src_size != target && src_size != AUTO) {
            // src is incompatible with explicit instruction size
            fprintf(stderr, "[ERROR] Incompatible size; source has size '%s'"
                ", but instruction has size '%s'.", size_names[src_size], 
                size_names[target]);
            exit(1);
        }

        return target; // return explicit size if no inference occurred
    }

    void emitprefix(const Arg& dest, Size size) {
        if (size == WORD) target->code().write<u8>(0x66); // 16-bit prefix
        u8 rex = 0x40;
        Register dest_reg = base_register(dest);

        if (operand_size(dest.type) == BYTE || size == BYTE) {
            // we need some REX prefix to enable the use of bpl, spl, dil, and sil registers
            if (dest_reg >= RBP && dest_reg < R8) rex |= 2; // we use the SIB bit since SIB is forbidden in 8-bit mode
        }
        
        if (is_64bit_register(dest_reg)) rex |= 1; // 64-bit r/m field

        if (is_scaled_addressing(dest.type) && 
            is_64bit_register(dest.data.scaled_index.index)) rex |= 2; // 64-bit SIB index
        
        if (size == QWORD) rex |= 8; // 64-bit operand size
        if (rex > 0x40) target->code().write(rex);
    }

    void emitprefix(const Arg& dest, const Arg& src, Size size) {
        if (size == WORD) target->code().write<u8>(0x66); // 16-bit prefix
        u8 rex = 0x40;
        Register src_reg = base_register(src);
        Register dest_reg = base_register(dest);

        if (operand_size(dest.type) == BYTE || operand_size(src.type) == BYTE || size == BYTE) {
            // we need some REX prefix to enable the use of bpl, spl, dil, and sil registers
            if ((src_reg >= RBP && src_reg < R8) || (dest_reg >= RBP && dest_reg < R8))
                rex |= 2; // we use the SIB bit since SIB is forbidden in 8-bit mode
        }
        
        if (is_memory(src.type)) {
            Register temp = src_reg;
            src_reg = dest_reg;
            dest_reg = temp;
        }
        if (is_64bit_register(dest_reg)) rex |= 1; // 64-bit r/m field

        if (is_scaled_addressing(dest.type) && 
            is_64bit_register(dest.data.scaled_index.index)) rex |= 2; // 64-bit SIB index
        if (is_scaled_addressing(src.type) && 
            is_64bit_register(src.data.scaled_index.index)) rex |= 2; // 64-bit SIB index
        
        if (is_64bit_register(src_reg)) rex |= 4; // 64-bit reg field
        
        if (size == QWORD) rex |= 8; // 64-bit operand size
        if (rex > 0x40) target->code().write(rex);
    }

    void write_immediate(const Arg& src, Size size, bool allow64 = false) {
        switch (size) {
            case BYTE:
                target->code().write(src.data.imm8);
                break;
            case WORD:
                target->code().write(little_endian(src.data.imm16));
                break;
            case DWORD:
                target->code().write(little_endian(src.data.imm32));
                break;
            case QWORD:
                if (allow64) {
                    target->code().write(little_endian(src.data.imm64));
                    break;
                }
                if ((src.data.imm64 > 0 ? src.data.imm64 : -src.data.imm64) 
                    & ~0xffffffff) {
                    fprintf(stderr, "[ERROR] Cannot represent immediate in 32-bits.\n");
                    exit(1);
                }
                target->code().write(little_endian((i32)src.data.imm64));
                break;
            default:
                fprintf(stderr, "[ERROR] Source operand size cannot be determined.\n");
                exit(1);
        }
    }

    void write_immediate(i64 imm, Size size, bool allow64 = false) {
        switch (size) {
            case BYTE:
                target->code().write<i8>(imm);
                break;
            case WORD:
                target->code().write(little_endian<i16>(imm));
                break;
            case DWORD:
                target->code().write(little_endian<i32>(imm));
                break;
            case QWORD:
                if (allow64) {
                    target->code().write(little_endian<i64>(imm));
                    break;
                }
                if ((imm > 0 ? imm : -imm) 
                    & ~0xffffffff) {
                    fprintf(stderr, "[ERROR] Cannot represent immediate in 32-bits.\n");
                    exit(1);
                }
                target->code().write(little_endian<i32>(imm));
                break;
            default:
                fprintf(stderr, "[ERROR] Source operand size cannot be determined.\n");
                exit(1);
        }
    }

    void emitargs(const Arg& dest, const Arg& src, Size size, i8 imod = -1) {
        Size dest_size = operand_size(dest.type), src_size = operand_size(src.type);
        if (dest_size == AUTO) dest_size = size;
        if (src_size == AUTO) src_size = size;

        // Mod R/M byte
        // 7 - - - 5 - - - - - 2 - - - - - 0
        //  Mod     Reg         R/M
        //
        // Mod = 11 when operation is register/register
        // Mod = 00 when operation is register/memory or memory/register
        // Reg = source register (base register), or 000 if immediate
        // R/M = destination register (base register)
        u8 modrm = 0;

        // SIB byte
        // 7 - - - 5 - - - - - 2 - - - - - 0
        //  Scale   Index       Base
        // 
        // Scale can be any x64::Scale
        // Index and Base are registers
        // Specifies an address of the form (Base + Index * Scale + Offset)
        // When Index, Base = RSP, represents RSP-relative addressing
        u8 sib = 0;
        bool has_sib = false;

        // offset/displacement
        i64 disp = 0;

        if (is_memory(dest.type)) {
            disp = memory_displacement(dest);
            
            if (is_absolute(src.type)) {
                sib |= RSP << 3;
                sib |= RBP;
                has_sib = true;
            }
            else if (is_scaled_addressing(dest.type)) {
                sib |= dest.data.scaled_index.scale << 6;
                sib |= (dest.data.scaled_index.index & 7) << 3;
                sib |= (dest.data.scaled_index.base & 7);
                has_sib = true;
            }
            else if (base_register(dest) == RSP) {
                sib |= RSP << 3;
                sib |= RSP;
                has_sib = true;
            }
        }
        else if (is_memory(src.type)) {
            disp = memory_displacement(src);
            
            if (is_absolute(src.type)) {
                sib |= RSP << 3;
                sib |= RBP;
                has_sib = true;
            }
            else if (is_scaled_addressing(src.type)) {
                sib |= src.data.scaled_index.scale << 6;
                sib |= (src.data.scaled_index.index & 7) << 3;
                sib |= (src.data.scaled_index.base & 7);
                has_sib = true;
            }
            else if (base_register(src) == RSP) {
                sib |= RSP << 3;
                sib |= RSP;
                has_sib = true;
            }
        }
        else modrm |= 0b11000000; // register-register mod

        if (is_register_offset(src.type) || is_register_offset(dest.type)
            || is_scaled_addressing(src.type) || is_scaled_addressing(dest.type)) {
            if (disp > -129 && disp < 128) modrm |= 0b01000000; // 8-bit offset
            else if (disp < -0x80000000l || disp > 0x7fffffffl) {
                fprintf(stderr, "[ERROR] Cannot represent memory offset %lx "
                    "in 32 bits.\n", (long int)disp);
                exit(1);
            }
            else modrm |= 0b10000000; // 32-bit offset
        }

        if (imod != -1) modrm |= imod << 3; // opcode extension in reg
        else if (is_scaled_addressing(src.type)) 
            modrm |= (base_register(dest) & 7) << 3;
        else if (!is_displacement_only(src.type) && !is_immediate(src.type)) 
            modrm |= (base_register(src) & 7) << 3; // source register in reg

        if (is_scaled_addressing(dest.type)) modrm |= RSP;
        else if (is_scaled_addressing(src.type) || is_absolute(src.type))
            modrm |= RSP;
        else if (is_rip_relative(src.type))
            modrm |= RBP, modrm |= (base_register(dest) & 7) << 3;
        else modrm |= (base_register(dest) & 7); // destination register in r/m byte

        target->code().write(modrm);
        if (has_sib) target->code().write(sib);

        if (is_displacement_only(src.type) || is_displacement_only(dest.type)) {
            target->code().write(little_endian((i32)disp));
        }
        else if (is_register_offset(src.type) || is_register_offset(dest.type)
            || is_scaled_addressing(src.type) || is_scaled_addressing(dest.type)) {
            if (disp > -129 && disp < 128) target->code().write((i8)disp);
            else target->code().write(little_endian((i32)disp));
        }

        if (is_immediate(src.type)) write_immediate(src, src_size);
    }

    void emitargs(const Arg& src, Size size, i8 imod = -1) {
        Arg dummy;
        dummy.type = REGISTER64;
        dummy.data.reg = (Register)(u8)imod;
        emitargs(src, dummy, size, imod);
    }

    // utility function to help encode simple binary arithmetic ops
    //  - ADD OR ADC SBB AND SUB XOR CMP
    void encode_arithmetic(const Arg& dest, const Arg& src, Size actual_size, i8 op) {
        emitprefix(dest, src, actual_size);
        if (is_immediate(src.type)) { // use opcode from 0x80 - 0x83
            Size src_size = operand_size(src.type);
            if (src_size == AUTO) src_size = actual_size;

            Arg imm_src = src;

            i64 val = immediate_value(src);
            if (val > -129 && val < 128) {
                src_size = BYTE;
                imm_src.type = IMM8;
            }

            u8 opcode = 0x80;
            if (actual_size != BYTE) opcode ++; // set bottom bit for non-8-bit mode
            if (actual_size != BYTE && src_size == BYTE) opcode += 2; // set next lowest bit for 8-bit immediate
            target->code().write(opcode);

            emitargs(dest, imm_src, actual_size, op);
        }
        else { // use normal opcode
            u8 opcode = op * 8;
            if (actual_size != BYTE) opcode ++; // set bottom bit for non-8-bit mode
            if (is_memory(src.type)) opcode += 2; // set next lowest bit for memory source
            target->code().write(opcode);

			if (is_memory(src.type)) emitargs(src, dest, actual_size);
            else emitargs(dest, src, actual_size);
        }
    }

    void add(const Arg& dest, const Arg& src, Size size) {
        verify_buffer();
        verify_args(dest, src);
        Size actual_size = resolve_size(dest, src, size);

        encode_arithmetic(dest, src, actual_size, 0);
    }

    void or_(const Arg& dest, const Arg& src, Size size) {
        verify_buffer();
        verify_args(dest, src);
        Size actual_size = resolve_size(dest, src, size);

        encode_arithmetic(dest, src, actual_size, 1);
    }

    void adc(const Arg& dest, const Arg& src, Size size) {
        verify_buffer();
        verify_args(dest, src);
        Size actual_size = resolve_size(dest, src, size);

        encode_arithmetic(dest, src, actual_size, 2);
    }

    void sbb(const Arg& dest, const Arg& src, Size size) {
        verify_buffer();
        verify_args(dest, src);
        Size actual_size = resolve_size(dest, src, size);

        encode_arithmetic(dest, src, actual_size, 3);
    }

    void and_(const Arg& dest, const Arg& src, Size size) {
        verify_buffer();
        verify_args(dest, src);
        Size actual_size = resolve_size(dest, src, size);

        encode_arithmetic(dest, src, actual_size, 4);
    }

    void sub(const Arg& dest, const Arg& src, Size size) {
        verify_buffer();
        verify_args(dest, src);
        Size actual_size = resolve_size(dest, src, size);

        encode_arithmetic(dest, src, actual_size, 5);
    }

    void xor_(const Arg& dest, const Arg& src, Size size) {
        verify_buffer();
        verify_args(dest, src);
        Size actual_size = resolve_size(dest, src, size);

        encode_arithmetic(dest, src, actual_size, 6);
    }

    void cmp(const Arg& dest, const Arg& src, Size size) {
        verify_buffer();
        verify_args(dest, src);
        Size actual_size = resolve_size(dest, src, size);

        encode_arithmetic(dest, src, actual_size, 7);
    }

    void mov(const Arg& dest, const Arg& src, Size size) {
        verify_buffer();
        verify_args(dest, src);
        Size actual_size = resolve_size(dest, src, size);

        emitprefix(dest, src, actual_size);
        if (is_immediate(src.type)) {
            i64 value = immediate_value(src);
            if (is_register(dest.type) && (value < -0x80000000l || value > 0x7fffffffl)) {
                u8 opcode = 0xb0;
                if (actual_size != BYTE) opcode += 8; // set bottom bit for non-8-bit mode
                opcode += (dest.data.reg & 7);
                target->code().write<u8>(opcode);

                write_immediate(src, actual_size, true);
            }
            else {
                u8 opcode = 0xc6;
                if (actual_size != BYTE) opcode ++; // set bottom bit for non-8-bit mode
                target->code().write(opcode);

                emitargs(dest, src, actual_size);
            }
        }
        else { // use normal opcode
            u8 opcode = 0x88;
            if (actual_size != BYTE) opcode ++; // set bottom bit for non-8-bit mode
            if (is_memory(src.type)) opcode += 2; // set next lowest bit for memory source
            target->code().write(opcode);
            
            Arg realsrc = src;
            if (is_label(src.type)) realsrc = riprel64(0);

			if (is_memory(src.type)) emitargs(realsrc, dest, actual_size);
            else emitargs(dest, realsrc, actual_size);

            if (is_label(src.type))
                target->reference(src.data.label, relative(DWORD), -4);
        }
    }
    
    void movsx(const Arg& dest, const Arg& src, Size size) {
        verify_buffer();
        verify_args(dest, src);
        Size src_size = operand_size(src.type);

        if (src_size != WORD && src_size != BYTE) {
            fprintf(stderr, "[ERROR] Invalid operand size; source parameter in "
                "'movsx' instruction must be word or byte-sized.\n");
            exit(1);
        }
        if (is_immediate(src.type)) {
            fprintf(stderr, "[ERROR] Invalid operand; immediate not permitted in "
                "'movsx' instruction.\n");
            exit(1);
        }
        if (is_memory(dest.type)) {
            fprintf(stderr, "[ERROR] Invalid operand; destination in binary "
                "'movsx' instruction cannot be memory.\n");
            exit(1);
        }

        u8 opcode = src_size == WORD ? 0xBF : 0xBE;

        emitprefix(dest, src, operand_size(dest.type));
        target->code().write<u8>(0x0f);
        target->code().write<u8>(opcode);

        Arg realsrc = src;
        if (is_label(src.type)) realsrc = riprel64(0);
        emitargs(realsrc, dest, operand_size(dest.type));
    }

    void movzx(const Arg& dest, const Arg& src, Size size) {
        verify_buffer();
        verify_args(dest, src);
        Size src_size = operand_size(src.type);

        if (src_size != WORD && src_size != BYTE) {
            fprintf(stderr, "[ERROR] Invalid operand size; source parameter in "
                "'movsx' instruction must be word or byte-sized.\n");
            exit(1);
        }
        if (is_immediate(src.type)) {
            fprintf(stderr, "[ERROR] Invalid operand; immediate not permitted in "
                "'movsx' instruction.\n");
            exit(1);
        }
        if (is_memory(dest.type)) {
            fprintf(stderr, "[ERROR] Invalid operand; destination in binary "
                "'movsx' instruction cannot be memory.\n");
            exit(1);
        }

        u8 opcode = src_size == WORD ? 0xB7 : 0xB6;

        emitprefix(dest, src, operand_size(dest.type));
        target->code().write<u8>(0x0f);
        target->code().write<u8>(opcode);

        Arg realsrc = src;
        if (is_label(src.type)) realsrc = riprel64(0);
        emitargs(realsrc, dest, operand_size(dest.type));
    }

    void imul(const Arg& dest, const Arg& src, Size size) {
        verify_buffer();
        verify_args(dest, src);
        Size actual_size = resolve_size(dest, src, size);

        emitprefix(src, dest, actual_size); // operands are reversed...not sure why :p
        if (is_immediate(src.type)) {
            fprintf(stderr, "[ERROR] Invalid operand; immediate not permitted "
                "in binary 'imul' instruction.\n");
            exit(1);
        }
        else if (is_memory(dest.type)) {
            fprintf(stderr, "[ERROR] Invalid operand; destination in binary "
                "'imul' instruction cannot be memory.\n");
            exit(1);
        }
        else { // use normal opcode
            target->code().write<u8>(0x0f, 0xaf);
            emitargs(src, dest, actual_size); // operands are reversed...not sure why :p
        }
    }

    void imul(const Arg& dest, const Arg& lhs, const Arg& rhs, Size size) {
        verify_buffer();
        verify_args(dest, lhs);
        Size actual_size = resolve_size(dest, lhs, size);

        emitprefix(lhs, dest, actual_size); // operands are reversed...not sure why :p
        if (!is_immediate(rhs.type)) {
            fprintf(stderr, "[ERROR] Invalid operand; third operand of ternary "
                "'imul' instruction must be immediate.\n");
            exit(1);
        }
        else if (is_immediate(lhs.type)) {
            fprintf(stderr, "[ERROR] Invalid operand; immediate not permitted "
                "in ternary 'imul' instruction.\n");
            exit(1);
        }
        else if (is_memory(dest.type)) {
            fprintf(stderr, "[ERROR] Invalid operand; destination in binary "
                "'imul' instruction cannot be memory.\n");
            exit(1);
        }
        else { // use normal opcode
            i64 val = immediate_value(rhs);
            u8 opcode = 0x69;
            if (val > -129 && val < 128) opcode = 0x6B; // 8-bit offset
            target->code().write<u8>(opcode);
            emitargs(lhs, dest, actual_size); // operands are reversed...not sure why :p
            if (opcode == 0x6B) write_immediate(val, BYTE);
            else write_immediate(val, actual_size);
        }
    }

    void encode_shift(const Arg& dest, const Arg& shift, 
			Size dest_size, i8 op) {
        emitprefix(dest, shift, dest_size);
        if (is_immediate(shift.type)) {
            Size src_size = operand_size(shift.type);
            if (src_size == AUTO) src_size = dest_size;

            Arg imm_src = shift;

            i64 val = immediate_value(shift);
            if (val > -129 && val < 128) {
                src_size = BYTE;
                imm_src.type = IMM8;
            }

            u8 opcode = 0xc0;
            if (src_size != BYTE) {
                    fprintf(stderr, "[ERROR] Cannot shift by more than -128 - 127, "
                            "given %ld.\n", (long int)val);
                    exit(1);
            }
            if (dest_size != BYTE) opcode += 1; // use 0xc1 for larger dests
            if (val == 1) opcode += 0x10; // use 0xd0/d1 for shifts of 1
				target->code().write(opcode);

            emitargs(dest, dest_size, op);
			if (val != 1) write_immediate(val, src_size); // immedate at end
        }
        else if (is_register(shift.type)) { // use normal opcode
            if (base_register(shift) != RCX) {
                    fprintf(stderr, "[ERROR] Cannot shift by register other than CL.\n");
                    exit(1);
            }
            u8 opcode = 0xd2;
            if (dest_size != BYTE) opcode ++; // set bottom bit for non-8-bit mode
            target->code().write(opcode);
			emitargs(dest, dest_size, op);
        }
    }

	void rol(const Arg& dest, const Arg& src, Size size) {
        verify_buffer();
        verify_args(dest, src);
        Size dest_size = resolve_size(dest, size);

        encode_shift(dest, src, dest_size, 0);
	}

    void ror(const Arg& dest, const Arg& src, Size size) {
        verify_buffer();
        verify_args(dest, src);
        Size dest_size = resolve_size(dest, size);

        encode_shift(dest, src, dest_size, 1);
	}

    void rcl(const Arg& dest, const Arg& src, Size size) {
        verify_buffer();
        verify_args(dest, src);
        Size dest_size = resolve_size(dest, size);

        encode_shift(dest, src, dest_size, 2);
	}

    void rcr(const Arg& dest, const Arg& src, Size size) {
        verify_buffer();
        verify_args(dest, src);
        Size dest_size = resolve_size(dest, size);

        encode_shift(dest, src, dest_size, 3);
	}

    void shl(const Arg& dest, const Arg& src, Size size) {
        verify_buffer();
        verify_args(dest, src);
        Size dest_size = resolve_size(dest, size);

        encode_shift(dest, src, dest_size, 4);
	}

    void shr(const Arg& dest, const Arg& src, Size size) {
        verify_buffer();
        verify_args(dest, src);
        Size dest_size = resolve_size(dest, size);

        encode_shift(dest, src, dest_size, 5);
	}

    void sar(const Arg& dest, const Arg& src, Size size) {
        verify_buffer();
        verify_args(dest, src);
        Size dest_size = resolve_size(dest, size);

        encode_shift(dest, src, dest_size, 7);
	}

    void idiv(const Arg& src, Size size) {
        verify_buffer();
        Size actual_size = resolve_size(src, size);

        if (is_immediate(src.type)) {
            fprintf(stderr, "[ERROR] Invalid operand; immediate not permitted "
                "in 'idiv' instruction.\n");
            exit(1);
        }

        emitprefix(src, actual_size);
		target->code().write<u8>(actual_size == BYTE ? 0xf6 : 0xf7);
        emitargs(src, actual_size, 7);
    }

	void not_(const Arg& src, Size size) {
        verify_buffer();
        Size actual_size = resolve_size(src, size);
				
		if (is_immediate(src.type)) {
            fprintf(stderr, "[ERROR] Invalid operand; immediate not permitted "
                "in 'not' instruction.\n");
            exit(1);
		}

        emitprefix(src, actual_size);
        target->code().write<u8>(actual_size == BYTE ? 0xf6 : 0xf7);
        emitargs(src, actual_size, 2);
	}

	void neg(const Arg& src, Size size) {
        verify_buffer();
        Size actual_size = resolve_size(src, size);
				
		if (is_immediate(src.type)) {
            fprintf(stderr, "[ERROR] Invalid operand; immediate not permitted "
                "in 'not' instruction.\n");
            exit(1);
		}

        emitprefix(src, actual_size);
        target->code().write<u8>(actual_size == BYTE ? 0xf6 : 0xf7);
        emitargs(src, actual_size, 3);
	}

	void inc(const Arg& src, Size size) {
        verify_buffer();
        Size actual_size = resolve_size(src, size);
				
		if (is_immediate(src.type)) {
            fprintf(stderr, "[ERROR] Invalid operand; immediate not permitted "
                "in 'inc' instruction.\n");
            exit(1);
		}

        emitprefix(src, actual_size);
        target->code().write<u8>(actual_size == BYTE ? 0xfe : 0xff);
        emitargs(src, actual_size, 0);
    }

	void dec(const Arg& src, Size size) {
        verify_buffer();
        Size actual_size = resolve_size(src, size);
				
        if (is_immediate(src.type)) {
            fprintf(stderr, "[ERROR] Invalid operand; immediate not permitted "
                "in 'dec' instruction.\n");
            exit(1);
        }

        emitprefix(src, actual_size);
        target->code().write<u8>(actual_size == BYTE ? 0xfe : 0xff);
        emitargs(src, actual_size, 1);
    }

    void push(const Arg& src, Size size) {
        verify_buffer();
        Size actual_size = resolve_size(src, size);

        if (actual_size != QWORD) emitprefix(src, actual_size);
        if (is_immediate(src.type)) {
            if (actual_size == BYTE) target->code().write<u8>(0x6a);
            else target->code().write<u8>(0x68);
            write_immediate(src, actual_size);
        }
        else if (is_memory(src.type)) {
            target->code().write<u8>(0xff);
            emitargs(src, actual_size, 6);
        }
        else {
            target->code().write<u8>(0x50 + (src.data.reg & 7));
        }
    }

    void pop(const Arg& src, Size size) {
        verify_buffer();
        Size actual_size = resolve_size(src, size);

        if (actual_size != QWORD) emitprefix(src, actual_size);
        if (is_immediate(src.type)) {
            fprintf(stderr, "[ERROR] Invalid operand; immediate not permitted "
                "in 'pop' instruction.\n");
            exit(1);
        }
        else if (is_memory(src.type)) {
            target->code().write<u8>(0x8f);
            emitargs(src, actual_size, 0);
        }
        else {
            target->code().write<u8>(0x58 + (src.data.reg & 7));
        }
    }

    void lea(const Arg& dest, const Arg& src, Size size) {
        verify_buffer();
        Size actual_size = resolve_size(dest, src, size);

        emitprefix(src, dest, actual_size);
        if (is_immediate(src.type)) {
            fprintf(stderr, "[ERROR] Invalid operand; immediate not permitted "
                "in 'lea' instruction.\n");
            exit(1);
        }
        else if (is_register(src.type)) {
                fprintf(stderr, "[ERROR] Invalid source operand; register not permitted "
                        "in 'lea' instruction.\n");
                exit(1);
        }
        else if (is_memory(src.type)) {
            if (!is_register(dest.type)) {
                    fprintf(stderr, "[ERROR] Invalid dest operand; destination in 'lea' "
                        "instruction must be register.\n");
            }
            Arg disp = src;
            if (is_label(src.type)) disp = riprel64(0);
            target->code().write<u8>(0x8d);
            if (is_displacement_only(disp.type)) emitargs(dest, disp, actual_size);
            else emitargs(disp, dest, actual_size);
            if (is_label(src.type))
                target->reference(src.data.label, relative(DWORD), -4);
        }
    }

    void cdq() {
        verify_buffer();

        target->code().write<u8>(0x99);
    }

    void ret() {
        verify_buffer();

        target->code().write<u8>(0xc3);
    }

    void syscall() {
        verify_buffer();

        target->code().write<u8>(0x0f, 0x05);
    }

    void label(jasmine::Symbol symbol) {
        target->define(symbol);
    }

    void label(const char* symbol) {
        label(jasmine::local(symbol));
    }

    void jmp(const Arg& dest, Size size) {
        verify_buffer();
        Size actual_size = resolve_size(dest, size);

        if (is_label(dest.type) || is_immediate(dest.type)) {
            if (actual_size > DWORD) actual_size = DWORD;
            i64 imm = 0;
            if (is_immediate(dest.type)) imm = dest.data.imm64;						
						
						if (imm < -0x8000000l || imm > 0x7fffffffl) {
								fprintf(stderr, "[ERROR] Call offset too large; must fit within 32 bits.\n");
								exit(1);
						}

            if (actual_size == WORD) target->code().write<u8>(0x66);
            target->code().write<u8>(actual_size == BYTE ? 0xeb : 0xe9); // opcode
            write_immediate(imm, actual_size);

            if (is_label(dest.type)) {
                i8 imm_size = 1;
                if (actual_size == WORD) imm_size = 2;
                else if (actual_size == DWORD) imm_size = 4;
                target->reference(dest.data.label, relative(actual_size), -imm_size);
            }
        }
        else {
            target->code().write<u8>(0xff);
            emitargs(dest, actual_size, 4);
        }
    }

    void jcc(const Arg& dest, Condition condition) {
        verify_buffer();
        Size size = operand_size(dest.type);
        if (is_label(dest.type) || is_immediate(dest.type)) {
            if (size == AUTO) {
                fprintf(stderr, "[ERROR] Cannot deduce operand size in conditional jump instruction.\n");
                exit(1);
            }
            if (size > DWORD) size = DWORD;

            i64 imm = 0;
            if (is_immediate(dest.type)) imm = dest.data.imm64;

						if (imm < -0x8000000l || imm > 0x7fffffffl) {
								fprintf(stderr, "[ERROR] Call offset too large; must fit within 32 bits.\n");
								exit(1);
						}

            if (size == WORD) target->code().write<u8>(0x66);
            if (size == BYTE) target->code().write<u8>(0x70 + (u8)condition); // jcc rel8
            else target->code().write<u8>(0x0f, 0x80 + (u8)condition); // jcc rel16 / rel32

            write_immediate(imm, size);

            if (is_label(dest.type)) {
                i8 imm_size = 1;
                if (size == WORD) imm_size = 2;
                else if (size == DWORD) imm_size = 4;
                target->reference(dest.data.label, relative(size), -imm_size);
            }
        }
        else {
            fprintf(stderr, "[ERROR] Cannot conditional jump by register or memory location.\n");
            exit(1);
        }
    }

    void call(const Arg& dest, Size size) {
        verify_buffer();
        Size actual_size = resolve_size(dest, size);
        if (actual_size <= BYTE) actual_size = WORD; // cannot call by smaller than dword

        if (is_label(dest.type) || is_immediate(dest.type)) {
			if (actual_size > DWORD) actual_size = DWORD;
            if (actual_size <= WORD) actual_size = DWORD; // cannot call by smaller than dword

            i64 imm = 0;
            if (is_immediate(dest.type)) imm = dest.data.imm64;

            if (imm < -0x8000000l || imm > 0x7fffffffl) {
                fprintf(stderr, "[ERROR] Call offset too large; must fit within 32 bits.\n");
                exit(1);
            }

            target->code().write<u8>(0xe8); // opcode
            write_immediate(imm, actual_size);
            if (is_label(dest.type)) target->reference(dest.data.label, relative(actual_size), -4);
        }
        else {
            emitprefix(dest, actual_size);
            target->code().write<u8>(0xff);
            emitargs(dest, actual_size, 2);
        }
    }

	void setcc(const Arg& dest, Condition condition, Size size) {
        verify_buffer();
        Size actual_size = resolve_size(dest, size);

        if (is_immediate(dest.type)) {
            fprintf(stderr, "[ERROR] Invalid operand; immediate not permitted "
                "in 'setcc' instruction.\n");
            exit(1);
        }
        else {
            emitprefix(dest, BYTE);
            target->code().write<u8>(0x0f, 0x90 + (u8)condition);
            emitargs(dest, actual_size, 0);
        }
	}

    void nop(u32 n_bytes) {
        verify_buffer();
        static const u8 nop1[] = {0x90};
        static const u8 nop2[] = {0x66, 0x90};
        static const u8 nop3[] = {0x0f, 0x1f, 0x00};
        static const u8 nop4[] = {0x0f, 0x1f, 0x40, 0x00};
        static const u8 nop5[] = {0x0f, 0x1f, 0x44, 0x00, 0x00};
        static const u8 nop6[] = {0x66, 0x0f, 0x1f, 0x44, 0x00, 0x00};
        static const u8 nop7[] = {0x0f, 0x1f, 0x80, 0x00, 0x00, 0x00, 0x00};
        static const u8 nop8[] = {0x0f, 0x1f, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00};
        static const u8 nop9[] = {0x66, 0x0f, 0x1f, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00};
        static const u8* nops[] = {nullptr, nop1, nop2, nop3, nop4, nop5, nop6, nop7, nop8, nop9};
        if (n_bytes >= 1 && n_bytes <= 9) {
            for (u32 i = 0; i < n_bytes; i ++) target->code().write<u8>(nops[n_bytes][i]);
        }
        else {
            fprintf(stderr, "[ERROR] Invalid 'nop' instruction size %u: can only generate nops between "
                "1 and 9 bytes (inclusive).\n", n_bytes);
            exit(1);
        }
    }
    
    void lit8(u8 val) {
        verify_buffer();
        target->code().write<u8>(val);
    }

    void lit16(u16 val) {
        verify_buffer();
        target->code().write(little_endian<u16>(val));
    }

    void lit32(u32 val) {
        verify_buffer();
        target->code().write(little_endian<u32>(val));
    }

    void lit64(u64 val) {
        verify_buffer();
        target->code().write(little_endian<u64>(val));
    }

    void litf32(float f) {
        verify_buffer();
        u32 u = *(u32*)&f;
        target->code().write(little_endian<u32>(u));
    }

    void litf64(double d) {
        verify_buffer();
        u64 u = *(u64*)&d;
        target->code().write(little_endian<u64>(u));
    }

    void rel32(jasmine::Symbol symbol) {
        verify_buffer();
        target->code().write<u32>(0);
        target->reference(symbol, jasmine::RefType::REL32_LE, -4);
    }

    void nop32(u32 val) {
        const u8 nops[][4] = {
            { 0x0f, 0x1f, 0x84, 0x00 },
            { 0x0f, 0x1f, 0x80 },
            { 0x48, 0xa9 },
            { 0xa9 }
        };
        u8 which = target->code().size() % 4;
        u8 n = 4 - which;
        for (u8 i = 0; i < n; i ++) target->code().write<u8>(nops[which][i]);
        target->code().write(little_endian<u32>(val));
    }
}