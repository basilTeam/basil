/*
 * Copyright (c) 2021, the Basil authors
 * All rights reserved.
 *
 * This source code is licensed under the 3-Clause BSD License, the full text
 * of which can be found in the LICENSE file in the root directory
 * of this project.
 */

#include "jobj.h"
#include "bc.h"
#include "sym.h"
#include "target.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"

namespace jasmine {
    bool operator==(const SymbolLocation& a, const SymbolLocation& b) {
        return a.section == b.section && a.offset == b.offset;
    }

    Object::Object(const Target& target_in):
        target(target_in), loaded_code(nullptr), loaded_data(nullptr), loaded_static(nullptr) {
        //
    }

    Object::Object(const char* path, const Target& target): Object(target) {
        read(path);
    }

    Object::Object(const Object& other):
        target(other.target),
        codebuf(other.codebuf), databuf(other.databuf), staticbuf(other.staticbuf),
        defs(other.defs), def_positions(other.def_positions),
        refs(other.refs), 
        loaded_code(other.loaded_code), loaded_data(other.loaded_data), loaded_static(other.loaded_static) {}
        
    Object& Object::operator=(const Object& other) {
        if (&other != this) {
            if (loaded_code) free_vmem(loaded_code, codebuf.size());
            if (loaded_data) free_vmem(loaded_data, databuf.size());
            if (loaded_static) free_vmem(loaded_static, staticbuf.size());
            target = other.target;
            codebuf = other.codebuf;
            databuf = other.databuf;
            staticbuf = other.staticbuf;
            defs = other.defs;
            def_positions = other.def_positions;
            refs = other.refs;
            loaded_code = other.loaded_code;
            loaded_data = other.loaded_data;
            loaded_static = other.loaded_static;
        }
        return *this;
    }

    Object::Object(Object&& other):
        target(other.target),
        codebuf(other.codebuf), databuf(other.databuf), staticbuf(other.staticbuf),
        defs(other.defs), def_positions(other.def_positions),
        refs(other.refs), 
        loaded_code(other.loaded_code), loaded_data(other.loaded_data), loaded_static(other.loaded_static) {}
        
    Object& Object::operator=(Object&& other) {
        if (&other != this) {
            if (loaded_code) free_vmem(loaded_code, codebuf.size());
            if (loaded_data) free_vmem(loaded_data, databuf.size());
            if (loaded_static) free_vmem(loaded_static, staticbuf.size());
            target = other.target;
            codebuf = other.codebuf;
            databuf = other.databuf;
            staticbuf = other.staticbuf;
            defs = other.defs;
            def_positions = other.def_positions;
            refs = other.refs;
            loaded_code = other.loaded_code;
            other.loaded_code = nullptr;
        }
        return *this;
    }

    Object::~Object() {
        if (loaded_code) free_vmem(loaded_code, codebuf.size());
        if (loaded_data) free_vmem(loaded_data, databuf.size());
        if (loaded_static) free_vmem(loaded_static, staticbuf.size());
    }

    const map<Symbol, SymbolLocation>& Object::symbols() const {
        return defs;
    }

    const map<SymbolLocation, SymbolRef>& Object::references() const {
        return refs;
    }
    
    const map<SymbolLocation, Symbol>& Object::symbol_positions() const {
        return def_positions;
    }

    const bytebuf& Object::code() const {
        return codebuf;
    }

    bytebuf& Object::code() {
        return codebuf;
    }

    const bytebuf& Object::data() const {
        return databuf;
    }

    bytebuf& Object::data() {
        return databuf;
    }

    const bytebuf& Object::stat() const {
        return staticbuf;
    }

    bytebuf& Object::stat() {
        return staticbuf;
    }

    const bytebuf& Object::get(ObjectSection section) const {
        switch (section) {
            case OS_UNDEF: panic("Can't get section 'none' from Jasmine object!");
            case OS_CODE: return codebuf;
            case OS_DATA: return databuf;
            case OS_STATIC: return staticbuf;
        }
    }

