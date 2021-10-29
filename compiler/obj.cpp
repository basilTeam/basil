/*
 * Copyright (c) 2021, the Basil authors
 * All rights reserved.
 *
 * This source code is licensed under the 3-Clause BSD License, the full text
 * of which can be found in the LICENSE file in the root directory
 * of this project.
 */

#include "obj.h"
#include "value.h"
#include "ast.h"
#include "source.h"
#include "forms.h"
#include "type.h"
#include "driver.h"

namespace basil {
    static const char* SECTION_NAMES[] = {
        "none",
        "source",
        "parsed",
        "evaled",
        "ast",
        "ir",
        "jasmine",
        "native",
        "library",
        "data",
        "license"
    };

    // Helpers to read/write various things

    static buffer TEMP;

    // Byte string
    // 4 bytes (LE)     - Size in bytes
    // ? bytes          - String contents, UTF-8

    void write_string(const string& s, bytebuf& buf) {
        buf.write<u32>(little_endian<u32>(s.size()));
        for (u32 i = 0; i < s.size(); i ++) buf.write<u8>(s[i]);
    }

    void write_string(const char* s, bytebuf& buf) {
        write_string(string(s), buf);
    }

    string read_string(bytebuf& buf) {
        u32 len = from_little_endian<u32>(buf.read<u32>());
        string s;
        for (u32 i = 0; i < len; i ++) s += buf.read<u8>();
        return s;
    }

    // UTF-8 string
    // 4 bytes (LE)     - Size in bytes
    // ? bytes          - String contents, UTF-8

    void write_string(const ustring& s, bytebuf& buf) {
        if (TEMP.size() != 0) panic("TEMP buffer was not empty before serializing string!");
        write(TEMP, s);
        u32 len = TEMP.size();
        buf.write<u32>(little_endian<u32>(len));
        for (u32 i = 0; i < len; i ++) buf.write<u8>(TEMP.read());
        if (TEMP.size() != 0) panic("TEMP buffer is not empty after serializing string!");
    }

    ustring read_ustring(bytebuf& buf) {
        u32 len = from_little_endian<u32>(buf.read<u32>());
        if (TEMP.size() != 0) panic("TEMP buffer was not empty before deserializing string!");
        for (u32 i = 0; i < len; i ++) TEMP.write(buf.read<u8>());
        string s(TEMP); // read
        if (TEMP.size() != 0) panic("TEMP buffer is not empty after deserializing string!");
        return ustring(s);
    }

    // Symbol
    // [UTF-8 String]   - Name as string

    void write_symbol(Symbol symbol, bytebuf& buf) {
        write_string(string_from(symbol), buf);
    }

    Symbol read_symbol(bytebuf& buf) {
        return symbol_from(read_ustring(buf));
    }

    // ParamKind
    // 1 byte           - Kind id
    //      -> PK_VARIABLE = 0
    //      -> PK_VARIADIC = 1
    //      -> PK_KEYWORD = 2
    //      -> PK_TERM = 3
    //      -> PK_TERM_VARIADIC = 4
    //      -> PK_QUOTED = 5
    //      -> PK_QUOTED_VARIADIC = 6
    //      -> PK_SELF = 7

    void write_param_kind(ParamKind kind, bytebuf& buf) {
        buf.write<u8>(kind);
    }

    ParamKind read_param_kind(bytebuf& buf) {
        return (ParamKind)buf.read<u8>();
    }

    // Param
    // [ParamKind] bytes    - Param kind
    //   Keyword, Variable, Variadic, Term, Term variadic, Quoted, Quoted variadic:
    //     [Symbol] bytes   - Param name

    void write_param(const Param& p, bytebuf& buf) {
        write_param_kind(p.kind, buf);
        switch (p.kind) {
            case PK_VARIABLE:
            case PK_VARIADIC:
            case PK_KEYWORD:
            case PK_TERM:
            case PK_TERM_VARIADIC:
            case PK_QUOTED:
            case PK_QUOTED_VARIADIC:
                write_symbol(p.name, buf);
                break;
            case PK_SELF:
                break;
            default:
                err({}, "Unsupported param kind '", p.kind, "'.");
                break;
        }
    }

