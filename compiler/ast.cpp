#include "ast.h"
#include "values.h"
#include "env.h"
#include "type.h"

namespace basil {
	ASTNode::ASTNode(SourceLocation loc):
		_loc(loc), _type(nullptr) {}

	ASTNode::~ASTNode() {}

	SourceLocation ASTNode::loc() const {
		return _loc;
	}

	const Type* ASTNode::type() {
		if (!_type) _type = lazy_type();
		return _type->concretify();
	}

	bool ASTNode::is_extern() const {
		return false;
	}

	ASTSingleton::ASTSingleton(const Type* type):
		ASTNode(NO_LOCATION), _type(type) {}

	const Type* ASTSingleton::lazy_type() {
		return _type;
	}

	ref<SSANode> ASTSingleton::emit(ref<BasicBlock>& parent) {
		return nullptr;
	}

	Location ASTSingleton::emit(Function& func) {
		return loc_none();
	}

	void ASTSingleton::format(stream& io) const {
		write(io, "<just ", _type, ">");
	}

	ASTVoid::ASTVoid(SourceLocation loc):
		ASTNode(loc) {}

	const Type* ASTVoid::lazy_type() {
		return VOID;
	}

	ref<SSANode> ASTVoid::emit(ref<BasicBlock>& parent) {
		return ref<SSAVoid>(parent);
	}

	Location ASTVoid::emit(Function& function) {
		return loc_immediate(0);
	}

	void ASTVoid::format(stream& io) const {
		write(io, "[]");
	}

	ASTInt::ASTInt(SourceLocation loc, i64 value):
		ASTNode(loc), _value(value) {}

	const Type* ASTInt::lazy_type() {
		return INT;
	}

	ref<SSANode> ASTInt::emit(ref<BasicBlock>& parent) {
		return newref<SSAInt>(parent, _value);
	}

	Location ASTInt::emit(Function& func) {
		return loc_immediate(_value);
	}

	void ASTInt::format(stream& io) const {
		write(io, _value);
	}

	ASTSymbol::ASTSymbol(SourceLocation loc, u64 value):
		ASTNode(loc), _value(value) {}

	const Type* ASTSymbol::lazy_type() {
		return SYMBOL;
	}

	ref<SSANode> ASTSymbol::emit(ref<BasicBlock>& parent) {
		return newref<SSASymbol>(parent, _value);
	}

	Location ASTSymbol::emit(Function& func) {
		return loc_immediate(_value);
	}

	void ASTSymbol::format(stream& io) const {
		write(io, symbol_for(_value));
	}

	ASTString::ASTString(SourceLocation loc, const string& value):
		ASTNode(loc), _value(value) {}

	const Type* ASTString::lazy_type() {
		return STRING;
	}

	ref<SSANode> ASTString::emit(ref<BasicBlock>& parent) {
		return newref<SSAString>(parent, _value);
	}

	Location ASTString::emit(Function& func) {
		return func.add(new AddressInsn(const_loc(next_label(), _value), type()));
	}

	void ASTString::format(stream& io) const {
		write(io, _value);
	}

	ASTBool::ASTBool(SourceLocation loc, bool value):
		ASTNode(loc), _value(value) {}

	const Type* ASTBool::lazy_type() {
		return BOOL;
	}

	ref<SSANode> ASTBool::emit(ref<BasicBlock>& parent) {
		return newref<SSABool>(parent, _value);
	}

	Location ASTBool::emit(Function& func) {
		return loc_immediate(_value ? 1 : 0);
	}

	void ASTBool::format(stream& io) const {
		write(io, _value);
	}

	ASTVar::ASTVar(SourceLocation loc, const ref<Env> env, u64 name):
		ASTNode(loc), _env(env), _name(name) {}

	const Type* ASTVar::lazy_type() {
		const Def* def = _env->find(symbol_for(_name));
		if (def && def->value.is_runtime())
			return ((const RuntimeType*)def->value.type())->base();

		err(loc(), "Undefined variable '", symbol_for(_name), "'.");
		return ERROR;
	}

	ref<SSANode> ASTVar::emit(ref<BasicBlock>& parent) {
		return nullptr; // todo
	}

	Location ASTVar::emit(Function& func) {
		const Def* def = _env->find(symbol_for(_name));
		if (!def) return loc_none();
		return def->location;
		// return func.add(new LoadInsn(def->location)); <-- why?
	}

	void ASTVar::format(stream& io) const {
		write(io, symbol_for(_name));
	}

