#include "ir.h"
#include "ops.h"
#include "util/hash.h"

namespace basil {
    using namespace x64;

    Location loc_none() {
        Location loc;
        loc.type = LOC_NONE;
        return loc;
    }

    Location loc_immediate(i64 i) {
        Location loc;
        loc.type = LOC_IMMEDIATE;
        loc.immediate = i;
        return loc;
    }

    Location loc_label(const string& label) {
        Location loc;
        loc.type = LOC_LABEL;
        loc.label_index = find_label(label);
        return loc;
    }

    Insn::Insn(InsnType kind) : _kind(kind), _loc(loc_none()), _func(nullptr) {}

    Insn::~Insn() {
        //
    }

    InsnType Insn::kind() const {
        return _kind;
    }

    void Insn::setfunc(Function* func) {
        _func = func;
    }

    const Location& Insn::loc() const {
        return _loc;
    }

    Location& Insn::loc() {
        if (_loc.type == LOC_NONE) _loc = lazy_loc();
        return _loc;
    }

    vector<Insn*>& Insn::succ() {
        return _succ;
    }

    const vector<Insn*>& Insn::succ() const {
        return _succ;
    }

    set<u32>& Insn::in() {
        return _in;
    }

    set<u32>& Insn::out() {
        return _out;
    }

    const set<u32>& Insn::in() const {
        return _in;
    }

    const set<u32>& Insn::out() const {
        return _out;
    }

    static u32 anonymous_locals = 0;
    static u32 anonymous_labels = 0;
    static vector<string> all_labels;
    static map<string, u32> label_map;
    static vector<LocalInfo> all_locals;
    static vector<ConstantInfo> all_constants;

    u32 find_label(const string& label) {
        auto it = label_map.find(label);
        if (it == label_map.end()) return add_label(label);
        return it->second;
    }

    u32 add_label(const string& label) {
        all_labels.push(label);
        label_map[label] = all_labels.size() - 1;
        return all_labels.size() - 1;
    }

    u32 next_label() {
        buffer b;
        write(b, ".L", anonymous_labels++);
        string s;
        read(b, s);
        all_labels.push(s);
        label_map[s] = all_labels.size() - 1;
        return all_labels.size() - 1;
    }

    Location next_local(const Type* t) {
        buffer b;
        write(b, ".t", anonymous_locals++);
        string s;
        read(b, s);
        all_locals.push({s, 0, t, -1, 0});

        Location loc;
        loc.type = LOC_LOCAL;
        loc.local_index = all_locals.size() - 1;
        return loc;
    }

    Location const_loc(u32 label, const string& constant) {
        ConstantInfo info;
        info.type = STRING;
        info.name = all_labels[label];
        for (u32 i = 0; i < constant.size(); i++) info.data.push(constant[i]);
        info.data.push('\0');

        all_constants.push(info);
        Location loc;
        loc.type = LOC_CONSTANT;
        loc.constant_index = all_constants.size() - 1;
        return loc;
    }

    Symbol symbol_for_label(u32 label, SymbolLinkage type) {
        return type == GLOBAL_SYMBOL ? global((const char*)all_labels[label].raw())
                                     : local((const char*)all_labels[label].raw());
    }

    void emit_constants(Object& object) {
        using namespace x64;
        writeto(object);
        for (const ConstantInfo& info : all_constants) {
            global_label(info.name);
            for (u8 b : info.data) object.code().write(b);
        }
    }

    const Type* ssa_type(const Location& loc) {
        switch (loc.type) {
            case LOC_NONE: return VOID;
            case LOC_LOCAL: return all_locals[loc.local_index].type;
            case LOC_CONSTANT: return all_constants[loc.constant_index].type;
            case LOC_IMMEDIATE: return INT; // close enough at this stage
            case LOC_LABEL: return INT;     // ...close enough :p
            case LOC_REGISTER: return INT;
        }
    }

    const u8 BINARY_INSN = 128; // BINARY_INSN kind flag

