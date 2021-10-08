#include "bc.h"
#include "stdlib.h"
#include "stdio.h"
#include "util/str.h"
#include "util/hash.h"
#include "util/sets.h"
#include "x64.h"

namespace jasmine {
    struct Insn;
    struct Op;

    const Type I8 = { K_I8, 0 }, I16 = { K_I16, 1 }, I32 = { K_I32, 2 }, I64 = { K_I64, 3 }, 
        U8 = { K_U8, 4 }, U16 = { K_U16, 5 }, U32 = { K_U32, 6 }, U64 = { K_U64, 7 }, 
        F32 = { K_F32, 8 }, F64 = { K_F64, 9 }, PTR = { K_PTR, 10 };

    static u64 TYPE_ID = 0;

    // Populates an instruction with something from a text representation.
    using Parser = void(*)(Context&, stream&, Insn&);

    // Populates an instruction with something pulled from encoded binary.
    using Disassembler = void(*)(Context& context, bytebuf&, const Object&, Insn&, ParamKind);

    // Validates an instruction at the param 'param'. Returns the next param
    // if validation is complete, the current param if more validation is necessary,
    // or -1 if an error occurred.
    using Validator = i64(*)(const Context& context, const Insn&, i64 param);

    // Writes an instruction component to Jasmine bytecode in an object file.
    using Assembler = i64(*)(const Context& context, Object&, const Insn&, i64 param);

    // Pretty-prints an instruction component at index 'param' to the provided stream.
    using Printer = i64(*)(const Context& context, stream&, const Insn&, i64 param);

    struct OpComponent {
        Parser parser;
        Disassembler disassembler;
        Validator validator;
        Assembler assembler;
        Printer printer;
    };

    // Represents the properties of a unique opcode, specifically how it is 
    // decoded from text and binary, and how it is validated.
    struct Op {
        Opcode opcode;
        vector<const OpComponent*> components;

        void add_components() {}

        template<typename... Args>
        void add_components(const OpComponent& component, const Args&... args) {
            components.push(&component);
            add_components(args...);
        }

        // Constructs an Op from a series of parsers, disassemblers, and validators.
        template<typename... Args>
        Op(Opcode opcode_in, const Args&... args): opcode(opcode_in) {
            add_components(args...);
        }
    };

    // Parsers

    bool is_separator(char ch) {
        return isspace(ch) || ch == ',' || ch == '\0' || ch == ')' || ch == ']' || ch == '}' 
            || ch == '(' || ch == '[' || ch == '{' || ch == ':' || ch == '.';
    }

    void consume_leading_space(stream& io) {
        while (isspace(io.peek()) && io.peek()) io.read();
        if (io.peek() == ';') { // comment
            while (io.peek() && io.read() != '\n');
            consume_leading_space(io); // continue onto next line
        }
    }

    void expect(char ch, stream& io) {
        consume_leading_space(io);
        if (io.peek() != ch) {
            fprintf(stderr, "Expected '%c', got '%c'.\n", ch, io.peek());
            exit(1);
        }
        else io.read();
    }

    string next_string(stream& io) {
        while (is_separator(io.peek())) io.read();
        string s;
        while (!is_separator(io.peek())) s += io.read();
        return s;
    }

    pair<string, Type> type_lookup[] = {
        pair<string, Type>("i8", I8),
        pair<string, Type>("i16", I16),
        pair<string, Type>("i32", I32),
        pair<string, Type>("i64", I64),
        pair<string, Type>("u8", U8),
        pair<string, Type>("u16", U16),
        pair<string, Type>("u32", U32),
        pair<string, Type>("u64", U64),
        pair<string, Type>("f32", F32),
        pair<string, Type>("f64", F64),
        pair<string, Type>("ptr", PTR) 
    };

    string typename_lookup[] = {
        "struct",
        "ptr",
        "f32",
        "f64",
        "i8",
        "i16",
        "i32",
        "i64",
        "u8",
        "u16",
        "u32",
        "u64"
    };

    static const u64 NUM_TYPE_ENTRIES = sizeof(type_lookup) / sizeof(pair<string, Type>);

    Type find_type(Context& context, const string& s) {
        for (u64 i = 0; i < NUM_TYPE_ENTRIES; i ++) {
            if (s == type_lookup[i].first) return type_lookup[i].second;
        }
        
        auto it = context.type_decls.find(s);
        if (it == context.type_decls.end()) {
            fprintf(stderr, "[ERROR] Undefined typename '%s'.\n", (const char*)s.raw());
            exit(1);
        }
        return { K_STRUCT, it->second };
    }

    u64 to_int(const string& s) {
        i64 acc = 0;
        for (u32 i = 0; i < s.size(); i ++) {
            char r = s[i];
            i32 val = r - '0';
            if (acc > 9223372036854775807l / 10) {
                fprintf(stderr, "[ERROR] Immediate parameter is too large.\n");
                exit(1);
            }
            acc *= 10;
            acc += val;
        }
        return acc;
    }

    void parse_type(Context& context, stream& io, Insn& insn) {
        consume_leading_space(io);
        string type_name = next_string(io);
        insn.type = find_type(context, type_name);
    }

    i64 parse_number(stream& io) {
        bool negative = io.peek() == '-';
        if (negative) io.read();

        string num = next_string(io);
        for (u32 i = 0; i < num.size(); i ++) {
            if (num[i] < '0' || num[i] > '9') {
                fprintf(stderr, "[ERROR] Unexpected character '%c' in immediate.\n", num[i]);
                exit(1);
            }
        }
        return negative ? -(i64)to_int(num) : (i64)to_int(num);
    }

    Reg parse_register(Context& context, bool undefined_error, stream& io) {
        expect('%', io);
        if (io.peek() >= '0' && io.peek() <= '9') {
            return { false, (u64)parse_number(io) };
        }
        else {
            string reg = next_string(io);
            auto it = context.global_decls.find(reg);
            if (it == context.global_decls.end() && undefined_error) {
                fprintf(stderr, "[ERROR] Undefined global register '%s'.\n", (const char*)reg.raw());
                exit(1);
            }
            else if (it == context.global_decls.end()) {
                return { true, context.global_decls.size() };
            }
            return it->second;
        }
    }

