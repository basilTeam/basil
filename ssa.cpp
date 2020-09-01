#include "ssa.h"
#include "util/hash.h"

namespace basil {
	using namespace x64;

	Location ssa_none() {
		Location loc;
		loc.type = SSA_NONE;
		return loc;
	}

	Location ssa_immediate(i64 i) {
		Location loc;
		loc.type = SSA_IMMEDIATE;
		loc.immediate = i;
		return loc;
	}

	Insn::Insn():
		_loc(ssa_none()), _func(nullptr) {}

	Insn::~Insn() {
		//
	}

	void Insn::setfunc(Function* func) {
		_func = func;
	}

	Location Insn::loc() {
		if (_loc.type == SSA_NONE) _loc = lazy_loc();
		return _loc;
	}

	static u32 anonymous_locals = 0;
	static u32 anonymous_labels = 0;
	static vector<string> all_labels;
	static map<string, u32> label_map;
	static vector<LocalInfo> all_locals;
	static vector<ConstantInfo> all_constants;

	u32 ssa_find_label(const string& label) {
		auto it = label_map.find(label);
		if (it == label_map.end()) return ssa_add_label(label);
		return it->second;
	}

	u32 ssa_add_label(const string& label) {
		all_labels.push(label);
		label_map[label] = all_labels.size() - 1;
		return all_labels.size() - 1;
	}

	u32 ssa_next_label() {
		buffer b;
		write(b, ".L", anonymous_labels ++);
		string s;
		read(b, s);
		all_labels.push(s);
		label_map[s] = all_labels.size() - 1;
		return all_labels.size() - 1;
	}

	Location ssa_next_local(const Type* t) {
		buffer b;
		write(b, ".t", anonymous_locals ++);
		string s;
		read(b, s);
		all_locals.push({ s, 0, t, {}});
		
		Location loc;
		loc.type = SSA_LOCAL;
		loc.local_index = all_locals.size() - 1;
		return loc;
	}

	Location ssa_const(u32 label, const string& constant) {
		ConstantInfo info;
		info.type = STRING;
		info.name = all_labels[label];
		for (u32 i = 0; i < constant.size(); i ++) info.data.push(constant[i]);
		info.data.push('\0');
		
		all_constants.push(info);
		Location loc;
		loc.type = SSA_CONSTANT;
		loc.constant_index = all_constants.size() - 1;
		return loc;
	}

	Symbol symbol_for_label(u32 label, SymbolLinkage type) {
		return type == GLOBAL_SYMBOL ?
			global((const char*)all_labels[label].raw())
			: local((const char*)all_labels[label].raw());
	}
	
	void ssa_emit_constants(Object& object) {
		using namespace x64;
		writeto(object);
		for (const ConstantInfo& info : all_constants) {
			label(symbol_for_label(label_map[info.name], GLOBAL_SYMBOL));
			for (u8 b : info.data) object.code().write(b);
		}
	}

	static map<string, u32> local_state_counts;

	Location ssa_next_local_for(const Location& loc) {
		const LocalInfo& info = all_locals[loc.local_index];
		auto it = local_state_counts.find(info.name);
		if (it == local_state_counts.end()) {
			LocalInfo new_info = { info.name, 0, info.type, info.value };
			all_locals.push(new_info);
			Location loc;
			loc.type = SSA_LOCAL;
			loc.local_index = all_locals.size() - 1;
			local_state_counts[info.name] = 1;
			return loc;
		}
		else {
			u32 num = it->second;
			LocalInfo new_info = info;
			new_info.index = num;
			all_locals.push(new_info);
			Location loc;
			loc.type = SSA_LOCAL;
			loc.local_index = all_locals.size() - 1;
			it->second ++;
			return loc;
		}
	}

	const Type* ssa_type(const Location& loc) {
		switch (loc.type) {
			case SSA_NONE:
				return VOID;
			case SSA_LOCAL:
				return all_locals[loc.local_index].type;
			case SSA_CONSTANT:
				return all_constants[loc.constant_index].type;
			case SSA_IMMEDIATE:
				return INT; // close enough at this stage
			case SSA_LABEL:
				return INT; // ...close enough :p
		}
	}