    i64 immediate_of(const Location& loc) {
        return loc.immediate;
    }

    const string& label_of(const Location& loc) {
        return all_labels[loc.label_index];
    }

    LocalInfo& local_of(const Location& loc) {
        return all_locals[loc.local_index];
    }

    ConstantInfo& constant_of(const Location& loc) {
        return all_constants[loc.constant_index];
    }

    Location loc_register(u32 reg) {
        Location loc;
        loc.type = LOC_REGISTER;
        loc.reg = reg;
        return loc;
    }

    void Function::liveness() {
        bool changed = true;
        while (changed) {
            changed = false;
            for (i64 i = i64(_insns.size()) - 1; i >= 0; i--) {
                Insn* in = _insns[i];
                u32 initial_in = in->in().size(), initial_out = in->out().size();
                for (const Insn* succ : in->succ()) {
                    for (u32 l : succ->in()) in->out().insert(l);
                }
                for (u32 l : in->out()) in->in().insert(l);
                in->liveout();
                if (in->in().size() != initial_in || in->out().size() != initial_out) changed = true;
            }
        }

        // for (i64 i = 0; i < _insns.size(); i ++) {
        // 	print(_insns[i], " { ");
        // 	for (auto u : _insns[i]->in()) print(all_locals[u].name, " ");
        // 	print("} -> { ");
        // 	for (auto u : _insns[i]->out()) print(all_locals[u].name, " ");
        // 	println("}");
        // }
    }

    void Function::to_registers() {
        map<u32, pair<i64, i64>> ranges;
        for (i64 i = 0; i < _insns.size(); i++) {
            for (u32 l : _insns[i]->out()) {
                if (_insns[i]->in().find(l) == _insns[i]->in().end()) ranges.put(l, {i, -1});
            }
            for (u32 l : _insns[i]->in()) {
                if (_insns[i]->out().find(l) == _insns[i]->out().end()) ranges[l].second = i;
            }
        }

        // for (auto& p : ranges) {
        // 	println(all_locals[p.first].name, " live between ", p.second.first, " and ", p.second.second);
        // }

        vector<vector<u32>> gens, kills;
        for (i64 i = 0; i < _insns.size(); i++) gens.push({}), kills.push({});
        for (auto& p : ranges) {
            gens[p.second.first].push(p.first);
            kills[p.second.second].push(p.first);
        }

        // for (i64 i = 0; i < _insns.size(); i ++) {
        // 	print(_insns[i], ": ");
        // 	for (u32 l : gens[i]) print("GEN-", all_locals[l].name, " ");
        // 	for (u32 l : kills[i]) print("KILL-", all_locals[l].name, " ");
        // 	println("");
        // }

        vector<u32> reg_stack = allocatable_registers();
        for (i64 i = 0; i < _insns.size(); i++) {
            for (u32 g : gens[i]) {
                if (all_locals[g].reg >= 0)
                    ; //
                else if (reg_stack.size() == 0)
                    all_locals[g].offset = -(_stack += 8); // spill
                else
                    all_locals[g].reg = reg_stack.back(), reg_stack.pop();
            }
            for (u32 k : kills[i]) {
                if (all_locals[k].reg != -1) reg_stack.push(all_locals[k].reg);
            }
        }

        for (u32 i = 0; i < all_locals.size(); i++) {
            if (all_locals[i].reg == -1 && all_locals[i].offset == 0)
                all_locals[i].reg = RAX; // clobber RAX for dead code (for now)
        }
    }

    Function::Function(u32 label) : _stack(0), _label(label), _end(next_label()) {
        _ret = loc_register(RAX);
    }

    Function::Function(const string& label) : Function(add_label(label)) {}

    Location Function::create_local(const Type* t) {
        Location l = basil::next_local(t);
        _locals.push(l);
        return l;
    }

    Function::~Function() {
        for (Insn* i : _insns) delete i;
        for (Function* f : _fns) delete f;
    }