    bytebuf& Object::get(ObjectSection section) {
        switch (section) {
            case OS_UNDEF: panic("Can't get section 'none' from Jasmine object!");
            case OS_CODE: return codebuf;
            case OS_DATA: return databuf;
            case OS_STATIC: return staticbuf;
        }
    }

    void* Object::get_loaded(ObjectSection section) const {
        switch (section) {
            case OS_UNDEF: panic("Can't get section 'none' from Jasmine object!");
            case OS_CODE: return loaded_code;
            case OS_DATA: return loaded_data;
            case OS_STATIC: return loaded_static;
        }
    }

    u64 Object::size(ObjectSection section) const {
        switch (section) {
            case OS_UNDEF: return 0;
            case OS_CODE: return codebuf.size();
            case OS_DATA: return databuf.size();
            case OS_STATIC: return staticbuf.size();
        }
    }

    void Object::define(Symbol symbol, ObjectSection section) {
        defs.put(symbol, { section, size(section) });
        def_positions.put({ section, size(section) },  symbol);
    }
    
    void Object::define_native(Symbol symbol, void* address) {
        target.trampoline(*this, symbol, (i64)address);
    }

    void Object::reference(Symbol symbol, ObjectSection section, RefType type, i8 field_offset) {
        auto it = defs.find(symbol);
        refs[{ section, size(section) }] = { symbol, type, field_offset };
    }

    void Object::resolve_refs() {
        for (auto& ref : refs) {
            u8* pos = (u8*)get_loaded(ref.first.section) + ref.first.offset;
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
        // TODO: make sure these are contiguous or at least near each other
        loaded_data = alloc_vmem(databuf.size());
        loaded_static = alloc_vmem(staticbuf.size());
        loaded_code = alloc_vmem(codebuf.size());

        u8* writer = (u8*)loaded_code;
        bytebuf copy = codebuf;
        while (copy.size()) *writer++ = copy.read();
        writer = (u8*)loaded_data;
        copy = databuf;
        while (copy.size()) *writer++ = copy.read();
        writer = (u8*)loaded_static;
        copy = staticbuf;
        while (copy.size()) *writer++ = copy.read();

        resolve_refs();

        for (u32 i = 0; i < code().size(); i ++) copy.write(((u8*)loaded_code)[i]);
        codebuf = copy, copy.clear();
        for (u32 i = 0; i < data().size(); i ++) copy.write(((u8*)loaded_data)[i]);
        databuf = copy, copy.clear();
        for (u32 i = 0; i < stat().size(); i ++) copy.write(((u8*)loaded_static)[i]);
        staticbuf = copy, copy.clear();

        protect_exec(loaded_code, codebuf.size());
        protect_data(loaded_data, databuf.size());
        protect_static(loaded_static, staticbuf.size());
    }

    void Object::write(const char* path) {
        FILE* file = fopen(path, "w");
        if (!file) {
            fprintf(stderr, "[ERROR] Could not open file '%s'.\n", path);
            exit(1);
        }
        write(file);
        fclose(file);
    }

    void Object::write(FILE* file) {
        bytebuf b;
        b.write("#!jasmine\n", 10);
        b.write("\xf0\x9f\xa6\x9d", 4); // friendly raccoon
        b.write(little_endian<u16>(JASMINE_MAJOR_VERSION)); // jasmine major version
        b.write(little_endian<u16>(JASMINE_MINOR_VERSION)); // jasmine minor version
        b.write(little_endian<u16>(JASMINE_PATCH_VERSION)); // jasmine patch version
        b.write(little_endian<u16>(target.arch)); // architecture
        b.write(little_endian<u16>(target.os)); // OS
        b.write(little_endian<u64>(codebuf.size())); // length of code
        b.write(little_endian<u64>(databuf.size())); // length of code
        b.write(little_endian<u64>(staticbuf.size())); // length of code
        
        bytebuf copy = codebuf;
        while (copy.size()) b.write(copy.read()); // copy over code
        copy = databuf;
        while (copy.size()) b.write(copy.read()); // copy over data
        copy = staticbuf;
        while (copy.size()) b.write(copy.read()); // copy over static

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
            u64 loc = u64(p.second.section) << 62 | (p.second.offset << 2) >> 2;
            b.write(little_endian<u64>(loc)); // location
            b.write<u32>(internal_syms[p.first]); // symbol id
        }

        b.write(little_endian<u64>(refs.size())); // number of refs
        for (auto& p : refs) {
            u64 loc = u64(p.first.section) << 62 | (p.first.offset << 2) >> 2;
            b.write(little_endian<u64>(loc)); // location
            b.write<u8>(p.second.type); // ref type
            b.write<i8>(p.second.field_offset); // field offset
            b.write<u32>(internal_syms[p.second.symbol]); // symbol id
        }

        while (b.size()) fputc(b.read(), file);
    }