	ASTExtern::ASTExtern(SourceLocation loc, const Type* type):
		ASTNode(loc), _type(type) {}

	const Type* ASTExtern::lazy_type() {
		return _type;
	}

	ref<SSANode> ASTExtern::emit(ref<BasicBlock>& parent) {
		return nullptr; // todo
	}

	Location ASTExtern::emit(Function& function) {
		return loc_none();
	}

	void ASTExtern::format(stream& io) const {
		write(io, "extern");
	}

	bool ASTExtern::is_extern() const {
		return true;
	}

	ASTUnary::ASTUnary(SourceLocation loc, ASTNode* child):
		ASTNode(loc), _child(child) {
		_child->inc();
	}

	ASTUnary::~ASTUnary() {
		_child->dec();
	}

	ASTBinary::ASTBinary(SourceLocation loc, ASTNode* left, ASTNode* right):		ASTNode(loc), _left(left), _right(right) {
		_left->inc(), _right->inc();
	}

	ASTBinary::~ASTBinary() {
		_left->dec(), _right->dec();
	}

	ASTBinaryMath::ASTBinaryMath(SourceLocation loc, ASTMathOp op, 
		ASTNode* left, ASTNode* right):
		ASTBinary(loc, left, right), _op(op) {}

	const Type* ASTBinaryMath::lazy_type() {
		if (_left->type() == ERROR || _right->type() == ERROR) return ERROR;
		const Type* lt = unify(_left->type(), INT);
		const Type* rt = unify(_right->type(), INT);
		const Type* result = unify(lt, rt);

		if (result != INT) {		
			err(loc(), "Invalid parameters to arithmetic expression: '", 
				_left->type(), "' and '", _right->type(), "'.");
			return ERROR;
		}
		return result;
	}

	ref<SSANode> ASTBinaryMath::emit(ref<BasicBlock>& parent) {
		switch (_op) {
			case AST_ADD:
				return newref<SSABinary>(parent, type(), SSA_ADD, _left->emit(parent), _right->emit(parent));
			case AST_SUB:
				return newref<SSABinary>(parent, type(), SSA_SUB, _left->emit(parent), _right->emit(parent));
			case AST_MUL:
				return newref<SSABinary>(parent, type(), SSA_MUL, _left->emit(parent), _right->emit(parent));
			case AST_DIV:
				return newref<SSABinary>(parent, type(), SSA_DIV, _left->emit(parent), _right->emit(parent));
			case AST_REM:
				return newref<SSABinary>(parent, type(), SSA_REM, _left->emit(parent), _right->emit(parent));
			default:
				return nullptr;
		}
	}

	Location ASTBinaryMath::emit(Function& func) {
		switch (_op) {
			case AST_ADD:
				return func.add(new AddInsn(_left->emit(func), _right->emit(func)));
			case AST_SUB:
				return func.add(new SubInsn(_left->emit(func), _right->emit(func)));
			case AST_MUL:
				return func.add(new MulInsn(_left->emit(func), _right->emit(func)));
			case AST_DIV:
				return func.add(new DivInsn(_left->emit(func), _right->emit(func)));
			case AST_REM:
				return func.add(new RemInsn(_left->emit(func), _right->emit(func)));
			default:
				return loc_none();
		}
	}

	static const char* MATH_OP_NAMES[] = {
		"+", "-", "*", "/", "%"
	};

	void ASTBinaryMath::format(stream& io) const {
		write(io, "(", MATH_OP_NAMES[_op], " ", _left, " ", _right, ")");
	}

	ASTBinaryLogic::ASTBinaryLogic(SourceLocation loc, ASTLogicOp op, 
		ASTNode* left, ASTNode* right):
		ASTBinary(loc, left, right), _op(op) {}

	const Type* ASTBinaryLogic::lazy_type() {
		if (_left->type() == ERROR || _right->type() == ERROR) return ERROR;
		const Type* lt = unify(_left->type(), BOOL);
		const Type* rt = unify(_right->type(), BOOL);
		const Type* result = unify(lt, rt);

		if (result != BOOL) {		
			err(loc(), "Invalid parameters to logical expression: '", 
				_left->type(), "' and '", _right->type(), "'.");
			return ERROR;
		}
		return BOOL;
	}

