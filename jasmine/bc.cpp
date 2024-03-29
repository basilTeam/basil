/*
 * Copyright (c) 2021, the Basil authors
 * All rights reserved.
 *
 * This source code is licensed under the 3-Clause BSD License, the full text
 * of which can be found in the LICENSE file in the root directory
 * of this project.
 */

#include "bc.h"
#include "stdlib.h"
#include "stdio.h"
#include "util/str.h"
#include "util/hash.h"
#include "util/perf.h"
#include "util/sets.h"
#include "x64.h"
#include "jobj.h"

namespace jasmine {
    struct Insn;
    struct Op;

    const Type I8 = { K_I8, 0 }, I16 = { K_I16, 1 }, I32 = { K_I32, 2 }, I64 = { K_I64, 3 }, 
        U8 = { K_U8, 4 }, U16 = { K_U16, 5 }, U32 = { K_U32, 6 }, U64 = { K_U64, 7 }, 
        F32 = { K_F32, 8 }, F64 = { K_F64, 9 }, PTR = { K_PTR, 10 };

    static u64 TYPE_ID = 0;

    
    u64 Type::size(const Target& target, const Context& ctx) const {
        switch (kind) {
            case K_U8:
            case K_I8:
                return 1;
            case K_U16:
            case K_I16:
                return 2;
            case K_U32:
            case K_I32:
            case K_F32:
                return 4;
            case K_U64:
            case K_I64:
            case K_F64:
                return 8;
            case K_PTR:
                return target.pointer_size();
            case K_STRUCT: {
                u64 sz = 0;
                for (const auto& member : ctx.type_info[id].members) {
                    if (member.type) sz += member.type->size(target, ctx) * member.count;
                    else sz += member.count;
                }
                return sz;
            }
            default:
                panic("Unimplemented type kind!");
                return 0;
        }
    }

