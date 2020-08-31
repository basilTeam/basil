#include "obj.h"
#include "sym.h"
#include "target.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace jasmine {
    Object::Object(Architecture architecture):
        arch(architecture), loaded_code(nullptr) {
        //
    }

    Object::Object(const char* path, Architecture architecture): Object(architecture) {
        read(path);
    }

    Object::~Object() {
        if (loaded_code) free_exec(loaded_code, buf.size());
    }

    byte_buffer& Object::code() {
        return buf;
    }

    u64 Object::size() const {
        return buf.size();
    }

    void Object::define(Symbol symbol) {
        defs.put(symbol, buf.size());
    }

    void Object::reference(Symbol symbol, RefType type, i8 field_offset) {
        refs[buf.size()] = { symbol, type, field_offset };
    }

    void Object::resolve_refs() {
        for (auto& ref : refs) {
            u8* pos = (u8*)loaded_code + ref.first;
            u8* sym = (u8*)find(ref.second.symbol);
						if (!sym) {
							fprintf(stderr, "[ERROR] Could not resolve ref '%s'.\n", 
								name(ref.second.symbol));
							exit(1);
						}
            u8* field = pos + ref.second.field_offset;
            switch (ref.second.type) {
                case REL8:
                    *(i8*)field = i8(sym - pos);
                    break;
                case REL16_LE:
                    *(i16*)field = little_endian<i16>(sym - pos);
                    break;
                case REL16_BE:
                    *(i16*)field = big_endian<i16>(sym - pos);
                    break;
                case REL32_LE:
                    *(i32*)field = little_endian<i32>(sym - pos);
                    break;
                case REL32_BE:
                    *(i32*)field = big_endian<i32>(sym - pos);
                    break;
                case REL64_LE:
                    *(i64*)field = little_endian<i64>(sym - pos);
                    break;
                case REL64_BE:
                    *(i64*)field = big_endian<i64>(sym - pos);
                    break;
                case ABS8:
                    *(i8*)field = i8(i64(sym));
                    break;
                case ABS16_LE:
                    *(i16*)field = little_endian<i16>(i64(sym));
                    break;
                case ABS16_BE:
                    *(i16*)field = big_endian<i16>(i64(sym));
                    break;
                case ABS32_LE:
                    *(i32*)field = little_endian<i32>(i64(sym));
                    break;
                case ABS32_BE:
                    *(i32*)field = big_endian<i32>(i64(sym));
                    break;
                case ABS64_LE:
                    *(i64*)field = little_endian<i64>(i64(sym));
                    break;
                case ABS64_BE:
                    *(i64*)field = big_endian<i64>(i64(sym));
                    break;
                default:
                    break;
            }
        }
    }

    void Object::load() {
        loaded_code = alloc_exec(buf.size());
        u8* writer = (u8*)loaded_code;
        byte_buffer code_copy = buf;
        while (code_copy.size()) *writer++ = code_copy.read();

        resolve_refs();

        protect_exec(loaded_code, buf.size());
    }

    void Object::write(const char* path) {
        FILE* file = fopen(path, "w");
        if (!file) {
            fprintf(stderr, "[ERROR] Could not open file '%s'.\n", path);
            exit(1);
        }
        
        byte_buffer b;
        b.write("#!jasmine\n", 10);

        b.write<u8>(JASMINE_VERSION); // version major
        b.write<u8>(arch); // architecture
        b.write("\xf0\x9f\xa6\x9d", 4); // friendly flamingo

        b.write(little_endian<u64>(buf.size())); // length of code
        byte_buffer code_copy = buf;
        while (code_copy.size()) b.write(code_copy.read()); // copy over code

        u32 internal_id = 0;
        map<Symbol, u32> internal_syms;
        vector<Symbol> sym_order;
        for (auto& p : defs) {
            auto it = internal_syms.find(p.first);
            if (it == internal_syms.end()) {
                internal_syms.put(p.first, internal_id ++);
                sym_order.push(p.first);
            }
        }
        for (auto& p : refs) {
            auto it = internal_syms.find(p.second.symbol);
            if (it == internal_syms.end()) {
                internal_syms.put(p.second.symbol, internal_id ++);
                sym_order.push(p.second.symbol);
            }
        }

        b.write(little_endian<u64>(internal_syms.size())); // number of symbols used in object
        for (auto& sym : sym_order) {
            b.write<u8>((u8)sym.type); // symbol type
            const char* str = name(sym);
            b.write(str, strlen(str) + 1); // symbol name, null-terminated
        }

        b.write(little_endian<u64>(defs.size())); // number of defs
        for (auto& p : defs) {
            b.write(little_endian<u64>(p.second)); // offset
            b.write<u32>(internal_syms[p.first]); // symbol id
        }

        b.write(little_endian<u64>(refs.size())); // number of refs
        for (auto& p : refs) {
            b.write(little_endian<u64>(p.first)); // offset
            b.write<u8>(p.second.type); // ref type
            b.write<i8>(p.second.field_offset); // field offset
            b.write<u32>(internal_syms[p.second.symbol]); // symbol id
        }

        while (b.size()) fputc(b.read(), file);
        fclose(file);
    }

    void Object::read(const char* path) {
        FILE* file = fopen(path, "r");
        if (!file) {
            fprintf(stderr, "[ERROR] Could not open file '%s'.\n", path);
            exit(1);
        }
        char symbol[1024];

        byte_buffer b;
        int ch;
        while ((ch = fgetc(file)) != EOF) b.write<u8>(ch);
        u8 shebang[10];
        for (int i = 0; i < 10; i ++) shebang[i] = b.read();
        if (strncmp((const char*)shebang, "#!jasmine\n", 10)) {
            fprintf(stderr, "[ERROR] Incorrect shebang.\n");
            exit(1);
        }

        u8 version = b.read(); // get version
        arch = (Architecture)b.read(); // get arch

        u8 magic[4];
        for (int i = 0; i < 4; i ++) magic[i] = b.read();
        if (strncmp((const char*)magic, "\xf0\x9f\xa6\x9d", 4)) {
            fprintf(stderr, "[ERROR] Incorrect magic number.\n");
            exit(1);
        }

        u64 code_length = from_little_endian(b.read<u64>()); // code length
        while (code_length) {
            if (!b.size()) {
                fprintf(stderr, "[ERROR] File contains less code than announced.\n");
                exit(1);
            }
            buf.write(b.read()); // read code from file
            -- code_length;
        }

        u64 sym_count = from_little_endian(b.read<u64>());
        vector<Symbol> internal_syms;
        while (sym_count) {
            SymbolLinkage type = (SymbolLinkage)b.read<u8>();

            u64 size = 0;
            while (b.peek()) {
                if (size > 1023) {
                    fprintf(stderr, "[ERROR] Encountered symbol longer than 1024 characters.\n");
                    exit(1);
                }
                symbol[size ++] = b.read();
            }
            symbol[size ++] = b.read();

            internal_syms.push(type == GLOBAL_SYMBOL ? global(symbol) : local(symbol));
            -- sym_count;
        }

        u64 def_count = from_little_endian(b.read<u64>());
        while (def_count) {
            if (!b.size()) {
                fprintf(stderr, "[ERROR] File contains fewer symbol defs than announced.\n");
                exit(1);
            }

            u64 offset = from_little_endian(b.read<u64>());
            Symbol sym = internal_syms[from_little_endian(b.read<u32>())];

            defs.put(sym, offset);
            -- def_count;
        }

        u64 ref_count = from_little_endian(b.read<u64>());
        while (ref_count) {
            if (!b.size()) {
                fprintf(stderr, "[ERROR] File contains fewer symbol refs than announced.\n");
                exit(1);
            }

            u64 offset = from_little_endian(b.read<u64>());
            RefType type = (RefType)b.read();
            i8 field_offset = b.read<i8>();
            Symbol sym = internal_syms[from_little_endian(b.read<u32>())];

            refs.put(offset, { sym, type, field_offset });
            -- ref_count;
        }
        fclose(file);
    }
    
    Architecture Object::architecture() const {
        return arch;
    }

    void* Object::find(Symbol symbol) const {
        if (!loaded_code) return nullptr;
				auto it = defs.find(symbol);
				if (it == defs.end()) return nullptr;
        return (u8*)loaded_code + it->second;
    }
}