	ref<SSANode> ASTBinaryLogic::emit(ref<BasicBlock>& parent) {
		switch (_op) {
			case AST_AND:
				return newref<SSABinary>(parent, type(), SSA_AND, _left->emit(parent), _right->emit(parent));
			case AST_OR:
				return newref<SSABinary>(parent, type(), SSA_OR, _left->emit(parent), _right->emit(parent));
			case AST_XOR:
				return newref<SSABinary>(parent, type(), SSA_XOR, _left->emit(parent), _right->emit(parent));
			default:
				return nullptr;
		}
	}

	Location ASTBinaryLogic::emit(Function& func) {
		switch (_op) {
			case AST_AND:
				return func.add(new AndInsn(_left->emit(func), _right->emit(func)));
			case AST_OR:
				return func.add(new OrInsn(_left->emit(func), _right->emit(func)));
			case AST_XOR:
				return func.add(new XorInsn(_left->emit(func), _right->emit(func)));
			default:
				return loc_none();
		}
	}

	static const char* LOGIC_OP_NAMES[] = {
		"and", "or", "xor", "not"
	};

	void ASTBinaryLogic::format(stream& io) const {
		write(io, "(", LOGIC_OP_NAMES[_op], " ", _left, " ", _right, ")");
	}

	ASTNot::ASTNot(SourceLocation loc, ASTNode* child):
		ASTUnary(loc, child) {}

	const Type* ASTNot::lazy_type() {
		if (_child->type() == ERROR) return ERROR;
		if (unify(_child->type(), BOOL) != BOOL) {
			err(loc(), "Invalid argument to 'not' expression: '", 
				_child->type(), "'.");
			return ERROR;
		}
		return BOOL;
	}

	ref<SSANode> ASTNot::emit(ref<BasicBlock>& parent) {
		return newref<SSAUnary>(parent, type(), SSA_NOT, _child->emit(parent));
	} 

	Location ASTNot::emit(Function& func) {
		return func.add(new NotInsn(_child->emit(func)));
	}

	void ASTNot::format(stream& io) const {
		write(io, "(not ", _child, ")");
	}

	ASTBinaryEqual::ASTBinaryEqual(SourceLocation loc, ASTEqualOp op, 
		ASTNode* left, ASTNode* right):
		ASTBinary(loc, left, right), _op(op) {}

	const Type* ASTBinaryEqual::lazy_type() {
		if (_left->type() == ERROR || _right->type() == ERROR) return ERROR;
		return BOOL;
	}

	ref<SSANode> ASTBinaryEqual::emit(ref<BasicBlock>& parent) {
		switch (_op) {
			case AST_EQUAL:
				return newref<SSABinary>(parent, type(), SSA_EQ, _left->emit(parent), _right->emit(parent));
			case AST_INEQUAL:
				return newref<SSABinary>(parent, type(), SSA_NOT_EQ, _left->emit(parent), _right->emit(parent));
			default:
				return nullptr;
		}
	}

	Location ASTBinaryEqual::emit(Function& func) {
		if (_left->type() == STRING || _right->type() == STRING) {
			vector<Location> args;
			args.push(_left->emit(func));
			args.push(_right->emit(func));
			Location label;
			label.type = LOC_LABEL;
			label.label_index = find_label("_strcmp");
			Location result = func.add(new CallInsn(label, args, INT));
			return func.add(new EqualInsn(result, loc_immediate(0)));
		}

		switch (_op) {
			case AST_EQUAL:
				return func.add(new EqualInsn(_left->emit(func), _right->emit(func)));
			case AST_INEQUAL:
				return func.add(new InequalInsn(_left->emit(func), _right->emit(func)));
			default:
				return loc_none();
		}
	}

	static const char* EQUAL_OP_NAMES[] = {
		"==", "!="
	};

	void ASTBinaryEqual::format(stream& io) const {
		write(io, "(", EQUAL_OP_NAMES[_op], " ", _left, " ", _right, ")");
	}

	ASTBinaryRel::ASTBinaryRel(SourceLocation loc, ASTRelOp op, 
		ASTNode* left, ASTNode* right):
		ASTBinary(loc, left, right), _op(op) {}

	const Type* ASTBinaryRel::lazy_type() {
		if (_left->type() == ERROR || _right->type() == ERROR) return ERROR;
		const Type* lt = unify(_left->type(), INT);
		const Type* rt = unify(_right->type(), INT);
		const Type* result = unify(lt, rt);

		if (result != INT) {
			lt = unify(_left->type(), STRING);
			rt = unify(_right->type(), STRING);
			result = unify(lt, rt);
			if (result != STRING) {
				err(loc(), "Invalid parameters to relational expression: '", 
				_left->type(), "' and '", _right->type(), "'.");
				return ERROR;
			}
		}
		return BOOL;
	}

