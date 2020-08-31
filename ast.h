#ifndef BASIL_AST_H
#define BASIL_AST_H

#include "util/defs.h"
#include "util/str.h"
#include "util/rc.h"
#include "util/io.h"
#include "errors.h"
#include "type.h"
#include "values.h"
#include "ssa.h"

namespace basil {
	class Def;
	class Env;
	
	class ASTNode : public RC {
		SourceLocation _loc;
		const Type* _type;
	protected:
		virtual const Type* lazy_type() = 0;
	public:
		ASTNode(SourceLocation loc);
		virtual ~ASTNode();

		SourceLocation loc() const;
		const Type* type();
		virtual Location emit(Function& function) = 0;
		virtual void format(stream& io) const = 0;
	};

	class ASTSingleton : public ASTNode {
		const Type* _type;
	protected:
		const Type* lazy_type() override;
	public:
		ASTSingleton(const Type* type);
	
		Location emit(Function& function) override;
		void format(stream& io) const override;
	};

	class ASTVoid : public ASTNode {
	protected:
		const Type* lazy_type() override;
	public:
		ASTVoid(SourceLocation loc);

		Location emit(Function& function) override;
		void format(stream& io) const override;
	};

	class ASTInt : public ASTNode {
		i64 _value;
	protected:
		const Type* lazy_type() override;
	public:
		ASTInt(SourceLocation loc, i64 value);

		Location emit(Function& function) override;
		void format(stream& io) const override;
	};

	class ASTSymbol : public ASTNode {
		u64 _value;
	protected:
		const Type* lazy_type() override;
	public:
		ASTSymbol(SourceLocation loc, u64 value);

		Location emit(Function& function) override;
		void format(stream& io) const override;
	};

	class ASTString : public ASTNode {
		string _value;
	protected:
		const Type* lazy_type() override;
	public:
		ASTString(SourceLocation, const string& value);

		Location emit(Function& function) override;
		void format(stream& io) const override;
	};

	class ASTBool : public ASTNode {
		bool _value;
	protected:
		const Type* lazy_type() override;
	public:
		ASTBool(SourceLocation loc, bool value);

		Location emit(Function& function) override;
		void format(stream& io) const override;
	};

	class ASTVar : public ASTNode {
		ref<Env> _env;
		u64 _name;
	protected:
		const Type* lazy_type() override;
	public:
		ASTVar(SourceLocation loc, const ref<Env> env, u64 name);

		Location emit(Function& function) override;
		void format(stream& io) const override;
	};

	class ASTUnary : public ASTNode {
	protected:
		ASTNode *_child;
	public:
		ASTUnary(SourceLocation loc, ASTNode* child);
		~ASTUnary();
	};

	class ASTBinary : public ASTNode {
	protected:
		ASTNode *_left, *_right;
	public:
		ASTBinary(SourceLocation loc, ASTNode* left, ASTNode* right);
		~ASTBinary();
	};
	
	enum ASTMathOp {
		AST_ADD,
		AST_SUB,
		AST_MUL,
		AST_DIV,
		AST_REM
	};

	class ASTBinaryMath : public ASTBinary {
		ASTMathOp _op;
	protected:
		const Type* lazy_type() override;
	public:
		ASTBinaryMath(SourceLocation loc, ASTMathOp op, 
			ASTNode* left, ASTNode* right);

		Location emit(Function& function) override;
		void format(stream& io) const override;
	};
	
	enum ASTLogicOp {
		AST_AND,
		AST_OR,
		AST_XOR,
		AST_NOT
	};

	class ASTBinaryLogic : public ASTBinary {
		ASTLogicOp _op;
	protected:
		const Type* lazy_type() override;
	public:
		ASTBinaryLogic(SourceLocation loc, ASTLogicOp op, 
			ASTNode* left, ASTNode* right);

		Location emit(Function& function) override;
		void format(stream& io) const override;
	};

	class ASTNot : public ASTUnary {
	protected:
		const Type* lazy_type() override;
	public:
		ASTNot(SourceLocation loc, ASTNode* child);

		Location emit(Function& function) override;
		void format(stream& io) const override;
	};
	
	enum ASTEqualOp {
		AST_EQUAL,
		AST_INEQUAL
	};

	class ASTBinaryEqual : public ASTBinary {
		ASTEqualOp _op;
	protected:
		const Type* lazy_type() override;
	public:
		ASTBinaryEqual(SourceLocation loc, ASTEqualOp op, 
			ASTNode* left, ASTNode* right);

		Location emit(Function& function) override;
		void format(stream& io) const override;
	};
	
	enum ASTRelOp {
		AST_LESS,
		AST_LESS_EQUAL,
		AST_GREATER,
		AST_GREATER_EQUAL
	};

	class ASTBinaryRel : public ASTBinary {
		ASTRelOp _op;
	protected:
		const Type* lazy_type() override;
	public:
		ASTBinaryRel(SourceLocation loc, ASTRelOp op, 
			ASTNode* left, ASTNode* right);