    Param read_param(bytebuf& buf) {
        ParamKind pk = read_param_kind(buf);
        switch (pk) {
            case PK_VARIABLE:
                return p_var(read_symbol(buf));
            case PK_VARIADIC:
                return p_variadic(read_symbol(buf));
            case PK_KEYWORD:
                return p_keyword(read_symbol(buf));
            case PK_TERM:
                return p_term(read_symbol(buf));
            case PK_TERM_VARIADIC:
                return p_term_variadic(read_symbol(buf));
            case PK_QUOTED:
                return p_quoted(read_symbol(buf));
            case PK_QUOTED_VARIADIC:
                return p_quoted_variadic(read_symbol(buf));
            case PK_SELF:
                return P_SELF;
            default:
                err({}, "Unsupported param kind '", pk, "'.");
                return P_SELF;
        }
    }

    // FormKind
    // 1 byte           - Form kind
    //      -> FK_TERM = 0
    //      -> FK_CALLABLE = 1
    //      -> FK_OVERLOADED = 2
    //      -> FK_COMPOUND = 3
    
    void write_form_kind(FormKind fk, bytebuf& buf) {
        buf.write<u8>(fk);
    }

    FormKind read_form_kind(bytebuf& buf) {
        return (FormKind)buf.read<u8>();
    }

    // Form
    // 1 byte           - Form kind
    //  Callable, Overloaded:
    //    1 byte           - Associativity
    //         -> ASSOC_LEFT = 0
    //         -> ASSOC_RIGHT = 1
    //    8 bytes (LE)     - Precedence as signed integer
    //  Callable:
    //    1 byte                - Number of params (N)
    //    [Param]*N bytes       - Param list
    //  Overloaded:
    //    4 bytes               - Number of overloads
    //    [Callable]*N bytes    - Overload list
    //  Compound:
    //    4 bytes                   - Number of members
    //    (Symbol, Value)*N bytes   - Member list

    void write_form(rc<Form> form, bytebuf& buf) {
        write_form_kind(form->kind, buf);
        if (form->kind == FK_CALLABLE || form->kind == FK_OVERLOADED) {
            buf.write<u8>(form->assoc);
            buf.write<i64>(little_endian<i64>(form->precedence));
        }
        if (form->kind == FK_CALLABLE) {
            buf.write<u8>(((rc<Callable>)form->invokable)->parameters->size());
            for (const Param& p : *((rc<Callable>)form->invokable)->parameters)
                write_param(p, buf);
        }
        else if (form->kind == FK_OVERLOADED) {
            buf.write<u32>(little_endian<u32>(((rc<Overloaded>)form->invokable)->overloads.size()));
            for (rc<Callable> callable : ((rc<Overloaded>)form->invokable)->overloads) {
                buf.write<u8>(callable->parameters->size());
                for (const Param& p : *callable->parameters)
                    write_param(p, buf);
            }
        }
        else if (form->kind == FK_COMPOUND) {
            err({}, "Compound forms are currently unsupported in object files.");
        }
    }

    rc<Form> read_form(bytebuf& buf) {
        static vector<rc<Form>> callables;
        static vector<Param> params;
        
        FormKind fk = read_form_kind(buf);
        if (fk == FK_CALLABLE) {
            Associativity assoc = (Associativity)buf.read<u8>();
            i64 prec = from_little_endian<i64>(buf.read<i64>());

            u8 nparams = buf.read<u8>();
            params.clear();
            for (u32 i = 0; i < nparams; i ++) params.push(read_param(buf));
            return f_callable(prec, assoc, params);
        }
        else if (fk == FK_OVERLOADED) {
            Associativity assoc = (Associativity)buf.read<u8>();
            i64 prec = from_little_endian<i64>(buf.read<i64>());

            u32 nforms = from_little_endian<u32>(buf.read<u32>());
            callables.clear();
            for (u32 i = 0; i < nforms; i ++) {
                u8 nparams = buf.read<u8>();
                params.clear();
                for (u32 j = 0; j < nparams; j ++) params.push(read_param(buf));
                callables.push(ref<Callable>(params, none<FormCallback>()));
            }
            return f_overloaded(prec, assoc, callables);
        }
        else if (fk == FK_COMPOUND) {
            err({}, "Compound forms are currently unsupported in object files.");
            return F_TERM;
        }
        else if (fk == FK_TERM) {
            return F_TERM;
        }
        else return F_TERM;
    }