	ref<SSANode> ASTBinaryRel::emit(ref<BasicBlock>& parent) {
		switch (_op) {
			case AST_LESS:
				return newref<SSABinary>(parent, type(), SSA_LESS, _left->emit(parent), _right->emit(parent));
			case AST_LESS_EQUAL:
				return newref<SSABinary>(parent, type(), SSA_LESS_EQ, _left->emit(parent), _right->emit(parent));
			case AST_GREATER:
				return newref<SSABinary>(parent, type(), SSA_GREATER, _left->emit(parent), _right->emit(parent));
			case AST_GREATER_EQUAL:
				return newref<SSABinary>(parent, type(), SSA_GREATER_EQ, _left->emit(parent), _right->emit(parent));
			default:
				return nullptr;
		}
	}

	Location ASTBinaryRel::emit(Function& func) {
		if (_left->type() == STRING || _right->type() == STRING) {
			vector<Location> args;
			args.push(_left->emit(func));
			args.push(_right->emit(func));
			Location label;
			label.type = LOC_LABEL;
			label.label_index = find_label("_strcmp");
			Location result = func.add(new CallInsn(label, args, INT));

			switch(_op) {
				case AST_LESS:
					return func.add(new LessInsn(result, loc_immediate(0)));
				case AST_LESS_EQUAL:
					return func.add(new LessEqualInsn(result, loc_immediate(0)));
				case AST_GREATER:
					return func.add(new GreaterInsn(result, loc_immediate(0)));
				case AST_GREATER_EQUAL:
					return func.add(new GreaterEqualInsn(result, loc_immediate(0)));
				default:
					return loc_none();
			}
		}

		switch (_op) {
			case AST_LESS:
				return func.add(new LessInsn(_left->emit(func), _right->emit(func)));
			case AST_LESS_EQUAL:
				return func.add(new LessEqualInsn(_left->emit(func), _right->emit(func)));
			case AST_GREATER:
				return func.add(new GreaterInsn(_left->emit(func), _right->emit(func)));
			case AST_GREATER_EQUAL:
				return func.add(new GreaterEqualInsn(_left->emit(func), _right->emit(func)));
			default:
				return loc_none();
		}
	}

	static const char* REL_OP_NAMES[] = {
		"<", "<=", ">", ">="
	};

	void ASTBinaryRel::format(stream& io) const {
		write(io, "(", REL_OP_NAMES[_op], " ", _left, " ", _right, ")");
	}

	ASTDefine::ASTDefine(SourceLocation loc, ref<Env> env, u64 name, ASTNode* value):
		ASTUnary(loc, value), _env(env), _name(name) {}

	const Type* ASTDefine::lazy_type() {
		return VOID;
	}

	ref<SSANode> ASTDefine::emit(ref<BasicBlock>& parent) {
		return newref<SSAStore>(parent, _env, _name, _child->emit(parent));
	}

	Location ASTDefine::emit(Function& func) {
		Location loc = func.create_local(symbol_for(_name), type());
		_env->find(symbol_for(_name))->location = loc;
		func.add(new StoreInsn(loc, _child->emit(func)));
		return loc_none();
	}

	void ASTDefine::format(stream& io) const {
		write(io, "(def ", symbol_for(_name), " ", _child, ")");
	}

	ASTCall::ASTCall(SourceLocation loc, ASTNode* func, const vector<ASTNode*>& args):
		ASTNode(loc), _func(func), _args(args) {
		_func->inc();
		for (ASTNode* n : _args) n->inc();
	}

	ASTCall::~ASTCall() {
		_func->dec();
		for (ASTNode* n : _args) n->dec();
	}

	const Type* ASTCall::lazy_type() {
		const Type* fntype = _func->type();
		const Type* argt = ((const FunctionType*)fntype)->arg();
		if (fntype == ERROR || argt == ERROR) return ERROR;
		vector<const Type*> argts;
		for (u32 i = 0; i < _args.size(); i ++) {
			if (_args[i]->type() == ERROR) return ERROR;
			argts.push(_args[i]->type());
		}
		const Type* provided_argt = find<ProductType>(argts);
		// println("calling ", _func, argt, " on ", provided_argt);
		if (!unify(argt, provided_argt)) {
			err(loc(), "Invalid arguments ", provided_argt, " to ", _func, ".");
			return ERROR;
		}
		return ((const FunctionType*)fntype)->ret();
	}

