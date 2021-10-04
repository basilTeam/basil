#include "obj.h"
#include "bc.h"
#include "sym.h"
#include "target.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"

namespace jasmine {
    Object::Object(Architecture architecture):
        arch(architecture), loaded_code(nullptr) {
        //
    }

    Object::Object(const char* path, Architecture architecture): Object(architecture) {
        read(path);
    }

    Object::Object(Object&& other):
        arch(other.arch), buf(other.buf), defs(other.defs), def_positions(other.def_positions),
        refs(other.refs), loaded_code(other.loaded_code) {}

    Object::~Object() {
        if (loaded_code) free_exec(loaded_code, buf.size());
    }

    const map<Symbol, u64>& Object::symbols() const {
        return defs;
    }

    const map<u64, SymbolRef>& Object::references() const {
        return refs;
    }
    
    const map<u64, Symbol>& Object::symbol_positions() const {
        return def_positions;
    }

    const bytebuf& Object::code() const {
        return buf;
    }

    bytebuf& Object::code() {
        return buf;
    }

    u64 Object::size() const {
        return buf.size();
    }

    void Object::define(Symbol symbol) {
        defs.put(symbol, buf.size());
        def_positions.put(buf.size(), symbol);
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
        bytebuf code_copy = buf;
        while (code_copy.size()) *writer++ = code_copy.read();

        resolve_refs();

        u32 len = code().size();
        for (int i = 0; i < len; i ++) code_copy.write(((u8*)loaded_code)[i]);
        buf = code_copy;

        protect_exec(loaded_code, buf.size());
    }

