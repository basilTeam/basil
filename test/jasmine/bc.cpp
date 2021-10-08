#include "jasmine/bc.h"
#include "test.h"

using namespace jasmine;

void write_asm(bytebuf buf, stream& io) {
    while (buf.size()) {
        u8 byte = buf.read();
        u8 upper = byte >> 4, lower = byte & 15;
        const char* hex = "0123456789abcdef";
        io.write(hex[upper]);
        io.write(hex[lower]);
        io.write(' ');
    }
    io.write('\n');
}

TEST(simple_parse) {
    buffer b;
    write(b, R"(
type Tree { 
    left : 8, 
    right : 8, 
    val : i64 * 4 
}

mov i64 %0, [%1 + Tree.right]
)");
    Context ctx;
    Insn insn = parse_insn(ctx, b);

    ASSERT_TRUE(insn.opcode == OP_TYPE);
    ASSERT_TRUE(insn.type.kind == K_STRUCT);

    Insn insn2 = parse_insn(ctx, b);

    ASSERT_TRUE(insn2.opcode == OP_MOV);
    ASSERT_TRUE(insn2.type.kind == K_I64);
    ASSERT_TRUE(insn2.params.size() == 2);
    ASSERT_TRUE(insn2.params[0].kind == PK_REG);
    ASSERT_EQUAL(insn2.params[0].data.reg.id, 0);
    ASSERT_FALSE(insn2.params[0].data.reg.global);
    ASSERT_TRUE(insn2.params[1].kind == PK_MEM);
    ASSERT_TRUE(insn2.params[1].data.mem.kind == MK_REG_TYPE);
    ASSERT_EQUAL(insn2.params[1].data.mem.reg.id, 1);
    ASSERT_TRUE(insn2.params[1].data.mem.type.id == insn.type.id); // should have same type as insn
}

TEST(round_trip) {
    buffer in;
    write(in,
"foo:\tlocal i64 %0\n"
"\tmov i64 %0, 1\n"
"\tcall i64 %1, foo(i64 %0, i64 0, i64 1, i64 2, i64 3)\n");
    buffer copy(in);
    Context ctx;
    Insn insns[3];
    for (u8 i = 0; i < 3; i ++) insns[i] = parse_insn(ctx, in);

    Object object({ JASMINE, UNSUPPORTED_OS });
    for (u8 i = 0; i < 3; i ++) assemble_insn(ctx, object, insns[i]);

    bytebuf buf = object.code();
    for (u8 i = 0; i < 3; i ++) insns[i] = disassemble_insn(ctx, buf, object);

    buffer out;
    for (u8 i = 0; i < 3; i ++) print_insn(ctx, out, insns[i]);

    string a(copy), b(out);
    ASSERT_EQUAL(a, b);
}

TEST(labeled_branches) {
    buffer in;
    write(in,
"foo:\tframe\n"
"\tparam i64 %0\n"
"\tlocal i64 %1\n"
"_L0:\tjeq i64 _L1 %0, %1\n"
"\tsub i64 %0, %0, 1\n"
"\tjump _L0\n"
"_L1:\tret i64 %0\n");
    buffer copy(in);
    Context ctx;
    Insn insns[7];
    for (u8 i = 0; i < 7; i ++) insns[i] = parse_insn(ctx, in);

    Object object({ JASMINE, UNSUPPORTED_OS });
    for (u8 i = 0; i < 7; i ++) assemble_insn(ctx, object, insns[i]);

    bytebuf buf = object.code();
    for (u8 i = 0; i < 7; i ++) insns[i] = disassemble_insn(ctx, buf, object);

    buffer out;
    for (u8 i = 0; i < 7; i ++) print_insn(ctx, out, insns[i]);

    string a(copy), b(out);
    ASSERT_EQUAL(a, b);
}

TEST(x86_arithmetic_spills) {
    onlyin(X86_64);

    buffer in;
    write(in,R"(
foo: frame
     mov i64 %0, 1
     mov i64 %1, 2
     add i64 %2, %0, %1
     mul i64 %2, %2, 3 
     div i64 %3, %1, %0
     add i64 %3, %2, %3
     ret i64 %3
)");
    // buffer copy(in);
    Context ctx;
    vector<Insn> insns = parse_all_insns(ctx, in);
    // println("");
    // println(string(copy));
    Object obj = compile_jasmine(insns, DEFAULT_TARGET);
    // print("ASM: "); write_asm(obj.code(), _stdout);
    // println("");
    
    obj.load();
    auto foo = (i64(*)())obj.find(global("foo"));
    // printf("symbol 'foo' loaded to address 0x%lx\n", (uintptr_t)foo);
    // println("foo() = ", foo());
    // println("");
    ASSERT_EQUAL(foo(), 11);
}

TEST(x86_simple_loop) {
    onlyin(X86_64);

    buffer in;
    write(in,R"(
foo: frame
     mov i64 %0, 1
rep: jeq i64 end %0, 10
     add i64 %0, %0, 1
     jump rep
end: ret i64 %0
)");
    // buffer copy(in);
    Context ctx;
    vector<Insn> insns = parse_all_insns(ctx, in);
    // println("");
    // println(string(copy));
    Object obj = compile_jasmine(insns, DEFAULT_TARGET);
    
    obj.load();
    // print("ASM: "); write_asm(obj.code(), _stdout);
    // println("");

    auto foo = (i64(*)())obj.find(global("foo"));
    // printf("symbol 'foo' loaded to address 0x%lx\n", (uintptr_t)foo);
    // println("foo() = ", foo());
    // println("");
    ASSERT_EQUAL(foo(), 10);
}

TEST(x86_fibonacci) {
    onlyin(X86_64);

    buffer in;
    write(in,R"(
fib: frame
     param i64 %0
     jge i64 rec %0, 2
     ret i64 %0
rec: sub i64 %0, %0, 1
     call i64 %1, fib(i64 %0)
     sub i64 %0, %0, 1
     call i64 %2, fib(i64 %0)
     add i64 %1, %1, %2
     ret i64 %1
)");
    // buffer copy(in);
    Context ctx;
    vector<Insn> insns = parse_all_insns(ctx, in);
    // println("");
    // println(string(copy));
    Object obj = compile_jasmine(insns, DEFAULT_TARGET);
    
    obj.load();
    print("ASM: "); write_asm(obj.code(), _stdout);
    println("");

    auto fib = (i64(*)(i64))obj.find(global("fib"));
    // printf("symbol 'foo' loaded to address 0x%lx\n", (uintptr_t)foo);
    // println("foo() = ", foo());
    // println("");
    ASSERT_EQUAL(fib(10), 55);
}