	ref<SSANode> ASTCall::emit(ref<BasicBlock>& parent) {
		vector<ref<SSANode>> args;
		for (ASTNode* n : _args) args.push(n->emit(parent));
		return newref<SSACall>(parent, type(), _func->emit(parent), args);
	}

	Location ASTCall::emit(Function& func) {
		Location fn = _func->emit(func);
		vector<Location> arglocs;
		const Type* argt = ((const FunctionType*)_func->type())->arg();
		for (u32 i = 0; i < _args.size(); i ++) {
			arglocs.push(_args[i]->emit(func));
		}
		for (u32 i = 0; i < _args.size(); i ++) {
			if (arglocs[i].type == LOC_LABEL) {
				arglocs[i] = func.add(new AddressInsn(arglocs[i], 
					((const ProductType*)argt)->member(i)));
			}
		}
		return func.add(new CallInsn(fn, arglocs, type()));
	}

	void ASTCall::format(stream& io) const {
		write(io, "(", _func);
		for (ASTNode* n : _args) write(io, " ", n);
		write(io, ")");
	}

	const Type* ASTIncompleteFn::lazy_type() {
		return find<FunctionType>(_args, find<TypeVariable>());
	}

	ASTIncompleteFn::ASTIncompleteFn(SourceLocation loc, const Type* args, i64 name):
		ASTNode(loc), _args(args), _name(name) {}

	Location ASTIncompleteFn::emit(Function& func) {
		return loc_label(symbol_for(_name));
	}

	void ASTIncompleteFn::format(stream& io) const {
		if (_name == -1) write(io, "<anonymous>");
		else write(io, symbol_for(_name));
	}

	ASTFunction::ASTFunction(SourceLocation loc, ref<Env> env, const Type* args_type, 
		const vector<u64>& args, ASTNode* body, i64 name):
		ASTNode(loc), _env(env), _args_type(args_type), _args(args), 
		_body(body), _name(name), _emitted(false) {
		_body->inc();
	}

	ASTFunction::~ASTFunction() {
		_body->dec();
	}

	const Type* ASTFunction::lazy_type() {
		if (_args_type == ERROR || _body->type() == ERROR) return ERROR;
		return find<FunctionType>(_args_type, _body->type());
	}

	ref<SSANode> ASTFunction::emit(ref<BasicBlock>& parent) {
		
	}

	Location ASTFunction::emit(Function& func) {
		if (_body->is_extern()) {
			_emitted = true;
			_label = add_label(symbol_for(_name));
			Location loc;
			loc.type = LOC_LABEL;
			loc.label_index = _label;
			return func.add(new AddressInsn(loc, type()));
		}
		else if (!_emitted) {
			_emitted = true;
			Function& fn = _name == -1 ? func.create_function() 
				: func.create_function(symbol_for(_name));
			_label = fn.label();
			for (u32 i = 0; i < _args.size(); i ++) {
				Def* def = _env->find(symbol_for(_args[i]));
				if (def) def->location = fn.add(new LoadArgumentInsn(i, 
					((ProductType*)_args_type)->member(i)));
			}
			fn.add(new RetInsn(_body->emit(fn)));
			fn.last()->succ().clear();
		}

		Location loc;
		loc.type = LOC_LABEL;
		loc.label_index = _label;
		return loc;
	}

	void ASTFunction::format(stream& io) const {
		if (_name == -1) write(io, "<anonymous>");
		else write(io, symbol_for(_name));
	}

	u32 ASTFunction::label() const {
		return _label;
	}

	ASTBlock::ASTBlock(SourceLocation loc, const vector<ASTNode*>& exprs):
		ASTNode(loc), _exprs(exprs) {
		for (ASTNode* n : _exprs) n->inc();
	}

	ASTBlock::~ASTBlock() {
		for (ASTNode* n : _exprs) n->dec();
	}

	const Type* ASTBlock::lazy_type() {
		for (ASTNode* n : _exprs) if (n->type() == ERROR) return ERROR;
		return _exprs.back()->type();
	}

	Location ASTBlock::emit(Function& func) {
		Location loc;
		for (ASTNode* n : _exprs) loc = n->emit(func);
		return loc;
	}

	void ASTBlock::format(stream& io) const {
		write(io, "(do");
		for (ASTNode* n : _exprs) write(io, " ", n);
		write(io, ")");
	}