    void parse_param(Context& context, stream& io, Insn& insn) {
        consume_leading_space(io);
        Param p;
        if (io.peek() == '[') { // memory param
            p.kind = PK_MEM;
            expect('[', io);
            parse_param(context, io, insn);
            Param ptr = insn.params.back();
            insn.params.pop();
            consume_leading_space(io);
            if (io.peek() == '+' || io.peek() == '-') {
                i64 negative = io.peek() == '-' ? -1 : 1;
                io.read();
                consume_leading_space(io);
                if ((io.peek() >= '0' && io.peek() <= '9') || io.peek() == '-') {
                    if (ptr.kind == PK_REG) {
                        p.data.mem.kind = MK_REG_OFF;
                        p.data.mem.reg = ptr.data.reg;
                        p.data.mem.off = negative * parse_number(io);
                    }
                    else {
                        p.data.mem.kind = MK_LABEL_OFF;
                        p.data.mem.label = ptr.data.label;
                        p.data.mem.off = negative * parse_number(io);
                    }
                }
                else {
                    string type_name = next_string(io);
                    Type t = find_type(context, type_name);
                    if (io.peek() == '.') {
                        if (t.kind != K_STRUCT) {
                            fprintf(stderr, "[ERROR] Tried to get field from non-struct type '%s'.\n", 
                                (const char*)type_name.raw());
                            exit(1);
                        }
                        string field = next_string(io);
                        const TypeInfo& info = context.type_info[t.id];
                        i64 off = 0;
                        for (int i = 0; i < info.members.size(); i ++) 
                            if (field == info.members[i].name) off = i;
                        p.data.mem.kind = ptr.kind == PK_REG ? MK_REG_TYPE : MK_LABEL_TYPE;
                        if (ptr.kind == PK_REG) p.data.mem.reg = ptr.data.reg;
                        else p.data.mem.label = ptr.data.label;
                        p.data.mem.off = off + 1;
                        p.data.mem.type = t;
                    }
                    else {
                        p.data.mem.kind = ptr.kind == PK_REG ? MK_REG_TYPE : MK_LABEL_TYPE;
                        if (ptr.kind == PK_REG) p.data.mem.reg = ptr.data.reg;
                        else p.data.mem.label = ptr.data.label;
                        p.data.mem.off = 0;
                        p.data.mem.type = t;
                    }
                }
            }
            else {
                if (ptr.kind == PK_REG) {
                    p.data.mem.kind = MK_REG_OFF;
                    p.data.mem.reg = ptr.data.reg;
                    p.data.mem.off = 0;
                }
                else {
                    p.data.mem.kind = MK_LABEL_OFF;
                    p.data.mem.label = ptr.data.label;
                    p.data.mem.off = 0;
                }
            }
            expect(']', io);
        }
        else if (io.peek() == '%') { // register
            p.kind = PK_REG;
            p.data.reg = parse_register(context, true, io);
        }
        else if ((io.peek() >= '0' && io.peek() <= '9') || io.peek() == '-') { // immediate
            p.kind = PK_IMM;
            p.data.imm.val = parse_number(io);
        }
        else { // label probably
            p.kind = PK_LABEL;
            p.data.label = local((const char*)next_string(io).raw());
        }
        insn.params.push(p);
    }

    void parse_another_param(Context& context, stream& io, Insn& insn) {
        expect(',', io);
        parse_param(context, io, insn);
    }

    void parse_variadic_param(Context& context, stream& io, Insn& insn) {
        expect('(', io);
        bool first = true;
        while (io.peek() != ')') {
            if (!first) expect(',', io);
            Type prev = insn.type;
            parse_type(context, io, insn);
            Type ann = insn.type;
            insn.type = prev;
            parse_param(context, io, insn);
            insn.params.back().annotation = some<Type>(ann);
            first = false;
        }
        expect(')', io);
    }

    Member parse_member(Context& context, stream& io) {
        string name = next_string(io);
        expect(':', io);
        consume_leading_space(io);
        u64 count = 0;
        optional<Type> type = none<Type>(); 
        if (io.peek() >= '0' && io.peek() <= '9') {
            count = parse_number(io);
        }
        else {
            string type_name = next_string(io);
            type = some<Type>(find_type(context, type_name));
            consume_leading_space(io);
            if (io.peek() == '*') {
                expect('*', io);
                consume_leading_space(io);
                count = parse_number(io);
            }
            else count = 1;
        }
        return Member{ name, count, type };
    }

    void parse_typedef(Context& context, stream& io, Insn& insn) {
        string name = next_string(io);
        vector<Member> members;
        expect('{', io);
        bool first = true;
        while (io.peek() != '}') {
            if (!first) expect(',', io);
            members.push(parse_member(context, io));
            first = false;
            consume_leading_space(io);
        }
        expect('}', io);
        if (context.type_decls.find(name) != context.type_decls.end()) {
            fprintf(stderr, "[ERROR] Duplicate type definition '%s'!\n", (const char*)name.raw());
            exit(1);
        }
        TypeInfo info = TypeInfo{ context.type_info.size(), name, members };
        context.type_info.push(info);
        context.type_decls.put(name, info.id);
        insn.type = Type{ K_STRUCT, info.id };
    }

    void assemble_60bit(bytebuf& io, i64 i, bool extra_bit) {
        i64 o = i;
        i64 n = 0;
        u8 data[8];
        while (i >= 4096) {
            data[n ++] = i % 256;
            i /= 256;
        }
        if (i >= 256) {
            data[n ++] = i % 256;
            i /= 256;
        }
        if (i >= 16) {
            fprintf(stderr, "[ERROR] Constant integer or id %ld is too big to be encoded within 60 bits!\n", o);
            exit(1);
        }
        data[n] = i | n << 5 | (extra_bit ? 1 : 0) << 4;
        while (n >= 0) io.write(data[n --]);
    }

    // Disassemblers

    pair<i64, bool> disassemble_60bit(bytebuf& buf) {
        u8 head = buf.read<u8>();
        u8 n = head >> 5 & 7;
        bool bit = head >> 4 & 1;

        i64 acc = head & 15;
        while (n > 0) {
            acc *= 256;
            acc += buf.read<u8>();
            n --;
        } 

        return { acc, bit };
    }

    string disassemble_string(const Context& context, bytebuf& buf) {
        u8 length = buf.read<u8>();
        string s;
        for (u32 i = 0; i < length; i ++) s += buf.read<u8>();
        return s;
    }

    Type disassemble_type(const Context& context, bytebuf& buf) {
        Type type;
        type.kind = Kind(buf.read<u8>() >> 4);
        if (type.kind == K_STRUCT) type.id = disassemble_60bit(buf).first;
        return type;
    }

    i64 disassemble_imm(const Context& context, bytebuf& buf) {
        auto result = disassemble_60bit(buf);
        return result.first * (result.second ? -1 : 1);
    }

    Reg disassemble_reg(const Context& context, bytebuf& buf) {
        auto result = disassemble_60bit(buf);
        return Reg{ result.second, u64(result.first) };
    }

    Symbol disassemble_label(const Context& context, bytebuf& buf, const Object& obj) {
        for (u8 i = 0; i < 4; i ++) buf.read<u8>();
        auto it = obj.references().find(obj.code().size() - buf.size());
        if (it == obj.references().end()) {
            fprintf(stderr, "[ERROR] Undefined symbol in object file!\n");
            exit(1);
        }
        return it->second.symbol;
    }

    void disassemble_param(Context& context, bytebuf& buf, const Object& obj, Insn& insn, ParamKind pk) {
        Param p;
        p.kind = pk;
        switch (pk) {
            case PK_REG:
                p.data.reg = disassemble_reg(context, buf);
                break;
            case PK_MEM: {
                p.data.mem.kind = MemKind(buf.read<u8>() >> 6 & 3);
                switch (p.data.mem.kind) {
                    case MK_REG_OFF:
                        p.data.mem.reg = disassemble_reg(context, buf);
                        p.data.mem.off = disassemble_imm(context, buf);
                        break;
                    case MK_LABEL_OFF:
                        p.data.mem.label = disassemble_label(context, buf, obj);
                        p.data.mem.off = disassemble_imm(context, buf);
                        break;
                    case MK_REG_TYPE:
                        p.data.mem.reg = disassemble_reg(context, buf);
                        p.data.mem.type = disassemble_type(context, buf);
                        p.data.mem.off = disassemble_imm(context, buf);
                        break;
                    case MK_LABEL_TYPE:
                        p.data.mem.label = disassemble_label(context, buf, obj);
                        p.data.mem.type = disassemble_type(context, buf);
                        p.data.mem.off = disassemble_imm(context, buf);
                        break;
                }
                break;
            }
            case PK_LABEL: 
                p.data.label = disassemble_label(context, buf, obj);
                break;
            case PK_IMM:
                p.data.imm = Imm{ disassemble_imm(context, buf) };
                break;
        }
        insn.params.push(p);
    }

    void disassemble_type(Context& context, bytebuf& buf, const Object& obj, Insn& insn, ParamKind pk) {
        // don't disassemble type directly
    }

