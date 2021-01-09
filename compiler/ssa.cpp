#include "ssa.h"
#include "env.h"
#include "values.h"

namespace basil {
    static map<string, u64> id_name_table;
    static vector<string> id_names;
    static vector<u64> id_counts;

    SSAIdent next_ident(const string& name = "") {
        auto it = id_name_table.find(name);
        if (it == id_name_table.end()) {
            id_name_table[name] = id_names.size();
            id_names.push(name);
            id_counts.push(0);
            // start new ident chain for name
            return { id_names.size() - 1, id_counts.back() };
        }
        else {
            // find next ident in chain, incrementing count
            return { it->second, ++ id_counts[it->second] };
        }
    }

    const u8 MAJOR_MASK = 0b11100000;
    const u8 MINOR_MASK = 0b00011000;

    SSANode::SSANode(ref<BasicBlock> parent, const Type* type, SSAKind kind, const string& name): 
        _parent(parent), _type(type), _kind(kind), _id(next_ident(name)) {}

    bool SSANode::is(SSAKind kind) const {
        return _kind == kind;
    }

    ref<BasicBlock> SSANode::parent() const {
        return _parent;
    }

    ref<BasicBlock>& SSANode::parent() {
        return _parent;
    }

    SSAIdent SSANode::id() const {
        return _id;
    }

    SSAKind SSANode::kind() const {
        return _kind;
    }

    const Type* SSANode::type() const {
        return _type;
    }

    static u64 bb_count = 0;
    string next_bb() {
        return format<string>(".BB", bb_count ++);
    }
    
    BasicBlock::BasicBlock(const string& label): _label(label) {
        if (!_label.size()) _label = next_bb();
    }

    const string& BasicBlock::label() const {
        return _label;
    }

    u32 BasicBlock::size() const {
        return _members.size();
    }

    const ref<SSANode>* BasicBlock::begin() const {
        return _members.begin();
    }

    ref<SSANode>* BasicBlock::begin() {
        return _members.begin();
    }

    const ref<SSANode>* BasicBlock::end() const {
        return _members.end();
    }

    ref<SSANode>* BasicBlock::end() {
        return _members.end();
    }

    void BasicBlock::push(ref<SSANode> node) {
        _members.push(node);
    }

    const vector<ref<SSANode>>& BasicBlock::members() const {
        return _members;
    }

    vector<ref<SSANode>>& BasicBlock::members() {
        return _members;
    }

    Location BasicBlock::emit(Function& fn) {
        Location loc;
        for (auto& member : _members) loc = member->emit(fn);
        return loc;
    }

    void BasicBlock::format(stream& io) const {
        writeln(io, label(), ":");
        for (auto& member : _members)
            writeln(io, "    ", member);
    }

    SSAInt::SSAInt(ref<BasicBlock> parent, i64 value): 
        SSANode(parent, INT, SSA_INT), _value(value) {}

    i64 SSAInt::value() const {
        return _value;
    }

    i64& SSAInt::value() {
        return _value;
    }

    Location SSAInt::emit(Function& fn) {
        return loc_immediate(_value);
    }

    void SSAInt::format(stream& io) const {
        write(io, id(), " = ", _value);
    }

    SSABool::SSABool(ref<BasicBlock> parent, bool value): 
        SSANode(parent, BOOL, SSA_BOOL), _value(value) {}

    bool SSABool::value() const {
        return _value;
    }

    bool& SSABool::value() {
        return _value;
    }

    Location SSABool::emit(Function& fn) {
        return loc_immediate(_value ? 1 : 0);
    }

    void SSABool::format(stream& io) const {
        write(io, id(), " = ", _value);
    }

    SSAString::SSAString(ref<BasicBlock> parent, const string& value): 
        SSANode(parent, STRING, SSA_STRING), _value(value) {}

    const string& SSAString::value() const {
        return _value;
    }

    string& SSAString::value() {
        return _value;
    }

    Location SSAString::emit(Function& fn) {
        return loc_none(); // todo
    }

    void SSAString::format(stream& io) const {
        write(io, id(), " = \"", _value, "\"");
    }

    SSAVoid::SSAVoid(ref<BasicBlock> parent): SSANode(parent, VOID, SSA_VOID) {}

    Location SSAVoid::emit(Function& fn) {
        return loc_immediate(0);
    }

    void SSAVoid::format(stream& io) const {
        write(io, id(), " = []");
    }

    SSASymbol::SSASymbol(ref<BasicBlock> parent, u64 value):
        SSANode(parent, SYMBOL, SSA_SYMBOL), _value(value) {}

    u64 SSASymbol::value() const {
        return _value;
    }

    u64& SSASymbol::value() {
        return _value;
    }

    Location SSASymbol::emit(Function& fn) {
        return loc_immediate(_value);
    }

    void SSASymbol::format(stream& io) const {
        write(io, id(), " = ", symbol_for(_value));
    }        
    
    SSAFunction::SSAFunction(ref<BasicBlock> parent, const Type* type, u64 name):
        SSANode(parent, type, SSA_FUNCTION, symbol_for(name)), _name(name) {}

