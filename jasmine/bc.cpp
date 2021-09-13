#include "bc.h"
#include "stdlib.h"
#include "stdio.h"
#include "util/str.h"
#include "util/hash.h"

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
    using Disassembler = void(*)(stream&, Insn&);

    // Validates an instruction at the param 'param'. Returns the next param
    // if validation is complete, the current param if more validation is necessary,
    // or -1 if an error occurred.
    using Validator = i64(*)(const Insn&, i64 param);

    // Writes an instruction component to Jasmine bytecode in an object file.
    using Assembler = i64(*)(stream&, const Insn&, i64 param);

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
        vector<OpComponent> components;

        void add_components() {}

        template<typename... Args>
        void add_components(const OpComponent& component, const Args&... args) {
            components.push(component);
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
            fprintf(stderr, "Expected '%c'.\n", ch);
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
                        p.data.mem.kind = MK_TYPE;
                        p.data.mem.reg = ptr.data.reg;
                        p.data.mem.off = off + 1;
                        p.data.mem.type = t;
                    }
                    else {
                        p.data.mem.kind = MK_TYPE;
                        p.data.mem.reg = ptr.data.reg;
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
        TypeInfo info = TypeInfo{ context.type_info.size(), name, members };
        context.type_info.push(info);
        context.type_decls.put(name, info.id);
        insn.type = Type{ K_STRUCT, info.id };
    }

    // Disassemblers
    
    // Validators

    // Assemblers

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
                case MK_TYPE:
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
            }
        }
    }

    i64 print_param(const Context& context, stream& io, const Insn& insn, i64 param) {
        const Param& p = insn.params[param];
        print_param(context, insn.params[param], io, " ");
        return param + 1;
    }

    i64 print_another_param(const Context& context, stream& io, const Insn& insn, i64 param) {
        const Param& p = insn.params[param];
        print_param(context, insn.params[param], io, ", ");
        return param + 1;
    }

    i64 print_variadic_param(const Context& context, stream& io, const Insn& insn, i64 param) {
        write(io, " (");
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
        nullptr,
        nullptr,
        nullptr,
        print_type
    };

    static OpComponent C_SRC = {
        parse_another_param,
        nullptr,
        nullptr,
        nullptr,
        print_another_param
    };

    static OpComponent C_DEST = {
        parse_param,
        nullptr, 
        nullptr,
        nullptr,
        print_param
    };

    static OpComponent C_VARIADIC = {
        parse_variadic_param,
        nullptr,
        nullptr,
        nullptr,
        print_variadic_param
    };

    static OpComponent C_LABEL = {
        parse_param,
        nullptr,
        nullptr,
        nullptr,
        print_label
    };

    static OpComponent C_TYPEDEF = {
        parse_typedef,
        nullptr,
        nullptr,
        nullptr,
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
            C_TYPE, C_DEST, C_SRC, C_LABEL
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
        nullary_op(OP_RET),
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
        auto it = OPCODE_TABLE.find(op);
        if (it == OPCODE_TABLE.end()) {
            fprintf(stderr, "Unknown opcode '%s'.\n", (const char*)op.raw());
            exit(1);
        }
        insn.opcode = it->second;
        for (const auto& comp : OPS[insn.opcode].components) comp.parser(context, io, insn);
        i64 i = 0;
        for (const auto& comp : OPS[insn.opcode].components) if (comp.validator) {
            i64 v = comp.validator(insn, i);
            if (v < 0) exit(1);
            else i = v;
        }
        return insn;
    }

    void assemble_insn(Context& context, stream& io, const Insn& insn) {

    }

    void print_insn(Context& context, stream& io, const Insn& insn) {
        i64 i = 0;
        write(io, "\t", OPCODE_NAMES[insn.opcode]);
        for (const auto& comp : OPS[insn.opcode].components) 
            i = comp.printer(context, io, insn, i);
        write(io, "\n");
    }

    Object jasmine_to_x86(Object& obj) {
        return Object(X86_64);
    }
}