    void disassemble_typedef(Context& context, bytebuf& buf, const Object& obj, Insn& insn, ParamKind pk) {
        string name = disassemble_string(context, buf);
        vector<Member> members;
        i64 num_members = disassemble_60bit(buf).first;
        for (u32 i = 0; i < num_members; i ++) {
            Member member;
            member.name = disassemble_string(context, buf);
            auto result = disassemble_60bit(buf);
            member.count = result.first;
            if (!result.second) member.type = none<Type>();
            else member.type = some<Type>(disassemble_type(context, buf));
            members.push(member);
        }
        TypeInfo info = TypeInfo{ context.type_info.size(), name, members };
        context.type_info.push(info);
        context.type_decls.put(name, info.id);
        insn.type = Type{ K_STRUCT, info.id };
    }

    void disassemble_variadic_param(Context& context, bytebuf& buf, const Object& obj, Insn& insn, ParamKind pk) {
        u8 n = buf.read<u8>();
        vector<ParamKind> kinds;
        ParamKind acc[4]; // this song and dance serves to reverse the kinds we read from the encoded call
        u8 m = 0;
        if (n > 0) {
            u8 packed = buf.read<u8>();
            for (u32 i = 0; i < n; i ++) {
                acc[m ++] = ParamKind(packed & 3);
                packed >>= 2;
                if (m == 4) {
                    packed = buf.read<u8>();
                    for (i64 j = 3; j >= 0; j --) kinds.push(acc[j]);
                    m = 0;
                }
            }
        }
        for (i64 j = m - 1; j >= 0; j --) kinds.push(acc[j]);

        for (ParamKind pk : kinds) {
            Type type = disassemble_type(context, buf);
            disassemble_param(context, buf, obj, insn, pk);
            insn.params.back().annotation = some<Type>(type);
        }
    }
    
    // Validators

    // Assemblers

    void assemble_string(const Context& context, const string& string, Object& obj) {
        obj.code().write<u8>(string.size());
        for (u32 i = 0; i < string.size(); i ++) obj.code().write<u8>(string[i]);
    }

    void assemble_type(const Context& context, const Type& type, Object& obj) {
        u8 typekind = type.kind << 4;
        obj.code().write(typekind);
        if (type.kind == K_STRUCT) assemble_60bit(obj.code(), type.id, false);
    }

    void assemble_typedef(const Context& context, const Type& type, Object& obj) {
        assemble_string(context, context.type_info[type.id].name, obj);
        assemble_60bit(obj.code(), context.type_info[type.id].members.size(), false);
        for (const Member& m : context.type_info[type.id].members) {
            assemble_string(context, m.name, obj);  // field name
            assemble_60bit(obj.code(), m.count, m.type);    // field count
            if (m.type) assemble_type(context, *m.type, obj);
        }
    }

    void assemble_param(const Context& context, const Param& param, Object& obj) {
        switch (param.kind) {
            case PK_REG:
                assemble_60bit(obj.code(), param.data.reg.id, param.data.reg.global);
                break;
            case PK_MEM:
                obj.code().write(param.data.mem.kind << 6); // memkind
                switch (param.data.mem.kind) {
                    case MK_REG_OFF:
                        assemble_60bit(obj.code(), param.data.mem.reg.id, param.data.mem.reg.global);
                        assemble_60bit(obj.code(), abs(param.data.mem.off), param.data.mem.off < 0);
                        break;
                    case MK_LABEL_OFF:
                        for (u8 i = 0; i < 4; i ++) obj.code().write<u8>(0);
                        obj.reference(param.data.mem.label, REL32_LE, -4);
                        assemble_60bit(obj.code(), abs(param.data.mem.off), param.data.mem.off < 0);
                        break;
                    case MK_REG_TYPE:
                        assemble_60bit(obj.code(), param.data.mem.reg.id, param.data.mem.reg.global);
                        assemble_type(context, param.data.mem.type, obj);
                        assemble_60bit(obj.code(), abs(param.data.mem.off), param.data.mem.off < 0);
                        break;
                    case MK_LABEL_TYPE:
                        for (u8 i = 0; i < 4; i ++) obj.code().write<u8>(0);
                        obj.reference(param.data.mem.label, REL32_LE, -4);
                        assemble_60bit(obj.code(), param.data.mem.off, false);
                        assemble_type(context, param.data.mem.type, obj);
                        assemble_60bit(obj.code(), abs(param.data.mem.off), param.data.mem.off < 0);
                        break;
                }
                break;
            case PK_LABEL:
                for (u8 i = 0; i < 4; i ++) obj.code().write<u8>(0);
                obj.reference(param.data.label, REL32_LE, -4);
                break;
            case PK_IMM:
                assemble_60bit(obj.code(), abs(param.data.imm.val), param.data.imm.val < 0);
                break;
        }
    }

    i64 assemble_type(const Context& context, Object& obj, const Insn& insn, i64 param) {
        return param; // don't assemble insn types independently
    }

    i64 assemble_typedef(const Context& context, Object& obj, const Insn& insn, i64 param) {
        assemble_typedef(context, insn.type, obj);
        return param + 1;
    }

    i64 assemble_param(const Context& context, Object& obj, const Insn& insn, i64 param) {
        assemble_param(context, insn.params[param], obj);
        return param + 1;
    }

    i64 assemble_label(const Context& context, Object& obj, const Insn& insn, i64 param) {
        assemble_param(context, insn.params[param], obj);
        return param + 1;
    }

    i64 assemble_variadic_param(const Context& context, Object& obj, const Insn& insn, i64 param) {
        u8 n = insn.params.size() - param;
        obj.code().write<u8>(n);

        // each byte contains 1-4 param kinds
        u8 acc = 0;
        for (i64 i = param; i < insn.params.size(); i ++) {
            if (i > param && (i - param) % 4 == 0) obj.code().write<u8>(acc), acc = 0;
            acc <<= 2;
            acc |= insn.params[i].kind;
        }
        if (n > 0) obj.code().write<u8>(acc);

        // encode type adjacent to value
        for (i64 i = param; i < insn.params.size(); i ++) {
            assemble_type(context, *insn.params[i].annotation, obj);
            assemble_param(context, insn.params[i], obj);
        }

        return insn.params.size();
    }

    // Printers

    void print_type(const Context& context, stream& io, Type t, const char* prefix) {
        if (t.kind == K_STRUCT) write(io, prefix, context.type_info[t.id].name);
        else write(io, prefix, typename_lookup[t.kind]);
    }

    i64 print_type(const Context& context, stream& io, const Insn& insn, i64 param) {
        print_type(context, io, insn.type, " ");
        return param;
    }

    void print_reg(const Context& context, stream& io, Reg reg) {
        if (reg.global) {
            write(io, context.global_info[reg.id]);
        } else {
            write(io, "%", reg.id);
        }
    }