    void Object::read(const char* path) {
        FILE* file = fopen(path, "r");
        if (!file) {
            fprintf(stderr, "[ERROR] Could not open file '%s'.\n", path);
            exit(1);
        }
        read(file);
        fclose(file);
    }

    void Object::read(FILE* file) {
        char symbol[1024];

        bytebuf b;
        int ch;
        while ((ch = fgetc(file)) != EOF) b.write<u8>(ch);
        u8 shebang[11];
        for (int i = 0; i < 10; i ++) shebang[i] = b.read();
        shebang[10] = '\0';
        if (strncmp((const char*)shebang, "#!jasmine\n", 10)) {
            fprintf(stderr, "[ERROR] Incorrect shebang - found '%s'.\n", shebang);
            exit(1);
        }

        u8 magic[4];
        for (int i = 0; i < 4; i ++) magic[i] = b.read();
        if (strncmp((const char*)magic, "\xf0\x9f\xa6\x9d", 4)) {
            fprintf(stderr, "[ERROR] Incorrect magic number.\n");
            exit(1);
        }

        u16 major = from_little_endian(b.read<u16>()), // get major version
            minor = from_little_endian(b.read<u16>()), // ...minor version
            patch = from_little_endian(b.read<u16>()); // ...and patch
        if (major > JASMINE_MAJOR_VERSION) {
            fprintf(stderr, "[ERROR] Jasmine object compiled with Jasmine version %u.%u.%u, "
                "but Jasmine installation only supports up to %u.x.x.\n", major, minor, patch, JASMINE_MAJOR_VERSION);
            exit(1);
        }
        
        Architecture arch = (Architecture)from_little_endian(b.read<u16>()); // get arch
        OS os = (OS)from_little_endian(b.read<u16>()); // get os
        target = { arch, os };

        u64 code_length = from_little_endian(b.read<u64>()); // code length
        u64 data_length = from_little_endian(b.read<u64>()); // data length
        u64 static_length = from_little_endian(b.read<u64>()); // static length

        while (code_length) {
            if (!b.size()) {
                fprintf(stderr, "[ERROR] File contains less code than announced.\n");
                exit(1);
            }
            codebuf.write(b.read()); // read code from file
            -- code_length;
        }
        while (data_length) {
            if (!b.size()) {
                fprintf(stderr, "[ERROR] File contains less data than announced.\n");
                exit(1);
            }
            databuf.write(b.read()); // read code from file
            -- data_length;
        }
        while (static_length) {
            if (!b.size()) {
                fprintf(stderr, "[ERROR] File contains smaller static section than announced.\n");
                exit(1);
            }
            staticbuf.write(b.read()); // read code from file
            -- static_length;
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
            symbol[size ++] = b.read(); // null char

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
            SymbolLocation loc = { ObjectSection(offset >> 62 & 3), (offset << 2) >> 2 };

            Symbol sym = internal_syms[from_little_endian(b.read<u32>())];

            defs.put(sym, loc);
            def_positions.put(loc, sym);
            -- def_count;
        }

        u64 ref_count = from_little_endian(b.read<u64>());
        while (ref_count) {
            if (!b.size()) {
                fprintf(stderr, "[ERROR] File contains fewer symbol refs than announced.\n");
                exit(1);
            }

            u64 offset = from_little_endian(b.read<u64>());
            SymbolLocation loc = { ObjectSection(offset >> 62 & 3), (offset << 2) >> 2 };

            RefType type = (RefType)b.read();
            i8 field_offset = b.read<i8>();
            Symbol sym = internal_syms[from_little_endian(b.read<u32>())];

            refs.put(loc, { sym, type, field_offset });
            -- ref_count;
        }
    }