	ASTIf::ASTIf(SourceLocation loc, ASTNode* cond, ASTNode* if_true, ASTNode* if_false):
		ASTNode(loc), _cond(cond), _if_true(if_true), _if_false(if_false) {
		_cond->inc();
		_if_true->inc();
		_if_false->inc();
	}

	ASTIf::~ASTIf() {
		_cond->dec();
		_if_true->dec();
		_if_false->dec();
	}

	const Type* ASTIf::lazy_type() {
		if (_cond->type() == ERROR || _if_true->type() == ERROR
				|| _if_false->type() == ERROR) return ERROR;
		if (unify(_cond->type(), BOOL) != BOOL) {
			err(_cond->loc(), "Expected condition of type 'bool', given '",
				_cond->type(), "'.");
			return ERROR;
		}
		const Type* left = _if_true->type(), *right = _if_false->type();
		const Type* t = unify(left, right);
		if (!t) {
			err(loc(), "Could not unify types for branches of if expression: '",
				left, "' and '", right, "'.");
			return ERROR;
		}

		return t;
	}

	Location ASTIf::emit(Function& func) {
		u32 _else = next_label(), _end = next_label();
		Location result = func.create_local(type());
		func.add(new IfZeroInsn(_else, _cond->emit(func)));
		Insn* ifz = func.last();
		Location true_result = _if_true->emit(func);
		func.add(new StoreInsn(result, true_result));
		func.add(new GotoInsn(_end));
		Insn* skip = func.last();
		func.add(new Label(_else));
		Insn* elselbl = func.last();
		Location false_result = _if_false->emit(func);
		func.add(new StoreInsn(result, false_result));
		func.add(new Label(_end));
		Insn* end = func.last();
		ifz->succ().push(elselbl); // ifz -> else
		skip->succ()[0] = end; // goto -> end
		return result;
	}

	void ASTIf::format(stream& io) const {
		write(io, "(if ", _cond, " ", _if_true, " ", _if_false, ")");
	}

	ASTWhile::ASTWhile(SourceLocation loc, ASTNode* cond, ASTNode* body):
		ASTNode(loc), _cond(cond), _body(body) {
		_cond->inc();
		_body->inc();
	}

	ASTWhile::~ASTWhile() {
		_cond->dec();
		_body->dec();
	}

	const Type* ASTWhile::lazy_type() {
		const Type* t = unify(_cond->type(), BOOL);
		if (!t) {
			err(loc(), "Invalid condition in 'while' statement: '", 
				_cond->type(), "'.");
			return ERROR;
		}
		if (_body->type() == ERROR) return ERROR;
		return VOID;
	}

	Location ASTWhile::emit(Function& func) {
		u32 _start = next_label(), _end = next_label();
		Location result = func.create_local(type());
		func.add(new Label(_start));
		Insn* start = func.last();
		func.add(new IfZeroInsn(_end, _cond->emit(func)));
		Insn* ifz = func.last();
		_body->emit(func);
		func.add(new GotoInsn(_start));
		Insn* loop = func.last();
		func.add(new Label(_end));
		Insn* end = func.last();
		ifz->succ().push(end);
		loop->succ()[0] = start;
		return result;
	}

	void ASTWhile::format(stream& io) const {
		write(io, "(while ", _cond, " ", _body, ")");
	}

	ASTIsEmpty::ASTIsEmpty(SourceLocation loc, ASTNode* list):
		ASTUnary(loc, list) {}

	const Type* ASTIsEmpty::lazy_type() {
		if (_child->type() == ERROR) return ERROR;
		const Type* ct = _child->type();
		if (ct->kind() != KIND_LIST && !ct->concrete()) 
			ct = unify(ct, find<ListType>(find<TypeVariable>()));
		if (!ct || (ct->kind() != KIND_LIST && ct != VOID)) {
			err(_child->loc(), "Invalid argument to 'empty?' expression: '", 
					_child->type(), "'.");
			return ERROR;
		}
		return BOOL;
	}

	Location ASTIsEmpty::emit(Function& func) {
		return func.add(new EqualInsn(_child->emit(func), loc_immediate(0)));
	}

	void ASTIsEmpty::format(stream& io) const {
		write(io, "(empty? ", _child, ")");
	}

	ASTHead::ASTHead(SourceLocation loc, ASTNode* list):
		ASTUnary(loc, list) {}