    void print_param(const Context& context, const Param& p, stream& io, const char* prefix) {
        switch (p.kind) {
            case PK_IMM:
                write(io, prefix, p.data.imm.val);
                break;
            case PK_LABEL:
                write(io, prefix, name(p.data.label));
                break;
            case PK_REG:
                write(io, prefix);
                print_reg(context, io, p.data.reg);
                break;
            case PK_MEM: switch (p.data.mem.kind) {
                case MK_REG_OFF:
                    write(io, prefix, "[");
                    print_reg(context, io, p.data.mem.reg);
                    if (p.data.mem.off != 0) 
                        write(io, p.data.mem.off < 0 ? " - " : " + ", 
                            p.data.mem.off < 0 ? -p.data.mem.off : p.data.mem.off);
                    write(io, "]");
                    break;
                case MK_LABEL_OFF:
                    write(io, prefix, "[", name(p.data.mem.label));
                    if (p.data.mem.off != 0) 
                        write(io, p.data.mem.off < 0 ? " - " : " + ", 
                            p.data.mem.off < 0 ? -p.data.mem.off : p.data.mem.off);
                    write(io, "]");
                    break;
                case MK_REG_TYPE:
                    write(io, prefix, "[");
                    print_reg(context, io, p.data.mem.reg);
                    write(io, " + ");
                    if (p.data.mem.off == 0) {
                        print_type(context, io, p.data.mem.type, "");
                    } 
                    else {
                        if (p.data.mem.type.kind != K_STRUCT) {
                            fprintf(stderr, "[ERROR] Expected struct type in field offset.\n");
                            exit(1);
                        }
                        print_type(context, io, p.data.mem.type, "");
                        write(io, ".");
                        write(io, context.type_info[p.data.mem.type.id].members[p.data.mem.off - 1].name);
                    }
                    write(io, "]");
                    break;
                case MK_LABEL_TYPE:
                    write(io, prefix, "[");
                    write(io, " ", name(p.data.mem.label));
                    write(io, " + ");
                    if (p.data.mem.off == 0) {
                        print_type(context, io, p.data.mem.type, "");
                    } 
                    else {
                        if (p.data.mem.type.kind != K_STRUCT) {
                            fprintf(stderr, "[ERROR] Expected struct type in field offset.\n");
                            exit(1);
                        }
                        print_type(context, io, p.data.mem.type, "");
                        write(io, ".");
                        write(io, context.type_info[p.data.mem.type.id].members[p.data.mem.off - 1].name);
                    }
                    write(io, "]");
                    break;
            }
        }
    }

    i64 print_param(const Context& context, stream& io, const Insn& insn, i64 param) {
        print_param(context, insn.params[param], io, " ");
        return param + 1;
    }

    i64 print_another_param(const Context& context, stream& io, const Insn& insn, i64 param) {
        print_param(context, insn.params[param], io, ", ");
        return param + 1;
    }

    i64 print_variadic_param(const Context& context, stream& io, const Insn& insn, i64 param) {
        write(io, "(");
        for (i64 i = param; i < insn.params.size(); i ++) {
            if (i == param) {
                print_type(context, io, *insn.params[i].annotation, "");
                print_param(context, insn.params[i], io, " ");
            }
            else {
                write(io, ", ");
                print_type(context, io, *insn.params[i].annotation, "");
                print_param(context, insn.params[i], io, " ");
            }
        }
        write(io, ")");
        return insn.params.size();
    }

    i64 print_label(const Context& context, stream& io, const Insn& insn, i64 param) {
        const Param& p = insn.params[param];
        if (p.kind != PK_LABEL) {
            fprintf(stderr, "[ERROR] Expected label parameter.\n");
            exit(1);
        }
        write(io, " ", name(p.data.label));
        return param + 1;
    }

    i64 print_typedef(const Context& context, stream& io, const Insn& insn, i64 param) {
        write(io, " ", context.type_info[insn.type.id].name);
        write(io, " ", "{");
        bool first = true;
        for (const auto& m : context.type_info[insn.type.id].members) {
            if (!first) write(io, ", ");
            if (m.type) {
                write(io, m.name, " : ");
                print_type(context, io, *m.type, "");
                if (m.count > 1) write(io, " * ", m.count);
            }
            else {
                write(io, m.name, " : ", m.count);
            }
            first = false;
        }
        write(io, "}");
        return param + 1;
    }

    // General-purpose tables and driver functions.

    static OpComponent C_TYPE = {
        parse_type,
        disassemble_type,
        nullptr,
        assemble_type,
        print_type
    };

    static OpComponent C_SRC = {
        parse_another_param,
        disassemble_param,
        nullptr,
        assemble_param,
        print_another_param
    };

    static OpComponent C_DEST = {
        parse_param,
        disassemble_param, 
        nullptr,
        assemble_param,
        print_param
    };

    static OpComponent C_VARIADIC = {
        parse_variadic_param,
        disassemble_variadic_param,
        nullptr,
        assemble_variadic_param,
        print_variadic_param
    };

    static OpComponent C_LABEL = {
        parse_param,
        disassemble_param,
        nullptr,
        assemble_label,
        print_label
    };

    static OpComponent C_TYPEDEF = {
        parse_typedef,
        disassemble_typedef,
        nullptr,
        assemble_typedef,
        print_typedef
    };

    Op ternary_op(Opcode opcode) {
        return Op(
            opcode,
            C_TYPE, C_DEST, C_SRC, C_SRC 
        );
    }

    Op binary_op(Opcode opcode) {
        return Op(
            opcode,
            C_TYPE, C_DEST, C_SRC
        );
    }

    Op unary_op(Opcode opcode) {
        return Op(
            opcode,
            C_TYPE, C_DEST
        );
    }

    Op nullary_op(Opcode opcode) {
        return Op(opcode);
    }

    Op call_op(Opcode opcode) {
        return Op(
            opcode,
            C_TYPE, C_DEST, C_SRC, C_VARIADIC
        );
    }

    Op label_binary_op(Opcode opcode) {
        return Op(
            opcode,
            C_TYPE, C_LABEL, C_DEST, C_SRC
        );
    }

    Op label_nullary_op(Opcode opcode) {
        return Op(
            opcode,
            C_LABEL
        );
    }

    Op typedef_op(Opcode opcode) {
        return Op(
            opcode,
            C_TYPEDEF
        );
    }

    Op OPS[] = {
        ternary_op(OP_ADD),     
        ternary_op(OP_SUB),     
        ternary_op(OP_MUL),     
        ternary_op(OP_DIV),     
        ternary_op(OP_REM),     
        ternary_op(OP_AND),     
        ternary_op(OP_OR),      
        ternary_op(OP_XOR),     
        binary_op(OP_NOT),      
        binary_op(OP_ICAST),    
        binary_op(OP_F32CAST),  
        binary_op(OP_F64CAST),  
        binary_op(OP_EXT),      
        binary_op(OP_ZXT),       
        ternary_op(OP_SL),       
        ternary_op(OP_SLR),      
        ternary_op(OP_SAR), 
        unary_op(OP_LOCAL),
        unary_op(OP_PARAM),
        unary_op(OP_PUSH),
        unary_op(OP_POP),
        nullary_op(OP_FRAME),
        unary_op(OP_RET),
        call_op(OP_CALL),
        label_binary_op(OP_JEQ),
        label_binary_op(OP_JNE),
        label_binary_op(OP_JL),
        label_binary_op(OP_JLE),
        label_binary_op(OP_JG),
        label_binary_op(OP_JGE),
        label_nullary_op(OP_JO),
        label_nullary_op(OP_JNO),
        label_nullary_op(OP_JUMP),
        nullary_op(OP_NOP),
        ternary_op(OP_CEQ),
        ternary_op(OP_CNE),
        ternary_op(OP_CL),
        ternary_op(OP_CLE),
        ternary_op(OP_CG),
        ternary_op(OP_CGE),
        binary_op(OP_MOV),
        binary_op(OP_XCHG),
        typedef_op(OP_TYPE),
        unary_op(OP_GLOBAL)
    };

