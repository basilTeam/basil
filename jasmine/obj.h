#ifndef JASMINE_OBJECT_H
#define JASMINE_OBJECT_H

#include "utils.h"
#include "target.h"
#include "sym.h"

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

    class Object {
        Architecture arch;
        byte_buffer buf;
        map<Symbol, u64> defs;
        map<u64, SymbolRef> refs;
        void* loaded_code;

        void resolve_refs();
    public:
        Object(Architecture architecture = DEFAULT_ARCH);
        Object(const char* path, Architecture architecture = DEFAULT_ARCH);
        ~Object();
        Object(const Object&) = delete;
        Object& operator=(const Object&) = delete;

        byte_buffer& code();
        u64 size() const;
        void define(Symbol symbol);
        void reference(Symbol symbol, RefType type, i8 field_offset);
        void load();
        void write(const char* path);
        void read(const char* path);
        void writeELF(const char* path);
        Architecture architecture() const;

        void* find(Symbol symbol) const;
        
        template<typename T>
        T* find(Symbol symbol) const {
            return (T*)Object::find(symbol);
        }
    };
}

#endif