    void Function::place_label(u32 label) {
        _labels[label] = _insns.size();
    }

    Function& Function::create_function() {
        _fns.push(new Function(next_label()));
        return *_fns.back();
    }

    Function& Function::create_function(const string& name) {
        _fns.push(new Function(name));
        return *_fns.back();
    }

    Location Function::create_local(const string& name, const Type* t) {
        LocalInfo info = {name, 0, t, -1, 0};
        all_locals.push(info);

        Location loc;
        loc.type = LOC_LOCAL;
        loc.local_index = all_locals.size() - 1;
        _locals.push(loc);
        return loc;
    }

    Location Function::add(Insn* insn) {
        insn->setfunc(this);
        if (_insns.size() > 0) _insns.back()->succ().push(insn);
        _insns.push(insn);
        return insn->loc();
    }

    u32 Function::label() const {
        return _label;
    }

    void Function::allocate() {
        for (Function* fn : _fns) fn->allocate();
        liveness();
        to_registers();
    }

    void Function::emit(Object& obj, bool exit) {
        for (Function* fn : _fns) fn->emit(obj);

        writeto(obj);
        global_label(all_labels[_label]);
        open_frame(_stack);
        for (Insn* i : _insns) i->emit();
        local_label(all_labels[_end]);
        if (exit && all_labels[_label] == "_start") {
            mov(x64::r64(x64::RAX), x64::imm64(60));
            mov(x64::r64(x64::RDI), x64::imm64(0));
            syscall();
        }
        close_frame(_stack);
    }

    void Function::format(stream& io) const {
        for (Function* fn : _fns) fn->format(io);
        writeln(io, all_labels[_label], ":");
        for (Insn* i : _insns) writeln(io, "    ", i);
    }

    u32 Function::end_label() const {
        return _end;
    }

    const Location& Function::ret_loc() const {
        return _ret;
    }

    Insn* Function::last() const {
        if (_insns.size() == 0) return nullptr;
        return _insns.back();
    }

    Location LoadInsn::lazy_loc() {
        return _func->create_local(ssa_type(_src));
    }

    LoadInsn::LoadInsn(Location src) : Insn(LOAD_INSN), _src(src) {}

    void LoadInsn::emit() {
        move(_loc, _src);
    }

    void LoadInsn::format(stream& io) const {
        write(io, _loc, " = ", _src);
    }

    void LoadInsn::liveout() {
        if (_src.type == LOC_LOCAL) in().insert(_src.local_index);
        if (_loc.type == LOC_LOCAL) in().erase(_loc.local_index);
    }

    Location StoreInsn::lazy_loc() {
        return _dest;
    }

    StoreInsn::StoreInsn(Location dest, Location src) : Insn(STORE_INSN), _dest(dest), _src(src) {}

    void StoreInsn::emit() {
        move(_dest, _src);
    }

    void StoreInsn::format(stream& io) const {
        write(io, _loc, " = ", _src);
    }

    void StoreInsn::liveout() {
        if (_src.type == LOC_LOCAL) in().insert(_src.local_index);
        if (_dest.type == LOC_LOCAL) in().erase(_dest.local_index);
    }

    LoadPtrInsn::LoadPtrInsn(Location src, const Type* t, i32 offset)
        : Insn(LOAD_PTR_INSN), _src(src), _type(t), _offset(offset) {}

    Location LoadPtrInsn::lazy_loc() {
        return _func->create_local(_type);
    }

    void LoadPtrInsn::emit() {
        load(_loc, _src, _offset);
    }

    void LoadPtrInsn::format(stream& io) const {
        write(io, _loc, " = *", _src);
    }

    void LoadPtrInsn::liveout() {
        if (_src.type == LOC_LOCAL) in().insert(_src.local_index);
        if (_loc.type == LOC_LOCAL) in().erase(_loc.local_index);
    }