		Location emit(Function& function) override;
		void format(stream& io) const override;
	};

	class ASTDefine : public ASTUnary {
		ref<Env> _env;
		u64 _name;
	protected:
		const Type* lazy_type() override;
	public:
		ASTDefine(SourceLocation loc, ref<Env> env, u64 name, ASTNode* value);

		Location emit(Function& function) override;
		void format(stream& io) const override;
	};

	class ASTCall : public ASTNode {
		ASTNode* _func;
		vector<ASTNode*> _args;
	protected:
		const Type* lazy_type() override;
	public:
		ASTCall(SourceLocation loc, ASTNode* func, const vector<ASTNode*>& args);
		~ASTCall();

		Location emit(Function& function) override;
		void format(stream& io) const override;
	};

	class ASTIncompleteFn : public ASTNode {
		const Type* _args;
		i64 _name;
	protected:
		const Type* lazy_type() override;
	public:
		ASTIncompleteFn(SourceLocation loc, const Type* args, i64 name);

		Location emit(Function& function) override;
		void format(stream& io) const override;
	};

	class ASTFunction : public ASTNode {
		ref<Env> _env;
		const Type* _args_type;
		vector<u64> _args;
		ASTNode* _body;
		i64 _name;
		bool _emitted;
		u32 _label;
	protected:
		const Type* lazy_type() override;
	public:
		ASTFunction(SourceLocation loc, ref<Env> env, const Type* args_type, 
			const vector<u64>& args, ASTNode* body, i64 name = -1);
		~ASTFunction();

		Location emit(Function& function) override;
		void format(stream& io) const override;
	};

	class ASTBlock : public ASTNode {
		vector<ASTNode*> _exprs;
	protected:
		const Type* lazy_type() override;
	public:
		ASTBlock(SourceLocation loc, const vector<ASTNode*>& exprs);
		~ASTBlock();

		Location emit(Function& function) override;
		void format(stream& io) const override;
	};

	class ASTIf : public ASTNode {
		ASTNode *_cond, *_if_true, *_if_false;
	protected:
		const Type* lazy_type() override;
	public:
		ASTIf(SourceLocation loc, ASTNode* cond, ASTNode* if_true, ASTNode* if_false);
		~ASTIf();

		Location emit(Function& function) override;
		void format(stream& io) const override;
	};

	class ASTWhile : public ASTNode {
		ASTNode *_cond, *_body;
	protected:
		const Type* lazy_type() override;
	public:
		ASTWhile(SourceLocation loc, ASTNode* cond, ASTNode* body);
		~ASTWhile();

		Location emit(Function& function) override;
		void format(stream& io) const override;
	};

	class ASTIsEmpty : public ASTUnary {
	protected:
		const Type* lazy_type() override;
	public:
		ASTIsEmpty(SourceLocation loc, ASTNode* list);

		Location emit(Function& function) override;
		void format(stream& io) const override;
	};

	class ASTHead : public ASTUnary {
	protected:
		const Type* lazy_type() override;
	public:
		ASTHead(SourceLocation loc, ASTNode* list);

		Location emit(Function& function) override;
		void format(stream& io) const override;
	};

	class ASTTail : public ASTUnary {
	protected:
		const Type* lazy_type() override;
	public:
		ASTTail(SourceLocation loc, ASTNode* list);

		Location emit(Function& function) override;
		void format(stream& io) const override;
	};

	class ASTCons : public ASTBinary {
	protected:
		const Type* lazy_type() override;
	public:
		ASTCons(SourceLocation loc, ASTNode* first, ASTNode* rest);

		Location emit(Function& function) override;
		void format(stream& io) const override;
	};

	class ASTDisplay : public ASTUnary {
	protected:
		const Type* lazy_type() override;
	public:
		ASTDisplay(SourceLocation loc, ASTNode* node);

		Location emit(Function& function) override;
		void format(stream& io) const override;
	};

  class ASTReadLine : public ASTNode {
    protected:
      const Type* lazy_type() override;
    public:
      ASTReadLine(SourceLocation loc);

      Location emit(Function& function) override;
      void format(stream& io) const override;
  };

  // class ASTNativeCall : public ASTNode {
  //     const string _func_name;
  //     const Type * _ret;
  //     vector<ASTNode*> _args;
  //   protected:
  //     const Type* lazy_type() override;
  //   public:
  //     ASTNativeCall(SourceLocation loc, const string &func_name, vector<ASTNode*> args);

  //     Location emit(Function& function) override;
  //     void format(stream& io) const override;
  // };

	class ASTAssign : public ASTUnary {
		ref<Env> _env;
		u64 _dest;
	protected:
		const Type* lazy_type() override;
	public:
		ASTAssign(SourceLocation loc, const ref<Env> env, 
			u64 dest, ASTNode* src);

		Location emit(Function& function) override;
		void format(stream& io) const override;
	};
}

void write(stream& io, basil::ASTNode* t); 
void write(stream& io, const basil::ASTNode* t); 

#endif