    Context& Object::get_context() {
        return ctx;
    }

    const Context& Object::get_context() const {
        return ctx;
    }

    void Object::set_context(const Context& ctx_in) {
        ctx = ctx_in;
    }
    
    const Target& Object::get_target() const {
        return target;
    }
    
    Object Object::retarget(const Target& new_target) {
        if (loaded_code) {
            fprintf(stderr, "[ERROR] Cannot retarget already-loaded jasmine object.\n");
            exit(1);
        }
        switch (target.arch) {
            case JASMINE: {
                vector<Insn> insns;
                bytebuf b = code();
                while (b.size()) insns.push(disassemble_insn(ctx, b, *this));
                return compile_jasmine(ctx, insns, new_target);
            }
            default:
                fprintf(stderr, "[ERROR] Tried to retarget to incompatible architecture.\n");
                exit(1);
                return Object(DEFAULT_TARGET);
        }
    }

    void* Object::find(Symbol symbol) const {
        auto it = defs.find(symbol);
        if (it == defs.end()) return nullptr;
        return (u8*)get_loaded(it->second.section) + it->second.offset;
    }
    
    void Object::append(const Object& other) {
        u64 code_size = code().size();

    }

    void Object::writeObj(const char* path) {
        FILE* file = fopen(path, "w");
        if (!file) {
            fprintf(stderr, "[ERROR] Could not open file '%s'.\n", path);
            exit(1);
        }
        writeObj(file);
        fclose(file);
    }

    void Object::writeObj(FILE* file) {
        switch (target.os) {
            case LINUX: return writeELF(file);
            case MACOS: return writeMachO(file);
            case WINDOWS: return writeCOFF(file);
            default: panic("Unsupported OS!");
        }
    }

    void Object::writeMachO(FILE* file) {
        panic("TODO: implement Mach-O object files.");
    }

    u16 coff_machine_for(Architecture arch) {
        switch (arch) {
            case X86_64: return 0x8664;
            case X86: return 0x014c;
            case AARCH64: return 0xaa64;
            default: 
                panic("Unsupported architecture!");
                return 0;
        }
    }

    u8 coff_reloc_for(Architecture arch, RefType type, i8 field_offset) {
        switch (arch) {
            case X86_64: switch(type) {
                case REL8:
                case REL16_BE:
                case REL16_LE:
                case REL64_BE:
                case REL64_LE:
                case ABS8:
                case ABS16_BE:
                case ABS16_LE:
                    panic("Unsupported relocation type for COFF x86_64!");
                case REL32_BE:
                case REL32_LE:
                    return 8; // IMAGE_REL_AMD64_REL32_4 
                case ABS32_BE:
                case ABS32_LE:
                    return 2; // IMAGE_REL_AMD64_ADDR32 
                case ABS64_BE:
                case ABS64_LE:
                    return 1; // IMAGE_REL_AMD64_ADDR64 
                default:
                    return 0;
                }
            default:
                fprintf(stderr, "Tried to emit COFF file for unsupported architecture.\n");
                exit(1);
                return 0;
        }
    }

    struct COFFSection {
        u8 name[8];
        u32 virtual_size = 0, virtual_addr = 0; // 0 for relocatable objects
        u32 size, offset;
        u32 reloc_offset, lineno_offset = 0;
        u16 n_relocs, n_linenos = 0;
        u32 flags;
        bytebuf data;