    // Kind
    // 1 byte           - Kind

    void write_kind(Kind kind, bytebuf& buf) {
        buf.write<u8>(kind);
    }

    Kind read_kind(bytebuf& buf) {
        return (Kind)buf.read<u8>();
    }

    // Source::Pos
    // 8 bytes (LE)     - Packed representation of Source::Pos

    void write_pos(Source::Pos pos, bytebuf& buf) {
        u64 packed = *(u64*)(&pos);
        buf.write<u64>(little_endian<u64>(packed));
    }

    Source::Pos read_pos(bytebuf& buf) {
        u64 packed = from_little_endian<u64>(buf.read<u64>());
        return *(Source::Pos*)&packed;
    }

    // Term
    // (terms are a subset of values that can be produced by the parser)
    // Kind (1 byte)
    //   Int:
    //      8 bytes (LE)        - Signed integer value
    //   Float:
    //      4 bytes (LE)        - 32-bit float value
    //   Double:
    //      8 bytes (LE)        - 64-bit float value
    //   Char:                  
    //      4 bytes (LE)        - UTF-8 code point
    //   String:
    //      [UTF-8 string]      - UTF-8 string value
    //   Symbol:
    //      [Symbol]            - Symbol value
    //   List:
    //      4 bytes (LE)        - Number of elements (N)
    //      [Term]*N            - Element list

    void write_term(const Value& term, const Section& section, bytebuf& buf) {
        write_kind(term.type.kind(), buf);
        write_pos(term.pos, buf);
        switch (term.type.kind()) {
            case K_INT: return buf.write<i64>(little_endian<i64>(term.data.i));
            case K_FLOAT: return buf.write<u32>(little_endian<u32>(*(u32*)&term.data.f32));
            case K_DOUBLE: return buf.write<u64>(little_endian<u64>(*(u64*)&term.data.f64));
            case K_CHAR: return buf.write<u32>(little_endian<u32>(term.data.ch.u));
            case K_STRING: return write_string(term.data.string->data, buf);
            case K_SYMBOL: return write_symbol(term.data.sym, buf);
            case K_VOID: return;
            case K_LIST: 
                buf.write<u32>(little_endian<u32>(v_list_len(term)));
                for (const Value& v : iter_list(term)) write_term(v, section, buf);
                return;
            default:
                err({}, "Tried to serialize unsupported term type '", term.type, "'.");
                return;
        }
    }

    Value read_term(const Section& section, bytebuf& buf) {
        Kind k = (Kind)buf.read();
        Source::Pos pos = read_pos(buf);
        switch (k) {
            case K_INT: return v_int(pos, from_little_endian<i64>(buf.read<i64>()));
            case K_FLOAT: return v_float(pos, (const float&)from_little_endian<u32>(buf.read<u32>()));
            case K_DOUBLE: return v_double(pos, (const double&)from_little_endian<u64>(buf.read<u64>()));
            case K_CHAR: return v_char(pos, rune{from_little_endian<u32>(buf.read<u32>())});
            case K_STRING: return v_string(pos, read_ustring(buf));
            case K_SYMBOL: return v_symbol(pos, read_symbol(buf));
            case K_VOID: return v_void(pos);
            case K_LIST: {
                vector<Value> values;
                u32 len = from_little_endian<u32>(buf.read<u32>());
                for (u32 i = 0; i < len; i ++)
                    values.push(read_term(section, buf));
                return v_list(pos, t_list(T_ANY), move(values));
            }
            default:
                err({}, "Tried to read unsupported term kind '", k, "'.");
                return v_error({});
        }
    }

    void write_def(Symbol name, const DefInfo& def, bytebuf& buf) {
        write_symbol(name, buf);
        buf.write<u32>(little_endian<u32>(def.offset));
        buf.write<u8>(def.form ? 1 : 0);
        buf.write<u8>(def.type ? 1 : 0);
        if (def.form) write_form(*def.form, buf);
        if (def.type) panic("Can't serialize types yet!"); // skip this for now
    }