    Location SSAFunction::emit(Function& fn) {
        return loc_label(symbol_for(_name));
    }

    void SSAFunction::format(stream& io) const {
        write(io, id(), " = &", symbol_for(_name));
    }

    SSALoadLabel::SSALoadLabel(ref<BasicBlock> parent, const Type* type, u64 name):
        SSANode(parent, type, SSA_LOAD_LABEL, symbol_for(name)), _name(name) {}

    Location SSALoadLabel::emit(Function& fn) {
        return loc_label(symbol_for(_name));
    }

    void SSALoadLabel::format(stream& io) const {
        write(io, id(), " = &", symbol_for(_name));
    }

    SSAStore::SSAStore(ref<BasicBlock> parent, ref<Env> env, u64 name, ref<SSANode> value):
        SSANode(parent, value->type(), SSA_STORE, symbol_for(name)), _env(env), _value(value) {}

    Location SSAStore::emit(Function& fn) {
        const string& name = id_names[id().base];
        auto def = _env->find(name);
        if (!def) return loc_none();
        if (def->location.type == LOC_NONE) return fn.add(new StoreInsn(def->location = 
            fn.create_local(name, basil::INT), _value->emit(fn)));
        return fn.add(new StoreInsn(def->location, _value->emit(fn)));
    }

    void SSAStore::format(stream& io) const {
        write(io, id(), " = ", _value->id());
    }

    SSABinary::SSABinary(ref<BasicBlock> parent, const Type* type, SSAKind kind, ref<SSANode> left, ref<SSANode> right):
        SSANode(parent, type, kind), _left(left), _right(right) {}

    ref<SSANode> SSABinary::left() const {
        return _left;
    }

    ref<SSANode>& SSABinary::left() {
        return _left;
    }

    ref<SSANode> SSABinary::right() const {
        return _right;
    }

    ref<SSANode>& SSABinary::right() {
        return _right;
    }

    Location SSABinary::emit(Function& fn) {
        switch (kind()) {
            case SSA_ADD:
                return fn.add(new AddInsn(_left->emit(fn), _right->emit(fn)));
            case SSA_SUB:
                return fn.add(new SubInsn(_left->emit(fn), _right->emit(fn)));
            case SSA_MUL:
                return fn.add(new MulInsn(_left->emit(fn), _right->emit(fn)));
            case SSA_DIV:
                return fn.add(new DivInsn(_left->emit(fn), _right->emit(fn)));
            case SSA_REM:
                return fn.add(new RemInsn(_left->emit(fn), _right->emit(fn)));
            case SSA_AND:
                return fn.add(new AndInsn(_left->emit(fn), _right->emit(fn)));
            case SSA_OR:
                return fn.add(new OrInsn(_left->emit(fn), _right->emit(fn)));
            case SSA_XOR:
                return fn.add(new XorInsn(_left->emit(fn), _right->emit(fn)));
            case SSA_EQ:
                return fn.add(new EqualInsn(_left->emit(fn), _right->emit(fn)));
            case SSA_NOT_EQ:
                return fn.add(new InequalInsn(_left->emit(fn), _right->emit(fn)));
            case SSA_GREATER:
                return fn.add(new GreaterInsn(_left->emit(fn), _right->emit(fn)));
            case SSA_GREATER_EQ:
                return fn.add(new GreaterEqualInsn(_left->emit(fn), _right->emit(fn)));
            case SSA_LESS:
                return fn.add(new LessInsn(_left->emit(fn), _right->emit(fn)));
            case SSA_LESS_EQ:
                return fn.add(new LessEqualInsn(_left->emit(fn), _right->emit(fn)));
            default:
                return loc_none();
        }
    }

    void SSABinary::format(stream& io) const {
        const char* op;
        switch(kind()) {
            case SSA_ADD: op = "+"; break;
            case SSA_SUB: op = "-"; break;
            case SSA_MUL: op = "*"; break;
            case SSA_DIV: op = "/"; break;
            case SSA_REM: op = "%"; break;
            case SSA_AND: op = "and"; break;
            case SSA_OR: op = "or"; break;
            case SSA_XOR: op = "xor"; break;
            case SSA_EQ: op = "=="; break;
            case SSA_NOT_EQ: op = "!="; break;
            case SSA_LESS: op = "<"; break;
            case SSA_LESS_EQ: op = "<="; break;
            case SSA_GREATER: op = ">"; break;
            case SSA_GREATER_EQ: op = ">="; break;
            default: op = "?"; break;
        }
        write(io, id(), " = ", _left->id(), " ", op, " ", _right->id());
    }

    SSAUnary::SSAUnary(ref<BasicBlock> parent, const Type* type, SSAKind kind, ref<SSANode> child):
        SSANode(parent, type, kind), _child(child) {}

    ref<SSANode> SSAUnary::child() const {
        return _child;
    }

    ref<SSANode>& SSAUnary::child() {
        return _child;
    }

    Location SSAUnary::emit(Function& fn) {
        switch (kind()) {
            case SSA_NOT:
                return fn.add(new NotInsn(_child->emit(fn)));
            default:
                return loc_none();
        }
    }

