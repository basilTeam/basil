#include "bc.h"
#include "vec.h"
#include "option.h"
#include "either.h"
#include "x64.h"
#include <cstdlib>

namespace jasmine {
    const Reg FP = 0, SP = 1, IP = 2, LABEL = 3;

    struct InsnClass {
        const char** names;
        i32* addends;
    };

    /*
    0 - 5       ADD     (8, 16, 32, 64, f32, f64)
    6 - 11      ADDI    (8, 16, 32, 64, f32, f64)
    12 - 17     SUB     (8, 16, 32, 64, f32, f64)
    18 - 23     SUBI    (8, 16, 32, 64, f32, f64)
    24 - 29     MUL     (8, 16, 32, 64, f32, f64)
    30 - 35     MULI    (8, 16, 32, 64, f32, f64)
    30 - 39     DIV     (s8, s16, s32, s64, u8, u16, u32, u64, f32, f64)
    36 - 45     DIVI    (s8, s16, s32, s64, u8, u16, u32, u64, f32, f64)
    46 - 55     REM     (s8, s16, s32, s64, u8, u16, u32, u64, f32, f64)
    56 - 65     REMI     (s8, s16, s32, s64, u8, u16, u32, u64, f32, f64)
    66 - 69     AND     (8, 16, 32, 64)
    70 - 73     ANDI    (8, 16, 32, 64)
    74 - 77     OR      (8, 16, 32, 64)
    78 - 81     ORI     (8, 16, 32, 64)
    82 - 85     XOR     (8, 16, 32, 64)
    86 - 89     XORI    (8, 16, 32, 64)
    90 - 93     NOT     (8, 16, 32, 64)
    94 - 97     NOTI    (8, 16, 32, 64)
    98 - 103    MOV     (8, 16, 32, 64, f32, f64)
    104 - 109   MOVI    (8, 16, 32, 64, f32, f64)
    110 - 115   CMP     (8, 16, 32, 64, f32, f64)
    116 - 121   CMPI    (8, 16, 32, 64, f32, f64)
    122 - 124   SXT     (8, 16, 32)
    125 - 127   ZXT     (8, 16, 32)
    128 - 129   ICAST   (f32, f64)
    130 - 134   F32CAST (8, 16, 32, 64, f64)
    135 - 139   F64CAST (8, 16, 32, 64, f32)
    140 - 143   SL      (8, 16, 32, 64)
    144 - 147   SLI     (8, 16, 32, 64)
    148 - 151   LSR     (8, 16, 32, 64)
    152 - 155   LSRI    (8, 16, 32, 64)
    156 - 159   ASR     (8, 16, 32, 64)
    160 - 163   ASRI    (8, 16, 32, 64)
    164 - 169   PUSH    (8, 16, 32, 64, f32, f64)
    170 - 175   PUSHI   (8, 16, 32, 64, f32, f64)
    176 - 181   POP     (8, 16, 32, 64, f32, f64)
    182 - 187   CALL    (8, 16, 32, 64, f32, f64)
    188 - 193   CALL(r) (8, 16, 32, 64, f32, f64)
    194 - 203   VAR     (s8, s16, s32, s64, u8, u16, u32, u64, f32, f64)
    204 - 213   ARG     (s8, s16, s32, s64, u8, u16, u32, u64, f32, f64)
    214 - 223   RET     (s8, s16, s32, s64, u8, u16, u32, u64, f32, f64)
    224 - 231   Jcc     (L, LE, G, GE, EQ, NE, O, NO)
    232         JUMP    
    233         MVAR
    234         MARG
    235         RET(void)
    236         FRAME
    */

    static const char* IFTSTRINGS[] = { "8", "16", "32", "64", "f32", "f64", "ptr" };
    static i32 IFTCODES[] = { 0, 1, 2, 3, 0, 1, 2, 3, 4, 5, 6 };
    static InsnClass IFT = { IFTSTRINGS, IFTCODES };