    pair<Symbol, DefInfo> read_def(bytebuf& buf) {
        Symbol s = read_symbol(buf);
        u32 offset = from_little_endian<u32>(buf.read<u32>());
        bool has_form = buf.read<u8>();
        bool has_type = buf.read<u8>();
        DefInfo info = { offset, none<rc<Form>>(), none<Type>() };
        if (has_form) info.form = read_form(buf);
        if (has_type) panic("Can't deserialize types yet!");
        return {s, info};
    }

    Section::Section(SectionType type_in, const ustring& name_in, const map<Symbol, DefInfo>& defs_in):
        type(type_in), name(name_in), defs(defs_in) {}

    Section::~Section() {}

    void Section::serialize_header(bytebuf& buf) {
        buf.write<u8>(type);
        write_string(name, buf);
        buf.write<u32>(little_endian<u32>(defs.size()));
        for (const auto& [k, v] : defs) {
            write_def(k, v, buf);
        }
    }

    struct SourceSection : public Section {
        rc<Source> src;
        SourceSection(const ustring& name):
            Section(ST_SOURCE, name, map<Symbol, DefInfo>()) {}
        
        void deserialize(bytebuf& buf) {
            u32 len = little_endian<u32>(buf.read<u32>());
            buffer tmp;
            for (u32 i = 0; i < len; i ++) {
                tmp.write(buf.read());
            }
            src = ref<Source>(tmp);
        }

        void serialize(bytebuf& buf) {
            if (TEMP.size() != 0) panic("TEMP buffer was not empty before serializing source!");
            for (u32 i = 0; i < src->size(); i ++) {
                const ustring& s = (*src)[i];
                write(TEMP, s);
            }
            u32 len = TEMP.size();
            buf.write<u32>(little_endian<u32>(len));
            for (u32 i = 0; i < len; i ++) buf.write(TEMP.read());
            if (TEMP.size() != 0) panic("TEMP buffer is not empty after serializing source!");
        }
    };

    struct ParsedSection : public Section {
        Value term;
        ParsedSection(const ustring& name):
            Section(ST_PARSED, name, map<Symbol, DefInfo>()) {}
        
        void deserialize(bytebuf& buf) {
            term = read_term(*this, buf);
        }

        void serialize(bytebuf& buf) {
            write_term(term, *this, buf);
        }
    };

    struct ModuleSection : public Section {
        rc<Env> env;
        Value main;
        ModuleSection(const ustring& name, const map<Symbol, DefInfo>& defs):
            Section(ST_EVAL, name, defs) {}
        
        void deserialize(bytebuf& buf) {
            panic("Unimplemented!");
        }

        void serialize(bytebuf& buf) {
            panic("Unimplemented!");
        }
    };

    struct ASTSection : public Section {
        map<Symbol, rc<AST>> functions;
        rc<AST> main;
        rc<Env> env;
        ASTSection(const ustring& name, const map<Symbol, DefInfo>& defs):
            Section(ST_AST, name, defs) {}
        
        void deserialize(bytebuf& buf) {
            panic("Unimplemented!");
        }

        void serialize(bytebuf& buf) {
            panic("Unimplemented!");
        }
    };

    struct IRSection : public Section {
        map<Symbol, rc<IRFunction>> functions;
        rc<IRFunction> main;
        IRSection(const ustring& name, const map<Symbol, DefInfo>& defs):
            Section(ST_IR, name, defs) {}
        
        void deserialize(bytebuf& buf) {
            panic("Unimplemented!");
        }

        void serialize(bytebuf& buf) {
            panic("Unimplemented!");
        }
    };

    struct JasmineSection : public Section {
        rc<jasmine::Object> object;
        JasmineSection(const ustring& name, const map<Symbol, DefInfo>& defs):
            Section(ST_JASMINE, name, defs) {}
        
        void deserialize(bytebuf& buf) {
            panic("Unimplemented!");
        }

        void serialize(bytebuf& buf) {
            panic("Unimplemented!");
        }
    };

    struct NativeSection : public Section {
        rc<jasmine::Object> object;
        NativeSection(const ustring& name, const map<Symbol, DefInfo>& defs):
            Section(ST_NATIVE, name, defs) {}
        