        void write_header(bytebuf& buf) {
            for (u8 i = 0; i < 8; i ++) buf.write<u8>(name[i]);
            buf.write<u32>(little_endian<u32>(virtual_size));
            buf.write<u32>(little_endian<u32>(virtual_addr));
            buf.write<u32>(little_endian<u32>(size));
            buf.write<u32>(little_endian<u32>(offset));
            buf.write<u32>(little_endian<u32>(reloc_offset));
            buf.write<u32>(little_endian<u32>(lineno_offset));
            buf.write<u16>(little_endian<u16>(n_relocs));
            buf.write<u16>(little_endian<u16>(n_linenos));
            buf.write<u32>(little_endian<u32>(flags));
        }
    };

    enum COFFSectionFlags {
        COFF_CODE = 0x20,           // contains code
        COFF_INIT = 0x40,           // initialized data
        COFF_UNINIT = 0x80,         // uninitialized data
        COFF_ALIGN1 = 0x100000,
        COFF_ALIGN2 = 0x200000,
        COFF_ALIGN4 = 0x300000,
        COFF_ALIGN8 = 0x400000,
        COFF_EXEC = 0x20000000,     // should be executable
        COFF_READ = 0x40000000,     // should be readable
        COFF_WRITE = 0x40000000,    // should be writable
    };