    u64 Type::offset(const Target& target, const Context& ctx, i64 field) const {
        u64 acc = 0;
        for (i64 i = 0; i < field - 1; i ++) {
            const auto& member = ctx.type_info[id].members[i];
            if (member.type) acc += member.type->size(target, ctx) * member.count;
            else acc += member.count;
        }
        return acc;
    }

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
        while (io && isspace(io.peek()) && io.peek()) io.read();
        if (io.peek() == ';') { // comment
            while (io && io.peek() && io.read() != '\n');
            consume_leading_space(io); // continue onto next line
        }
    }

    void expect(char ch, stream& io) {
        consume_leading_space(io);
        if (io.peek() != ch) {
            fprintf(stderr, "[ERROR] Expected '%c', got '%c'.\n", ch, io.peek());
            exit(1);
        }
        else io.read();
    }

    string next_string(stream& io) {
        while (io && is_separator(io.peek())) io.read();
        string s;
        while (io && !is_separator(io.peek())) s += io.read();
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
                        for (u32 i = 0; i < info.members.size(); i ++) 
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
        while (io && io.peek() != ')') {
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
        while (io && io.peek() != '}') {
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
        data[n] = i % 16 | (extra_bit ? 1 : 0) << 4;
        i /= 16;
        while (i > 0) {
            data[++ n] = i % 256;
            i /= 256;
        }
        if (n >= 8) {
            fprintf(stderr, "[ERROR] Constant integer or id %ld is too big to be encoded within 60 bits!\n", o);
            exit(1);
        }
        data[0] |= n << 5;
        for (u8 j = 0; j <= n; j ++) io.write(data[j]);
    }

    // Disassemblers

    pair<i64, bool> disassemble_60bit(bytebuf& buf) {
        u8 head = buf.read<u8>();
        u8 n = head >> 5 & 7;
        bool bit = head >> 4 & 1;

        i64 acc = head & 15, pow = 16;
        while (n > 0) {
            acc += i64(buf.read<u8>()) * pow;
            pow *= 256;
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
        auto it = obj.references().find({ OS_CODE, obj.code().size() - buf.size() });
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
                        obj.reference(param.data.mem.label, OS_CODE, REL32_LE, -4);
                        assemble_60bit(obj.code(), abs(param.data.mem.off), param.data.mem.off < 0);
                        break;
                    case MK_REG_TYPE:
                        assemble_60bit(obj.code(), param.data.mem.reg.id, param.data.mem.reg.global);
                        assemble_type(context, param.data.mem.type, obj);
                        assemble_60bit(obj.code(), abs(param.data.mem.off), param.data.mem.off < 0);
                        break;
                    case MK_LABEL_TYPE:
                        for (u8 i = 0; i < 4; i ++) obj.code().write<u8>(0);
                        obj.reference(param.data.mem.label, OS_CODE, REL32_LE, -4);
                        assemble_60bit(obj.code(), param.data.mem.off, false);
                        assemble_type(context, param.data.mem.type, obj);
                        assemble_60bit(obj.code(), abs(param.data.mem.off), param.data.mem.off < 0);
                        break;
                }
                break;
            case PK_LABEL:
                for (u8 i = 0; i < 4; i ++) obj.code().write<u8>(0);
                obj.reference(param.data.label, OS_CODE, REL32_LE, -4);
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
        unary_op(OP_GLOBAL), 
        ternary_op(OP_ROL),      
        ternary_op(OP_ROR), 
        call_op(OP_SYSCALL), 
        unary_op(OP_LIT),
        unary_op(OP_STAT)
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
        string("global"), OP_GLOBAL,
        string("rol"), OP_ROL,
        string("ror"), OP_ROR,
        string("syscall"), OP_SYSCALL,
        string("lit"), OP_LIT,
        string("stat"), OP_STAT
    );

    string OPCODE_NAMES[] = {
        "add", "sub", "mul", "div", "rem",
        "and", "or", "xor", "not",
        "icast", "f32cast", "f64cast", "ext", "zxt",
        "sl", "slr", "sar",
        "local", "param",
        "push", "pop",
        "frame", "ret", "call",
        "jeq", "jne", "jl", "jle", "jg", "jge",
        "jump", "nop",
        "ceq", "cne", "cl", "cle", "cg", "cge",
        "mov", "xchg",
        "type", "global",
        "rol", "ror",
        "syscall", 
        "lit", "stat"
    };

    Insn parse_insn(Context& context, stream& io) {
        Insn insn;
        string op = next_string(io);
        string label = "";
        // println("op = ", op);
        if (io.peek() == ':') {
            label = op;
            io.read();
            op = next_string(io);
            // println("op2 = ", op);
        }
        // println("op3 = ", op);
        auto it = OPCODE_TABLE.find(op);
        if (it == OPCODE_TABLE.end()) {
            // println("op4 = ", op);
            fprintf(stderr, "Unknown opcode '%s'.\n", (const char*)op.raw());
            exit(1);
        }
        insn.opcode = it->second;
        if (label.size()) {
            insn.label = some<Symbol>(insn.opcode == OP_FRAME ? 
                global((const char*)label.raw()) : local((const char*)label.raw()));
        }
        insn.type = Type{ K_I64, 0 };
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
        u32 i = 0;
        while (io) {
            // if (i ++ % 10000 == 0) println("read ", i, " insns");
            consume_leading_space(io);
            if (io) insns.push(parse_insn(context, io));
        }
        return insns;
    }

    Insn disassemble_insn(Context& context, bytebuf& buf, const Object& obj) {
        Insn insn;
        u64 offset = obj.code().size() - buf.size();
        auto it = obj.symbol_positions().find({ OS_CODE, offset });
        if (it != obj.symbol_positions().end())
            insn.label = some<Symbol>(it->second);
        u8 opcode = buf.read<u8>();
        u8 typearg = buf.read<u8>();
        insn.opcode = Opcode(opcode >> 2 & 63);
        insn.type.kind = Kind(typearg & 15);
        if (insn.type.kind == K_STRUCT) insn.type.id = disassemble_60bit(buf).first;
        else insn.type.id = 0;

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

    vector<Insn> disassemble_all_insns(Context& context, const Object& obj) {
        vector<Insn> insns;
        bytebuf buf(obj.code());
        while (buf.size()) insns.push(disassemble_insn(context, buf, obj));
        return insns;
    }

    void assemble_insn(const Context& context, Object& obj, const Insn& insn) {
        // [opcode 6         ] [p1 2] [p2 2] [p3 2] [typekind 4]
        if (insn.label) obj.define(*insn.label, OS_CODE);
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

    void print_insn(const Context& context, stream& io, const Insn& insn) {
        i64 i = 0;
        if (insn.label) write(io, name(*insn.label), ':');
        write(io, "\t", OPCODE_NAMES[insn.opcode]);
        for (const auto& comp : OPS[insn.opcode].components) 
            i = comp->printer(context, io, insn, i);
        write(io, "\n");
    }

    namespace bc {
        static Object* obj = nullptr;
        static Context* ctx = nullptr;

        void verify_buffer() {
            if (!obj || !ctx) { // requires that a buffer exist to write to
                fprintf(stderr, "[ERROR] Cannot assemble; no target buffer set.\n");
                exit(1);
            }
        }

        Param r(u64 id) {
            Param p;
            p.kind = PK_REG;
            p.data.reg.global = false;
            p.data.reg.id = id;
            return p;
        }

        Param gr(Symbol symbol) {
            return gr(name(symbol));
        }

        Param gr(const char* name) {
            verify_buffer();
            Param p;
            p.kind = PK_REG;
            p.data.reg.global = true;
            auto decl = ctx->global_decls.find(name);
            if (decl == ctx->global_decls.end()) {
                fprintf(stderr, "[ERROR] Undefined global register '%s'.\n", name);
                exit(1);
            }
            p.data.reg = decl->second;
            return p;
        }

        Param imm(i64 i) {
            Param p;
            p.kind = PK_IMM;
            p.data.imm = Imm{i};
            return p;
        }

        Param immfp(double d) {
            Param p;
            p.kind = PK_IMM;
            p.data.imm = Imm{*(i64*)&d};
            return p;
        }

        Param m(u64 reg) {
            Param p;
            p.kind = PK_MEM;
            p.data.mem.kind = MK_REG_OFF;
            p.data.mem.reg = { false, reg };
            p.data.mem.off = 0;
            return p;
        }

        Param m(u64 reg, i64 off) {
            Param p;
            p.kind = PK_MEM;
            p.data.mem.kind = MK_REG_OFF;
            p.data.mem.reg = { false, reg };
            p.data.mem.off = off;
            return p;
        }
        
        Param m(Symbol label) {
            Param p;
            p.kind = PK_MEM;
            p.data.mem.kind = MK_LABEL_OFF;
            p.data.mem.label = label;
            p.data.mem.off = 0;
            return p;
        }
        
        Param m(Symbol label, i64 off) {
            Param p;
            p.kind = PK_MEM;
            p.data.mem.kind = MK_LABEL_OFF;
            p.data.mem.label = label;
            p.data.mem.off = off;
            return p;
        }

        Param m(u64 reg, Type type) {
            Param p;
            p.kind = PK_MEM;
            p.data.mem.kind = MK_REG_TYPE;
            p.data.mem.reg = { false, reg };
            p.data.mem.type = type;
            return p;
        }

        Param m(u64 reg, Type type, Symbol field) {
            verify_buffer();
            if (type.kind != K_STRUCT) {
                fprintf(stderr, "[ERROR] Tried to get field of non-struct type.\n");
                exit(1);
            }

            Param p;
            p.kind = PK_MEM;
            p.data.mem.kind = MK_REG_TYPE;
            p.data.mem.reg = { false, reg };
            p.data.mem.type = type;
            const auto& info = ctx->type_info[type.id];
            for (u32 i = 0; i < info.members.size(); i ++) {
                if (info.members[i].name == name(field)) p.data.mem.off = i;
            }

            return p;
        }

        Param m(Symbol label, Type type) {
            Param p;
            p.kind = PK_MEM;
            p.data.mem.kind = MK_LABEL_TYPE;
            p.data.mem.label = label;
            p.data.mem.type = type;
            return p;
        }

        Param m(Symbol label, Type type, Symbol field) {
            verify_buffer();
            if (type.kind != K_STRUCT) {
                fprintf(stderr, "[ERROR] Tried to get field of non-struct type.\n");
                exit(1);
            }

            Param p;
            p.kind = PK_MEM;
            p.data.mem.kind = MK_LABEL_TYPE;
            p.data.mem.label = label;
            p.data.mem.type = type;
            const auto& info = ctx->type_info[type.id];
            for (u32 i = 0; i < info.members.size(); i ++) {
                if (info.members[i].name == name(field)) p.data.mem.off = i;
            }

            return p;
        }

        Param l(Symbol symbol) {
            Param p;
            p.kind = PK_LABEL;
            p.data.label = symbol;
            return p;
        }

        Param l(const char* name) {
            Param p;
            p.kind = PK_LABEL;
            p.data.label = jasmine::local(name);
            return p;
        }

        void writeto(Object& obj_in) {
            obj = &obj_in;
            ctx = &obj_in.get_context();
        }

        void nullary(Opcode op) {
            verify_buffer();
            Insn insn;
            insn.type = I64;
            insn.opcode = op;
            assemble_insn(*ctx, *obj, insn);
        }

        void unary(Opcode op, Type type, const Param& dest) {
            verify_buffer();
            Insn insn;
            insn.opcode = op;
            insn.type = type;
            insn.params.push(dest);
            assemble_insn(*ctx, *obj, insn);
        }

        void binary(Opcode op, Type type, const Param& dest, const Param& src) {
            verify_buffer();
            Insn insn;
            insn.opcode = op;
            insn.type = type;
            insn.params.push(dest);
            insn.params.push(src);
            assemble_insn(*ctx, *obj, insn);
        }

        void ternary(Opcode op, Type type, const Param& dest, const Param& lhs, const Param& rhs) {
            verify_buffer();
            Insn insn;
            insn.opcode = op;
            insn.type = type;
            insn.params.push(dest);
            insn.params.push(lhs);
            insn.params.push(rhs);
            assemble_insn(*ctx, *obj, insn);
        }

        void add(Type type, const Param& dest, const Param& lhs, const Param& rhs) {
            ternary(OP_ADD, type, dest, lhs, rhs);
        }

        void sub(Type type, const Param& dest, const Param& lhs, const Param& rhs) {
            ternary(OP_SUB, type, dest, lhs, rhs);
        }
        
        void mul(Type type, const Param& dest, const Param& lhs, const Param& rhs) {
            ternary(OP_MUL, type, dest, lhs, rhs);
        }
        
        void div(Type type, const Param& dest, const Param& lhs, const Param& rhs) {
            ternary(OP_DIV, type, dest, lhs, rhs);
        }
        
        void rem(Type type, const Param& dest, const Param& lhs, const Param& rhs) {
            ternary(OP_REM, type, dest, lhs, rhs);
        }
        
        void and_(Type type, const Param& dest, const Param& lhs, const Param& rhs) {
            ternary(OP_AND, type, dest, lhs, rhs);
        }
        
        void or_(Type type, const Param& dest, const Param& lhs, const Param& rhs) {
            ternary(OP_OR, type, dest, lhs, rhs);
        }
        
        void xor_(Type type, const Param& dest, const Param& lhs, const Param& rhs) {
            ternary(OP_XOR, type, dest, lhs, rhs);
        }
        
        void not_(Type type, const Param& dest, const Param& operand) {
            binary(OP_NOT, type, dest, operand);
        }

        void icast(Type type, const Param& dest, const Param& operand) {
            binary(OP_ICAST, type, dest, operand);
        }

        void f32cast(Type type, const Param& dest, const Param& operand) {
            binary(OP_F32CAST, type, dest, operand);
        }

        void f64cast(Type type, const Param& dest, const Param& operand) {
            binary(OP_F64CAST, type, dest, operand);
        }

        void sxt(Type type, const Param& dest, const Param& operand) {
            binary(OP_EXT, type, dest, operand);
        }

        void zxt(Type type, const Param& dest, const Param& operand) {
            binary(OP_ZXT, type, dest, operand);
        }

        void sl(Type type, const Param& dest, const Param& lhs, const Param& rhs) {
            ternary(OP_SL, type, dest, lhs, rhs);
        }

        void slr(Type type, const Param& dest, const Param& lhs, const Param& rhs) {
            ternary(OP_SLR, type, dest, lhs, rhs);
        }

        void sar(Type type, const Param& dest, const Param& lhs, const Param& rhs) {
            ternary(OP_SAR, type, dest, lhs, rhs);
        }

        void local(Type type, const Param& dest) {
            unary(OP_LOCAL, type, dest);
        }

        void param(Type type, const Param& dest) {
            unary(OP_PARAM, type, dest);
        }
        
        void push(Type type, const Param& operand) {
            unary(OP_PUSH, type, operand);
        }
        
        void pop(Type type, const Param& dest) {
            unary(OP_POP, type, dest);
        }
        
        void frame() {
            nullary(OP_FRAME);
        }

        void ret(Type type, const Param& src) {
            unary(OP_RET, type, src);
        }

        static Insn call_tmp;
        static bool building_call = false;

        void begincall(Type type, const Param& dest, const Param& func) {
            if (building_call) {
                fprintf(stderr, "[ERROR] Cannot build call instruction; already building call.\n");
                exit(1);
            }
            building_call = true;
            call_tmp.opcode = OP_CALL;
            call_tmp.type = type;
            call_tmp.params.clear();
            call_tmp.params.push(dest);
            call_tmp.params.push(func);
        }

        void arg(Type type, const Param& param) {
            call_tmp.params.push(param);
            call_tmp.params.back().annotation = some<Type>(type);
        }

        void endcall() {
            if (!building_call) {
                fprintf(stderr, "[ERROR] Cannot end call instruction; no call instruction is actively "
                    "being assembled.\n");
                exit(1);
            }
            building_call = false;
            assemble_insn(*ctx, *obj, call_tmp);
        }

        void call_recur() {
            endcall();
        }

        void jeq(Type type, Symbol symbol, const Param& lhs, const Param& rhs) {
            ternary(OP_JEQ, type, l(symbol), lhs, rhs);
        }

        void jne(Type type, Symbol symbol, const Param& lhs, const Param& rhs) {
            ternary(OP_JNE, type, l(symbol), lhs, rhs);
        }

        void jl(Type type, Symbol symbol, const Param& lhs, const Param& rhs) {
            ternary(OP_JL, type, l(symbol), lhs, rhs);
        }

        void jle(Type type, Symbol symbol, const Param& lhs, const Param& rhs) {
            ternary(OP_JLE, type, l(symbol), lhs, rhs);
        }

        void jg(Type type, Symbol symbol, const Param& lhs, const Param& rhs) {
            ternary(OP_JG, type, l(symbol), lhs, rhs);
        }

        void jge(Type type, Symbol symbol, const Param& lhs, const Param& rhs) {
            ternary(OP_JGE, type, l(symbol), lhs, rhs);
        }

        void jump(Symbol symbol) {
            unary(OP_JUMP, I64, l(symbol));
        }
        
        void nop() {
            nullary(OP_NOP);
        }

        void ceq(Type type, const Param& dest, const Param& lhs, const Param& rhs) {
            ternary(OP_CEQ, type, dest, lhs, rhs);
        }

        void cne(Type type, const Param& dest, const Param& lhs, const Param& rhs) {
            ternary(OP_CNE, type, dest, lhs, rhs);
        }

        void cl(Type type, const Param& dest, const Param& lhs, const Param& rhs) {
            ternary(OP_CL, type, dest, lhs, rhs);
        }

        void cle(Type type, const Param& dest, const Param& lhs, const Param& rhs) {
            ternary(OP_CLE, type, dest, lhs, rhs);
        }

        void cg(Type type, const Param& dest, const Param& lhs, const Param& rhs) {
            ternary(OP_CG, type, dest, lhs, rhs);
        }

        void cge(Type type, const Param& dest, const Param& lhs, const Param& rhs) {
            ternary(OP_CGE, type, dest, lhs, rhs);
        }

        void mov(Type type, const Param& dest, const Param& operand) {
            binary(OP_MOV, type, dest, operand);
        }

        static string type_name;
        static vector<Member> type_members;

        void begintype(const string& name) {
            type_members.clear();
            type_name = name;
        }

        void member(const string& name, u64 size) {
            type_members.push({ name, size, none<Type>() });
        }

        void member(const string& name, u64 size, Type type) {
            type_members.push({ name, size, some<Type>(type) });
        }

        void endtype() {
            ctx->type_info.push({ 
                ctx->type_info.size(), 
                type_name, 
                type_members 
            });
            ctx->type_decls[type_name] = ctx->type_info.size() - 1;
            
            Insn insn;
            insn.opcode = OP_TYPE;
            insn.type = { K_STRUCT, ctx->type_info.size() - 1 };
            assemble_insn(*ctx, *obj, insn);
        }
        
        void type_recur() {
            endtype();
        }

        void label(Symbol symbol, ObjectSection section) {
            verify_buffer();
            obj->define(symbol, section);
        }

        void label(const char* symbol, ObjectSection section) {
            verify_buffer();
            obj->define(jasmine::local(symbol), section);
        }

        void global(Type type, Symbol symbol) {
            verify_buffer();
            // TODO: implement global registers
        }

        void lit(Param imm, Type type) {
            Insn insn;
            insn.opcode = OP_LIT;
            insn.type = type;
            insn.params.push(imm);
            assemble_insn(*ctx, *obj, insn);
        }
        
        void lit8(u8 val) {
            lit(imm(val), U8);
        }

        void lit16(u16 val) {
            lit(imm(val), U16);
        }

        void lit32(u32 val) {
            lit(imm(val), U32);
        }

        void lit64(u64 val) {
            lit(imm(val), U64);
        }

        void litf32(float f) {
            lit(immfp(f), F32);
        }

        void litf64(double d) {
            lit(immfp(d), F64);
        }
    }

    LiveRange::LiveRange() {}

    LiveRange::LiveRange(Reg reg_in, Type type_in):
        reg(reg_in), type(type_in) {}

    struct Function {
        const Context& ctx;
        u64 first, last;
        u64 stack;
        vector<LiveRange> ranges;
        vector<Location> params;
        u32 n_params;
        vector<vector<LiveRange*>> starts_by_insn, ends_by_insn, preserved_regs;
        map<u64, u64> assignments;

        Function(const Context& ctx_in): ctx(ctx_in), stack(0), n_params(0) {}

        void format(const vector<Insn>& insns, stream& io) const {
            for (u64 i = first; i <= last; i ++) {
                insns[i].format(io);
            }
        }
    };

    vector<Function> find_functions(const Context& ctx, const vector<Insn>& insns) {
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
                    fn = some<Function>(ctx);
                    fn->first = i, fn->last = i;
                }
            }
            else if ((insns[i].opcode == OP_RET || insns[i].opcode == OP_LIT) && fn) { // ret or data
                fn->last = i;
            }
        }
        if (fn && fn->first != fn->last) functions.push(*fn);

        return functions;
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
            for (u32 i = 1; i < in.params.size(); i ++) {
                if (in.params[i].kind == PK_REG) live.insert(in.params[i].data.reg.id);
                if (in.params[i].kind == PK_MEM && 
                    (in.params[i].data.mem.kind == MK_REG_OFF || in.params[i].data.mem.kind == MK_REG_TYPE))
                    live.insert(in.params[i].data.mem.reg.id);
            }
        }
        else {
            for (u32 i = 0; i < in.params.size(); i ++) {
                if (in.params[i].kind == PK_REG) live.insert(in.params[i].data.reg.id);
                if (in.params[i].kind == PK_MEM && 
                    (in.params[i].data.mem.kind == MK_REG_OFF || in.params[i].data.mem.kind == MK_REG_TYPE))
                    live.insert(in.params[i].data.mem.reg.id);
            }
        }
        return live != old || changed;
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

                    changed |= sets[i - function.first].second |= sets[it->second - function.first].first;
                }
                else if (in.opcode >= OP_JEQ && in.opcode < OP_JUMP) { // conditional jump
                    auto it = local_syms.find(in.params[0].data.label);
                    if (it == local_syms.end()) {
                        fprintf(stderr, "[ERROR] Tried to jump to label '%s' outside of current function.\n", 
                            name(in.params[0].data.label));
                        exit(1);
                    }
                    
                    changed |= sets[i - function.first].second |= sets[it->second - function.first].first;
                    changed |= sets[i - function.first].second |= sets[i + 1 - function.first].first;
                }
                else {
                    changed |= sets[i - function.first].second |= sets[i + 1 - function.first].first;
                }
                
                changed = liveout(insns[i], sets[i - function.first].first, sets[i - function.first].second) || changed;
            }
        }

        // in this next phase, we do a pseudo-SSA, uniquely numbering as many distinct
        // registers as possible
        // in order for two register occurrences to be distinct, they must have:
        //  - different register ids (trivial: %0 is different from %1)
        //  - clearly different defining instructions (but we don't explore branches when going up the insn stream)
        auto& assignments = function.assignments;

        for (u64 i = function.first; i <= function.last; i ++) {
            u64 idx = i - function.first;
            const Insn& insn = insns[i];
            if (destructive(insn) && insns[i].params[0].kind == PK_REG) 
                assignments[idx] = insns[i].params[0].data.reg.id;
        }

        vector<map<u64, u64>> dom_assign;
        map<u64, u64> eqv; // we use this to mark assignment ids as identical
        for (u64 i = function.first; i <= function.last; i ++) 
            dom_assign.push({}); // map for every instruction (kinda hefty...maybe optimize this later)
        for (u64 i = function.first; i <= function.last; i ++) {
            u64 idx = i - function.first;
            map<u64, u64>& bind = dom_assign[idx];
            
            auto it = assignments.find(idx);
            if (it != assignments.end()) bind.put(it->second, idx); // associate assignments with themselves

            for (u32 r : sets[idx].first) if (idx > 0) {
                // if we do not already know an assignment for live register r,
                // copy it from the previous instruction
                const auto& prev = dom_assign[idx - 1];
                auto it = prev.find(r);
                auto existing = bind.find(r);
                if (existing != bind.end() && it != prev.end()) {
                    eqv[existing->second] = it->second;
                    eqv[it->second] = existing->second;
                }
                else if (it != prev.end()) bind[r] = it->second; // use prev binding
                else if (existing == bind.end()) 
                    panic("Expected previous assignment of register %", r, " on instruction ", idx + function.first, "! Might be undefined!");
            }
            
            if (insns[i].opcode >= OP_JEQ && insns[i].opcode <= OP_JUMP) { // jump instruction
                auto it = local_syms.find(insns[i].params[0].data.label);
                if (it == local_syms.end()) {
                    fprintf(stderr, "[ERROR] Tried to jump to label '%s' outside of current function.\n", 
                        name(insns[i].params[0].data.label));
                    exit(1);
                }
                // copy bindings to jump targets
                u64 dest = it->second - function.first;
                for (const auto& [r, i] : bind) {
                    auto it = dom_assign[dest].find(r); // check for existing binding
                    if (it != dom_assign[dest].end() && it->second != i) { // unify these two bindings
                        eqv[it->second] = i;
                        eqv[i] = it->second;
                    }
                    else dom_assign[dest].put(r, i);
                }
            }
        }

        // eliminate eqv
        map<u64, u64> canonical; // canonical values for all eqv entries
        for (const auto& [a, b] : eqv) {
            if (a < b) canonical[b] = a;
            else canonical[a] = b;
        }
        
        changed = true;
        while (changed) {
            changed = false;
            for (u64 i = function.first; i <= function.last; i ++) {
                for (auto& [r, i] : dom_assign[i - function.first]) {
                    auto it = canonical.find(i);
                    if (it != canonical.end() && it->second < i) i = it->second, changed = true;
                }
            }
        }
        eqv.clear();
        // println("");
        // for (u64 i = function.first; i <= function.last; i ++) {
        //     print("#", i, ": {");
        //     bool first = true;
        //     for (u32 r : sets[i - function.first].first) {
        //         Param p;
        //         p.kind = PK_REG;
        //         p.data.reg = {false, r};
        //         print(first ? "" : " ");
        //         print_param(function.ctx, p, _stdout, "");
        //         first = false;
        //     }
        //     print("} => {");
        //     first = true;
        //     for (u32 r : sets[i - function.first].second) {
        //         Param p;
        //         p.kind = PK_REG;
        //         p.data.reg = {false, r};
        //         print(first ? "" : " ");
        //         print_param(function.ctx, p, _stdout, "");
        //         first = false;
        //     }
        //     print("}\t");
        //     print("[");
        //     first = true;
        //     for (const auto& [k, v] : dom_assign[i - function.first]) {
        //         if (!first) print(", ");
        //         first = false;
        //         print("%", k, ": ", v + function.first);
        //         if (eqv.contains(v)) print("=", eqv[v] + function.first);
        //     }
        //     print("]\t");
        //     print_insn(function.ctx, _stdout, insns[i]);
        // }
        // println("");

        map<u64, LiveRange> ranges; // map unique assignments to ranges
        for (u64 i = function.first; i <= function.last; i ++) {
            u64 idx = i - function.first;
            const auto& live = sets[idx];
            for (u32 r : live.second) {
                u64 assignment = dom_assign[idx][r];
                if (!live.first.contains(r) || (i > function.first && !sets[idx - 1].second.contains(r))) {
                    // start of interval
                    if (!ranges.contains(assignment)) {
                        LiveRange range(Reg{false, r}, insns[i].type);
                        if (insns[i].opcode == OP_PARAM) range.param_idx = some<u32>(function.n_params ++);
                        ranges.put(assignment, range);
                    }
                    ranges[assignment].intervals.push({ idx, idx });
                }
                else ranges[assignment].intervals.back().second = idx; // continue interval to this insn
            }
            for (u32 r : live.first) if (!live.second.contains(r)) { // end of interval
                ranges[dom_assign[idx][r]].intervals.back().second = idx;
            }
        }
        for (const auto& [_, range] : ranges) function.ranges.push(range);
        for (LiveRange& r : function.ranges) for (const auto& in : r.intervals) {
            for (i64 i = in.first + 1; i < in.second; i ++) if (insns[function.first + i].opcode == OP_CALL) {
                function.preserved_regs[i].push(&r);
            }
        }

        // for (const LiveRange& r : function.ranges) {
        //     Param p;
        //     p.kind = PK_REG;
        //     p.data.reg = r.reg;
        //     print("register ");
        //     print_param({}, p, _stdout, "");
        //     print(" live from ");
        //     bool first = true;
        //     for (const auto& [a, b] : r.intervals) {
        //         if (!first) print(", ");
        //         first = false;
        //         print(a, " to ", b);
        //     }
        //     println("");
        // }
        // for (u32 i = 0; i < function.preserved_regs.size(); i ++) {
        //     print("instruction ", i, " has preserved regs: ");
        //     for (LiveRange* r : function.preserved_regs[i]) print("%", r->reg.id, " ");
        //     print(" | ");
        //     print_insn(function.ctx, _stdout, insns[function.first + i]);
        // }
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
            if (mappings[i] && // don't worry about dests
                (!f.assignments.contains(insn_idx) || f.assignments[insn_idx] != i)) {
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
        for (LiveRange& r : f.ranges) for (const auto& [a, b] : r.intervals) {
            f.starts_by_insn[a].push(&r);
            f.ends_by_insn[b].push(&r);
        }

        vector<vector<LiveRange*>> live_at;
        for (u64 i = f.first; i <= f.last; i ++) {
            vector<LiveRange*> params;
            for (u64 j = 0; j < insns[i].params.size(); j ++) params.push(nullptr);
            live_at.push(move(params));
        }
        for (LiveRange& r : f.ranges) for (const auto& [a, b] : r.intervals) {
            for (u64 j = a; j <= b; j ++) {
                const Insn& insn = insns[f.first + j];
                for (u64 k = 0; k < insn.params.size(); k ++) {
                    if (insn.params[k].kind == PK_REG && r.reg.id == insn.params[k].data.reg.id)
                        live_at[j][k] = &r;
                }
            }
        }
        for (u64 i = f.first; i <= f.last; i ++) {
            // for (const Param& p : insns[i].params) 
            //     if (p.kind == PK_REG && mappings[p.data.reg.id]) ranges.push(mappings[p.data.reg.id]);
            //     else ranges.push(nullptr);
            // for (LiveRange* r : f.starts_by_insn[i - f.first]) 
            //     if (insns[i].params[0].kind == PK_REG && r->reg.id == insns[i].params[0].data.reg.id)
            //         ranges[0] = r; // pull dest from freshly-started range
            target.hint(insns[i], live_at[i - f.first]);
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
                if (r->loc.type != LT_NONE) { // already allocated in a previous interval
                    if (r->loc.type == LT_REGISTER) {
                        if (kregs.contains(*r->loc.reg)) { // remove register and continue
                            kregs.erase(*r->loc.reg);
                            mappings[*r->loc.reg] = r;
                            continue; 
                        }
                        else r = mappings[*r->loc.reg]; // remap existing range and fall through
                    }
                }

                if (kregs.begin() != kregs.end()) {
                    auto reg = r->hint && r->hint->type == LT_REGISTER ? *r->hint->reg : *kregs.begin();
                    // println("\tallocated %", r->reg.id, " to ", x64::REGISTER_NAMES[reg]);
                    r->loc = loc_reg(reg);
                    kregs.erase(reg);
                    mappings[*r->loc.reg] = r;
                }
                else if (!r->param_idx && (!r->hint || r->hint->type == LT_STACK_MEMORY))
                    r->loc = loc_stack(-i64(f.stack += r->type.size(target, f.ctx))); // spill non-parameters
            }
        }

        // for (const LiveRange& r : f.ranges) {
        //     Param p;
        //     p.kind = PK_REG;
        //     p.data.reg = r.reg;
        //     print("register ");
        //     print_param({}, p, _stdout, "");
        //     print(" from ", r.first, " to ", r.last);
        //     if (r.loc.type == LT_NONE) println(" not yet allocated! (probably a parameter)");
        //     else if (r.loc.type == LT_REGISTER) println(" allocated to register %", x64::REGISTER_NAMES[*r.loc.reg]);
        //     else println(" spilled to [%rbp - ", -*r.loc.offset, "]");
        // }
        // println("");
    }

    // X86_64 Codegen

    optional<x64::Arg> to_x64_arg(const Target& target, Type type, const Function& function, 
        const map<u64, LiveRange*>& reg_bindings, const Param& p) {
        x64::Size size = x64::BYTE;
        switch (type.kind) {
            case K_I8:
            case K_U8:
                size = x64::BYTE;
                break;
            case K_I16:
            case K_U16:
                size = x64::WORD;
                break;
            case K_I32:
            case K_U32:
                size = x64::DWORD;
                break;
            case K_I64:
            case K_U64:
            default:
                size = x64::QWORD;
                break;
        }

        switch (p.kind) {
            case PK_IMM:
                return some<x64::Arg>(x64::imm(p.data.imm.val));
            case PK_REG: {
                static x64::Arg(*regs[4])(x64::Register) = {
                    x64::r8, x64::r16, x64::r32, x64::r64
                };
                static x64::Arg(*mems[4])(x64::Register, i64) = {
                    x64::m8, x64::m16, x64::m32, x64::m64
                };
                if (reg_bindings.find(p.data.reg.id) == reg_bindings.end()) return none<x64::Arg>();
                if (reg_bindings[p.data.reg.id]->loc.type == LT_REGISTER) { // bound to register
                    return some<x64::Arg>(regs[size]((x64::Register)*reg_bindings[p.data.reg.id]->loc.reg));
                }
                else {
                    return some<x64::Arg>(mems[size](x64::RBP, -i64(*reg_bindings[p.data.reg.id]->loc.offset)));
                }
            }
            case PK_MEM: {
                const auto& m = p.data.mem;
                switch (m.kind) {
                    case MK_REG_OFF:
                        if (reg_bindings[m.reg.id]->loc.type == LT_REGISTER) {
                            return some<x64::Arg>(x64::m64((x64::Register)*reg_bindings[m.reg.id]->loc.reg, m.off));
                        }
                        else if (reg_bindings[m.reg.id]->loc.type == LT_STACK_MEMORY) {
                            return some<x64::Arg>(x64::m64(x64::RBP, *reg_bindings[m.reg.id]->loc.offset + m.off));
                        }
                        else if (reg_bindings[m.reg.id]->loc.type == LT_PUSHED_R2L) {
                            if (function.stack) {
                                return some<x64::Arg>(x64::m64(x64::RBP, 16 + *reg_bindings[m.reg.id]->loc.offset + m.off));
                            }
                            else return some<x64::Arg>(x64::m64(x64::RSP, 8 + *reg_bindings[m.reg.id]->loc.offset + m.off));
                        }
                        else return none<x64::Arg>();
                    case MK_REG_TYPE: {
                        i64 offset = m.type.offset(target, function.ctx, m.off);
                        if (reg_bindings[m.reg.id]->loc.type == LT_REGISTER) {
                            return some<x64::Arg>(m64((x64::Register)*reg_bindings[m.reg.id]->loc.reg, offset));
                        }
                        else if (reg_bindings[m.reg.id]->loc.type == LT_STACK_MEMORY) {
                            return some<x64::Arg>(m64(x64::RBP, *reg_bindings[m.reg.id]->loc.offset + offset));
                        }
                        else if (reg_bindings[m.reg.id]->loc.type == LT_PUSHED_R2L) {
                            if (function.stack) {
                                return some<x64::Arg>(x64::m64(x64::RBP, 16 + *reg_bindings[m.reg.id]->loc.offset + offset));
                            }
                            else return some<x64::Arg>(x64::m64(x64::RSP, 8 + *reg_bindings[m.reg.id]->loc.offset + offset));
                        }
                        else return none<x64::Arg>();
                    }
                    case MK_LABEL_OFF:
                        // if (reg_bindings[m.reg.id]->loc.type == LT_REGISTER) {
                        //     return some<x64::Arg>(m64((x64::Register)*reg_bindings[m.reg.id]->loc.reg, m.off));
                        // }
                        // else 
                        return none<x64::Arg>();
                    case MK_LABEL_TYPE:
                        // if (reg_bindings[m.reg.id]->loc.type == LT_REGISTER) {
                        //     return some<x64::Arg>(m64((x64::Register)*reg_bindings[m.reg.id]->loc.reg, m.off));
                        // }
                        // else 
                        return none<x64::Arg>();
                    default:
                        return none<x64::Arg>();
                }
            }
            case PK_LABEL:
                return some<x64::Arg>(x64::label64(p.data.label));
            default:
                return none<x64::Arg>();
        }
    }

    void move_x64(const x64::Arg& dest, const x64::Arg& src) {
        using namespace x64;
        if (is_register(dest.type) && dest == src)
            return; // no no-op moves
        if (is_label(src.type) && is_register(dest.type))
            return lea(dest, src);
        if (is_label(src.type)) {
            push(r64(RAX));
            lea(r64(RAX), src);
            mov(dest, r64(RAX));
            pop(r64(RAX));
            return;
        }
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
        ObjectSection section = OS_CODE;
        if (insn.opcode == OP_LIT) section = OS_DATA;
        if (insn.opcode == OP_STAT) section = OS_STATIC;
        if (insn.label) {
            if (section == OS_CODE && obj.get(section).size() % 8 != 0) {
                u32 ideal = ((obj.code().size() + 7) / 8) * 8;
                x64::nop(ideal - obj.code().size()); // align branch targets to 8 bytes
            }
            obj.define(*insn.label, section);
        }
        using namespace x64;

        static Condition conds_x64[6] = {
            EQUAL, NOT_EQUAL,
            LESS, LESS_OR_EQUAL,
            GREATER, GREATER_OR_EQUAL
        };

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
                cqo();
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
                if (ret.type == LT_REGISTER) if (args[0].data.reg != RSP) move_x64(args[0], r64((Register)*ret.reg));

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
                if (is_immediate(args[1].type) && is_immediate(args[2].type)) {
                    move_x64(r64(RAX), args[1]);
                    cmp(r64(RAX), args[2]);
                    jcc(args[0], conds_x64[insn.opcode - OP_JEQ]);
                }
                else {
                    cmp(args[1], args[2]);
                    jcc(args[0], conds_x64[insn.opcode - OP_JEQ]);
                }
                return;
            case OP_NOP:
                return;
            case OP_CEQ:
            case OP_CNE:
            case OP_CL:
            case OP_CLE:
            case OP_CG:
            case OP_CGE:
                if (is_immediate(args[1].type) && is_immediate(args[2].type)) {
                    move_x64(args[0], args[1]);
                    cmp(args[0], args[2]);
                }
                else cmp(args[1], args[2]);
                setcc(args[0], conds_x64[insn.opcode - OP_CEQ]);
                return;
            case OP_JUMP:
                jmp(args[0]);
                return;
            case OP_MOV: 
                return move_x64(args[0], args[1]);
            // case OP_XCHG: 
            //     return move_x64(args[0], args[1]);
            case OP_LIT: {
                const auto& literal = insn.params[0];
                switch (insn.type.kind) {
                    case K_I8: lit8((u8)literal.data.imm.val); break;
                    case K_I16: lit16((u16)literal.data.imm.val); break;
                    case K_I32: lit32((u32)literal.data.imm.val); break;
                    case K_I64: lit64((u64)literal.data.imm.val); break;
                    case K_PTR: lit64((u64)literal.data.imm.val); break;
                    case K_U8: lit8(literal.data.imm.val); break;
                    case K_U16: lit16(literal.data.imm.val); break;
                    case K_U32: lit32(literal.data.imm.val); break;
                    case K_U64: lit64(literal.data.imm.val); break;
                    case K_F32: litf32(*(double*)&literal.data.imm); break;
                    case K_F64: litf64(*(double*)&literal.data.imm); break;
                    case K_STRUCT:
                        fprintf(stderr, "[ERROR] Cannot emit struct literals.\n");
                        exit(1);
                        break;
                    default:
                        break;
                }
            }
            default:
                return;
        }
    }

    void generate_x64(Function& f, vector<Insn>& insns, Object& obj) {
        using namespace x64;
        writeto(obj);        

        // place parameters
        vector<Kind> param_kinds;
        vector<LiveRange*> param_ranges;
        for (u32 i = 0; i < f.n_params; i ++) param_ranges.push(nullptr), param_kinds.push(K_I64);
        for (LiveRange& r : f.ranges) if (r.param_idx) {
            param_ranges[*r.param_idx] = &r;
            param_kinds[*r.param_idx] = r.type.kind;
        }
        vector<Location> param_locs = obj.get_target().place_parameters(param_kinds);
        i64 push_offset = 0;
        for (LiveRange* r : param_ranges) {
            r->loc = param_locs[*r->param_idx];
            if (r->loc.type == LT_PUSHED_R2L) {
                r->loc.offset = some<i64>(push_offset);
                push_offset += r->type.size(obj.get_target(), f.ctx);
            }
        }

        static vector<x64::Arg> args;
        map<u64, LiveRange*> reg_bindings;
        for (u32 i = f.first; i <= f.last; i ++) {
            const Insn& insn = insns[i];

            if (insn.opcode == OP_LIT || insn.opcode == OP_STAT) {
                generate_x64_insn(f, insn, i, args, obj);
                continue;
            }

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
                auto arg = to_x64_arg(obj.get_target(), insn.type, f, reg_bindings, p);
                if (!arg && insn.opcode != OP_CALL && insn.opcode != OP_SYSCALL) useless = true;
                else if (!arg) args.push(r64(RSP)); // rsp signifies lack of real parameter
                else args.push(*arg);
            }
            if (useless) continue;
            
            generate_x64_insn(f, insn, i, args, obj);
        }
    }
    
    Object compile_jasmine(const Context& ctx, const vector<Insn>& insns_in, const Target& target) {
        vector<Insn> insns = insns_in;

        // platform-independent analysis
        perf_begin("finding functions");
        vector<Function> functions = find_functions(ctx, insns);
        perf_end("finding functions");

        perf_begin("computing live ranges");
        for (Function& f : functions) compute_ranges(f, insns);
        perf_end("computing live ranges");

        // associate virtual registers with native ones
        perf_begin("allocating registers");
        for (Function& f : functions) allocate_registers(f, insns, target);
        perf_end("allocating registers");

        Object obj(target); // create our destination object
        obj.set_context(ctx);

        // generate native instructions
        perf_begin("generating native code");
        switch (target.arch) {
            case X86_64: 
                for (Function& f : functions) generate_x64(f, insns, obj);
                break;
            default:
                panic("Unimplemented architecture!");
                break;
        }
        perf_end("generating native code");

        return obj;
    }
}