	const Type* ASTHead::lazy_type() {
		if (_child->type() == ERROR) return ERROR;
		const Type* ct = _child->type();
		if ((ct->kind() != KIND_LIST && !ct->concrete()) || ct == VOID) 
			ct = unify(ct, find<ListType>(find<TypeVariable>()));
		if (!ct || ct->kind() != KIND_LIST) {
			err(_child->loc(), "Invalid argument to 'head' expression: '",
				_child->type(), "'.");
			return ERROR;
		}
		return ((const ListType*) _child->type())->element();
	}

	Location ASTHead::emit(Function& func) {
		return func.add(new LoadPtrInsn(_child->emit(func), type(), 0));
	}

	void ASTHead::format(stream& io) const {
		write(io, "(head ", _child, ")");
	}

	ASTTail::ASTTail(SourceLocation loc, ASTNode* list):
		ASTUnary(loc, list) {}

	const Type* ASTTail::lazy_type() {
		if (_child->type() == ERROR) return ERROR;
		const Type* ct = _child->type();
		if ((ct->kind() != KIND_LIST && !ct->concrete()) || ct == VOID) 
			ct = unify(ct, find<ListType>(find<TypeVariable>()));
		if (!ct || ct->kind() != KIND_LIST) {
			err(_child->loc(), "Invalid argument to 'tail' expression: '",
				_child->type(), "'.");
			return ERROR;
		}
		return _child->type();
	}

	Location ASTTail::emit(Function& func) {
		return func.add(new LoadPtrInsn(_child->emit(func), type(), 8));
	}

	void ASTTail::format(stream& io) const {
		write(io, "(tail ", _child, ")");
	}

	ASTCons::ASTCons(SourceLocation loc, ASTNode* first, ASTNode* rest):
		ASTBinary(loc, first, rest) {}

	const Type* ASTCons::lazy_type() {
		const Type *first = _left->type(), *rest = _right->type();
		if (first == ERROR || rest == ERROR) return ERROR;
		if (rest == VOID) return find<ListType>(first);
		else {
			if (rest->kind() == KIND_TYPEVAR) 
				return unify(rest, find<ListType>(first));
			if (rest->kind() != KIND_LIST) {
				err(_right->loc(), "Invalid argument to 'cons' expression.");
				return ERROR;
			}
			const Type* element = ((const ListType*)rest)->element();
			if (unify(first, element) != element) {
				err(_left->loc(), "Invalid arguments to 'cons' expression: '",
					first, "' and '", rest, "'.");
				return ERROR;
			}
			return rest;
		}
	}

	Location ASTCons::emit(Function& func) {
		vector<Location> args;
		args.push(_left->emit(func));
		args.push(_right->emit(func));
		Location label;
		label.type = LOC_LABEL;
		label.label_index = find_label("_cons");
		return func.add(new CallInsn(label, args, type()));
	}

	void ASTCons::format(stream& io) const {
		write(io, "(cons ", _left, " ", _right, ")");
	}

	const Type* ASTLength::lazy_type() {
		const Type *child = _child->type();
		if (child == ERROR) return ERROR;
		if (unify(_child->type(), STRING) != STRING
			&& unify(_child->type(), find<ListType>(find<TypeVariable>()))->kind() 
				!= KIND_LIST) {
			err(_child->loc(), "Argument to 'length' expression must be string or list, ",
				"given '", _child->type());
			return ERROR;
		}
		return INT;
	}

	ASTLength::ASTLength(SourceLocation loc, ASTNode* child): 
		basil::ASTUnary(loc, child) {}

  	Location ASTLength::emit(Function& func) {
		vector<Location> args;
		args.push(_child->emit(func));

		Location label;
		label.type = LOC_LABEL;
		if (_child->type() == STRING) label.label_index = find_label("_strlen");
		else label.label_index = find_label("_listlen");
		
		return func.add(new CallInsn(label, args, INT));
  	}

  	void ASTLength::format(stream& io) const {
    	write(io, "(length ", _child, ")");
  	}

	const Type* ASTDisplay::lazy_type() {
		return VOID;
	}

	ASTDisplay::ASTDisplay(SourceLocation loc, ASTNode* node):
		ASTUnary(loc, node) {}