    map<string, Opcode> OPCODE_TABLE = map_of<string, Opcode>(
        string("add"), OP_ADD,
        string("sub"), OP_SUB,
        string("mul"), OP_MUL,
        string("div"), OP_DIV,
        string("rem"), OP_REM,
        string("and"), OP_AND,
        string("or"), OP_OR,
        string("xor"), OP_XOR,
        string("not"), OP_NOT,
        string("icast"), OP_ICAST,
        string("f32cast"), OP_F32CAST,
        string("f64cast"), OP_F64CAST,
        string("ext"), OP_EXT,
        string("zxt"), OP_ZXT,
        string("sl"), OP_SL,
        string("slr"), OP_SLR,
        string("sar"), OP_SAR,
        string("local"), OP_LOCAL,
        string("param"), OP_PARAM,
        string("push"), OP_PUSH,
        string("pop"), OP_POP,
        string("frame"), OP_FRAME,
        string("ret"), OP_RET,
        string("call"), OP_CALL,
        string("jeq"), OP_JEQ,
        string("jne"), OP_JNE,
        string("jl"), OP_JL,
        string("jle"), OP_JLE,
        string("jg"), OP_JG,
        string("jge"), OP_JGE,
        string("jo"), OP_JO,
        string("jno"), OP_JNO,
        string("jump"), OP_JUMP,
        string("nop"), OP_NOP,
        string("ceq"), OP_CEQ,
        string("cne"), OP_CNE,
        string("cl"), OP_CL,
        string("cle"), OP_CLE,
        string("cg"), OP_CG,
        string("cge"), OP_CGE,
        string("mov"), OP_MOV,
        string("xchg"), OP_XCHG,
        string("type"), OP_TYPE,
        string("global"), OP_GLOBAL
    );

    string OPCODE_NAMES[] = {
        "add", "sub", "mul", "div", "rem",
        "and", "or", "xor", "not",
        "icast", "f32cast", "f64cast", "ext", "zxt",
        "sl", "slr", "sar",
        "local", "param",
        "push", "pop",
        "frame", "ret", "call",
        "jeq", "jne", "jl", "jle", "jg", "jge", "jo", "jno",
        "jump", "nop",
        "ceq", "cne", "cl", "cle", "cg", "cge",
        "mov", "xchg",
        "type", "global"
    };

    Insn parse_insn(Context& context, stream& io) {
        Insn insn;
        string op = next_string(io);
        if (io.peek() == ':') {
            insn.label = some<Symbol>(local((const char*)op.raw()));
            io.read();
            op = next_string(io);
        }
        auto it = OPCODE_TABLE.find(op);
        if (it == OPCODE_TABLE.end()) {
            fprintf(stderr, "Unknown opcode '%s'.\n", (const char*)op.raw());
            exit(1);
        }
        insn.opcode = it->second;
        for (const auto& comp : OPS[insn.opcode].components) comp->parser(context, io, insn);
        i64 i = 0;
        for (const auto& comp : OPS[insn.opcode].components) if (comp->validator) {
            i64 v = comp->validator(context, insn, i);
            if (v < 0) exit(1);
            else i = v;
        }
        return insn;
    }

    vector<Insn> parse_all_insns(Context& context, stream& io) {
        vector<Insn> insns;
        while (io) {
            while (io && isspace(io.peek())) io.read(); // consume leading spaces
            if (io) insns.push(parse_insn(context, io));
        }
        return insns;
    }

    Insn disassemble_insn(Context& context, bytebuf& buf, const Object& obj) {
        Insn insn;
        u64 offset = obj.code().size() - buf.size();
        auto it = obj.symbol_positions().find(offset);
        if (it != obj.symbol_positions().end())
            insn.label = some<Symbol>(it->second);
        u8 opcode = buf.read<u8>();
        u8 typearg = buf.read<u8>();
        insn.opcode = Opcode(opcode >> 2 & 63);
        insn.type.kind = Kind(typearg & 15);
        if (insn.type.kind == K_STRUCT) insn.type.id = disassemble_60bit(buf).first;

        ParamKind pk[3];
        pk[0] = ParamKind(opcode & 3);
        pk[1] = ParamKind(typearg >> 6 & 3);
        pk[2] = ParamKind(typearg >> 4 & 3);
        i64 i = 0;
        for (const auto& comp : OPS[insn.opcode].components) {
            comp->disassembler(context, buf, obj, insn, pk[i]);
            if (comp != &C_TYPE && comp != &C_TYPEDEF && comp != &C_VARIADIC) i ++;
        }
        i = 0;
        for (const auto& comp : OPS[insn.opcode].components) if (comp->validator) {
            i64 v = comp->validator(context, insn, i);
            if (v < 0) exit(1);
            else i = v;
        }
        return insn;
    }

    vector<Insn> disassemble_all_insns(Context& context, bytebuf& buf, const Object& obj) {
        vector<Insn> insns;
        while (buf.size()) insns.push(disassemble_insn(context, buf, obj));
        return insns;
    }

    void assemble_insn(Context& context, Object& obj, const Insn& insn) {
        // [opcode 6         ] [p1 2] [p2 2] [p3 2] [typekind 4]
        if (insn.label) obj.define(*insn.label);
        u8 opcode = (insn.opcode & 63) << 2;
        if (insn.params.size() > 0) opcode |= insn.params[0].kind;
        u8 typearg = insn.type.kind;
        if (insn.params.size() > 1) typearg |= insn.params[1].kind << 6;
        if (insn.params.size() > 2) typearg |= insn.params[2].kind << 4;
        obj.code().write<u8>(opcode);
        obj.code().write<u8>(typearg);
        if (insn.type.kind == K_STRUCT) assemble_60bit(obj.code(), insn.type.id, false);
        
        i64 i = 0;
        for (const auto& comp : OPS[insn.opcode].components) 
            i = comp->assembler(context, obj, insn, i);
    }

    void print_insn(Context& context, stream& io, const Insn& insn) {
        i64 i = 0;
        if (insn.label) write(io, name(*insn.label), ':');
        write(io, "\t", OPCODE_NAMES[insn.opcode]);
        for (const auto& comp : OPS[insn.opcode].components) 
            i = comp->printer(context, io, insn, i);
        write(io, "\n");
    }

    LiveRange::LiveRange(Reg reg_in, Type type_in, u64 f, u64 l):
        reg(reg_in), type(type_in), first(f), last(l) {}

    struct Function {
        u64 first, last;
        u64 stack;
        vector<LiveRange> ranges;
        vector<vector<LiveRange*>> starts_by_insn, ends_by_insn, preserved_regs;

        void format(const vector<Insn>& insns, stream& io) const {
            for (u64 i = first; i <= last; i ++) {
                insns[i].format(io);
            }
        }
    };

    vector<Function> find_functions(const vector<Insn>& insns) {
        vector<Function> functions;
        optional<Function> fn = none<Function>();
        for (u32 i = 0; i < insns.size(); i ++) {
            if (insns[i].opcode == OP_FRAME) { // frame
                if (fn) {
                    if (fn->first == fn->last) {
                        fprintf(stderr, "[ERROR] Found second 'frame' instruction before 'ret'.\n");
                        exit(1);
                    }
                    else {
                        functions.push(*fn);
                        fn = none<Function>();
                        // fallthrough to none case
                    }
                }
                if (!fn) {
                    fn = some<Function>();
                    fn->first = i, fn->last = i;
                }
            }
            else if (insns[i].opcode == OP_RET && fn) { // ret
                fn->last = i;
            }
        }
        if (fn && fn->first != fn->last) functions.push(*fn);

        return functions;
    }

    bool unify(bitset& out, bitset& in) {
        bool changed = false;
        for (u32 r : in) changed = out.insert(r) || changed;
        return changed;
    }

    bool destructive(const Insn& in) {
        return (in.params.size() > 0 && in.opcode != OP_PUSH && in.opcode != OP_NOT && in.opcode != OP_RET)
            && (in.params[0].kind == PK_REG || in.params[0].kind == PK_MEM);
    }

    bool liveout(const Insn& in, bitset& live, const bitset& out) {
        bitset old = live;
        bool changed = false;
        live = out;
        if (destructive(in)) {
            live.erase(in.params[0].data.reg.id);
            for (u32 i = 1; i < in.params.size(); i ++) if (in.params[i].kind == PK_REG)
                live.insert(in.params[i].data.reg.id);
        }
        else {
            for (u32 i = 0; i < in.params.size(); i ++) if (in.params[i].kind == PK_REG)
                live.insert(in.params[i].data.reg.id);
        }
        for (u32 i : live) if (!old.contains(i)) changed = true;
        for (u32 i : old) if (!live.contains(i)) changed = true;
        return changed;
    }