    void Object::write(const char* path) {
        FILE* file = fopen(path, "w");
        if (!file) {
            fprintf(stderr, "[ERROR] Could not open file '%s'.\n", path);
            exit(1);
        }
        
        bytebuf b;
        b.write("#!jasmine\n", 10);

        b.write<u8>(JASMINE_VERSION); // version major
        b.write<u8>(arch); // architecture
        b.write("\xf0\x9f\xa6\x9d", 4); // friendly flamingo

        b.write(little_endian<u64>(buf.size())); // length of code
        bytebuf code_copy = buf;
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

        bytebuf b;
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
            def_positions.put(offset, sym);
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
    
    Object Object::retarget(Architecture architecture) {
        if (loaded_code) {
            fprintf(stderr, "[ERROR] Cannot retarget already-loaded jasmine object.\n");
            exit(1);
        }
        switch (arch) {
            case JASMINE: {
                vector<Insn> insns;
                bytebuf b = code();
                Context ctx;
                while (b.size()) insns.push(disassemble_insn(ctx, b, *this));
                switch (architecture) {
                    case X86_64: 
                        return jasmine_to_x86(insns);
                    default: 
                        break;
                }
                break;
            }
            default:
                break;
        }
        fprintf(stderr, "[ERROR] Tried to retarget to incompatible architecture.\n");
        exit(1);
        return Object(architecture);
    }

    void* Object::find(Symbol symbol) const {
        if (!loaded_code) return nullptr;
        auto it = defs.find(symbol);
        if (it == defs.end()) return nullptr;
        return (u8*)loaded_code + it->second;
    }

    struct ELFSectionHeader { 
        u32 type; 
        u64 flags; 
        u32 name_index;
        bytebuf* buf;
        u32 entry_size;
        u32 link = 0, info = 0;
    };

    static const u64 ELF_SHF_WRITE = 0x01,
        ELF_SHF_ALLOC = 0x02,
        ELF_SHF_EXECINSTR = 0x04,
        ELF_SHF_STRINGS = 0x20;

    u16 elf_machine_for(Architecture arch) {
        switch (arch) {
            case X86:
                return 0x03;
            case X86_64:
                return 0x3e;
            case AARCH64:
                return 0xb7;
            default:
                return 0;
        }
    }

    u8 elf_reloc_for(Architecture arch, RefType type, SymbolLinkage linkage) {
        switch (arch) {
            case X86_64: switch(type) {
                case REL8:
                    return 15;
                case REL16_BE:
                case REL16_LE:
                    return 13;
                case REL32_BE:
                case REL32_LE:
                    return linkage == GLOBAL_SYMBOL ? 4 : 2;
                case REL64_BE:
                case REL64_LE:
                    return 24;
                case ABS8:
                    return 14;
                case ABS16_BE:
                case ABS16_LE:
                    return 12;
                case ABS32_BE:
                case ABS32_LE:
                    return 10;
                case ABS64_BE:
                case ABS64_LE:
                    return 1;
                default:
                    return 0;
                }
            default:
                fprintf(stderr, "Tried to emit ELF file for unsupported architecture.\n");
                exit(1);
                return 0;
        }
    }

    void Object::resolve_ELF_addends() {
        auto size = buf.size();
        u8* rawtext = new u8[size];
        u32 i = 0;
        while (buf.size()) rawtext[i ++] = buf.read();

        for (auto& ref : refs) {
            u8* pos = (u8*)rawtext + ref.first;
            u8* field = pos + ref.second.field_offset;
            switch (ref.second.type) {
                case ABS8:
                case REL8:
                    *(i8*)field = i8(ref.second.field_offset);
                    break;
                case ABS16_LE:
                case REL16_LE:
                    *(i16*)field = little_endian<i16>(ref.second.field_offset);
                    break;
                case ABS16_BE:
                case REL16_BE:
                    *(i16*)field = big_endian<i16>(ref.second.field_offset);
                    break;
                case ABS32_LE:
                case REL32_LE:
                    *(i32*)field = little_endian<i32>(ref.second.field_offset);
                    break;
                case ABS32_BE:
                case REL32_BE:
                    *(i32*)field = big_endian<i32>(ref.second.field_offset);
                    break;
                case ABS64_LE:
                case REL64_LE:
                    *(i64*)field = little_endian<i64>(ref.second.field_offset);
                    break;
                case ABS64_BE:
                case REL64_BE:
                    *(i64*)field = big_endian<i64>(ref.second.field_offset);
                    break;
                default:
                    break;
            }
        }
        for (u32 i = 0; i < size; i ++) buf.write(rawtext[i]);
        delete[] rawtext;
    }

    void Object::writeELF(const char* path) {
        FILE* file = fopen(path, "w");
        if (!file) {
            fprintf(stderr, "[ERROR] Could not open file '%s'.\n", path);
            exit(1);
        }

        bytebuf elf;
        elf.write((char)0x7f, 'E', 'L', 'F');
        elf.write<u8>(0x02); // ELFCLASS64
        elf.write<u8>((EndianOrder)host_order.value == EndianOrder::UTIL_LITTLE_ENDIAN ?
            1 : 2); // endianness
        elf.write<u8>(1); // EV_CURRENT
        for (int i = 7; i < 16; i ++) elf.write('\0');

        elf.write<u16>(1); // relocatable
        elf.write<u16>(elf_machine_for(arch));
        elf.write<u32>(1); // original elf version
        elf.write<u64>(0); // entry point
        elf.write<u64>(0); // no program header
        elf.write<u64>(0x40); // section header starts after elf header
        elf.write<u32>(0); // no flags
        elf.write<u16>(0x40); // elf header size
        elf.write<u16>(0); // phentsize (unused)
        elf.write<u16>(0); // phnum (unused)
        elf.write<u16>(0x40); // section header entry size
        elf.write<u16>(6); // num sections
        elf.write<u16>(1); // section header strings are section 0

        bytebuf strtab, symtab;
        strtab.write('\0');
        symtab.write<u64>(0); // reserved symbol 0
        symtab.write<u64>(0);
        symtab.write<u64>(0);
        map<Symbol, u64> sym_indices;
        vector<pair<Symbol, u64>> locals, globals, total;
        for (auto& entry : defs) {
            if (entry.first.type == LOCAL_SYMBOL) locals.push(entry);
            else globals.push(entry);
        }
        for (auto& entry : refs) {
            if (defs.find(entry.second.symbol) == defs.end())
                globals.push({ entry.second.symbol, -1ul });
        }
        for (auto& entry : locals) total.push(entry);
        for (auto& entry : globals) total.push(entry);
        for (u32 i = 0; i < total.size(); i ++)
            sym_indices[total[i].first] = i + 1; // skip reserved symbol 0
        for (auto& entry : total) {
            u32 ind = strtab.size();
            strtab.write(name(entry.first), strlen(name(entry.first)) + 1);
            symtab.write<u32>(ind); // name
            u8 info = 0;
            info |= (entry.first.type == LOCAL_SYMBOL ? 0 : 1) << 4; // binding
            info |= 2; // function value assumed...for now
            symtab.write(info);
            symtab.write('\0'); // padding
            symtab.write<u16>(entry.second == -1ul ? 0 : 4); // .text section index
            symtab.write<u64>(entry.second == -1ul ? 0 : entry.second); // address
            symtab.write<u64>(8); // symbol size = word size?
        }

        resolve_ELF_addends();
        bytebuf rel;
        for (auto& entry : refs) {
            u64 sym = entry.first;
            rel.write<u64>(sym + entry.second.field_offset);
            u64 info = 0;
            info |= sym_indices[entry.second.symbol] << 32l;
            info |= elf_reloc_for(arch, entry.second.type, entry.second.symbol.type);
            rel.write<u64>(info);
        }

        bytebuf shstrtab, shdrs;
        vector<pair<string, ELFSectionHeader>> sections;
        shstrtab.write('\0');
        bytebuf empty;
        sections.push({ "", { 0, 0, 0, &empty, 0 } });
        sections.push({ ".shstrtab", { 3, ELF_SHF_STRINGS, 0, &shstrtab, 0 } });
        sections.push({ ".strtab", { 3, ELF_SHF_STRINGS, 0, &strtab, 0 } });
        sections.push({ ".symtab", { 2, 0, 0, &symtab, 24, 2, locals.size() + 1 } });
        sections.push({ ".text", { 1, ELF_SHF_ALLOC | ELF_SHF_EXECINSTR, 0, &buf, 0 } });
        sections.push({ ".rel.text", { 9, 0, 0, &rel, 16, 3, 4 } });
        sections.push({ ".data", { 1, ELF_SHF_ALLOC | ELF_SHF_WRITE, 0, &empty, 0 } });
        sections.push({ ".bss", { 1, ELF_SHF_ALLOC | ELF_SHF_WRITE, 0, &empty, 0 } });
        for (auto& entry : sections) {
            entry.second.name_index = shstrtab.size();
            shstrtab.write((const char*)entry.first.raw(), entry.first.size() + 1);
        }
        u64 offset = 0x40 + sections.size() * 0x40; // combined size of shdrs and elf header
        for (auto& entry : sections) {
            shdrs.write<u32>(entry.second.name_index); // offset to string
            shdrs.write<u32>(entry.second.type);
            shdrs.write<u64>(entry.second.flags);
            shdrs.write<u64>(0);
            shdrs.write<u64>(offset);
            shdrs.write<u64>(entry.second.buf->size());
            offset += entry.second.buf->size();
            shdrs.write<u32>(entry.second.link);
            shdrs.write<u32>(entry.second.info);
            shdrs.write<u64>(1);
            shdrs.write<u64>(entry.second.entry_size);
        }

        while (shdrs.size()) elf.write(shdrs.read());

        for (auto& entry : sections) {
            while (entry.second.buf->size()) elf.write(entry.second.buf->read());
        }
        
        while (elf.size()) fputc(elf.read(), file);
        fclose(file);
    }
}

template<>
u64 hash(const u64& u) {
    return raw_hash(u);
}

template<>
u64 hash(const jasmine::SymbolRef& symbol) {
    return raw_hash(symbol);
}