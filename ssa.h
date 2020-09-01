#ifndef BASIL_SSA_H
#define BASIL_SSA_H

#include "util/defs.h"
#include "util/str.h"
#include "util/vec.h"
#include "util/io.h"
#include "type.h"

#include "jasmine/x64.h"

namespace basil {
	using namespace jasmine;

	enum LocationType {
		SSA_NONE,
		SSA_LOCAL,
		SSA_IMMEDIATE,
		SSA_CONSTANT,
		SSA_LABEL
	};

	struct LocalInfo {
		string name;
		u32 index;
		const Type* type;
		x64::Arg value;
	};

	struct ConstantInfo {
		string name;
		vector<u8> data;
		const Type* type;
		x64::Arg value;
	};

	struct Location {
		union {
			u32 local_index;
			i64 immediate;
			u32 constant_index;
			u32 label_index;
		};
		LocationType type;

		const Type* value_type();
	};

	Location ssa_none();
	Location ssa_immediate(i64 i);
	const Type* ssa_type(const Location& loc);
	x64::Arg x64_arg(const Location& loc);

	class Function;

	class Insn {
	protected:
		Function* _func;
		Location _loc;
		virtual Location lazy_loc() = 0;

		friend class Function;
		void setfunc(Function* func);
	public:
		Insn();
		virtual ~Insn();

		Location loc();
		virtual void emit() = 0;
		virtual void format(stream& io) const = 0;
	};

	u32 ssa_find_label(const string& label);
	u32 ssa_add_label(const string& label);
	u32 ssa_next_label();
	Location ssa_next_local(const Type* t);
	Location ssa_const(u32 label, const string& constant);
	void ssa_emit_constants(Object& object);

	class Function {
		vector<Function*> _fns;
		vector<Insn*> _insns;
		i64 _stack;
		vector<Location> _locals;
		map<u32, u32> _labels;
		u32 _label;
		Function(u32 label);
	public:
		Function(const string& label);
		~Function();

		void place_label(u32 label);
		Function& create_function();
		Function& create_function(const string& name);
		Location create_local(const Type* t);
		Location create_local(const string& name, const Type* t);
		Location next_local(const Location& loc);
		Location add(Insn* insn);
		u32 label() const;
		void allocate();
		void emit(Object& obj);
		void format(stream& io) const;
	};

	class LoadInsn : public Insn {
		Location _src;
	protected:
		Location lazy_loc() override;
	public:
		LoadInsn(Location src);

		void emit() override;
		void format(stream& io) const override;
	};

	class StoreInsn : public Insn {
		Location _dest, _src;
		bool _init;
	protected:
		Location lazy_loc() override;
	public:
		StoreInsn(Location dest, Location src, bool init);

		void emit() override;
		void format(stream& io) const override;
	};	
	
	class LoadPtrInsn : public Insn {
		Location _src;
		const Type* _type;
		i32 _offset;
	protected:
		Location lazy_loc() override;
	public:
		LoadPtrInsn(Location src, const Type* t, i32 offset);

		void emit() override;
		void format(stream& io) const override;
	};

	class StorePtrInsn : public Insn {
		Location _dest, _src;
		i32 _offset;
	protected:
		Location lazy_loc() override;
	public:
		StorePtrInsn(Location dest, Location src, i32 offset);

		void emit() override;
		void format(stream& io) const override;
	};

	class AddressInsn : public Insn {
		Location _src;
		const Type* _type;
	protected:
		Location lazy_loc() override;
	public:
		AddressInsn(Location src, const Type* t);

		void emit() override;
		void format(stream& io) const override;
	};

	class BinaryInsn : public Insn {
		const char* _name;
	protected:
		Location _left, _right;
	public:
		BinaryInsn(const char* name, Location left, Location right);

		void format(stream& io) const override;
	};

	class BinaryMathInsn : public BinaryInsn {
	protected:
		Location lazy_loc() override;
	public:
		BinaryMathInsn(const char* name, Location left,
			Location right);
	};

	class AddInsn : public BinaryMathInsn {
	public:
		AddInsn(Location left, Location right);
		void emit() override;
	};

	class SubInsn : public BinaryMathInsn {
	public:
		SubInsn(Location left, Location right);
		void emit() override;
	};