    void compute_ranges(Function& function, const vector<Insn>& insns) {
        vector<pair<bitset, bitset>> sets;
        map<Symbol, u64> local_syms;
        for (u64 i = function.first; i <= function.last; i ++) {
            sets.push({});
            function.preserved_regs.push({});
            if (insns[i].label) local_syms.put(*insns[i].label, i);
        }

        sets.push({});
        bool changed = true;
        while (changed) {
            changed = false;
            for (i64 i = i64(function.last); i >= i64(function.first); i --) {
                const Insn& in = insns[i];
                if (in.opcode == OP_JUMP) { // unconditional jump
                    auto it = local_syms.find(in.params[0].data.label);
                    if (it == local_syms.end()) {
                        fprintf(stderr, "[ERROR] Tried to jump to label '%s' outside of current function.\n", 
                            name(in.params[0].data.label));
                        exit(1);
                    }

                    changed = unify(sets[i - function.first].second, sets[it->second - function.first].first) || changed;
                }
                else if (in.opcode >= OP_JEQ && in.opcode < OP_JUMP) { // conditional jump
                    auto it = local_syms.find(in.params[0].data.label);
                    if (it == local_syms.end()) {
                        fprintf(stderr, "[ERROR] Tried to jump to label '%s' outside of current function.\n", 
                            name(in.params[0].data.label));
                        exit(1);
                    }
                    
                    changed = unify(sets[i - function.first].second, sets[it->second - function.first].first) || changed;
                    changed = unify(sets[i - function.first].second, sets[i + 1 - function.first].first) || changed;
                }
                else {
                    changed = unify(sets[i - function.first].second, sets[i + 1 - function.first].first) || changed;
                }
                
                changed = liveout(insns[i], sets[i - function.first].first, sets[i - function.first].second) || changed;
            }
        }

        // println("");
        // for (u64 i = function.first; i <= function.last; i ++) {
        //     print("{");
        //     bool first = true;
        //     for (u32 r : sets[i - function.first].first) {
        //         Param p;
        //         p.kind = PK_REG;
        //         p.data.reg = {false, r};
        //         print(first ? "" : " ");
        //         print_param({}, p, _stdout, "");
        //         first = false;
        //     }
        //     print("} => {");
        //     first = true;
        //     for (u32 r : sets[i - function.first].second) {
        //         Param p;
        //         p.kind = PK_REG;
        //         p.data.reg = {false, r};
        //         print(first ? "" : " ");
        //         print_param({}, p, _stdout, "");
        //         first = false;
        //     }
        //     print("}\t");
        //     Context ctx;
        //     print_insn(ctx, _stdout, insns[i]);
        // }
        // println("");

        map<u64, optional<LiveRange>> ranges;
        for (u64 i = function.first; i <= function.last; i ++) {
            const Insn& in = insns[i];
            for (const Param& p : in.params) if (p.kind == PK_REG)
                ranges[p.data.reg.id] = none<LiveRange>();
        }
        u32 param_idx = 0;
        for (u64 i = function.first; i <= function.last; i ++) {
            const auto& live = sets[i - function.first];
            for (auto& [reg, range] : ranges) {
                if (range && (insns[i].type.kind != range->type.kind
                    || (insns[i].type.kind == K_STRUCT && insns[i].type.id != range->type.id))) {
                    fprintf(stderr, "[ERROR] Found inconsistent types for virtual register %%%lu.\n", reg);
                    exit(1);
                }

                if (!range && live.second.contains(reg)) { // new definition
                    range = some<LiveRange>(Reg{false, reg}, insns[i].type, i, i);
                }
                else if (range && !live.first.contains(reg)) { // range ended last instruction
                    range->last = i - 1;
                    function.ranges.push(*range);
                    range = none<LiveRange>();
                }
                
                if (range && insns[i].opcode == OP_PARAM) range->param_idx = some<u32>(param_idx ++); // arg
                
                if (range && !live.second.contains(reg)) { // range ends this instruction
                    range->last = i;
                    function.ranges.push(*range);
                    range = none<LiveRange>();
                }
            }
        }
        for (auto& [reg, range] : ranges) if (range) range->last = function.last, function.ranges.push(*range); // add any ranges we didn't catch

        for (LiveRange& r : function.ranges) {
            for (u64 i = r.first + 1; i < r.last; i ++) if (insns[i].opcode == OP_CALL)
                function.preserved_regs[i - function.first].push(&r);
        }

        for (const LiveRange& r : function.ranges) {
            Param p;
            p.kind = PK_REG;
            p.data.reg = r.reg;
            // print("register ");
            // print_param({}, p, _stdout, "");
            // println(" live from ", r.first, " to ", r.last);
        }
        // println("");
    }

    void clobber(Function& f, const Insn& insn, u32 insn_idx, const_slice<bitset*> regs, 
        vector<LiveRange*>& mappings, const Target& target) {
        bitset clobbers = target.clobbers(insn);

        vector<LiveRange*> fixup;
        vector<pair<bitset*, u32>> to_release;
        for (u32 i : clobbers) {
            for (bitset* b : regs) if (b->contains(i)) {
                b->erase(i);
                to_release.push({ b, i });
            }
            if (mappings[i] && mappings[i]->first != insn_idx) { // don't worry about dests
                fixup.push(mappings[i]); // reallocate conflicting variables
                to_release.push({ regs[mappings[i]->type.kind], i });
            }
        }
        for (LiveRange* r : fixup) {
            bitset& kregs = *regs[r->type.kind];
            bool remapped = false;
            for (u32 i : kregs) {
                if (r->illegal.contains(i)) continue; // can't reallocate to previously-clobbered reg

                r->loc = loc_reg(i);
                kregs.erase(i);
                mappings[*r->loc.reg] = r;
                // println("\treallocated %", r->reg.id, " to ", x64::REGISTER_NAMES[*r->loc.reg]);
                remapped = true;
                break; // we've remapped to a valid register, so we're done
            }
            if (!remapped) r->loc = loc_stack(-i64(f.stack += 8ul)); // spill
        }
        // release clobbered registers for use after this instruction
        for (const auto& [kregs, r] : to_release) {
            if (mappings[r] && mappings[r]->first != r) mappings[r] = nullptr;
            kregs->insert(r); 
            // println("\treleased ", x64::REGISTER_NAMES[r]);
        }
    }