    void SSAUnary::format(stream& io) const {
        const char* op;
        switch(kind()) {
            case SSA_NOT: op = "not"; break;
            default: op = "?"; break;
        }
        write(io, id(), " = ", op, " ", _child->id());
    }

    SSACall::SSACall(ref<BasicBlock> parent, const Type* type, ref<SSANode> fn, const vector<ref<SSANode>>& args):
        SSANode(parent, type, SSA_CALL), _fn(fn), _args(args) {}

    ref<SSANode> SSACall::fn() const {
        return _fn;
    }

    ref<SSANode>& SSACall::fn() {
        return _fn;
    }

    const vector<ref<SSANode>>& SSACall::args() const {
        return _args;
    }

    vector<ref<SSANode>>& SSACall::args() {
        return _args;
    }

    Location SSACall::emit(Function& fn) {
        Location func = _fn->emit(fn);
        vector<Location> arglocs;
        for (auto node : _args) arglocs.push(node->emit(fn));
        for (u32 i = 0; i < _args.size(); i ++) {
            if (arglocs[i].type == LOC_LABEL) {
                arglocs[i] = fn.add(new AddressInsn(arglocs[i], _args[i]->type()));
            }
        }
        return fn.add(new CallInsn(func, arglocs, type()));
    }

    void SSACall::format(stream& io) const {
        write(io, id(), " = ", _fn->id(), "(");
        bool first = true;
        for (auto node : _args) {
            write(io, first ? ", " : "", node->id());
            first = false;
        }
        write(io, ")");
    }

    SSARet::SSARet(ref<BasicBlock> parent, ref<SSANode> value):
        SSANode(parent, VOID, SSA_RET), _value(value) {}

    ref<SSANode> SSARet::value() const {
        return _value;
    }

    ref<SSANode>& SSARet::value() {
        return _value;
    }  

    Location SSARet::emit(Function& fn) {
        return fn.add(new RetInsn(_value->emit(fn)));
    }

    void SSARet::format(stream& io) const {
        write(io, "return ", _value->id());
    }

    SSAGoto::SSAGoto(ref<BasicBlock> parent, ref<BasicBlock> target):
        SSANode(parent, VOID, SSA_GOTO), _target(target) {}

    ref<BasicBlock> SSAGoto::target() const {
        return _target;
    }

    ref<BasicBlock>& SSAGoto::target() {
        return _target;
    }

    Location SSAGoto::emit(Function& fn) {
        return fn.add(new GotoInsn(symbol_value(_target->label()))); // todo: bb labels
    }

    void SSAGoto::format(stream& io) const {
        write(io, "goto ", _target->label());
    }

    SSAIf::SSAIf(ref<BasicBlock> parent, ref<SSANode> cond, ref<BasicBlock> if_true, ref<BasicBlock> if_false):
        SSANode(parent, VOID, SSA_IF), _cond(cond), _if_true(if_true), _if_false(if_false) {}

    ref<SSANode> SSAIf::cond() const {
        return _cond;
    }

    ref<SSANode>& SSAIf::cond() {
        return _cond;
    }

    ref<BasicBlock> SSAIf::if_true() const {
        return _if_true;
    }

    ref<BasicBlock>& SSAIf::if_true() {
        return _if_true;
    }

    ref<BasicBlock> SSAIf::if_false() const {
        return _if_false;
    }

    ref<BasicBlock>& SSAIf::if_false() {
        return _if_false;
    }

    Location SSAIf::emit(Function& fn) {
        return loc_none();
    }

    void SSAIf::format(stream& io) const {
        write(io, "if ", _cond->id(), " then ", _if_true->label(), " else ", _if_false->label());
    }

    SSAPhi::SSAPhi(ref<BasicBlock> parent, const Type* type, ref<SSANode> left, ref<SSANode> right,
        ref<BasicBlock> left_block, ref<BasicBlock> right_block):
        SSANode(parent, type, SSA_PHI), _left(left), _right(right), 
        _left_block(left_block), _right_block(right_block) {}

    ref<SSANode> SSAPhi::left() const {
        return _left;
    }

    ref<SSANode>& SSAPhi::left() {
        return _left;
    }

    ref<SSANode> SSAPhi::right() const {
        return _right;
    }

    ref<SSANode>& SSAPhi::right() {
        return _right;
    }
    
    ref<BasicBlock> SSAPhi::left_block() const {
        return _left_block;
    }

    ref<BasicBlock>& SSAPhi::left_block() {
        return _left_block;
    }

    ref<BasicBlock> SSAPhi::right_block() const {
        return _right_block;
    }

    ref<BasicBlock>& SSAPhi::right_block() {
        return _right_block;
    }

    Location SSAPhi::emit(Function& fn) {
        return loc_none();
    }

    void SSAPhi::format(stream& io) const {
        write(io, id(), " = phi ", _left, "(", _left_block->label(), ") ", _right, "(", _right_block->label(), ")");
    }   
}

void write(stream& io, const basil::SSAIdent& id) {
    write(io, basil::id_names[id.base], '#', id.count);
}

void write(stream& io, const ref<basil::SSANode> node) {
    node->format(io);
}