	x64::Arg x64_arg(const Location& loc) {
		switch (loc.type) {
			case SSA_NONE:
				return imm(0);
			case SSA_LOCAL:
				return all_locals[loc.local_index].value; 
			case SSA_CONSTANT:
				return label64(
					global((const char*)all_constants[loc.constant_index].name.raw()));
			case SSA_IMMEDIATE:
				return imm(loc.immediate);
			case SSA_LABEL:
				return label64(global((const char*)all_labels[loc.label_index].raw()));
		}
	}

	Function::Function(u32 label):
		_stack(0), _label(label) {}

	Function::Function(const string& label):
		_stack(0), _label(ssa_add_label(label)) {}

	Location Function::create_local(const Type* t) {
		Location l = ssa_next_local(t);
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
		_fns.push(new Function(ssa_next_label()));
		return *_fns.back();
	}

	Function& Function::create_function(const string& name) {
		_fns.push(new Function(name));
		return *_fns.back();
	}

	Location Function::create_local(const string& name, const Type* t) {
		LocalInfo info = { name, 0, t, {}};
		all_locals.push(info);
		local_state_counts[name] = 1;

		Location loc;
		loc.type = SSA_LOCAL;
		loc.local_index = all_locals.size() - 1;
		_locals.push(loc);
		return loc;
	}

	Location Function::next_local(const Location& loc) {
		Location next = ssa_next_local_for(loc);
		_locals.push(next);
		return next;
	}

	Location Function::add(Insn* insn) {
		insn->setfunc(this);
		_insns.push(insn);
		return insn->loc();
	}

	u32 Function::label() const {
		return _label;
	}

	void Function::allocate() {
		for (Function* fn : _fns) fn->allocate();
		for (Location l : _locals) {
			LocalInfo& info = all_locals[l.local_index];
			info.value = x64::m64(RBP, -(_stack += 8)); // assumes everything is a word
		}
	}

	void Function::emit(Object& obj) {
		for (Function* fn : _fns) fn->emit(obj);

		writeto(obj);
		Symbol label = global((const char*)all_labels[_label].raw());
		x64::label(label);
		push(r64(RBP));
		mov(r64(RBP), r64(RSP));
		sub(r64(RSP), imm(_stack));

		for (Insn* i : _insns) i->emit();

		mov(r64(RSP), r64(RBP));
		pop(r64(RBP));
		ret();
	}

	void Function::format(stream& io) const {
		for (Function* fn : _fns) fn->format(io);
		writeln(io, all_labels[_label], ":");
		for (Insn* i : _insns) writeln(io, "    ", i);
	}

	Location LoadInsn::lazy_loc() {
		return _func->create_local(ssa_type(_src));
	}

	LoadInsn::LoadInsn(Location src):
		_src(src) {}

	void LoadInsn::emit() {
		auto temp = r64(RAX), src = x64_arg(_src), dst = x64_arg(_loc);
		mov(temp, src);
		mov(dst, temp);
	}

	void LoadInsn::format(stream& io) const {
		write(io, _loc, " = ", _src);
	}

	Location StoreInsn::lazy_loc() {
		if (_init) return _dest;
		else return _func->next_local(_dest);
	}

	StoreInsn::StoreInsn(Location dest, Location src, bool init):
		_dest(dest), _src(src), _init(init) {}

	void StoreInsn::emit() {
		auto temp = r64(RAX), src = x64_arg(_src), dst = x64_arg(_dest);
		mov(temp, src);
		mov(dst, temp);
	}

	void StoreInsn::format(stream& io) const {
		write(io, _loc, " = ", _src);
	}

	LoadPtrInsn::LoadPtrInsn(Location src, const Type* t, i32 offset):
		_src(src), _type(t), _offset(offset) {}

	Location LoadPtrInsn::lazy_loc() {
		return _func->create_local(_type);
	}

	void LoadPtrInsn::emit() {
		auto src = x64_arg(_src), dst = x64_arg(_loc);
		mov(r64(RAX), src);
		mov(r64(RDX), m64(RAX, _offset));
		mov(dst, r64(RDX));
	}