    void allocate_registers(Function& f, const vector<Insn>& insns, const Target& target) {
        bitset* regs[NUM_KINDS];
        map<u64, bitset> dedup_map;
        // we want to associate bitsets with each kind, but multiple kinds can map to the
        // same registers on most architectures. we use dedup_map to store the unique bitsets
        // returned by the target (using addresses as keys), and store pointers to them in
        // regs.
        for (u32 k = 0; k < NUM_KINDS; k ++) {
            const bitset& set = target.register_set((Kind)k);
            auto it = dedup_map.find(u64(&set));
            if (it != dedup_map.end()) regs[k] = &it->second;
            else regs[k] = &(dedup_map[u64(&set)] = bitset(set));
        }

        u32 cur_arg = 0;

        // we want a mapping for every available register id
        vector<LiveRange*> mappings;
        for (u32 k = 0; k < NUM_KINDS; k ++) for (u32 r : *regs[k])
            while (mappings.size() < r) mappings.push(nullptr);

        for (u64 i = f.first; i <= f.last; i ++) {
            f.starts_by_insn.push({});
            f.ends_by_insn.push({});
        }
        for (LiveRange& r : f.ranges) {
            f.starts_by_insn[r.first - f.first].push(&r);
            f.ends_by_insn[r.last - f.first].push(&r);
        }

        for (u64 i = f.first; i <= f.last; i ++) {
            static vector<LiveRange*> ranges;
            ranges.clear();
            for (const Param& p : insns[i].params) 
                if (p.kind == PK_REG && mappings[p.data.reg.id]) ranges.push(mappings[p.data.reg.id]);
                else ranges.push(nullptr);
            for (LiveRange* r : f.starts_by_insn[i - f.first]) 
                if (insns[i].params[0].kind == PK_REG && r->reg.id == insns[i].params[0].data.reg.id)
                    ranges[0] = r; // pull dest from freshly-started range
            target.hint(insns[i], ranges);
        }

        for (u64 i = f.first; i <= f.last; i ++) {
            clobber(f, insns[i], i, { NUM_KINDS, regs }, mappings, target);

            for (LiveRange* r : f.ends_by_insn[i - f.first]) {
                if (r->loc.type == LT_REGISTER) {
                    regs[r->type.kind]->insert(*r->loc.reg);
                    mappings[*r->loc.reg] = nullptr;
                }
            }

            for (LiveRange* r : f.starts_by_insn[i - f.first]) {
                bitset& kregs = *regs[r->type.kind];
                if (kregs.begin() != kregs.end()) {
                    auto reg = r->hint ? *r->hint : *kregs.begin();
                    // println("\tallocated %", r->reg.id, " to ", x64::REGISTER_NAMES[reg]);
                    r->loc = loc_reg(reg);
                    kregs.erase(reg);
                    mappings[*r->loc.reg] = r;
                }
                else r->loc = loc_stack(-i64(f.stack += 8ul)); // spill
            }
        }

        // for (const LiveRange& r : f.ranges) {
        //     Param p;
        //     p.kind = PK_REG;
        //     p.data.reg = r.reg;
        //     print("register ");
        //     print_param({}, p, _stdout, "");
        //     print(" from ", r.first, " to ", r.last);
        //     if (r.loc.type == LT_REGISTER) println(" allocated to register %", x64::REGISTER_NAMES[*r.loc.reg]);
        //     else println(" spilled to [%rbp - ", -*r.loc.offset, "]");
        // }
        // println("");
    }

    // X86_64 Codegen

    optional<x64::Arg> to_x64_arg(const map<u64, LiveRange*>& reg_bindings, const Param& p) {
        switch (p.kind) {
            case PK_IMM:
                return some<x64::Arg>(x64::imm(p.data.imm.val));
            case PK_REG:
                if (reg_bindings.find(p.data.reg.id) == reg_bindings.end()) return none<x64::Arg>();
                if (reg_bindings[p.data.reg.id]->loc.type == LT_REGISTER) // bound to register
                    return some<x64::Arg>(x64::r64((x64::Register)*reg_bindings[p.data.reg.id]->loc.reg));
                else 
                    return some<x64::Arg>(x64::m64(x64::RBP, -i64(*reg_bindings[p.data.reg.id]->loc.offset)));
            case PK_MEM: {
                const auto& m = p.data.mem;
                switch (m.kind) {
                    case MK_REG_OFF:
                        if (reg_bindings[m.reg.id]->loc.type == LT_REGISTER) {
                            return some<x64::Arg>(m64((x64::Register)*reg_bindings[m.reg.id]->loc.reg, m.off));
                        }
                        else {
                            panic("can't efficiently handle memory parameter");
                        }
                    case MK_REG_TYPE:

                    case MK_LABEL_OFF:
                    case MK_LABEL_TYPE:
                        break;
                }
                return some<x64::Arg>(x64::imm(0));
            }
            case PK_LABEL:
                return some<x64::Arg>(x64::label32(p.data.label));
        }
    }

    void move_x64(const x64::Arg& dest, const x64::Arg& src) {
        using namespace x64;
        if (is_register(dest.type) && dest == src)
            return; // no no-op moves
        if (is_register(dest.type) && is_immediate(src.type) && immediate_value(src) == 0)
            return xor_(dest, dest); // faster clear register
        if (is_register(dest.type) || is_register(src.type) || is_immediate(src.type))
            return mov(dest, src); // simple move
        else {
            push(src); // push/pop memory move
            pop(dest);
        }
    }

    i64 log2(i64 n) {
        i64 acc = 0;
        while (n > 1) {
            n >>= 1;
            acc ++;
        }
        return acc;
    }

    // ensures, for ternary jasmine instructions, that any immediate src is in args[2]
    void commute_ternary(vector<x64::Arg>& args) {
        using namespace x64;
        if (is_immediate(args[1].type)) {
            auto tmp = args[2];
            args[2] = args[1];
            args[1] = tmp;
        }
    }
    
