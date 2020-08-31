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
		if (_type->kind() == KIND_TYPEVAR 
			&& ((const TypeVariable*)_type)->actual() != ANY)
			return ((const TypeVariable*)_type)->actual(); // unwrap concrete typevars
		return _type;
	}

	ASTSingleton::ASTSingleton(const Type* type):
		ASTNode(NO_LOCATION), _type(type) {}

	const Type* ASTSingleton::lazy_type() {
		return _type;
	}

	Location ASTSingleton::emit(Function& func) {
		return ssa_none();
	}

	void ASTSingleton::format(stream& io) const {
		write(io, "<just ", _type, ">");
	}

	ASTVoid::ASTVoid(SourceLocation loc):
		ASTNode(loc) {}

	const Type* ASTVoid::lazy_type() {
		return VOID;
	}

	Location ASTVoid::emit(Function& function) {
		return ssa_immediate(0);
	}

	void ASTVoid::format(stream& io) const {
		write(io, "[]");
	}

	ASTInt::ASTInt(SourceLocation loc, i64 value):
		ASTNode(loc), _value(value) {}

	const Type* ASTInt::lazy_type() {
		return INT;
	}

	Location ASTInt::emit(Function& func) {
		return ssa_immediate(_value);
	}

	void ASTInt::format(stream& io) const {
		write(io, _value);
	}

	ASTSymbol::ASTSymbol(SourceLocation loc, u64 value):
		ASTNode(loc), _value(value) {}

	const Type* ASTSymbol::lazy_type() {
		return SYMBOL;
	}

	Location ASTSymbol::emit(Function& func) {
		return ssa_immediate(_value);
	}

	void ASTSymbol::format(stream& io) const {
		write(io, symbol_for(_value));
	}

	ASTString::ASTString(SourceLocation loc, const string& value):
		ASTNode(loc), _value(value) {}

	const Type* ASTString::lazy_type() {
		return STRING;
	}

	Location ASTString::emit(Function& func) {
		return func.add(new AddressInsn(ssa_const(ssa_next_label(), _value), type()));
	}

	void ASTString::format(stream& io) const {
		write(io, _value);
	}

	ASTBool::ASTBool(SourceLocation loc, bool value):
		ASTNode(loc), _value(value) {}

	const Type* ASTBool::lazy_type() {
		return BOOL;
	}

	Location ASTBool::emit(Function& func) {
		return ssa_immediate(_value ? 1 : 0);
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

	Location ASTVar::emit(Function& func) {
		const Def* def = _env->find(symbol_for(_name));
		if (!def) return ssa_none();
		return func.add(new LoadInsn(def->location));
	}

	void ASTVar::format(stream& io) const {
		write(io, symbol_for(_name));
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
				return ssa_none();
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

	Location ASTBinaryLogic::emit(Function& func) {
		switch (_op) {
			case AST_AND:
				return func.add(new AndInsn(_left->emit(func), _right->emit(func)));
			case AST_OR:
				return func.add(new OrInsn(_left->emit(func), _right->emit(func)));
			case AST_XOR:
				return func.add(new XorInsn(_left->emit(func), _right->emit(func)));
			default:
				return ssa_none();
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

	Location ASTNot::emit(Function& function) {
		return function.add(new NotInsn(_child->emit(function)));
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

	Location ASTBinaryEqual::emit(Function& func) {
    if (_left->type() == STRING || _right->type() == STRING) {
      func.add(new StoreArgumentInsn(_left->emit(func), 0, _left->type()));
      func.add(new StoreArgumentInsn(_right->emit(func), 1, _right->type()));
      Location result = func.add(new CallInsn(ssa_find_label("_strcmp"), INT));
      return func.add(new EqualInsn(result, ssa_immediate(0)));
    }

		switch (_op) {
			case AST_EQUAL:
				return func.add(new EqualInsn(_left->emit(func), _right->emit(func)));
			case AST_INEQUAL:
				return func.add(new InequalInsn(_left->emit(func), _right->emit(func)));
			default:
				return ssa_none();
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

	Location ASTBinaryRel::emit(Function& func) {
    if (_left->type() == STRING || _right->type() == STRING) {
      func.add(new StoreArgumentInsn(_left->emit(func), 0, _left->type()));
      func.add(new StoreArgumentInsn(_right->emit(func), 1, _right->type()));
      Location result = func.add(new CallInsn(ssa_find_label("_strcmp"), INT));

      switch(_op) {
        case AST_LESS:
          return func.add(new LessInsn(result, ssa_immediate(0)));
        case AST_LESS_EQUAL:
          return func.add(new LessEqualInsn(result, ssa_immediate(0)));
        case AST_GREATER:
          return func.add(new GreaterInsn(result, ssa_immediate(0)));
        case AST_GREATER_EQUAL:
          return func.add(new GreaterEqualInsn(result, ssa_immediate(0)));
        default:
          return ssa_none();
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
				return ssa_none();
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

	Location ASTDefine::emit(Function& func) {
		Location loc = func.create_local(symbol_for(_name), type());
		_env->find(symbol_for(_name))->location = loc;
		func.add(new StoreInsn(loc, _child->emit(func), true));
		return ssa_none();
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
		for (u32 i = 0; i < _args.size(); i ++) {
			if (_args[i]->type() == ERROR) return ERROR;
			if (!unify(_args[i]->type(), ((const ProductType*)argt)->member(i))) {
				err(_args[i]->loc(), "Invalid argument ", i, " to function call.");
				return ERROR;
			}
		}
		return ((const FunctionType*)fntype)->ret();
	}

	Location ASTCall::emit(Function& func) {
		Location fn = _func->emit(func);
		vector<Location> arglocs;
		const Type* argt = ((const FunctionType*)_func->type())->arg();
		for (u32 i = 0; i < _args.size(); i ++) {
			arglocs.push(_args[i]->emit(func));
		}
		for (u32 i = 0; i < _args.size(); i ++) {
			func.add(new StoreArgumentInsn(arglocs[i], i, 
				((const ProductType*)argt)->member(i)));
		}
		return func.add(new CallInsn(fn.label_index, type()));
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
		Location loc;
		loc.type = SSA_LABEL;
		loc.label_index = ssa_find_label(symbol_for(_name));
		return loc;
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

	Location ASTFunction::emit(Function& func) {
		if (!_emitted) {
			Function& fn = _name == -1 ? func.create_function() 
				: func.create_function(symbol_for(_name));
			_label = fn.label();
			for (u32 i = 0; i < _args.size(); i ++) {
				Def* def = _env->find(symbol_for(_args[i]));
				if (def) def->location = fn.add(new LoadArgumentInsn(i, 
					((ProductType*)_args_type)->member(i)));
			}
			fn.add(new RetInsn(_body->emit(fn)));

			_emitted = true;
		}

		Location loc;
		loc.type = SSA_LABEL;
		loc.label_index = _label;
		return loc;
	}

	void ASTFunction::format(stream& io) const {
		if (_name == -1) write(io, "<anonymous>");
		else write(io, symbol_for(_name));
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
		u32 _else = ssa_next_label(), _end = ssa_next_label();
		Location result = func.create_local(type());
		func.add(new IfZeroInsn(_else, _cond->emit(func)));
		Location true_result = _if_true->emit(func);
		func.add(new StoreInsn(result, true_result, true));
		func.add(new GotoInsn(_end));
		func.add(new Label(_else));
		Location false_result = _if_false->emit(func);
		func.add(new StoreInsn(result, false_result, true));
		func.add(new Label(_end));
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
		u32 _start = ssa_next_label(), _end = ssa_next_label();
		Location result = func.create_local(type());
		func.add(new Label(_start));
		func.add(new IfZeroInsn(_end, _cond->emit(func)));
		_body->emit(func);
		func.add(new GotoInsn(_start));
		func.add(new Label(_end));
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

	Location ASTIsEmpty::emit(Function& function) {
		return function.add(new EqualInsn(_child->emit(function), ssa_immediate(0)));
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

	Location ASTHead::emit(Function& function) {
		return function.add(new LoadPtrInsn(_child->emit(function), type(), 0));
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

	Location ASTTail::emit(Function& function) {
		return function.add(new LoadPtrInsn(_child->emit(function), type(), 8));
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

	Location ASTCons::emit(Function& function) {
		Location l = _left->emit(function), r = _right->emit(function);
		function.add(new StoreArgumentInsn(l, 0, _left->type()));
		function.add(new StoreArgumentInsn(r, 1, _right->type()));
		return function.add(new CallInsn(ssa_find_label("_cons"), type()));
	}

	void ASTCons::format(stream& io) const {
		write(io, "(cons ", _left, " ", _right, ")");
	}

	const Type* ASTDisplay::lazy_type() {
		return VOID;
	}

	ASTDisplay::ASTDisplay(SourceLocation loc, ASTNode* node):
		ASTUnary(loc, node) {}

	Location ASTDisplay::emit(Function& function) {
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
		function.add(new StoreArgumentInsn(_child->emit(function), 0, _child->type()));
		return function.add(new CallInsn(ssa_find_label(name), type()));
	}

	void ASTDisplay::format(stream& io) const {
		write(io, "(display ", _child, ")");
	}

  const Type* ASTReadLine::lazy_type() {
    return STRING;
  }
  
  ASTReadLine::ASTReadLine(SourceLocation loc) : ASTNode(loc) {}

  Location ASTReadLine::emit(Function& function) {
    return function.add(new CallInsn(ssa_find_label("_read_line"), STRING));
  }

  void ASTReadLine::format(stream& io) const {
    write(io, "(read-line)");
  }

  // const Type* ASTNativeCall::lazy_type() {

  // }

  // ASTNativeCall::ASTNativeCall(SourceLocation loc, const string &func, vector<ASTNode*> args)
  //   : ASTNode(loc), _func(func), _args(args) {}

  // Location ASTNativeCall::emit(Function& function) {
  //   for (int i = 0; i < _args.size(); i ++)
  //     function.add(new StoreArgumentInsn(_args[i]->emit(function), i, _args[i]->type()));
  //   // return function.add(new CallInsn(ssa_find_label(_func), ));
  // }

  // void ASTNativeCall::format(stream& io) const {
  //   write(io, "(", _func, _args.size() ? " " : "");
  //   for (const ASTNode *arg : _args) {
  //     write(io, " ", arg);
  //   }
  //   write(io, ")");
  // }
  

	
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
		if (!def) return ssa_none();
		return func.add(new StoreInsn(def->location, _child->emit(func), false));
	}

	void ASTAssign::format(stream& io) const {
    write(io, "(= ", symbol_for(_dest), " ", _child, ")");
	}
}

void write(stream& io, basil::ASTNode* n) {
	n->format(io);
}

void write(stream& io, const basil::ASTNode* n) {
	n->format(io);
}