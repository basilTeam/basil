/*
 * Copyright (c) 2021, the Basil authors
 * All rights reserved.
 *
 * This source code is licensed under the 3-Clause BSD License, the full text
 * of which can be found in the LICENSE file in the root directory
 * of this project.
 */

#include "test.h"
#include "jasmine/obj.h"
#include "jasmine/x64.h"

using namespace jasmine;
using namespace x64;

SETUP {
    onlyin(X86_64);
}

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

TEST(simple_arithmetic) {
    Object obj;
    writeto(obj);

    label(global("foo"));
    push(r64(RBP));
    mov(r64(RBP), r64(RSP));
    mov(r64(RAX), imm(1));
    add(r64(RAX), imm(2));
    mov(r64(RDX), imm(3));
    imul(r64(RAX), r64(RDX));
    sub(r64(RAX), imm(3));
    cdq();
    mov(r64(RCX), imm(2));
    idiv(r64(RCX));
    mov(r64(RSP), r64(RBP));
    pop(r64(RBP));
    ret();

    obj.load();
    auto foo = (int(*)())obj.find(global("foo"));
    ASSERT_EQUAL(foo(), 3);
}

TEST(small_loop) {
    Object obj;
    writeto(obj);

    label(global("foo"));
        push(r64(RBP));
        mov(r64(RBP), r64(RSP));
        sub(r64(RSP), imm(4));
        mov(m32(RBP, -4), r32(RDI));
    label(local("loop"));
        cmp(m32(RBP, -4), imm(10));
        jcc(label32(local("end")), GREATER_OR_EQUAL);
        inc(m64(RBP, -4));
        jmp(label32(local("loop")));
    label(local("end"));
        mov(r32(RAX), m32(RBP, -4));
        mov(r64(RSP), r64(RBP));
        pop(r64(RBP));
        ret();
    
    obj.load();
    auto foo = (int(*)(int))obj.find(global("foo"));
    ASSERT_EQUAL(foo(0), 10);
    ASSERT_EQUAL(foo(5), 10);
    ASSERT_EQUAL(foo(10), 10);
}

TEST(recursive) {
    Object obj;
    writeto(obj);

    label(global("factorial"));
        cmp(r16(RDI), imm(0));
        jcc(label32(local("recur")), NOT_EQUAL);
        mov(r16(RAX), imm(1));
        ret();
    label(local("recur"));
        push(r16(RDI));
        sub(r16(RDI), imm(1));
        call(label32(global("factorial")));
        pop(r16(RDI));
        imul(r16(RAX), r16(RDI));
        ret();
    
    obj.load();
    auto factorial = (i16(*)(i16))obj.find(global("factorial"));
    ASSERT_EQUAL(factorial(5), 120);
    
    ASSERT_EQUAL(factorial(0), 1);
    ASSERT_EQUAL(factorial(1), 1);
}

TEST(extends) {
    Object obj;
    writeto(obj);

    label(global("add_zerox"));
        add(r8(RDI), r8(RSI));
        movzx(r32(RAX), r8(RDI));
        ret();
    label(global("add_signx"));
        add(r8(RDI), r8(RSI));
        movsx(r32(RAX), r8(RDI));
        ret();
    label(global("foo"));
        mov(r8(RAX), imm(-1));
        movsx(r16(RAX), r8(RAX));
        movzx(r32(RAX), r16(RAX));
        ret();

    obj.load();
    auto add_zerox = (i32(*)(i8, i8))obj.find(global("add_zerox"));
    auto add_signx = (i32(*)(i8, i8))obj.find(global("add_signx"));
    auto foo = (i32(*)())obj.find(global("foo"));

    ASSERT_EQUAL(add_zerox(0, 0), 0);
    ASSERT_EQUAL(add_zerox(10, 20), 30);
    ASSERT_EQUAL(add_signx(10, 20), 30);
    ASSERT_EQUAL(add_zerox(64, 64), 128);
    ASSERT_EQUAL(add_signx(64, 64), -128);
    ASSERT_EQUAL(foo(), 65535);
}

TEST(indexing) {
    Object obj;
    writeto(obj);

    label(global("sum_array"));
        // RDI = array ptr, RSI = length
        mov(r64(RCX), imm(0)); // index
        xor_(r64(RAX), r64(RAX));
    label(local("sum_loop"));
        lea(r64(RDX), m64(RSI, -1)); // compare against size - 1
        cmp(r64(RCX), r64(RDX));
        jcc(label32(local("sum_end")), GREATER_OR_EQUAL);
        add(r64(RAX), m64(RDI, RCX, SCALE8, 8));
        inc(r64(RCX));
        jmp(label32(local("sum_loop")));
    label(local("sum_end"));
        ret();
    label(global("fill_array"));
        // RDI = array ptr, RSI = length, RDX = value
        mov(r64(R10), imm(0)); // index
    label(local("fill_loop"));
        lea(r64(RBX), m64(RSI, -1)); // compare against size - 1
        cmp(r64(R10), r64(RBX));
        jcc(label32(local("fill_end")), GREATER_OR_EQUAL);
        mov(m32(RDI, R10, SCALE4, 4), r32(RDX));
        inc(r64(R10));
        jmp(label32(local("fill_loop")));
    label(local("fill_end"));
        ret();

    obj.load();
    auto sum_array = (i64(*)(const i64*, i64))obj.find(global("sum_array"));
    auto fill_array = (void(*)(i32*, i64, i32))obj.find(global("fill_array"));
    const i64 array[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };
    ASSERT_EQUAL(sum_array(array, 8), 35); // sum from 2 to 8 (we skip the first)

    i32 array2[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
    fill_array(array2, 8, 1);
    ASSERT_EQUAL(array2[0], 0);
    ASSERT_EQUAL(array2[1], 1);
    ASSERT_EQUAL(array2[7], 1);
}

TEST(nops) {
    Object obj;
    writeto(obj);

    label(global("foo"));
        mov(r64(RAX), imm(13));
        for (u32 i = 1; i <= 9; i ++) nop(i);
        ret();

    obj.load();
    auto foo = (i64(*)())obj.find(global("foo"));
    ASSERT_EQUAL(foo(), 13);
}

TEST(nop_payloads) {
    Object obj;
    writeto(obj);

    label(global("foo"));
        nop(3);
        nop32(4000);
        nop(2);
        nop32(3000);
        nop(1);
        nop32(2000);
        nop32(1000);
    label(local("ret"));
        ret();

    obj.load();
    auto foo = (void(*)())obj.find(global("foo"));
    foo();

    auto ret = (uint32_t*)obj.find(local("ret"));
    ret --;
    ASSERT_EQUAL(*ret --, 1000);
    ret --;
    ASSERT_EQUAL(*ret --, 2000);
    ret --;
    ASSERT_EQUAL(*ret --, 3000);
    ret --;
    ASSERT_EQUAL(*ret --, 4000);
    ret --;
}