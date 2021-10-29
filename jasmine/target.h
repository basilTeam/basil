/*
 * Copyright (c) 2021, the Basil authors
 * All rights reserved.
 *
 * This source code is licensed under the 3-Clause BSD License, the full text
 * of which can be found in the LICENSE file in the root directory
 * of this project.
 */

#ifndef JASMINE_VERSION_H
#define JASMINE_VERSION_H

#include "utils.h"
#include "util/sets.h"
#include "util/io.h"
#include "util/option.h"

namespace jasmine {
    const u16 JASMINE_MAJOR_VERSION = 1, JASMINE_MINOR_VERSION = 0, JASMINE_PATCH_VERSION = 0;

    class Object;
    struct Symbol;

    enum Architecture : u16 {
        UNSUPPORTED_ARCH = 0,
        X86_64 = 1,
        AMD64 = 1,
        X86 = 2,
        AARCH64 = 3,
        JASMINE = 4, // architecture for jasmine bytecode
    };

    enum OS : u16 {
        UNSUPPORTED_OS = 0,
        LINUX = 1,
        WINDOWS = 2,
        MACOS = 3
    };

    // Abstract value kinds used in Jasmine instructions and to describe generic values
    // in native instruction sets.
    enum Kind {
        K_STRUCT, K_PTR, K_F32, K_F64,
        K_I8, K_I16, K_I32, K_I64,
        K_U8, K_U16, K_U32, K_U64,
        NUM_KINDS
    };

    // The type used to represent a system-agnostic register. This value is interpreted
    // differently by the different backends, but generally should correspond to whatever
    // integer id is associated with the register it represents.
    using GenericRegister = u32;

    // Describes the different locations that a value may be stored, in abstract, platform-
    // independent terms.
    enum LocationType {
        LT_NONE,
        LT_REGISTER, // This value is stored in a hardware register.
        LT_MEMORY, // This value is stored in some kind of generic memory.
        LT_STATIC_MEMORY, // This value is stored in static memory.
        LT_STACK_MEMORY, // This value is stored on the stack.
        LT_PUSHED_L2R, // This value is a parameter, pushed left-to-right onto the stack.
        LT_PUSHED_R2L // This value is a parameter, pushed right-to-left onto the stack.
    };

    struct Insn;
    struct LiveRange;

    struct Location {
        LocationType type = LT_NONE; // What kind of location is this?
        optional<GenericRegister> reg; // If it's a register, which register?
        optional<i64> offset; // If it's a stack or static memory location, what's its offset?
    };

    // Constructs locations for various things.
    Location loc_reg(GenericRegister reg);
    Location loc_stack(i64 offset);

    // The type used to represent a particular target for native compilation.
    struct Target {
        Architecture arch;
        OS os;

        // Returns a list of the available registers for the provided kind on this target
        // platform.
        const_slice<GenericRegister> registers(Kind kind) const;

        // Returns the sequence of registers returned by registers(kind), but as a bitset.
        const bitset& register_set(Kind kind) const;
        
        // Returns a list of registers available for parameters of the provided kind
        // on this target platform.
        const_slice<GenericRegister> parameter_registers(Kind kind) const;

        // Returns the sequence of registers returned by parameter_registers(kind), but as
        // a bitset.
        const bitset& parameter_register_set(Kind kind) const;

        // Returns the determined locations for each of the provided parameters in 
        // accordance with this target's calling convention.
        vector<Location> place_parameters(const vector<Kind>& param_kinds) const;

        // Returns the location of a returned value of the provided kind, in accordance
        // with this target's calling convention.
        Location locate_return_value(Kind kind) const;

        // Returns the set of registers that are clobbered on this platform by the provided
        // Jasmine virtual instruction.
        bitset clobbers(const Insn& insn) const;

        // Applies an optional register hint to an instruction.
        void hint(const Insn& insn, const vector<LiveRange*>& params) const;

        // Returns the size of a pointer in bytes for this target.
        u64 pointer_size() const;

        // Writes a small system-specific trampoline to the provided object, forwarding a function
        // call to the provided symbol to the given absolute address.
        void trampoline(Object& obj, Symbol label, i64 address) const;
    };

    #if defined(BASIL_X86_64)
        const Architecture DEFAULT_ARCH = X86_64;
    #elif defined(BASIL_X86_32)
        const Architecture DEFAULT_ARCH = X86;
    #elif defined(BASIL_AARCH64)
        const Architecture DEFAULT_ARCH = AARCH64;
    #endif

    #if defined(BASIL_WINDOWS)
        const OS DEFAULT_OS = WINDOWS;
    #elif defined(BASIL_MACOS)
        const OS DEFAULT_OS = MACOS;
    #elif defined(BASIL_LINUX)
        const OS DEFAULT_OS = LINUX;
    #endif

    const Target DEFAULT_TARGET = { DEFAULT_ARCH, DEFAULT_OS };
}

#endif