    void generate_x64_insn(Function& f, const Insn& insn, u32 insn_idx, vector<x64::Arg>& args, Object& obj) {
        if (insn.label) {
            if (obj.code().size() % 8 != 0) {
                u32 ideal = ((obj.code().size() + 7) / 8) * 8;
                x64::nop(ideal - obj.code().size()); // align branch targets to 8 bytes
            }
            obj.define(*insn.label);
        }
        using namespace x64;
        switch (insn.opcode) {
            case OP_ADD:
                commute_ternary(args);
                if (is_register(args[1].type) && !is_memory(args[2].type)) {
                    if (is_immediate(args[2].type)) {
                        i64 val = immediate_value(args[2]);
                        if (val == 0) return move_x64(args[0], args[1]);
                        else if (val == 1) {
                            move_x64(args[0], args[1]);
                            return inc(args[0]);
                        }
                        else if (val == -1) {
                            move_x64(args[0], args[1]);
                            return dec(args[0]);
                        }
                        else if (is_register(args[0].type)) {
                            return lea(args[0], m64(args[1].data.reg, val));
                        }
                    }
                    else if (is_register(args[0].type)) {
                        return lea(args[0], m64(args[1].data.reg, args[2].data.reg, SCALE1, 0));
                    }
                }
                move_x64(args[0], args[1]);
                add(args[0], args[2]);
                return;
            case OP_SUB:
                if (!is_memory(args[1].type) && !is_memory(args[2].type)) {
                    if (is_immediate(args[1].type)) {
                        i64 val = immediate_value(args[2]);
                        if (val == 0) {
                            move_x64(args[0], args[1]);
                            return neg(args[0]);
                        }
                    }
                    else if (is_immediate(args[2].type)) {
                        i64 val = immediate_value(args[2]);
                        if (val == 0) return move_x64(args[0], args[1]);
                        else if (val == 1) {
                            move_x64(args[0], args[1]);
                            return dec(args[0]);
                        }
                        else if (val == -1) {
                            move_x64(args[0], args[1]);
                            return inc(args[0]);
                        }
                        else if (is_register(args[0].type)) {
                            return lea(args[0], m64(args[1].data.reg, -val));
                        }
                    }
                    else if (is_register(args[0].type)) {
                        return lea(args[0], m64(args[1].data.reg, args[2].data.reg, SCALE1, 0));
                    }
                }
                move_x64(args[0], args[1]);
                sub(args[0], args[2]);
                return;
            case OP_MUL: {
                commute_ternary(args);
                if (is_immediate(args[2].type)) {
                    i64 val = immediate_value(args[2]);
                    if (val == 0) return move_x64(args[0], imm(0));
                    else if (val == 1) return move_x64(args[0], args[1]);
                    else if (val == -1) {
                        move_x64(args[0], args[1]);
                        return neg(args[0]);
                    }
                    else if (!(val & (val - 1))) {
                        move_x64(args[0], args[1]);
                        return shl(args[0], imm(log2(val))); // shl for po2 multiplies
                    }
                }
                auto dest = args[0];
                if (!is_register(dest.type)) dest = r64(RAX);
                if (is_immediate(args[2].type)) { // imul does not permit immediate src
                    imul(dest, args[1], args[2]);
                }
                else {
                    move_x64(dest, args[1]);
                    imul(dest, args[2]);
                }
                if (dest != args[0]) move_x64(args[0], dest);
                return;
            }
            case OP_DIV: {
                if (is_immediate(args[1].type)) {
                    i64 val = immediate_value(args[1]);
                    if (val == 0) return move_x64(args[0], imm(0));
                }
                if (is_immediate(args[2].type)) {
                    i64 val = immediate_value(args[2]);
                    if (val == 1) return move_x64(args[0], args[1]);
                    else if (val == -1) {
                        move_x64(args[0], args[1]);
                        return neg(args[0]);
                    }
                    else if (!(val & (val - 1))) {
                        move_x64(args[0], args[1]);
                        return shr(args[0], imm(log2(val))); // shr for po2 divisions
                    }
                }
                auto src = args[2];
                if (is_immediate(args[2].type)) { // idiv does not permit immediate divisor
                    move_x64(r64(RCX), args[2]);
                    src = r64(RCX);
                }
                move_x64(r64(RAX), args[1]);
                cdq();
                idiv(src);
                move_x64(args[0], r64(RAX));
                return;
            }
            case OP_REM: {
                if (is_immediate(args[1].type)) {
                    i64 val = immediate_value(args[1]);
                    if (val == 0) return move_x64(args[0], imm(0));
                }
                if (is_immediate(args[2].type)) {
                    i64 val = immediate_value(args[2]);
                    if (val == 1) return move_x64(args[0], imm(0));
                    else if (val == -1) return move_x64(args[0], imm(0));
                    else if (!(val & (val - 1))) {
                        move_x64(args[0], args[1]);
                        mov(r64(RAX), imm(val - 1));
                        and_(args[0], r64(RAX));
                    }
                }
                auto src = args[2];
                if (is_immediate(args[2].type)) { // idiv does not permit immediate divisor
                    move_x64(r64(RCX), args[2]);
                    src = r64(RCX);
                }
                move_x64(r64(RAX), args[1]);
                cdq();
                idiv(src);
                move_x64(args[0], r64(RDX));
                return;
            }
            case OP_LOCAL:
                return;
            case OP_PARAM:
                return;
            case OP_PUSH:
                push(args[0]);
                return;
            case OP_POP:
                pop(args[0]);
                return;
            case OP_FRAME:
                if (f.stack) {
                    push(r64(RBP));
                    mov(r64(RBP), r64(RSP));
                    sub(r64(RSP), imm(f.stack));
                }
                return;
            case OP_RET:
                move_x64(r64((Register)*obj.get_target().locate_return_value(insn.type.kind).reg), args[0]);
                if (f.stack) {
                    mov(r64(RSP), r64(RBP));
                    pop(r64(RBP));
                }
                ret();
                return;
            case OP_CALL: {
                for (LiveRange* r : f.preserved_regs[insn_idx - f.first]) if (r->loc.type == LT_REGISTER)
                    push(r64((Register)*r->loc.reg));

                auto fn = insn.params[1].kind == PK_LABEL ? args[1] : r64(RAX);
                if (insn.params[1].kind != PK_LABEL) move_x64(r64(RAX), args[1]);

                vector<Kind> param_kinds;
                for (u32 i = 2; i < insn.params.size(); i ++) param_kinds.push(insn.params[i].annotation->kind);
                auto params = obj.get_target().place_parameters(param_kinds); // compute parameter locations

                for (u32 i = 2; i < insn.params.size(); i ++) {
                    if (params[i - 2].type == LT_REGISTER) 
                        move_x64(r64((Register)*params[i - 2].reg), args[i]);
                    else if (params[i - 2].type == LT_STACK_MEMORY && params[i - 2].offset)
                        move_x64(m64(RBP, *params[i - 2].offset), args[i]);
                    else if (params[i - 2].type == LT_PUSHED_L2R)
                        push(args[i]);
                }
                for (i64 i = i64(insn.params.size()) - 1; i >= 2; i --) {
                    if (params[i - 2].type == LT_PUSHED_R2L) 
                        push(args[i]);
                }
                call(fn);

                Location ret = obj.get_target().locate_return_value(insn.type.kind);
                if (ret.type == LT_REGISTER) move_x64(args[0], r64((Register)*ret.reg));

                for (i64 i = i64(f.preserved_regs[insn_idx - f.first].size()) - 1; i >= 0; i --) {
                    const auto& r = f.preserved_regs[insn_idx - f.first][i];
                    if (r->loc.type == LT_REGISTER) pop(r64((Register)*r->loc.reg));
                }

                return;
            }
            case OP_JEQ:
            case OP_JNE:
            case OP_JL:
            case OP_JLE:
            case OP_JG:
            case OP_JGE:
            case OP_JO:
            case OP_JNO:
                static Condition conds_x64[8] = {
                    EQUAL, NOT_EQUAL,
                    LESS, LESS_OR_EQUAL,
                    GREATER, GREATER_OR_EQUAL,
                    OVERFLOW, NOT_OVERFLOW
                };
                cmp(args[1], args[2]);
                jcc(args[0], conds_x64[insn.opcode - OP_JEQ]);
                return;
            case OP_NOP:
                return;
            case OP_CEQ:
            case OP_CNE:
            case OP_CL:
            case OP_CLE:
            case OP_CG:
            case OP_CGE:
                cmp(args[1], args[2]);
                setcc(args[0], conds_x64[insn.opcode - OP_CEQ]);
                return;
            case OP_JUMP:
                jmp(args[0]);
                return;
            case OP_MOV: 
                return move_x64(args[0], args[1]);
            // case OP_XCHG: 
            //     return move_x64(args[0], args[1]);
            default:
                return;
        }
    }

    void generate_x64(Function& f, vector<Insn>& insns, Object& obj) {
        using namespace x64;
        writeto(obj);        

        static vector<x64::Arg> args;
        map<u64, LiveRange*> reg_bindings;
        for (u32 i = f.first; i <= f.last; i ++) {
            const Insn& insn = insns[i];
            for (LiveRange* r : f.starts_by_insn[i - f.first]) {
                reg_bindings[r->reg.id] = r;
            }
            // look ahead for destination registers
            if (insn.params.size() && insn.params[0].kind == PK_REG && i < f.last) 
                for (LiveRange* r : f.starts_by_insn[(i + 1) - f.first])
                    if (r->reg.id == insn.params[0].data.reg.id) 
                        reg_bindings[r->reg.id] = r;

            args.clear();
            bool useless = false; // useless instructions can happen when 
                                  // the destination of this instruction is unused
            for (const Param& p : insn.params) {
                auto arg = to_x64_arg(reg_bindings, p);
                if (!arg) useless = true;
                else args.push(*arg);
            }
            if (useless) continue;
            
            generate_x64_insn(f, insn, i, args, obj);
        }
    }
    
    Object compile_jasmine(const vector<Insn>& insns_in, const Target& target) {
        vector<Insn> insns = insns_in;

        // platform-independent analysis
        vector<Function> functions = find_functions(insns);
        for (Function& f : functions) compute_ranges(f, insns);

        // associate virtual registers with native ones
        for (Function& f : functions) allocate_registers(f, insns, target);

        Object obj(target); // create our destination object

        // generate native instructions
        switch (target.arch) {
            case X86_64: 
                for (Function& f : functions) generate_x64(f, insns, obj);
                break;
            default:
                panic("Unimplemented architecture!");
                break;
        }

        return obj;
    }
}