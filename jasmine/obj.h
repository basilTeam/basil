/*
 * Copyright (c) 2021, the Basil authors
 * All rights reserved.
 *
 * This source code is licensed under the 3-Clause BSD License, the full text
 * of which can be found in the LICENSE file in the root directory
 * of this project.
 */

#ifndef JASMINE_OBJECT_H
#define JASMINE_OBJECT_H

#include "utils.h"
#include "target.h"
#include "sym.h"
#include "bc.h"

namespace jasmine {
    enum RefType : u8 {
        REL8, REL16_LE, REL32_LE, REL64_LE, // relative reference, e.g. for jumps or rip-relative addressing
        REL16_BE, REL32_BE, REL64_BE,
        ABS8, ABS16_LE, ABS32_LE, ABS64_LE, // absolute reference
        ABS16_BE, ABS32_BE, ABS64_BE
    };
    
    struct SymbolRef {
        Symbol symbol;
        RefType type;
        i8 field_offset;
    };

    struct Context;

    class Object {
        Target target;
        bytebuf buf;
        Context ctx;
        map<Symbol, u64> defs;
        map<u64, Symbol> def_positions;
        map<u64, SymbolRef> refs;
        void* loaded_code;

        void resolve_refs();
        void resolve_ELF_addends();
    public:
        Object(const Target& target = DEFAULT_TARGET);
        Object(const char* path, const Target& target = DEFAULT_TARGET);
        ~Object();
        Object(const Object& other);
        Object& operator=(const Object& other);
        Object(Object&& other);
        Object& operator=(Object&& other);

        const map<Symbol, u64>& symbols() const;
        const map<u64, SymbolRef>& references() const;
        const map<u64, Symbol>& symbol_positions() const;
        const bytebuf& code() const;
        bytebuf& code();
        u64 size() const;
        void define(Symbol symbol);
        void define_native(Symbol symbol, void* address);
        void reference(Symbol symbol, RefType type, i8 field_offset);
        void load();
        void write(const char* path);
        void read(const char* path);
        void write(FILE* file);
        void read(FILE* file);
        void writeELF(const char* path);
        void writeELF(FILE* file);
        Context& get_context();
        const Context& get_context() const;
        void set_context(const Context& ctx);
        const Target& get_target() const;
        Object retarget(const Target& new_target);

        void* find(Symbol symbol) const;
        
        template<typename T>
        T* find(Symbol symbol) const {
            return (T*)Object::find(symbol);
        }
    };
}

template<>
u64 hash(const jasmine::SymbolRef& symbol);

#endif