	class MulInsn : public BinaryMathInsn {
	public:
		MulInsn(Location left, Location right);
		void emit() override;
	};

	class DivInsn : public BinaryMathInsn {
	public:
		DivInsn(Location left, Location right);
		void emit() override;
	};

	class RemInsn : public BinaryMathInsn {
	public:
		RemInsn(Location left, Location right);
		void emit() override;
	};

	class BinaryLogicInsn : public BinaryInsn {
	protected:
		Location lazy_loc() override;
	public:
		BinaryLogicInsn(const char* name, Location left,
			Location right);
	};

	class AndInsn : public BinaryLogicInsn {
	public:
		AndInsn(Location left, Location right);
		void emit() override;
	};

	class OrInsn : public BinaryLogicInsn {
	public:
		OrInsn(Location left, Location right);
		void emit() override;
	};

	class XorInsn : public BinaryLogicInsn {
	public:
		XorInsn(Location left, Location right);
		void emit() override;
	};

	class NotInsn : public Insn {
		Location _src;
	protected:
		Location lazy_loc() override;
	public:
		NotInsn(Location src);

		void emit() override;
		void format(stream& io) const override;
	};

	class BinaryEqualityInsn : public BinaryInsn {
	protected:
		Location lazy_loc() override;
	public:
		BinaryEqualityInsn(const char* name, Location left,
			Location right);
	};

	class EqualInsn : public BinaryEqualityInsn {
	public:
		EqualInsn(Location left, Location right);
		void emit() override;
	};

	class InequalInsn : public BinaryEqualityInsn {
	public:
		InequalInsn(Location left, Location right);
		void emit() override;
	};

	class BinaryRelationInsn : public BinaryInsn {
	protected:
		Location lazy_loc() override;
	public:
		BinaryRelationInsn(const char* name, Location left,
			Location right);
	};

	class LessInsn : public BinaryRelationInsn {
	public:
		LessInsn(Location left, Location right);
		void emit() override;
	};

	class LessEqualInsn : public BinaryRelationInsn {
	public:
		LessEqualInsn(Location left, Location right);
		void emit() override;
	};

	class GreaterInsn : public BinaryRelationInsn {
	public:
		GreaterInsn(Location left, Location right);
		void emit() override;
	};

	class GreaterEqualInsn : public BinaryRelationInsn {
	public:
		GreaterEqualInsn(Location left, Location right);
		void emit() override;
	};

	class RetInsn : public Insn {
		Location _src;
	protected:
		Location lazy_loc() override;
	public:
		RetInsn(Location src);

		void emit() override;
		void format(stream& io) const override;
	};

	class LoadArgumentInsn : public Insn {
		u32 _index;
		const Type* _type;
	protected:
		Location lazy_loc() override;
	public:
		LoadArgumentInsn(u32 index, const Type* type);

		void emit() override;
		void format(stream& io) const override;
	};

	class StoreArgumentInsn : public Insn {
		Location _src;
		u32 _index;
		const Type* _type;
	protected:
		Location lazy_loc() override;
	public:
		StoreArgumentInsn(Location src, u32 index, const Type* type);

		void emit() override;
		void format(stream& io) const override;
	};

	class CallInsn : public Insn {
		Location _fn;
		const Type* _ret;
	protected:
		Location lazy_loc() override;
	public:
		CallInsn(Location fn, const Type* ret);

		void emit() override;
		void format(stream& io) const override;
	};

	class Label : public Insn {
		u32 _label;
	protected:
		Location lazy_loc() override;
	public:
		Label(u32 label);

		void emit() override;
		void format(stream& io) const override;
	};

	class GotoInsn : public Insn {
		u32 _label;
	protected:
		Location lazy_loc() override;
	public:
		GotoInsn(u32 label);

		void emit() override;
		void format(stream& io) const override;
	};

	class IfZeroInsn : public Insn {
		u32 _label;
		Location _cond;
		const Type* _result;
	protected:
		Location lazy_loc() override;
	public:
		IfZeroInsn(u32 label, Location cond);

		void emit() override;
		void format(stream& io) const override;
	};
}

void write(stream& io, const basil::Location& loc);
void write(stream& io, basil::Insn* insn);
void write(stream& io, const basil::Insn* insn);
void write(stream& io, const basil::Function& func);

#endif