    static const char* IUFTSTRINGS[] = { "s8", "s16", "s32", "s64", "u8", "u16", "u32", "u64", "f32", "f64", "ptr" };
    static i32 IUFTCODES[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
    static InsnClass IUFT = { IUFTSTRINGS, IUFTCODES };

    static const char* ITSTRINGS[] = { "8", "16", "32", "64", "ptr" };
    static i32 ITCODES[] = { 0, 1, 2, 3, 0, 1, 2, 3, -1, -1, 4 };
    static InsnClass IT = { ITSTRINGS, ITCODES };

    static const char* INOPTRTSTRINGS[] = { "8", "16", "32", "64" };
    static i32 INOPTRTCODES[] = { 0, 1, 2, 3, 0, 1, 2, 3, -1, -1, -1 };
    static InsnClass INOPTRT = { INOPTRTSTRINGS, INOPTRTCODES };

    static const char* IUNOPTRTSTRINGS[] = { "s8", "s16", "s32", "s64", "u8", "u16", "u32", "u64" };
    static i32 IUNOPTRTCODES[] = { 0, 1, 2, 3, 4, 5, 6, 7, -1, -1, -1 };
    static InsnClass IUNOPTRT = { IUNOPTRTSTRINGS, IUNOPTRTCODES };

    static const char* SXTTSTRINGS[] = { "s8", "s16", "s32", "u8", "u16", "u32" };
    static i32 SXTTCODES[] = { 0, 1, 2, -1, 3, 4, 5, -1, -1, -1, -1 };
    static InsnClass SXTT = { SXTTSTRINGS, SXTTCODES };

    static const char* ZXTTSTRINGS[] = { "8", "16", "32" };
    static i32 ZXTTCODES[] = { 0, 1, 2, -1, 0, 1, 2, -1, -1, -1, -1 };
    static InsnClass ZXTT = { ZXTTSTRINGS, ZXTTCODES };

    static const char* ICASTTSTRINGS[] = { "f32", "f64" };
    static i32 ICASTTCODES[] = { -1, -1, -1, -1, -1, -1, -1, -1, 0, 1, -1 };
    static InsnClass ICASTT = { ICASTTSTRINGS, ICASTTCODES };

    static const char* F32CASTTSTRINGS[] = { "8", "16", "32", "64", "f64" };
    static i32 F32CASTTCODES[] = { 0, 1, 2, 3, 0, 1, 2, 3, -1, 4, -1 };
    static InsnClass F32CASTT = { F32CASTTSTRINGS, F32CASTTCODES };

    static const char* F64CASTTSTRINGS[] = { "8", "16", "32", "64", "f32" };
    static i32 F64CASTTCODES[] = { 0, 1, 2, 3, 0, 1, 2, 3, 4, -1, -1 };
    static InsnClass F64CASTT = { F64CASTTSTRINGS, F64CASTTCODES };

    static const char* FPTSTRINGS[] = { "", "f" };
    static i32 FPTCODES[] = { 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0 };
    static InsnClass FPT = { FPTSTRINGS, FPTCODES };

    static InsnClass NONET = { nullptr, nullptr };

    static const char* OP_NAMES[] = {
        "add", "add", "add", "add", "add", "add", "add", "add", "add", "add", "add", "add", "add", "add", 
        "sub", "sub", "sub", "sub", "sub", "sub", "sub", "sub", "sub", "sub", "sub", "sub", "sub", "sub",
        "mul", "mul", "mul", "mul", "mul", "mul", "mul", "mul", "mul", "mul", "mul", "mul", "mul", "mul", "mul", "mul", "mul", "mul", "mul", "mul", "mul", "mul", 
        "div", "div", "div", "div", "div", "div", "div", "div", "div", "div", "div", "div", "div", "div", "div", "div", "div", "div", "div", "div", "div", "div", 
        "rem", "rem", "rem", "rem", "rem", "rem", "rem", "rem", "rem", "rem", "rem", "rem", "rem", "rem", "rem", "rem", "rem", "rem", "rem", "rem", "rem", "rem", 
        "and", "and", "and", "and", "and", "and", "and", "and", "and", "and", 
        "or", "or", "or", "or", "or", "or", "or", "or", "or", "or", 
        "xor", "xor", "xor", "xor", "xor", "xor", "xor", "xor", "xor", "xor", 
        "not", "not", "not", "not", "not", "not", "not", "not", "not", "not", 
        "mov", "mov", "mov", "mov", "mov", "mov", "mov", "mov", "mov", "mov", "mov", "mov", "mov", "mov", 
        "cmp", "cmp", "cmp", "cmp", "cmp", "cmp", "cmp", "cmp", "cmp", "cmp", "cmp", "cmp", "cmp", "cmp", 
        "sxt", "sxt", "sxt", "sxt", "sxt", "sxt", "zxt", "zxt", "zxt", 
        "icast", "icast", "f32cast", "f32cast", "f32cast", "f32cast", "f32cast", "f64cast", "f64cast", "f64cast", "f64cast", "f64cast", 
        "shl", "shl", "shl", "shl", "shl", "shl", "shl", "shl", 
        "shr", "shr", "shr", "shr", "shr", "shr", "shr", "shr", "shr", "shr", "shr", "shr", "shr", "shr", "shr", "shr", 
        "frame", "ret", 
        "push", "push", "push", "push", "push", "push", "push", "push", "push", "push", "push", "push", "push", "push", 
        "pop", "pop", "pop", "pop", "pop", "pop", "pop", 
        "call", "call", "call", "call", "call", "call", "call", "callr", "callr", "callr", "callr", "callr", "callr", "callr", 
        "jl", "jle", "jg", "jge", "jeq", "jne", "jump", "arg", "arg"
    };

    static u32 OP_BASES[] = {
        0, 0, 0, 0, 0, 0, 0, 7, 7, 7, 7, 7, 7, 7, 
        14, 14, 14, 14, 14, 14, 14, 21, 21, 21, 21, 21, 21, 21, 
        28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 
        50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 
        72, 72, 72, 72, 72, 72, 72, 72, 72, 72, 72, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 
        94, 94, 94, 94, 94, 99, 99, 99, 99, 99, 
        104, 104, 104, 104, 104, 109, 109, 109, 109, 109, 
        114, 114, 114, 114, 114, 119, 119, 119, 119, 119, 
        124, 124, 124, 124, 124, 129, 129, 129, 129, 129, 
        134, 134, 134, 134, 134, 134, 134, 141, 141, 141, 141, 141, 141, 141, 
        148, 148, 148, 148, 148, 148, 148, 155, 155, 155, 155, 155, 155, 155,
        162, 162, 162, 162, 162, 162, 168, 168, 168, 
        171, 171, 173, 173, 173, 173, 173, 178, 178, 178, 178, 178, 
        183, 183, 183, 183, 187, 187, 187, 187, 
        191, 191, 191, 191, 191, 191, 191, 191, 199, 199, 199, 199, 199, 199, 199, 199, 
        207, 208,
        209, 209, 209, 209, 209, 209, 209, 216, 216, 216, 216, 216, 216, 216, 
        223, 223, 223, 223, 223, 223, 223, 
        230, 230, 230, 230, 230, 230, 230, 237, 237, 237, 237, 237, 237, 237,
        244, 245, 246, 247, 248, 249, 250, 251, 251
    };

    static bool OP_IMMEDIATES[] = {
        0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1,
        0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 
        0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 
        0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 
        0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 
        0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1,
        0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
        0, 0, 0, 0, 1, 1, 1, 1, 
        0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 
        0, 0, 
        0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1,
        0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1
    };

    static u8 OP_DESTRUCTIVE[] = {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // mov replaces the left operand entirely
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        1, 1, 1, 1, 1, 1, 1, 1, 1, // sxt is just an extending move
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // casts are just converting moves
        0, 0, 0, 0, 0, 0, 0, 0, 
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        1, 1, 1, 1, 1, 1, 1, // pop overwrites its operand
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // call overwrites the left operand
        0, 0, 0, 0, 0, 0, 0, 1, 1 // arg defines its left operand
    };

    static InsnClass OP_NAME_TABLES[] = {
        IFT, IFT, IFT, IFT, IFT, IFT, IFT, IFT, IFT, IFT, IFT, IFT, IFT, IFT, // add / addi
        IFT, IFT, IFT, IFT, IFT, IFT, IFT, IFT, IFT, IFT, IFT, IFT, IFT, IFT, // sub / subi
        IUFT, IUFT, IUFT, IUFT, IUFT, IUFT, IUFT, IUFT, IUFT, IUFT, IUFT, IUFT, IUFT, IUFT, IUFT, IUFT, IUFT, IUFT, IUFT, IUFT, IUFT, IUFT, // mul / muli
        IUFT, IUFT, IUFT, IUFT, IUFT, IUFT, IUFT, IUFT, IUFT, IUFT, IUFT, IUFT, IUFT, IUFT, IUFT, IUFT, IUFT, IUFT, IUFT, IUFT, IUFT, IUFT, // div / divi
        IUFT, IUFT, IUFT, IUFT, IUFT, IUFT, IUFT, IUFT, IUFT, IUFT, IUFT, IUFT, IUFT, IUFT, IUFT, IUFT, IUFT, IUFT, IUFT, IUFT, IUFT, IUFT, // rem / remi
        IT, IT, IT, IT, IT, IT, IT, IT, IT, IT, // and / andi
        IT, IT, IT, IT, IT, IT, IT, IT, IT, IT, // or / ori
        IT, IT, IT, IT, IT, IT, IT, IT, IT, IT, // xor / xori
        IT, IT, IT, IT, IT, IT, IT, IT, IT, IT, // not / noti
        IFT, IFT, IFT, IFT, IFT, IFT, IFT, IFT, IFT, IFT, IFT, IFT, IFT, IFT, // mov / movi
        IFT, IFT, IFT, IFT, IFT, IFT, IFT, IFT, IFT, IFT, IFT, IFT, IFT, IFT, // cmp / cmpi
        SXTT, SXTT, SXTT, SXTT, SXTT, SXTT, ZXTT, ZXTT, ZXTT, // sxt / zxt
        ICASTT, ICASTT, F32CASTT, F32CASTT, F32CASTT, F32CASTT, F32CASTT, F64CASTT, F64CASTT, F64CASTT, F64CASTT, F64CASTT, // icast / f32cast / f64cast
        INOPTRT, INOPTRT, INOPTRT, INOPTRT, INOPTRT, INOPTRT, INOPTRT, INOPTRT, // shl
        IUNOPTRT, IUNOPTRT, IUNOPTRT, IUNOPTRT, IUNOPTRT, IUNOPTRT, IUNOPTRT, IUNOPTRT, IUNOPTRT, IUNOPTRT, IUNOPTRT, IUNOPTRT, IUNOPTRT, IUNOPTRT, IUNOPTRT, IUNOPTRT, // shr 
        NONET, NONET, // frame / ret
        IFT, IFT, IFT, IFT, IFT, IFT, IFT, IFT, IFT, IFT, IFT, IFT, IFT, IFT, // push / pushi
        IFT, IFT, IFT, IFT, IFT, IFT, IFT, // pop
        IFT, IFT, IFT, IFT, IFT, IFT, IFT, IFT, IFT, IFT, IFT, IFT, IFT, IFT, // call / rcall
        NONET, NONET, NONET, NONET, NONET, NONET, NONET, FPT, FPT, // the rest of them lol
    };

    static u8 OP_ARITIES[] = {
        2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, // add
        2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, // sub
        2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, // mul
        2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, // div
        2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, // rem
        2, 2, 2, 2, 2, 2, 2, 2, 2, 2, // and
        2, 2, 2, 2, 2, 2, 2, 2, 2, 2, // or
        2, 2, 2, 2, 2, 2, 2, 2, 2, 2, // xor
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // not
        2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, // mov
        2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, // cmp
        2, 2, 2, 2, 2, 2, 2, 2, 2, // sxt / zxt
        2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, // casts
        2, 2, 2, 2, 2, 2, 2, 2, // shl
        2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, // shr
        0, 0, // frame / ret
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // push
        1, 1, 1, 1, 1, 1, 1, // pop
        1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, // call / rcall
        0, 0, 0, 0, 0, 0, 0, 1, 1 // the rest of them
    };

    Param r(Reg reg) {
        Param p;
        p.kind = PK_REG;
        p.data.reg = reg + 4;
        return p;
    }

    Param imm(Imm i) {
        Param p;
        p.kind = PK_IMM;
        p.data.imm = i;
        return p;
    }

    Param m(Reg reg) {
        return m(reg + 4, 0);
    }

    Param m(Reg reg, i64 off) {
        Param p;
        p.kind = PK_MEM;
        p.data.mem = {reg + 4, off};
        return p;
    }

    Param l(Symbol symbol) {
        Param p;
        p.kind = PK_LABEL;
        p.data.label = symbol;
        return p;
    }

    Param l(const char* name) {
        return l(local(name));
    }

    static Object* target = nullptr;

    void writeto(Object& buf) {
        target = &buf;
    }

    void write_opcode(stream& io, u8 opcode) {
        write(io, OP_NAMES[opcode]);
        const char** table = OP_NAME_TABLES[opcode].names;
        if (table && *table[opcode - OP_BASES[opcode]]) write(io, ".", table[opcode - OP_BASES[opcode]]);
    }

    void write_param(stream& io, const Param& p) {
        switch (p.kind) {
            case PK_REG: 
                if (p.data.reg == 0) write(io, "%fp");
                else if (p.data.reg == 1) write(io, "%sp");
                else if (p.data.reg == 2) write(io, "%ip");
                else if (p.data.reg == 3) write(io, "<label>");
                else write(io, "%", p.data.reg - 4);
                break;
            case PK_IMM:
                write(io, p.data.imm);
                break;
            case PK_MEM:
                write(io, "[");
                write_param(io, r(p.data.mem.reg));
                if (p.data.mem.off > 0) write(io, " + ", p.data.mem.off);
                if (p.data.mem.off < 0) write(io, " - ", -p.data.mem.off);
                write(io, "]");
                break;
            case PK_LABEL:
                write(io, name(p.data.label));
            default: break;
        }
    }

    void verify_buffer() {
        if (!target) { // requires that a buffer exist to write to
            fprintf(stderr, "[ERROR] Cannot assemble; no target buffer set.\n");
            exit(1);
        }
        if (target->architecture() != JASMINE) {
            fprintf(stderr, "[ERROR] Target buffer is not targeting jasmine bytecode.");
            exit(1);
        }
    }

    void verify_unary(const Param& lhs) {
        if (lhs.kind == PK_IMM) {
            fprintf(stderr, "[ERROR] Destination parameter cannot be an immediate.\n");
            exit(1);
        }
    }

    void verify_binary(const Param& lhs, const Param& rhs) {
        if (lhs.kind == PK_IMM) {
            fprintf(stderr, "[ERROR] Destination parameter cannot be an immediate.\n");
            exit(1);
        }
    }

    void encode_varuint(bool mem, u64 u) {
        u8 len = 1;
        u8 data[8];
        data[0] = u % 16; 
        u /= 16;
        while (u) len += 1, data[len - 1] = u % 256, u /= 256;
        data[0] |= (mem ? 128 : 0);
        data[0] |= ((len - 1 & 7) << 4);
        for (u32 i = 0; i < len; i ++) target->code().write(data[i]);
    }

    void encode_varint(i64 i) {
        encode_varuint(i < 0, i < 0 ? (u64)-i : (u64)i);
    }

    i64 decode_varint(byte_buffer& buf) {
        u8 first = buf.read<u8>();
        u8 len = (first >> 4 & 7) + 1;
        i64 acc = first & 15;
        for (u32 i = 1; i < len; i ++) {
            acc += i64(buf.read<u8>()) << ((i - 1) * 8 + 4);
        }
        if (first & 128) acc *= -1;
        return acc;
    }

    u64 decode_varuint(byte_buffer& buf) {
        i64 i = decode_varint(buf);
        return i < 0 ? (u64)-i : (u64)i;
    }

    void assemble(const Param& p) {
        switch (p.kind) {
            case PK_REG:
                encode_varuint(false, p.data.reg);
                break;
            case PK_MEM:
                encode_varuint(true, p.data.mem.reg);
                encode_varint(p.data.mem.off);
                break;
            case PK_IMM:
                encode_varint(p.data.imm);
                break;
            case PK_LABEL:
                encode_varuint(true, LABEL);
                for (int i = 0; i < 4; i ++) target->code().write(0);
                target->reference(p.data.label, REL32_LE, -4);
                break;
            case PK_NONE:
                break;
        }
    }

    void assemble(u8 code, OpType type, const Param& lhs, const Param& rhs) {
        if (OP_NAME_TABLES[code].addends) code += OP_NAME_TABLES[code].addends[type];
        target->code().write<u8>(code);
        assemble(lhs);
        assemble(rhs);
    }

    void assemble(u8 code, OpType type, const Param& operand) {
        if (OP_NAME_TABLES[code].addends) code += OP_NAME_TABLES[code].addends[type];
        target->code().write<u8>(code);
        assemble(operand);
    }

    void add(OpType type, const Param& lhs, const Param& rhs) {
        verify_buffer();
        verify_binary(lhs, rhs);
        if (rhs.kind == PK_IMM) assemble(7, type, lhs, rhs);
        else assemble(0, type, lhs, rhs);
    }

    void sub(OpType type, const Param& lhs, const Param& rhs) {
        verify_buffer();
        verify_binary(lhs, rhs);
        if (rhs.kind == PK_IMM) assemble(21, type, lhs, rhs);
        else assemble(14, type, lhs, rhs);
    }

    void mul(OpType type, const Param& lhs, const Param& rhs) {
        verify_buffer();
        verify_binary(lhs, rhs);
        if (rhs.kind == PK_IMM) assemble(39, type, lhs, rhs);
        else assemble(28, type, lhs, rhs);
    }

    void div(OpType type, const Param& lhs, const Param& rhs) {
        verify_buffer();
        verify_binary(lhs, rhs);
        if (rhs.kind == PK_IMM) assemble(61, type, lhs, rhs);
        else assemble(50, type, lhs, rhs);
    }

    void rem(OpType type, const Param& lhs, const Param& rhs) {
        verify_buffer();
        verify_binary(lhs, rhs);
        if (rhs.kind == PK_IMM) assemble(83, type, lhs, rhs);
        else assemble(72, type, lhs, rhs);
    }

    void and_(OpType type, const Param& lhs, const Param& rhs) {
        verify_buffer();
        verify_binary(lhs, rhs);
        if (rhs.kind == PK_IMM) assemble(99, type, lhs, rhs);
        else assemble(94, type, lhs, rhs);
    }

    void or_(OpType type, const Param& lhs, const Param& rhs) {
        verify_buffer();
        verify_binary(lhs, rhs);
        if (rhs.kind == PK_IMM) assemble(109, type, lhs, rhs);
        else assemble(104, type, lhs, rhs);
    }

    void xor_(OpType type, const Param& lhs, const Param& rhs) {
        verify_buffer();
        verify_binary(lhs, rhs);
        if (rhs.kind == PK_IMM) assemble(119, type, lhs, rhs);
        else assemble(114, type, lhs, rhs);
    }

    void not_(OpType type, const Param& operand) {
        verify_buffer();
        verify_unary(operand);
        assemble(124, type, operand);
    }

    void mov(OpType type, const Param& lhs, const Param& rhs) {
        verify_buffer();
        verify_binary(lhs, rhs);
        if (rhs.kind == PK_IMM) assemble(141, type, lhs, rhs);
        else assemble(134, type, lhs, rhs);
    }

    void cmp(OpType type, const Param& lhs, const Param& rhs) {
        verify_buffer();
        verify_binary(lhs, rhs);
        if (rhs.kind == PK_IMM) assemble(155, type, lhs, rhs);
        else assemble(148, type, lhs, rhs);
    }

    void sxt(OpType type, const Param& lhs, const Param& rhs) {
        verify_buffer();
        verify_binary(lhs, rhs);
        assemble(162, type, lhs, rhs);
    }

    void zxt(OpType type, const Param& lhs, const Param& rhs) {
        verify_buffer();
        verify_binary(lhs, rhs);
        assemble(168, type, lhs, rhs);
    }

    void icast(OpType type, const Param& lhs, const Param& rhs) {
        verify_buffer();
        verify_binary(lhs, rhs);
        assemble(171, type, lhs, rhs);
    }

    void f32cast(OpType type, const Param& lhs, const Param& rhs) {
        verify_buffer();
        verify_binary(lhs, rhs);
        assemble(173, type, lhs, rhs);
    }

    void f64cast(OpType type, const Param& lhs, const Param& rhs) {
        verify_buffer();
        verify_binary(lhs, rhs);
        assemble(178, type, lhs, rhs);
    }

    void shl(OpType type, const Param& lhs, const Param& rhs) {
        verify_buffer();
        verify_binary(lhs, rhs);
        if (rhs.kind == PK_IMM) assemble(187, type, lhs, rhs);
        else assemble(183, type, lhs, rhs);
    }

    void shr(OpType type, const Param& lhs, const Param& rhs) {
        verify_buffer();
        verify_binary(lhs, rhs);
        if (rhs.kind == PK_IMM) assemble(199, type, lhs, rhs);
        else assemble(191, type, lhs, rhs);
    }

    void frame(u64 size, u64 num_regs) {
        verify_buffer();
        target->code().write<u8>(207);
        encode_varuint(false, size);
        encode_varuint(false, num_regs);
    }

    void ret() {
        verify_buffer();
        target->code().write<u8>(208);
    }

    void push(OpType type, const Param& operand) {
        verify_buffer();
        if (operand.kind == PK_IMM) assemble(216, type, operand);
        else assemble(209, type, operand);
    }

    void pop(OpType type, const Param& operand) {
        verify_buffer();
        assemble(223, type, operand);
    }

    void call(OpType type, const Param& dest, Symbol symbol) {
        verify_buffer();
        verify_unary(dest);
        assemble(230, type, dest);
        for (int i = 0; i < 4; i ++) target->code().write<u8>(0);
        target->reference(symbol, RefType::REL32_LE, -4);
    }

    void rcall(OpType type, const Param& dest, const Param& func) {
        verify_buffer();
        verify_binary(dest, func);
        assemble(237, type, dest, func);
    }

    void jl(Symbol symbol) {
        verify_buffer();
        target->code().write<u8>(244);
        for (int i = 0; i < 4; i ++) target->code().write<u8>(0);
        target->reference(symbol, RefType::REL32_LE, -4);
    }

    void jle(Symbol symbol) {
        verify_buffer();
        target->code().write<u8>(245);
        for (int i = 0; i < 4; i ++) target->code().write<u8>(0);
        target->reference(symbol, RefType::REL32_LE, -4);
    }

    void jg(Symbol symbol) {
        verify_buffer();
        target->code().write<u8>(246);
        for (int i = 0; i < 4; i ++) target->code().write<u8>(0);
        target->reference(symbol, RefType::REL32_LE, -4);
    }

    void jge(Symbol symbol) {
        verify_buffer();
        target->code().write<u8>(247);
        for (int i = 0; i < 4; i ++) target->code().write<u8>(0);
        target->reference(symbol, RefType::REL32_LE, -4);
    }

    void jeq(Symbol symbol) {
        verify_buffer();
        target->code().write<u8>(248);
        for (int i = 0; i < 4; i ++) target->code().write<u8>(0);
        target->reference(symbol, RefType::REL32_LE, -4);
    }

    void jne(Symbol symbol) {
        verify_buffer();
        target->code().write<u8>(249);
        for (int i = 0; i < 4; i ++) target->code().write<u8>(0);
        target->reference(symbol, RefType::REL32_LE, -4);
    }

    void jump(Symbol symbol) {
        verify_buffer();
        target->code().write<u8>(250);
        for (int i = 0; i < 4; i ++) target->code().write<u8>(0);
        target->reference(symbol, RefType::REL32_LE, -4);
    }
    
    void arg(OpType type, const Param& operand) {
        verify_buffer();
        if (operand.kind != PK_REG) {
            fprintf(stderr, "Operand to 'arg' instruction was not a register.\n");
            exit(1);
        }
        assemble(251, type, operand);
    }

    void label(Symbol symbol) {
        target->define(symbol);
    }

    void label(const char* symbol) {
        label(local(symbol));
    }

    Param read_immediate(byte_buffer& code) {
        return imm(decode_varint(code));
    }

    Param noparam() {
        Param p;
        p.kind = PK_NONE;
        return p;
    }

    Param read_param(byte_buffer& code, const Object& obj) {
        u8 first = code.peek();
        if (first & 128) { // mem
            u64 reg = decode_varuint(code);
            if (reg == LABEL) {
                for (int i = 0; i < 4; i ++) code.read();
                auto it = obj.references().find(obj.code().size() - code.size());
                if (it == obj.references().end()) {
                    fprintf(stderr, "[ERROR] Could not resolve symbol in label parameter.");
                    exit(1);
                }
                return l(it->second.symbol);
            }
            else {
                Param p;
                p.kind = PK_MEM;
                p.data.mem = { reg, decode_varint(code) };
                return p;
            }
        }
        else { // reg
            u64 reg = decode_varuint(code);
            Param p;
            p.kind = PK_REG;
            p.data.reg = reg;
            return p;
        }
    }

    struct Insn {
        optional<Symbol> label;
        u8 opcode;
        Param left, right;
        u64 lconst, rconst;
        Symbol calldst;

        Insn(byte_buffer& code, const map<u64, Symbol>& rdefs, Object& obj) {
            u64 pos = obj.code().size() - code.size();
            auto it = rdefs.find(pos);
            if (it != rdefs.end()) label = some<Symbol>(it->second);
            else label = none<Symbol>();
            opcode = code.read<u8>();
            if (OP_BASES[opcode] == 207) { // frame
                lconst = decode_varuint(code);
                rconst = decode_varuint(code);
            }
            else if (OP_BASES[opcode] == 230) {
                left = read_param(code, obj);
                for (int i = 0; i < 4; i ++) code.read();
                auto it = obj.references().find(obj.code().size() - code.size());
                if (it == obj.references().end()) {
                    fprintf(stderr, "Could not resolve destination label in call instruction.\n");
                    exit(1);
                }
                calldst = it->second.symbol;
            }
            else if (OP_BASES[opcode] >= 244 && OP_BASES[opcode] <= 250) { // call jump
                for (int i = 0; i < 4; i ++) code.read();
                auto it = obj.references().find(obj.code().size() - code.size());
                if (it == obj.references().end()) {
                    fprintf(stderr, "Could not resolve destination label in call instruction.\n");
                    exit(1);
                }
                calldst = it->second.symbol;
            }
            else {
                u8 arity = OP_ARITIES[opcode];
                bool immediate = OP_IMMEDIATES[opcode];
                if (arity == 0) left = noparam(), right = noparam();
                else if (arity == 1) 
                    left = immediate ? read_immediate(code) : read_param(code, obj), right = noparam();
                else if (arity == 2) 
                    left = read_param(code, obj), right = immediate ? read_immediate(code) : read_param(code, obj);
            }
        }

        Insn(u8 opcode_in): 
            label(none<Symbol>()), opcode(opcode_in), left(noparam()), right(noparam()) {}
        Insn(u8 opcode_in, const Param& operand): 
            label(none<Symbol>()), opcode(opcode_in), left(operand), right(noparam()) {}
        Insn(u8 opcode_in, const Param& left_in, const Param& right_in): 
            label(none<Symbol>()), opcode(opcode_in), left(left_in), right(right_in) {}

        void format(stream& io) const {
            if (label) write(io, name(*label), ":");
            write(io, "\t");
            write_opcode(io, opcode);
            if (OP_BASES[opcode] == 207) write(io, " ", lconst, ", ", rconst, "\n");
            else if (OP_BASES[opcode] == 230) {
                write(io, " ");
                write_param(io, left);
                write(io, ", ", name(calldst), "\n");
            }
            else if (OP_BASES[opcode] >= 244 && OP_BASES[opcode] <= 250) {
                write(io, " ", name(calldst), "\n");
            }
            else {
                write(io, " ");
                if (left.kind != PK_NONE) write_param(io, left);
                if (right.kind != PK_NONE) write(io, ", "), write_param(io, right);
                write(io, "\n");
            }
        }

        bool inout(set<Reg>& live) const {
            u32 init = live.size();
            if (left.kind == PK_REG) {
                if (OP_DESTRUCTIVE[opcode]) live.erase(left.data.reg);
                else live.insert(left.data.reg);
            }
            else if (left.kind == PK_MEM) live.insert(left.data.mem.reg);

            if (right.kind == PK_REG) live.insert(right.data.reg);
            else if (right.kind == PK_MEM) live.insert(right.data.mem.reg);
            
            return live.size() != init;
        }

        void clobber(set<u64>& clobbers, Architecture arch) const {
            switch (arch) {
                case X86_64: {
                    if (left.kind == PK_MEM && right.kind == PK_MEM) clobbers.insert(x64::RAX);
                    switch (OP_BASES[opcode]) {
                        case 50:
                        case 61:
                        case 72:
                        case 83: // div and rem
                            clobbers.insert(x64::RAX);
                            clobbers.insert(x64::RDX);
                            break;
                        case 230:
                        case 237: // call
                            clobbers.insert(x64::RAX);
                            break;
                    }
                    break;
                }
                default:
                    break;
            }
        }
    };

    vector<Insn> decode(Object& obj) {
        byte_buffer code = obj.code();
        map<u64, Symbol> syms;
        for (const auto& [k, v] : obj.symbols()) {
            syms[v] = k;
        }
        vector<Insn> insns;
        while (code.size()) insns.push(Insn(code, syms, obj));
        return insns;
    }

    void disassemble(Object& obj, stream& out) {
        auto insns = decode(obj);
        for (const Insn& insn : insns) insn.format(out);
    }

    struct Location {
        bool is_reg;
        u32 val;
    };

    struct LiveRange {
        Reg reg;
        u64 first, last;
        Location loc;
        bool arg_hint = false;

        LiveRange(Reg reg_in, u64 f, u64 l):
            reg(reg_in), first(f), last(l) {}
    };

    struct Function {
        u64 first, last;
        u64 num_regs, stack;
        vector<LiveRange> ranges;

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
            if (insns[i].opcode == 207) { // frame
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
                    fn->stack = insns[i].lconst, fn->num_regs = insns[i].rconst;
                }
            }
            else if (insns[i].opcode == 208 && fn) { // ret
                fn->last = i;
            }
        }
        if (fn && fn->first != fn->last) functions.push(*fn);