	Location ASTDisplay::emit(Function& func) {
		const char* name;
		if (_child->type() == INT) name = "_display_int";
		else if (_child->type() == SYMBOL) name = "_display_symbol";
		else if (_child->type() == BOOL) name = "_display_bool";
		else if (_child->type() == STRING) name = "_display_string";
		else if (_child->type() == find<ListType>(INT)) name = "_display_int_list";
		else if (_child->type() == find<ListType>(SYMBOL)) name = "_display_symbol_list";
		else if (_child->type() == find<ListType>(BOOL)) name = "_display_bool_list";
		else if (_child->type() == find<ListType>(STRING)) name = "_display_string_list";
		else if (_child->type() == VOID) name = "_display_int_list";
		vector<Location> args;
		args.push(_child->emit(func));
		Location label;
		label.type = LOC_LABEL;
		label.label_index = find_label(name);
		return func.add(new CallInsn(label, args, type()));
	}

	void ASTDisplay::format(stream& io) const {
		write(io, "(display ", _child, ")");
	}

  	ASTNativeCall::ASTNativeCall(SourceLocation loc, const string &func_name, const Type* ret): 
	  	basil::ASTNode(loc), _func_name(func_name), _ret(ret) {}

	ASTNativeCall::ASTNativeCall(SourceLocation loc, const string &func_name, const Type *ret, const vector<ASTNode*>& args, const vector<const Type*>& arg_types): 
		ASTNode(loc), _func_name(func_name), _ret(ret), _args(args), _arg_types(arg_types) {
		for (ASTNode* node : _args) node->inc();
	}

	ASTNativeCall::~ASTNativeCall() {
		for (ASTNode* node : _args) node->dec();
	}

	const Type* ASTNativeCall::lazy_type() {
		for (int i = 0; i < _args.size(); i ++) {
			if (!unify(_args[i]->type(), _arg_types[i])) {
				err(_args[i]->loc(), "Expected '", _arg_types[i], "', given '", _args[i]->type(), "'.");
			}
		}
		return _ret;
	}

  	Location ASTNativeCall::emit(Function& func) {
		vector<Location> args;
		for (int i = 0; i < _args.size(); i ++)
			args.push(_args[i]->emit(func));
		Location label;
		label.type = LOC_LABEL;
		label.label_index = find_label(_func_name);
    	return func.add(new CallInsn(label, args, _ret));
  	}

  void ASTNativeCall::format(stream& io) const {
    write(io, "(", _func_name);
    for (const ASTNode *arg : _args) write(io, " ", arg);
    write(io, ")");
  }
	
	ASTAssign::ASTAssign(SourceLocation loc, const ref<Env> env,
		u64 dest, ASTNode* src): ASTUnary(loc, src), _env(env), _dest(dest) {}

		const Type* ASTAssign::lazy_type() {
		const Type* src_type = _child->type();
		const Def* def = _env->find(symbol_for(_dest));
		const Type* dest_type = ((const RuntimeType*)def->value.type())->base();
		if (src_type == ERROR || dest_type == ERROR)
			return ERROR;
		if (!unify(src_type, dest_type)) {
			err(loc(), "Invalid arguments to assignment '", src_type, "' and '", dest_type, "'.");
			return ERROR;
		}
		return VOID;
	}

	Location ASTAssign::emit(Function& func) {
		const Def* def = _env->find(symbol_for(_dest));
		if (!def) return loc_none();
		return func.add(new StoreInsn(def->location, _child->emit(func)));
	}

	void ASTAssign::format(stream& io) const {
    	write(io, "(= ", symbol_for(_dest), " ", _child, ")");
	}
	
	ASTAnnotate::ASTAnnotate(SourceLocation loc, ASTNode* value, const Type* type):
		ASTNode(loc), _value(value), _type(type) {
		_value->inc();
	}

	ASTAnnotate::~ASTAnnotate() {
		_value->dec();
	}

	const Type* ASTAnnotate::lazy_type() {
		const Type* inferred = unify(_value->type(), _type);
		if (!inferred) {
			err(_value->loc(), "Could not assign type '", _type,
				"' to value of incompatible type '", _value->type(), "'.");
			return ERROR;
		}
		return inferred;
	}

	ref<SSANode> ASTAnnotate::emit(ref<BasicBlock>& parent) {
		return _value->emit(parent);
	}

	Location ASTAnnotate::emit(Function& function) {
		return _value->emit(function);
	}

	void ASTAnnotate::format(stream& io) const {
		write(io, "(: ", _value, " ", _type, ")");
	}
}

void write(stream& io, basil::ASTNode* n) {
	n->format(io);
}

void write(stream& io, const basil::ASTNode* n) {
	n->format(io);
}