	void LoadPtrInsn::format(stream& io) const {
		write(io, _loc, " = *", _src);
	}

	StorePtrInsn::StorePtrInsn(Location dest, Location src, i32 offset):
		_dest(dest), _src(src), _offset(offset) {}

	Location StorePtrInsn::lazy_loc() {
		return ssa_none();
	}

	void StorePtrInsn::emit() {
		auto src = x64_arg(_src), dst = x64_arg(_dest);
		mov(r64(RAX), dst);
		mov(r64(RDX), src);
		mov(m64(RAX, _offset), r64(RDX));
	}

	void StorePtrInsn::format(stream& io) const {
		write(io, "*", _dest, " = ", _src);
	}

	AddressInsn::AddressInsn(Location src, const Type* t):
		_src(src), _type(t) {}

	Location AddressInsn::lazy_loc() {
		return _func->create_local(_type);
	}

	void AddressInsn::emit() {
		auto src = x64_arg(_src), dst = x64_arg(_loc);
		lea(r64(RAX), src);
		mov(dst, r64(RAX));
	}

	void AddressInsn::format(stream& io) const {
		write(io, _loc, " = &", _src);
	}

	void emit_binary(void(*op)(const x64::Arg&, const x64::Arg&, x64::Size),
		Location dst, Location left, Location right, x64::Size size = AUTO) {
		auto temp = r64(RAX), _left = x64_arg(left), 
			_right = x64_arg(right), _dst = x64_arg(dst);
		mov(temp, _left);
		op(temp, _right, size);
		mov(_dst, temp);
	}

	void emit_compare(x64::Condition cond, Location dst, 
		Location left, Location right) {
		auto temp = r64(RAX), _left = x64_arg(left), 
			_right = x64_arg(right), _dst = x64_arg(dst);
		mov(temp, _left);
		cmp(temp, _right);
		if (is_memory(_dst.type)) {
			mov(temp, imm(0));
			setcc(temp, cond);
			mov(_dst, temp);
		}
		else {
			mov(_dst, imm(0));
			setcc(_dst, cond);
		}
	}

	BinaryInsn::BinaryInsn(const char* name, Location left, 
		Location right):
		_name(name), _left(left), _right(right) {}

	void BinaryInsn::format(stream& io) const {
		write(io, _loc, " = ", _left, " ", _name, " ", _right);
	}
	
	Location BinaryMathInsn::lazy_loc() {
		return _func->create_local(ssa_type(_left));
	}

	BinaryMathInsn::BinaryMathInsn(const char* name, Location left,
		Location right):
		BinaryInsn(name, left, right) {}

	AddInsn::AddInsn(Location left, Location right):
		BinaryMathInsn("+", left, right) {}

	void AddInsn::emit() {
		emit_binary(add, _loc, _left, _right);
	}

	SubInsn::SubInsn(Location left, Location right):
		BinaryMathInsn("-", left, right) {}

	void SubInsn::emit() {
		emit_binary(sub, _loc, _left, _right);
	}

	MulInsn::MulInsn(Location left, Location right):
		BinaryMathInsn("*", left, right) {}

	void MulInsn::emit() {
		auto temp = r64(RAX), left = x64_arg(_left), 
			right = x64_arg(_right), dst = x64_arg(_loc);
		mov(temp, left);
		if (_right.type == SSA_IMMEDIATE) {
			mov(r64(RDX), right);
			imul(temp, r64(RDX));
		}
		else imul(temp, right);
		mov(dst, temp);
	}

	DivInsn::DivInsn(Location left, Location right):
		BinaryMathInsn("/", left, right) {}

	void DivInsn::emit() {
		auto rax = r64(RAX), rcx = r64(RCX), rdx = r64(RDX), 
			left = x64_arg(_left), right = x64_arg(_right), 
			dst = x64_arg(_loc);
		mov(rax, left);
		cdq();
		if (_right.type == SSA_IMMEDIATE) {
			mov(rcx, right);
			idiv(rcx);
		} 
		else idiv(right, QWORD);
		mov(dst, rax);
	}