        void deserialize(bytebuf& buf) {
            panic("Unimplemented!");
        }

        void serialize(bytebuf& buf) {
            panic("Unimplemented!");
        }
    };

    rc<Section> make_section(SectionType type, const ustring& name, const map<Symbol, DefInfo>& defs) {
        switch (type) {
            case ST_SOURCE:
                return ref<SourceSection>(name);
            case ST_PARSED:
                return ref<ParsedSection>(name);
            case ST_EVAL:
                return ref<ModuleSection>(name, defs);
            case ST_AST:
                return ref<ASTSection>(name, defs);
            case ST_IR:
                return ref<IRSection>(name, defs);
            case ST_JASMINE:
                return ref<JasmineSection>(name, defs);
            case ST_NATIVE:
                return ref<NativeSection>(name, defs);
            default:
                err({}, "Attempted to load unsupported section type '", SECTION_NAMES[type], "'!");
                return nullptr;
        }
    }

    Object::Object(): version{BASIL_MAJOR_VERSION, BASIL_MINOR_VERSION, BASIL_PATCH_VERSION} {}

    // Loads this object in full from the provided stream.
    void Object::read(stream& io) {
        bytebuf buf;
        while (io) buf.write<u8>(io.read());
        char magic[11];
        for (u32 i = 0; i < 10; i ++) magic[i] = buf.read<u8>();
        magic[10] = '\0';
        if (string(magic) != "#!basil\n\v\v")
            return err({}, "Incorrect magic bytes in Basil object!");
        
        u16 major = from_little_endian<u16>(buf.read<u16>());
        u16 minor = from_little_endian<u16>(buf.read<u16>());
        u16 patch = from_little_endian<u16>(buf.read<u16>());
        if (major > BASIL_MAJOR_VERSION 
            || minor > BASIL_MINOR_VERSION
            || patch > BASIL_PATCH_VERSION)
            return err({}, "Basil object requires at least compiler version ", 
                major, ".", minor, ".", patch, ", but compiler is of incompatible version ",
                BASIL_MAJOR_VERSION, ".", BASIL_MINOR_VERSION, ".", BASIL_PATCH_VERSION, "!");
        version = {BASIL_MAJOR_VERSION, BASIL_MINOR_VERSION, BASIL_PATCH_VERSION}; // bump up to current
        
        u32 num_sections = from_little_endian<u32>(buf.read<u32>());

        i32 main_id = from_little_endian<i32>(buf.read<i32>()); // main section
        main_section = main_id >= 0 ? some<u32>(main_id) : none<u32>();

        if (main_section && *main_section >= num_sections)
            return err({}, "Main section index is too high: main index is ", *main_section, 
                ", but object only has ", num_sections, " sections.");

        for (u32 i = 0; i < 8; i ++) buf.read<u8>(); // unused

        for (u32 i = 0; i < num_sections; i ++) {
            SectionType type = (SectionType)buf.read<u8>();
            ustring name = read_ustring(buf);
            u32 num_defs = from_little_endian<u32>(buf.read<u32>());
            map<Symbol, DefInfo> defs;
            for (u32 j = 0; j < num_defs; j ++)
                defs.insert(read_def(buf));
            rc<Section> section = make_section(type, name, defs);
            section->deserialize(buf);
            sections.push(section);
        }
    }

    // Writes this object to the provided stream.
    void Object::write(stream& io) {
        bytebuf buf;
        for (u32 i = 0; i < 10; i ++) buf.write<u8>("#!basil\n\v\v"[i]); // magic bytes
        buf.write<u16>(little_endian<u16>(version.major));
        buf.write<u16>(little_endian<u16>(version.minor));
        buf.write<u16>(little_endian<u16>(version.patch));
        buf.write<u32>(little_endian<u32>(sections.size()));
        buf.write<i32>(little_endian<i32>(main_section ? *main_section : -1));
        for (u32 i = 0; i < 8; i ++) buf.write<u8>(0); // unused
        for (rc<Section> section : sections) {
            section->serialize_header(buf);
            section->serialize(buf);
        }
        while (buf.size()) io.write(buf.read());
    }