    void Object::writeCOFF(FILE* file) {
        bytebuf coff;

        COFFSection text;
        u32 n_sections = 1; // we only support a text section for now

        map<Symbol, bool> all_symbols; // all symbols, mapped to whether or not they are locally defined
        for (const auto& [k, v] : defs) {
            all_symbols[k] = true;
        }
        for (const auto& [k, v] : refs) {
            if (defs.contains(v.symbol)) all_symbols[v.symbol] = true;
            else all_symbols[v.symbol] = false;
        }
        
        // precompute string table size
        u32 string_table_size = 0;
        for (const auto& [k, v] : all_symbols) {
            string label = name(k);
            if (label.size() > 8) string_table_size += label.size() + 1;
        }
        string_table_size += 4; // account for the size itself

        // compose string table
        bytebuf strings; // string table
        strings.write<u32>(little_endian<u32>(string_table_size)); // starts with 4-byte size
        map<Symbol, u32> string_offsets;
        for (const auto& [k, v] : all_symbols) {
            string label = name(k);
            if (label.size() > 8) {
                string_offsets[k] = strings.size();
                strings.write(name(k), strlen(name(k)) + 1);
            }
        }

        bytebuf symbols; // symbol table
        map<Symbol, u64> symbol_indices;
        u32 n_symbols = 0;
        for (const auto& [k, v] : all_symbols) {
            symbol_indices[k] = n_symbols ++; // store symbol index for relocations

            string label = name(k);
            if (label.size() <= 8) for (u8 i = 0; i < 8; i ++) {
                if (i < label.size()) symbols.write(label[i]);
                else symbols.write('\0'); // pad short strings out to 8 bytes
            }
            else symbols.write<u32>(0), symbols.write<u32>(little_endian<u32>(string_offsets[k])); // strtab offset
            
            auto it = defs.find(k);
            // if (it != defs.end()) symbols.write<u32>(little_endian<u32>(it->second)); // symbol offset within text
            // else symbols.write<u32>(0); // external symbol

            symbols.write<u16>(little_endian<u16>(it == defs.end() ? 0 : 1)); // 0 means undefined
            symbols.write<u16>(little_endian<u16>(k.type == LOCAL_SYMBOL ? 0 : 0x2000)); // global symbols are functions
            
            u8 symbol_class;
            if (v) symbol_class = k.type == LOCAL_SYMBOL ? 3 : 2; // static for locals, function for globals
            else symbol_class = 2; // externally defined
            symbols.write<u8>(symbol_class);

            if (symbol_class == 2 && v) { // function definition
                symbols.write<u8>(1); // 1 auxiliary symbol record

                n_symbols ++; // advance past auxiliary symbol record
                symbols.write<u32>(little_endian<u32>(n_symbols ++)); // .bf record index
                symbols.write<u32>(0); // we don't compute the real totalsize currently
                symbols.write<u32>(0, 0); // no line-number info
                symbols.write<u16>(0);

                symbols.write<u8>('.', 'b', 'f', '\0');
                symbols.write<u32>(0);
                symbols.write<u32>(0); // value unused in .bf symbol
                symbols.write<u16>(little_endian<u16>(1));
                symbols.write<u16>(0);
                symbols.write<u8>(101, 1); // function, 1 aux record
                n_symbols ++;
                symbols.write<u32>(0, 0, 0, 0); // we don't actually use the aux record
                symbols.write<u16>(0);

                n_symbols ++;
                symbols.write<u8>('.', 'l', 'f', '\0');
                symbols.write<u32>(0);
                symbols.write<u32>(0); // value unused in .lf symbol
                symbols.write<u16>(little_endian<u16>(1));
                symbols.write<u16>(0);
                symbols.write<u8>(101, 0);

                n_symbols ++;
                symbols.write<u8>('.', 'e', 'f', '\0');
                symbols.write<u32>(0);
                symbols.write<u32>(0); // value unused in .ef symbol
                symbols.write<u16>(little_endian<u16>(1));
                symbols.write<u16>(0);
                symbols.write<u8>(101, 1);
                n_symbols ++;
                symbols.write<u32>(0, 0, 0, 0); // we don't actually use the aux record
                symbols.write<u16>(0);
            }
            else symbols.write<u8>(0); // no auxiliary symbol records
        }

        static const u32 shdr_size = 40, coff_hdr_size = 20;

        // construct text section

        for (u8 i = 0; i < 8; i ++) text.name[i] = ".text\0\0\0"[i];
        text.flags = COFF_CODE | COFF_EXEC | COFF_READ | COFF_ALIGN8;
        text.offset = strings.size() + symbols.size() + shdr_size * n_sections + coff_hdr_size;
        text.reloc_offset = text.offset + code().size();
        text.lineno_offset = 0;
        text.n_relocs = refs.size();
        text.n_linenos = 0;
        text.size = code().size();
        text.data = code();

        // compose relocation information
        
        bytebuf relocs;
        for (const auto& ref : refs) {
            // relocs.write<u32>(little_endian<u32>(ref.first + ref.second.field_offset)); // base address
            relocs.write<u32>(little_endian<u32>(symbol_indices[ref.second.symbol]));
            relocs.write<u16>(little_endian<u16>(
                coff_reloc_for(target.arch, ref.second.type, ref.second.field_offset))
            );
        }

        // COFF header

        u32 timestamp = 0;
        coff.write<u16>(little_endian<u16>(coff_machine_for(target.arch)));
        coff.write<u16>(little_endian<u16>(n_sections)); // number of sections
        coff.write<u32>(little_endian<u32>(timestamp));
        coff.write<u32>(little_endian<u32>(n_sections * shdr_size + coff_hdr_size)); // symbol table is after headers
        coff.write<u32>(little_endian<u32>(n_symbols)); // number of symbols
        coff.write<u16>(little_endian<u16>(0)); // no optional header, this is a relocatable object
        coff.write<u16>(little_endian<u16>(0)); // we generally don't set any image flags

        text.write_header(coff); // section header

        while (symbols.size()) coff.write<u8>(symbols.read());
        while (strings.size()) coff.write<u8>(strings.read());
        while (text.data.size()) coff.write<u8>(text.data.read());
        while (relocs.size()) coff.write<u8>(relocs.read());
        
        while (coff.size()) fputc(coff.read(), file);
    }

    struct ELFSectionHeader { 
        u64 flags;
        bytebuf* buf;
        u32 type; 
        u32 name_index;
        u32 entry_size;
        u32 link, info;