	RemInsn::RemInsn(Location left, Location right):
		BinaryMathInsn("%", left, right) {}

	void RemInsn::emit() {
		auto rax = r64(RAX), rcx = r64(RCX), rdx = r64(RDX), 
			left = x64_arg(_left), right = x64_arg(_right), 
			dst = x64_arg(_loc);
		mov(rax, left);
		cdq();
		if (_right.type == SSA_IMMEDIATE) {
			mov(rcx, right);
			idiv(rcx);
		} 
		else idiv(right, QWORD);
		mov(dst, rdx);
	}

	BinaryLogicInsn::BinaryLogicInsn(const char* name, Location left,
		Location right):
		BinaryInsn(name, left, right) {}

	Location BinaryLogicInsn::lazy_loc() {
		return _func->create_local(BOOL);
	}

	AndInsn::AndInsn(Location left, Location right):
		BinaryLogicInsn("and", left, right) {}

	void AndInsn::emit() {
		emit_binary(and_, _loc, _left, _right);
	}

	OrInsn::OrInsn(Location left, Location right):
		BinaryLogicInsn("or", left, right) {}

	void OrInsn::emit() {
		emit_binary(or_, _loc, _left, _right);
	}

	XorInsn::XorInsn(Location left, Location right):
		BinaryLogicInsn("xor", left, right) {}

	void XorInsn::emit() {
		emit_binary(xor_, _loc, _left, _right);
	}

	NotInsn::NotInsn(Location src):
		_src(src) {}

	Location NotInsn::lazy_loc() {
		return _func->create_local(BOOL);
	}

	void NotInsn::emit() {
		auto src = x64_arg(_src), dst = x64_arg(_loc);
		xor_(r64(RDX), r64(RDX));
		mov(r64(RAX), src);
		cmp(r64(RAX), imm(0));
		setcc(r64(RDX), ZERO);
		mov(dst, r64(RDX));
	}

	void NotInsn::format(stream& io) const {
		write(io, _loc, " = ", "not ", _src);
	}

	BinaryEqualityInsn::BinaryEqualityInsn(const char* name, 
		Location left, Location right):
		BinaryInsn(name, left, right) {}

	Location BinaryEqualityInsn::lazy_loc() {
		return _func->create_local(BOOL);
	}

	EqualInsn::EqualInsn(Location left, Location right):
		BinaryEqualityInsn("==", left, right) {}

	void EqualInsn::emit() {
		emit_compare(EQUAL, _loc, _left, _right);
	}

	InequalInsn::InequalInsn(Location left, Location right):
		BinaryEqualityInsn("!=", left, right) {}

	void InequalInsn::emit() {
		emit_compare(NOT_EQUAL, _loc, _left, _right);
	}

	BinaryRelationInsn::BinaryRelationInsn(const char* name, 
		Location left, Location right):
		BinaryInsn(name, left, right) {}

	Location BinaryRelationInsn::lazy_loc() {
		return _func->create_local(BOOL);
	}

	LessInsn::LessInsn(Location left, Location right):
		BinaryRelationInsn("<", left, right) {}

	void LessInsn::emit() {
		emit_compare(LESS, _loc, _left, _right);
	}

	LessEqualInsn::LessEqualInsn(Location left, Location right):
		BinaryRelationInsn("<=", left, right) {}

	void LessEqualInsn::emit() {
		emit_compare(LESS_OR_EQUAL, _loc, _left, _right);
	}

	GreaterInsn::GreaterInsn(Location left, Location right):
		BinaryRelationInsn(">", left, right) {}

	void GreaterInsn::emit() {
		emit_compare(GREATER, _loc, _left, _right);
	}

	GreaterEqualInsn::GreaterEqualInsn(Location left, Location right):
		BinaryRelationInsn(">=", left, right) {}

	void GreaterEqualInsn::emit() {
		emit_compare(GREATER_OR_EQUAL, _loc, _left, _right);
	}

	Location RetInsn::lazy_loc() {
		return ssa_none();
	}

	RetInsn::RetInsn(Location src):
		_src(src) {}

	void RetInsn::emit() {
		auto rax = r64(RAX), src = x64_arg(_src);
		mov(rax, src);
	}