        return functions;
    }

    bool unify(set<Reg>& out, set<Reg>& in) {
        u32 init = out.size();
        for (Reg r : in) out.insert(r);
        return init != out.size();
    }

    void compute_ranges(Function& function, const vector<Insn>& insns) {
        // for (u64 i = 0; i < function.num_regs; i ++) function.ranges.push({});
        vector<pair<set<Reg>, set<Reg>>> sets;
        map<Symbol, u64> local_syms;
        for (u64 i = function.first; i <= function.last; i ++) {
            sets.push({});
            if (insns[i].label) local_syms.put(*insns[i].label, i);
        }

        sets.push({});
        bool changed = true;
        while (changed) {
            changed = false;
            for (i64 i = i64(function.last); i >= i64(function.first); i --) {
                const Insn& in = insns[i];
                u32 outinit = sets[i - function.first].second.size();
                if (in.opcode == 250) { // unconditional jump
                    auto it = local_syms.find(in.calldst);
                    if (it == local_syms.end()) {
                        fprintf(stderr, "[ERROR] Tried to jump to label outside of current function.\n");
                        exit(1);
                    }

                    unify(sets[i - function.first].second, sets[it->second - function.first].first);
                }
                else if (in.opcode >= 244 && in.opcode < 250) { // conditional jump
                    auto it = local_syms.find(in.calldst);
                    if (it == local_syms.end()) {
                        fprintf(stderr, "[ERROR] Tried to jump to label outside of current function.\n");
                        exit(1);
                    }
                    
                    unify(sets[i - function.first].second, sets[it->second - function.first].first);
                    unify(sets[i - function.first].second, sets[i + 1 - function.first].first);
                }
                else {
                    unify(sets[i - function.first].second, sets[i + 1 - function.first].first);
                }
                changed = changed || sets[i - function.first].second.size() != outinit;

                u32 init = sets[i - function.first].first.size();
                unify(sets[i - function.first].first, sets[i - function.first].second);
                insns[i].inout(sets[i - function.first].first);
                changed = changed || sets[i - function.first].first.size() != init;
            }
        }

        // for (u64 i = function.first; i <= function.last; i ++) {
        //     print("{");
        //     bool first = true;
        //     for (Reg r : sets[i - function.first].first) {
        //         Param p;
        //         p.kind = PK_REG;
        //         p.data.reg = r;
        //         print(first ? "" : " ");
        //         write_param(_stdout, p);
        //         first = false;
        //     }
        //     print("} => {");
        //     first = true;
        //     for (Reg r : sets[i - function.first].second) {
        //         Param p;
        //         p.kind = PK_REG;
        //         p.data.reg = r;
        //         print(first ? "" : " ");
        //         write_param(_stdout, p);
        //         first = false;
        //     }
        //     print("}\t");
        //     insns[i].format(_stdout);
        // }
        // println("");

        vector<optional<LiveRange>> ranges;
        for (u64 i = 0; i < function.num_regs; i ++) ranges.push(none<LiveRange>());
        for (u64 i = function.first; i <= function.last; i ++) {
            const auto& live = sets[i - function.first];
            for (Reg r = 0; r < function.num_regs; r ++) {
                if (!ranges[r] && live.second.contains(r)) { // new definition
                    ranges[r] = some<LiveRange>(r, i, i);
                    if (OP_BASES[insns[i].opcode] == 251) ranges[r]->arg_hint = true; // arg
                }
                else if (ranges[r] && !live.first.contains(r)) { // range ended last instruction
                    ranges[r]->last = i - 1;
                    function.ranges.push(*ranges[r]);
                    ranges[r] = none<LiveRange>();
                }
                
                if (ranges[r] && !live.second.contains(r)) { // range ends this instruction
                    ranges[r]->last = i;
                    function.ranges.push(*ranges[r]);
                    ranges[r] = none<LiveRange>();
                }
            }
        }
        for (auto& r : ranges) if (r) r->last = function.last, function.ranges.push(*r); // add any ranges we didn't catch

        for (const LiveRange& r : function.ranges) {
            Param p;
            p.kind = PK_REG;
            p.data.reg = r.reg;
            print("register ");
            write_param(_stdout, p);
            println(" live from ", r.first, " to ", r.last);
        }
        println("");
    }

    void populate_arg_registers(Architecture arch, vector<u32>& regs) {
        switch (arch) {
            case X86_64: {
                using namespace x64;
                regs.push(x64::RDI); // just sysv for now...
                regs.push(x64::RSI);
                regs.push(x64::RDX);
                regs.push(x64::RCX);
                regs.push(x64::R8);
                regs.push(x64::R9);
                break;
            }
            default:
                fprintf(stderr, "Unsupported architecture.\n");
                exit(1);
                break;
        }
    }

    u32 return_register(Architecture arch) {
        switch (arch) {
            case X86_64: {
                using namespace x64;
                return x64::RAX;
            }
            default:
                fprintf(stderr, "Unsupported architecture.\n");
                exit(1);
                return 0;
        }
    }

    void populate_registers(Architecture arch, vector<u32>& regs) {
        switch (arch) {
            case X86_64: {
                using namespace x64;
                for (u32 i = 0; i < 16; i ++) regs.push(i); // rax - r15
                break;
            }
            default:
                fprintf(stderr, "Unsupported architecture.\n");
                exit(1);
                break;
        }
    }

    void allocate_registers(Function& f, const vector<Insn>& insns, Architecture arch)  {
        vector<u32> args, regs;
        set<u64> clobbers;
        populate_arg_registers(arch, args);
        populate_registers(arch, regs);

        u32 cur_arg = 0;

        vector<u32> available;
        for (i64 i = i64(regs.size()) - 1; i >= 0; i --) {
            available.push(regs[i]);
        }
        map<u64, LiveRange*> mappings;
        for (u32 r : regs) mappings.put(r, nullptr);

        vector<vector<LiveRange*>> starts_by_insn, ends_by_insn;
        for (u64 i = f.first; i <= f.last; i ++) {
            starts_by_insn.push({});
            ends_by_insn.push({});
        }
        for (LiveRange& r : f.ranges) {
            starts_by_insn[r.first - f.first].push(&r);
            ends_by_insn[r.last - f.first].push(&r);
        }

        for (u64 i = f.first; i <= f.last; i ++) {
            for (LiveRange* r : ends_by_insn[i - f.first]) {
                if (r->loc.is_reg) {
                    available.push(r->loc.val);
                    mappings[r->loc.val] = nullptr;
                }
            }
            for (LiveRange* r : starts_by_insn[i - f.first]) {
                if (r->arg_hint) {
                    r->loc = { true, args[cur_arg] };
                    bool found = false;
                    for (u32 i = 0; i < available.size(); i ++) { // remove arg register
                        if (available[i] == r->loc.val) found = true;
                        if (found && i < available.size() - 1) available[i] = available[i + 1];
                    }
                    if (found) available.pop();
                    LiveRange* existing = mappings[r->loc.val];
                    mappings.put(r->loc.val, r);
                    cur_arg ++;

                    if (existing) r = existing;
                    else continue;
                }
                if (available.size()) {
                    r->loc = { true, available.back() };
                    available.pop();
                    mappings.put(r->loc.val, r);
                }
                else r->loc = { false, u32(f.stack += 8ul) }; // spill
            }

            clobbers.clear();
            insns[i].clobber(clobbers, arch);
            for (u64 c = 0; c < clobbers.size(); c ++) {
                LiveRange* existing = mappings[c];
                if (existing) {
                    if (available.size()) {
                        r->loc = { true, available.back() };
                        available.pop();
                        mappings.put(r->loc.val, r);
                    }
                    else r->loc = { false, u32(f.stack += 8ul) }; // spill
                    mappings.put(r->loc.val, nullptr);
                }
            }
        }

        for (const LiveRange& r : f.ranges) {
            Param p;
            p.kind = PK_REG;
            p.data.reg = r.reg;
            print("register ");
            write_param(_stdout, p);
            print(" from ", r.first, " to ", r.last);
            if (r.loc.is_reg) println(" allocated to register %", x64::REGISTER_NAMES[r.loc.val]);
            else println(" spilled to [%rbp - ", r.loc.val, "]");
        }
        println("");
    }

    Object jasmine_to_x86(Object& obj) {
        vector<Insn> insns = decode(obj);
        vector<Function> functions = find_functions(insns);
        for (Function& f : functions) compute_ranges(f, insns);
        for (Function& f : functions) allocate_registers(f, X86_64);

        return Object(X86_64);
    }
}