    rc<Source> source_from_section(rc<Section> section) {
        if (section->type != ST_SOURCE) panic("Tried to read source text from non-source section!");
        return ((rc<SourceSection>)section)->src;
    }

    Value parsed_from_section(rc<Section> section) {
        if (section->type != ST_PARSED) panic("Tried to read parsed program from non-parsed section!");
        return ((rc<ParsedSection>)section)->term;
    }

    rc<Env> module_from_section(rc<Section> section) {
        if (section->type != ST_EVAL) panic("Tried to read module from non-module section!");
        return ((rc<ModuleSection>)section)->env;
    }

    Value module_main(rc<Section> section) {
        if (section->type != ST_EVAL) panic("Tried to get module main from non-module section!");
        return ((rc<ModuleSection>)section)->main;
    }

    const map<Symbol, rc<AST>>& ast_from_section(rc<Section> section) {
        if (section->type != ST_AST) panic("Tried to read AST from non-AST section!");
        return ((rc<ASTSection>)section)->functions;
    }

    rc<AST> ast_main(rc<Section> section) {
        if (section->type != ST_AST) panic("Tried to get AST main from non-AST section!");
        return ((rc<ASTSection>)section)->main;
    }

    rc<Env> ast_env(rc<Section> section) {
        if (section->type != ST_AST) panic("Tried to get AST main from non-AST section!");
        return ((rc<ASTSection>)section)->env;
    }

    const map<Symbol, rc<IRFunction>>& ir_from_section(rc<Section> section) {
        if (section->type != ST_IR) panic("Tried to read IR from non-IR section!");
        return ((rc<IRSection>)section)->functions;
    }

    rc<IRFunction> ir_main(rc<Section> section) {
        if (section->type != ST_IR) panic("Tried to get IR main from non-IR section!");
        return ((rc<IRSection>)section)->main;
    }

    const jasmine::Object& jasmine_from_section(rc<Section> section) {
        if (section->type != ST_JASMINE) panic("Tried to read Jasmine object from non-Jasmine section!");
        return *((rc<JasmineSection>)section)->object;
    }

    const jasmine::Object& native_from_section(rc<Section> section) {
        if (section->type != ST_NATIVE) panic("Tried to read native object from non-native section!");
        return *((rc<NativeSection>)section)->object;
    }


    // rc<Env> module_from_section(rc<Section> section) {

    // }

    // rc<AST> ast_from_section(rc<Section> section) {

    // }

    rc<Section> source_section(const ustring& name, rc<Source> source) {
        rc<SourceSection> section = ref<SourceSection>(name);
        section->src = source;
        return section;   
    }

    rc<Section> parsed_section(const ustring& name, const Value& term) {
        rc<ParsedSection> section = ref<ParsedSection>(name);  
        section->term = term;
        return section;
    }

    rc<Section> module_section(const ustring& name, const Value& main, rc<Env> env) {
        rc<ModuleSection> section = ref<ModuleSection>(name, map<Symbol, DefInfo>());  
        section->env = env;
        section->main = main;
        return section;
    }

    rc<Section> ast_section(const ustring& name, rc<AST> main, const map<Symbol, rc<AST>>& functions, rc<Env> env) {
        rc<ASTSection> section = ref<ASTSection>(name, map<Symbol, DefInfo>());  
        section->functions = functions;
        section->main = main;
        section->env = env;
        return section;
    }

    rc<Section> ir_section(const ustring& name, rc<IRFunction> main, const map<Symbol, rc<IRFunction>>& functions) {
        rc<IRSection> section = ref<IRSection>(name, map<Symbol, DefInfo>());  
        section->functions = functions;
        section->main = main;
        return section;
    }

    rc<Section> jasmine_section(const ustring& name, rc<jasmine::Object> object) {
        rc<JasmineSection> section = ref<JasmineSection>(name, map<Symbol, DefInfo>());  
        section->object = object;
        return section;
    }

    rc<Section> native_section(const ustring& name, rc<jasmine::Object> object) {
        rc<NativeSection> section = ref<NativeSection>(name, map<Symbol, DefInfo>());  
        section->object = object;
        return section;
    }
}