	void RetInsn::format(stream& io) const {
		write(io, "return ", _src);
	}

	static const x64::Register X64_ARG_REGISTERS[] = {
		RDI, RSI, RDX, RCX, R8, R9
	};

	LoadArgumentInsn::LoadArgumentInsn(u32 index, const Type* type):
		_index(index), _type(type) {}

	Location LoadArgumentInsn::lazy_loc() {
		return _func->create_local(_type);
	}

	void LoadArgumentInsn::emit() {
		mov(x64_arg(_loc), r64(X64_ARG_REGISTERS[_index]));
	}

	void LoadArgumentInsn::format(stream& io) const {
		write(io, _loc, " = $", _index);
	}

	StoreArgumentInsn::StoreArgumentInsn(Location src, u32 index, const Type* type):
		_src(src), _index(index), _type(type) {}

	Location StoreArgumentInsn::lazy_loc() {
		return ssa_none();
	}

	void StoreArgumentInsn::emit() {
		mov(r64(X64_ARG_REGISTERS[_index]), x64_arg(_src));
	}

	void StoreArgumentInsn::format(stream& io) const {
		write(io, "$", _index, " = ", _src);
	}

	CallInsn::CallInsn(Location fn, const Type* ret):
		_fn(fn), _ret(ret) {}

	Location CallInsn::lazy_loc() {
		return _func->create_local(_ret);
	}

	void CallInsn::emit() {
		if (_fn.type == SSA_LABEL)
			call(label64(symbol_for_label(_fn.label_index, GLOBAL_SYMBOL)));
		else {
			mov(r64(RAX), x64_arg(_fn));
			call(r64(RAX));
		}
		mov(x64_arg(_loc), r64(RAX));
	}

	void CallInsn::format(stream& io) const {
		write(io, _loc, " = ", _fn, "()");
	}

	Label::Label(u32 label):
		_label(label) {}

	Location Label::lazy_loc() {
		return ssa_none();
	}

	void Label::emit() {
		label(symbol_for_label(_label, LOCAL_SYMBOL));
	}

	void Label::format(stream& io) const {
		write(io, "\b\b\b\b", all_labels[_label], ":");
	}

	GotoInsn::GotoInsn(u32 label):
		_label(label) {}

	Location GotoInsn::lazy_loc() {
		return ssa_none();
	}

	void GotoInsn::emit() {
		jmp(label64(symbol_for_label(_label, LOCAL_SYMBOL)));
	}

	void GotoInsn::format(stream& io) const {
		write(io, "goto ", all_labels[_label]);
	}

	IfZeroInsn::IfZeroInsn(u32 label, Location cond):
		_label(label), _cond(cond) {}

	Location IfZeroInsn::lazy_loc() {
		return ssa_none();
	}

	void IfZeroInsn::emit() {
		auto cond = x64_arg(_cond);
		if (is_immediate(cond.type)) {
			mov(r64(RAX), cond);
			cmp(r64(RAX), imm(0));
		}
		else cmp(x64_arg(_cond), imm(0));
		jcc(label64(symbol_for_label(_label, LOCAL_SYMBOL)), EQUAL);
	}

	void IfZeroInsn::format(stream& io) const {
		write(io, "if not ", _cond, " goto ", all_labels[_label]);
	}
}

void write(stream& io, const basil::Location& loc) {
	switch (loc.type) {
		case basil::SSA_NONE:
			write(io, "none");
			return;
		case basil::SSA_LOCAL:
			write(io, basil::all_locals[loc.local_index].name);
			if (basil::all_locals[loc.local_index].index > 0
					|| basil::all_locals[loc.local_index].name[0] != '.')
				write(io, ".", basil::all_locals[loc.local_index].index);
			return;
		case basil::SSA_IMMEDIATE:
			write(io, loc.immediate);
			return;
		case basil::SSA_LABEL:
			write(io, basil::all_labels[loc.label_index]);
			return;
		case basil::SSA_CONSTANT:
			write(io, basil::all_constants[loc.constant_index].name);
			return;
		default:
			return;
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