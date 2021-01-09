#ifndef BASIL_OPS_H
#define BASIL_OPS_H

#include "util/defs.h"
#include "util/vec.h"
#include "ir.h"
#include "jasmine/target.h"

namespace basil {
    Architecture arch();
    void set_arch(Architecture arch);

    u32 arg_register(u32 i);
    u32 clobber_register(u32 i);
    vector<u32> allocatable_registers();

    void store(const Location& dest, const Location& src, u32 offset);
    void load(const Location& dest, const Location& src, u32 offset);
    void lea(const Location& dest, const Location& src);
    void move(const Location& dest, const Location& src);
    void add(const Location& dest, const Location& lhs, const Location& rhs);
    void sub(const Location& dest, const Location& lhs, const Location& rhs);
    void mul(const Location& dest, const Location& lhs, const Location& rhs);
    void div(const Location& dest, const Location& lhs, const Location& rhs);
    void rem(const Location& dest, const Location& lhs, const Location& rhs);
    void neg(const Location& dest, const Location& src);
    void and_op(const Location& dest, const Location& lhs, const Location& rhs);
    void or_op(const Location& dest, const Location& lhs, const Location& rhs);
    void xor_op(const Location& dest, const Location& lhs, const Location& rhs);
    void not_op(const Location& dest, const Location& src);
    void equal(const Location& dest, const Location& lhs, const Location& rhs);
    void not_equal(const Location& dest, const Location& lhs, const Location& rhs);
    void less(const Location& dest, const Location& lhs, const Location& rhs);
    void less_equal(const Location& dest, const Location& lhs, const Location& rhs);
    void greater(const Location& dest, const Location& lhs, const Location& rhs);
    void greater_equal(const Location& dest, const Location& lhs, const Location& rhs);
    void jump(const Location& dest);
    void jump_if_zero(const Location& dest, const Location& cond);
    void set_arg(u32 i, const Location& src);
    void get_arg(const Location& dest, u32 i);
    void call(const Location& dest, const Location& func);
    void global_label(const string& name);
    void local_label(const string& name);
    void push(const Location& src);
    void pop(const Location& dest);
    void open_frame(u32 size);
    void close_frame(u32 size);
}

#endif