        ELFSectionHeader(u32 type_in, u64 flags_in, u32 name_in, bytebuf* buf_in,
            u32 entry_size_in, u32 link_in = 0, u32 info_in = 0):
            flags(flags_in), buf(buf_in), type(type_in), name_index(name_in),
            entry_size(entry_size_in), link(link_in), info(info_in) {}
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
        u64 codesize = codebuf.size(), datasize = databuf.size(), staticsize = staticbuf.size();
        u8* raw[4] = {
            nullptr,
            codebuf.size() ? new u8[codebuf.size()] : nullptr,
            databuf.size() ? new u8[databuf.size()] : nullptr,
            staticbuf.size() ? new u8[staticbuf.size()] : nullptr
        };
        u32 i = 0;
        while (codebuf.size()) raw[OS_CODE][i ++] = codebuf.read();
        i = 0;
        while (databuf.size()) raw[OS_DATA][i ++] = databuf.read();
        i = 0;
        while (staticbuf.size()) raw[OS_STATIC][i ++] = staticbuf.read();

        for (auto& ref : refs) {
            u8* pos = (u8*)raw[ref.first.section] + ref.first.offset;
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
        for (u32 i = 0; i < codesize; i ++) codebuf.write(raw[OS_CODE][i]);
        for (u32 i = 0; i < datasize; i ++) databuf.write(raw[OS_DATA][i]);
        for (u32 i = 0; i < staticsize; i ++) staticbuf.write(raw[OS_STATIC][i]);
        for (u32 i = 0; i < 4; i ++) if (raw[i]) delete[] raw[i];
    }