    StorePtrInsn::StorePtrInsn(Location dest, Location src, i32 offset)
        : Insn(STORE_PTR_INSN), _dest(dest), _src(src), _offset(offset) {}

    Location StorePtrInsn::lazy_loc() {
        return loc_none();
    }

    void StorePtrInsn::emit() {
        store(_dest, _src, _offset);
    }

    void StorePtrInsn::format(stream& io) const {
        write(io, "*", _dest, " = ", _src);
    }

    void StorePtrInsn::liveout() {
        if (_src.type == LOC_LOCAL) in().insert(_src.local_index);
        if (_dest.type == LOC_LOCAL) in().insert(_dest.local_index);
    }

    AddressInsn::AddressInsn(Location src, const Type* t) : Insn(ADDRESS_INSN), _src(src), _type(t) {}

    Location AddressInsn::lazy_loc() {
        return _func->create_local(_type);
    }

    void AddressInsn::emit() {
        lea(_loc, _src);
    }

    void AddressInsn::format(stream& io) const {
        write(io, _loc, " = &", _src);
    }

    void AddressInsn::liveout() {
        if (_src.type == LOC_LOCAL) in().insert(_src.local_index);
        if (_loc.type == LOC_LOCAL) in().erase(_loc.local_index);
    }

    BinaryInsn::BinaryInsn(InsnType kind, const char* name, Location left, Location right)
        : Insn(kind), _name(name), _left(left), _right(right) {}

    void BinaryInsn::format(stream& io) const {
        write(io, _loc, " = ", _left, " ", _name, " ", _right);
    }

    void BinaryInsn::liveout() {
        if (_left.type == LOC_LOCAL) in().insert(_left.local_index);
        if (_right.type == LOC_LOCAL) in().insert(_right.local_index);
        if (_loc.type == LOC_LOCAL) in().erase(_loc.local_index);
    }

    Location BinaryMathInsn::lazy_loc() {
        return _func->create_local(ssa_type(_left));
    }

    BinaryMathInsn::BinaryMathInsn(InsnType kind, const char* name, Location left, Location right)
        : BinaryInsn(kind, name, left, right) {}

    AddInsn::AddInsn(Location left, Location right) : BinaryMathInsn(ADD_INSN, "+", left, right) {}

    void AddInsn::emit() {
        add(_loc, _left, _right);
    }

    SubInsn::SubInsn(Location left, Location right) : BinaryMathInsn(SUB_INSN, "-", left, right) {}

    void SubInsn::emit() {
        sub(_loc, _left, _right);
    }

    MulInsn::MulInsn(Location left, Location right) : BinaryMathInsn(MUL_INSN, "*", left, right) {}

    void MulInsn::emit() {
        mul(_loc, _left, _right);
    }

    DivInsn::DivInsn(Location left, Location right) : BinaryMathInsn(DIV_INSN, "/", left, right) {}

    void DivInsn::emit() {
        div(_loc, _left, _right);
    }

    RemInsn::RemInsn(Location left, Location right) : BinaryMathInsn(REM_INSN, "%", left, right) {}

    void RemInsn::emit() {
        rem(_loc, _left, _right);
    }

    BinaryLogicInsn::BinaryLogicInsn(InsnType kind, const char* name, Location left, Location right)
        : BinaryInsn(kind, name, left, right) {}

    Location BinaryLogicInsn::lazy_loc() {
        return _func->create_local(BOOL);
    }

    AndInsn::AndInsn(Location left, Location right) : BinaryLogicInsn(AND_INSN, "and", left, right) {}

    void AndInsn::emit() {
        and_op(_loc, _left, _right);
    }

    OrInsn::OrInsn(Location left, Location right) : BinaryLogicInsn(OR_INSN, "or", left, right) {}

    void OrInsn::emit() {
        or_op(_loc, _left, _right);
    }

    XorInsn::XorInsn(Location left, Location right) : BinaryLogicInsn(XOR_INSN, "xor", left, right) {}

    void XorInsn::emit() {
        xor_op(_loc, _left, _right);
    }

    NotInsn::NotInsn(Location src) : Insn(NOT_INSN), _src(src) {}

    Location NotInsn::lazy_loc() {
        return _func->create_local(BOOL);
    }

    void NotInsn::emit() {
        not_op(_loc, _src);
    }

    void NotInsn::format(stream& io) const {
        write(io, _loc, " = ", "not ", _src);
    }

    void NotInsn::liveout() {
        if (_src.type == LOC_LOCAL) in().insert(_src.local_index);
        if (_loc.type == LOC_LOCAL) in().erase(_loc.local_index);
    }

    BinaryEqualityInsn::BinaryEqualityInsn(InsnType kind, const char* name, Location left, Location right)
        : BinaryInsn(kind, name, left, right) {}

    Location BinaryEqualityInsn::lazy_loc() {
        return _func->create_local(BOOL);
    }

    EqualInsn::EqualInsn(Location left, Location right) : BinaryEqualityInsn(EQ_INSN, "==", left, right) {}

    void EqualInsn::emit() {
        equal(_loc, _left, _right);
    }

    InequalInsn::InequalInsn(Location left, Location right) : BinaryEqualityInsn(NOT_EQ_INSN, "!=", left, right) {}

    void InequalInsn::emit() {
        not_equal(_loc, _left, _right);
    }

    BinaryRelationInsn::BinaryRelationInsn(InsnType kind, const char* name, Location left, Location right)
        : BinaryInsn(kind, name, left, right) {}

    Location BinaryRelationInsn::lazy_loc() {
        return _func->create_local(BOOL);
    }

    LessInsn::LessInsn(Location left, Location right) : BinaryRelationInsn(LESS_INSN, "<", left, right) {}

    void LessInsn::emit() {
        less(_loc, _left, _right);
    }

    LessEqualInsn::LessEqualInsn(Location left, Location right) : BinaryRelationInsn(LESS_EQ_INSN, "<=", left, right) {}

    void LessEqualInsn::emit() {
        less_equal(_loc, _left, _right);
    }

    GreaterInsn::GreaterInsn(Location left, Location right) : BinaryRelationInsn(GREATER_INSN, ">", left, right) {}

    void GreaterInsn::emit() {
        greater(_loc, _left, _right);
    }

    GreaterEqualInsn::GreaterEqualInsn(Location left, Location right)
        : BinaryRelationInsn(GREATER_EQ_INSN, ">=", left, right) {}

    void GreaterEqualInsn::emit() {
        greater_equal(_loc, _left, _right);
    }

    Location RetInsn::lazy_loc() {
        return loc_none();
    }

    RetInsn::RetInsn(Location src) : Insn(RET_INSN), _src(src) {}

    void RetInsn::emit() {
        move(_func->ret_loc(), _src);
        if (this != _func->last()) {
            Location loc;
            loc.type = LOC_LABEL;
            loc.label_index = _func->end_label();
            jump(loc);
        }
    }

    void RetInsn::format(stream& io) const {
        write(io, "return ", _src);
    }

    void RetInsn::liveout() {
        while (in().size()) in().erase(*in().begin());    // all other variables die here
        while (out().size()) out().erase(*out().begin()); // all other variables die here
        if (_src.type == LOC_LOCAL) in().insert(_src.local_index);
    }

    LoadArgumentInsn::LoadArgumentInsn(u32 index, const Type* type) : Insn(LOAD_ARG_INSN), _index(index), _type(type) {}

    Location LoadArgumentInsn::lazy_loc() {
        Location l = _func->create_local(_type);
        return l;
    }

    void LoadArgumentInsn::emit() {
        get_arg(_loc, _index);
    }

    void LoadArgumentInsn::format(stream& io) const {
        write(io, _loc, " = $", _index);
    }

    void LoadArgumentInsn::liveout() {
        if (_loc.type == LOC_LOCAL) in().erase(_loc.local_index);
    }