    void Object::writeELF(FILE* file) {
        u16 section_indices[4] = { 0, 0, 0, 0 }; // default to zero for all sections
        u16 section_idx = 4;
        if (codebuf.size()) section_indices[OS_CODE] = section_idx, section_idx += 2;
        if (databuf.size()) section_indices[OS_DATA] = section_idx, section_idx += 2;
        if (staticbuf.size()) section_indices[OS_STATIC] = section_idx, section_idx += 2;

        bytebuf elf;
        elf.write((char)0x7f, 'E', 'L', 'F');
        elf.write<u8>(0x02); // ELFCLASS64
        elf.write<u8>((EndianOrder)host_order.value == EndianOrder::UTIL_LITTLE_ENDIAN ?
            1 : 2); // endianness
        elf.write<u8>(1); // EV_CURRENT
        for (int i = 7; i < 16; i ++) elf.write('\0');

        elf.write<u16>(1); // relocatable
        elf.write<u16>(elf_machine_for(target.arch));
        elf.write<u32>(1); // original elf version
        elf.write<u64>(0); // entry point
        elf.write<u64>(0); // no program header
        elf.write<u64>(0x40); // section header starts after elf header
        elf.write<u32>(0); // no flags
        elf.write<u16>(0x40); // elf header size
        elf.write<u16>(0); // phentsize (unused)
        elf.write<u16>(0); // phnum (unused)
        elf.write<u16>(0x40); // section header entry size
        elf.write<u16>(section_idx); // num sections
        elf.write<u16>(1); // section header strings are section 0

        bytebuf strtab, symtab;
        strtab.write('\0');
        symtab.write<u64>(0); // reserved symbol 0
        symtab.write<u64>(0);
        symtab.write<u64>(0);
        map<Symbol, u64> sym_indices;
        vector<pair<Symbol, SymbolLocation>> locals, globals, total;
        for (auto& entry : defs) {
            if (entry.first.type == LOCAL_SYMBOL) locals.push(entry);
            else globals.push(entry);
        }
        for (auto& entry : refs) {
            if (defs.find(entry.second.symbol) == defs.end())
                globals.push({ entry.second.symbol, { OS_UNDEF, -1ul }});
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
            info |= (entry.first.type == LOCAL_SYMBOL ? 0 : 1) << 4; // bindings
             // function value assumed for global code
            info |= (entry.first.type == LOCAL_SYMBOL || entry.second.section != OS_CODE ? 0 : 2);
            symtab.write(info);
            symtab.write('\0'); // padding
            symtab.write<u16>(section_indices[entry.second.section]); // section index
            symtab.write<u64>(entry.second.section == OS_UNDEF ? 0 : entry.second.offset); // address
            symtab.write<u64>(0); // symbol size (0 for object files i think)
        }

        resolve_ELF_addends();
        bytebuf reltext, reldata, relstatic;
        for (auto& entry : refs) {
            SymbolLocation sym = entry.first;
            reltext.write<u64>(sym.offset + entry.second.field_offset);
            u64 info = 0;
            info |= sym_indices[entry.second.symbol] << 32l;
            info |= elf_reloc_for(target.arch, entry.second.type, entry.second.symbol.type);
            reltext.write<u64>(info);
        }

        bytebuf shstrtab, shdrs;
        vector<pair<string, ELFSectionHeader>> sections;
        shstrtab.write('\0');
        bytebuf empty;
        sections.push({ "", ELFSectionHeader(0, 0, 0, &empty, 0) });
        sections.push({ ".shstrtab", ELFSectionHeader(3, ELF_SHF_STRINGS, 0, &shstrtab, 0) });
        sections.push({ ".strtab", ELFSectionHeader(3, ELF_SHF_STRINGS, 0, &strtab, 0) });
        sections.push({ ".symtab", ELFSectionHeader(2, 0, 0, &symtab, 24, 2, locals.size() + 1) });
        if (codebuf.size()) {
            sections.push({ ".text", ELFSectionHeader(1, ELF_SHF_ALLOC | ELF_SHF_EXECINSTR, 0, &codebuf, 0) });
            sections.push({ ".rel.text", ELFSectionHeader(9, 0, 0, &reltext, 16, 3, sections.size() - 1) });
        }
        if (databuf.size()) {
            sections.push({ ".rodata", ELFSectionHeader(1, ELF_SHF_ALLOC, 0, &databuf, 0) });
            sections.push({ ".rel.rodata", ELFSectionHeader(9, 0, 0, &reldata, 16, 3, sections.size() - 1) });
        }
        if (staticbuf.size()) {
            sections.push({ ".data", ELFSectionHeader(1, ELF_SHF_ALLOC | ELF_SHF_WRITE, 0, &staticbuf, 0) });
            sections.push({ ".rel.data", ELFSectionHeader(9, 0, 0, &relstatic, 16, 3, sections.size() - 1) });
        }
        for (auto& entry : sections) {
            entry.second.name_index = shstrtab.size();
            shstrtab.write((const char*)entry.first.raw(), entry.first.size() + 1);
        }
        u64 offset = 0x40 + sections.size() * 0x40; // combined size of shdrs and elf header
        for (u32 i = 0; i < sections.size(); i ++) {
            auto& entry = sections[i];
            shdrs.write<u32>(entry.second.name_index); // offset to string
            shdrs.write<u32>(entry.second.type);
            shdrs.write<u64>(entry.second.flags);
            shdrs.write<u64>(0);
            shdrs.write<u64>(offset);
            shdrs.write<u64>(entry.second.buf->size());
            offset += entry.second.buf->size();
            shdrs.write<u32>(entry.second.link);
            shdrs.write<u32>(entry.second.info);
            shdrs.write<u64>(little_endian<u64>(1));
            shdrs.write<u64>(entry.second.entry_size);
        }

        while (shdrs.size()) elf.write(shdrs.read());

        for (auto& entry : sections) {
            while (entry.second.buf->size()) elf.write(entry.second.buf->read());
        }
        
        while (elf.size()) fputc(elf.read(), file);
    }
}

template<>
u64 hash(const jasmine::SymbolRef& symbol) {
    return hash<u64>((u64)symbol.symbol.id
        | (u64)symbol.field_offset << 32
        | (u64)symbol.type << 40
        | (u64)symbol.symbol.type << 48);
}

template<>
u64 hash(const jasmine::SymbolLocation& symbol) {
    return hash<u64>((u64)symbol.section << 62 | symbol.offset);
}