    CallInsn::CallInsn(Location fn, const vector<Location>& args, const Type* ret)
        : Insn(CALL_INSN), _fn(fn), _args(args), _ret(ret) {}

    Location CallInsn::lazy_loc() {
        return _func->create_local(_ret);
    }

    void CallInsn::emit() {
        vector<Location> saved;
        for (u32 i : in()) {
            if (out().find(i) != out().end() && all_locals[i].reg >= 0) {
                Location loc;
                loc.type = LOC_LOCAL;
                loc.local_index = i;
                saved.push(loc);
            }
        }
        for (u32 i = 0; i < saved.size(); i++) push(saved[i]);
        for (u32 i = 0; i < _args.size(); i++) set_arg(i, _args[i]);
        call(_loc, _fn);
        for (i64 i = i64(saved.size()) - 1; i >= 0; i--) pop(saved[i]);
    }

    void CallInsn::format(stream& io) const {
        write(io, _loc, " = ", _fn, "()");
    }

    void CallInsn::liveout() {
        for (const Location& loc : _args)
            if (loc.type == LOC_LOCAL) in().insert(loc.local_index);
        if (_fn.type == LOC_LOCAL) in().insert(_fn.local_index);
        if (_loc.type == LOC_LOCAL) in().erase(_loc.local_index);
    }

    Label::Label(u32 label) : Insn(LABEL), _label(label) {}

    Location Label::lazy_loc() {
        return loc_none();
    }

    void Label::emit() {
        local_label(all_labels[_label]);
    }

    void Label::format(stream& io) const {
        write(io, "\b\b\b\b", all_labels[_label], ":");
    }

    void Label::liveout() {
        //
    }

    GotoInsn::GotoInsn(u32 label) : Insn(GOTO_INSN), _label(label) {}

    Location GotoInsn::lazy_loc() {
        return loc_none();
    }

    void GotoInsn::emit() {
        jmp(label64(symbol_for_label(_label, LOCAL_SYMBOL)));
    }

    void GotoInsn::format(stream& io) const {
        write(io, "goto ", all_labels[_label]);
    }

    void GotoInsn::liveout() {
        //
    }

    IfZeroInsn::IfZeroInsn(u32 label, Location cond) : Insn(IFZERO_INSN), _label(label), _cond(cond) {}

    Location IfZeroInsn::lazy_loc() {
        return loc_none();
    }

    void IfZeroInsn::emit() {
        Location label;
        label.type = LOC_LABEL;
        label.label_index = _label;
        jump_if_zero(label, _cond);
    }

    void IfZeroInsn::format(stream& io) const {
        write(io, "if not ", _cond, " goto ", all_labels[_label]);
    }

    void IfZeroInsn::liveout() {
        if (_cond.type == LOC_LOCAL) in().insert(_cond.local_index);
    }
} // namespace basil

void write(stream& io, const basil::Location& loc) {
    switch (loc.type) {
        case basil::LOC_NONE: write(io, "none"); return;
        case basil::LOC_LOCAL:
            write(io, basil::all_locals[loc.local_index].name);
            if (basil::all_locals[loc.local_index].index > 0 || basil::all_locals[loc.local_index].name[0] != '.')
                write(io, ".", basil::all_locals[loc.local_index].index);
            return;
        case basil::LOC_IMMEDIATE: write(io, loc.immediate); return;
        case basil::LOC_LABEL: write(io, basil::all_labels[loc.label_index]); return;
        case basil::LOC_CONSTANT: write(io, basil::all_constants[loc.constant_index].name); return;
        case basil::LOC_REGISTER: write(io, "r", loc.reg); return;
        default: return;
    }
}

void write(stream& io, basil::Insn* insn) {
    insn->format(io);
}

void write(stream& io, const basil::Insn* insn) {
    insn->format(io);
}

void write(stream& io, const basil::Function& func) {
    func.format(io);
}