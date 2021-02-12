#include "type.h"

namespace basil {

  Type::Type(u64 hash) : _hash(hash) {}
  
  u64 Type::hash() const {
    return _hash;
  }

	bool Type::concrete() const {
		return this != ANY;
	}

	const Type* Type::concretify() const {
		return this;
	}
  
  bool Type::coerces_to(const Type* other) const {
    return other == this
      || other == ANY 
      || other->kind() == KIND_RUNTIME && this->coerces_to(((const RuntimeType*)other)->base())
      || this == VOID && other->kind() == KIND_LIST
      || other->kind() == KIND_NAMED && ((const NamedType*)other)->base() == this
      || other->kind() == KIND_SUM && ((const SumType*)other)->has(this);
  }

  SingletonType::SingletonType(const string& repr) : Type(::hash(repr)),  _repr(repr) {}

  TypeKind SingletonType::kind() const {
    return KIND_SINGLETON;
  }

  bool SingletonType::operator==(const Type& other) const {
    return other.kind() == kind() && ((const SingletonType&) other)._repr == _repr;
  }
  
  void SingletonType::format(stream& io) const {
    write(io, _repr);
  }

  // NumericType

  NumericType::NumericType(u32 size, bool floating):
    Type(2659161638339667757ul ^ ::hash(size) ^ ::hash(floating)),
    _size(size), _floating(floating) {}

  bool NumericType::floating() const {
    return _floating;
  }

  u32 NumericType::size() const {
    return _size;
  }

  TypeKind NumericType::kind() const {
    return KIND_NUMERIC;
  }

  bool NumericType::operator==(const Type& other) const {
    return other.kind() == KIND_NUMERIC
      && ((const NumericType&)other)._floating == _floating
      && ((const NumericType&)other)._size == _size;
  }

  bool NumericType::coerces_to(const Type* other) const {
    if (Type::coerces_to(other)) return true;
    if (other->kind() != KIND_NUMERIC) return false;

    bool other_floating = ((const NumericType*)other)->floating();
    u32 other_size = ((const NumericType*)other)->size();
    if (floating() == other_floating) return other_size >= size(); // both float, or both non-float
    else if (!floating() && other_floating) return other_size > size(); // works for power-of-two type sizes
    return false;
  }

  void NumericType::format(stream& io) const {
    write(io, floating() ? "f" : "i", size());
  }

  // NamedType

  NamedType::NamedType(const string& name, const Type* base):
    Type(1921110990418496011ul ^ ::hash(name) ^ base->hash()),
    _name(name), _base(base) {}

  const string& NamedType::name() const {
    return _name;
  }

  const Type* NamedType::base() const {
    return _base;
  }

  TypeKind NamedType::kind() const {
    return KIND_NAMED;
  }

  bool NamedType::operator==(const Type& other) const {
    return other.kind() == KIND_NAMED
      && ((const NamedType&)other)._base == _base 
      && ((const NamedType&)other)._name == _name;
  }

  bool NamedType::coerces_to(const Type* other) const {
    return Type::coerces_to(other) || other == _base;
  }

  void NamedType::format(stream& io) const {
    write(io, _name);
  }

  // ListType

  ListType::ListType(const Type* element):
    Type(element->hash() ^ 11340086872871314823ul), _element(element) {}

	bool ListType::concrete() const {
		return _element->concrete();
	}

	const Type* ListType::concretify() const {
    return find<ListType>(_element->concretify());
	}

  const Type* ListType::element() const {
    return _element;
  }

  TypeKind ListType::kind() const {
    return KIND_LIST;
  }

  bool ListType::operator==(const Type& other) const {
    return other.kind() == kind() && 
      ((const ListType&) other).element() == element();
  }

  bool ListType::coerces_to(const Type* other) const {
    return Type::coerces_to(other) || other == TYPE && _element == TYPE;
  }

  void ListType::format(stream& io) const {
    write(io, "[", _element, "]");
  }
  
  // ArrayType

  ArrayType::ArrayType(const Type* element):
    Type(element->hash() ^ 11745103813974897731ul), 
    _element(element), _fixed(false), _size(0) {}

  ArrayType::ArrayType(const Type* element, u32 size):
    Type(element->hash() ^ 5095961086520768179ul * ::hash(size)), 
    _element(element), _fixed(true), _size(size) {}

  const Type* ArrayType::element() const {
    return _element;
  }

  u32 ArrayType::count() const {
    return _size;
  }

  bool ArrayType::fixed() const {
    return _fixed;
  }

  bool ArrayType::concrete() const {
    return _element->concrete();
  }

  const Type* ArrayType::concretify() const {
    return _fixed ? find<ArrayType>(_element->concretify(), _size) 
      : find<ArrayType>(_element->concretify());
  }

  TypeKind ArrayType::kind() const {
    return KIND_ARRAY;
  }

  bool ArrayType::operator==(const Type& other) const {
    return other.kind() == kind() && 
      ((const ArrayType&) other).element() == element() && 
      ((const ArrayType&) other).fixed() == fixed() && 
      (((const ArrayType&) other).count() == count() || !fixed());
  }

  bool ArrayType::coerces_to(const Type* other) const {
    if (Type::coerces_to(other)) return true;
    
    if (other->kind() == KIND_ARRAY 
        && ((const ArrayType*)other)->element() == element() 
        && !((const ArrayType*)other)->fixed()) return true;

    if (fixed() && other->kind() == KIND_PRODUCT 
        && ((const ProductType*)other)->count() == count()) {
      for (u32 i = 0; i < count(); i ++)
        if (((const ProductType*)other)->member(i) != element()) return false;
      return true;
    }

    return false;
  }

  void ArrayType::format(stream& io) const {
    if (_fixed) write(io, _element, "[", _size, "]");
    else write(io, _element, "[]");
  }

  u64 set_hash(const set<const Type*>& members) {
    u64 h = 6530804687830202173ul;
    for (const Type* t : members) h ^= t->hash();
    return h;
  }

  SumType::SumType(const set<const Type*>& members):
    Type(2853124965035107823ul ^ set_hash(members)), _members(members) {}

  bool SumType::has(const Type* member) const {
    return _members.find(member) != _members.end();
  }

  TypeKind SumType::kind() const {
    return KIND_SUM;
  }

  bool SumType::operator==(const Type& other) const {
    if (other.kind() != kind()) return false;
    for (const Type* t : _members)
      if (!((const SumType&) other).has(t)) return false;
    return true;
  }

  bool SumType::coerces_to(const Type* other) const {
    if (Type::coerces_to(other)) return true;

    if (other->kind() == KIND_SUM) {
      for (const Type* t : _members) 
        if (!((const SumType*)other)->has(t)) return false;
      return true;
    }

    return false;
  }

  void SumType::format(stream& io) const {
    write(io, "(");
    bool first = true;
    for (const Type* t : _members) {
      write(io, first ? "" : " | ", t);
      first = false;
    }
    write(io, ")");
  }

  IntersectType::IntersectType(const set<const Type*>& members):
    Type(15263450813870290249ul ^ set_hash(members)), _members(members), _has_function(false) {
    for (const Type* t : _members) if (t->kind() == KIND_FUNCTION) _has_function = true;
  }

  bool IntersectType::has(const Type* member) const {
    return _members.find(member) != _members.end();
  }

  bool IntersectType::has_function() const {
    return _has_function;
  }

  TypeKind IntersectType::kind() const {
    return KIND_INTERSECT;
  }

  bool IntersectType::operator==(const Type& other) const {
    if (other.kind() != kind()) return false;
    for (const Type *t : _members)
      if (!((const IntersectType&)other).has(t)) return false;
    return true;
  }

  bool IntersectType::coerces_to(const Type* other) const {
    if (Type::coerces_to(other)) return true;
    if (has(other)) return true;
    if (other->kind() == KIND_INTERSECT) {
      for (const Type* t : ((const IntersectType*)other)->_members) 
        if (!has(t)) return false;
      return true;
    }
    return false;
  }

  void IntersectType::format(stream& io) const {
    write(io, "(");
    bool first = true;
    for (const Type* t : _members) {
      write(io, first ? "" : " & ", t);
      first = false;
    }
    write(io, ")");
  }

  u64 vector_hash(const vector<const Type*>& members) {
    u64 h = 10472618355682807153ul;
    for (const Type* t : members) h ^= t->hash();
    return h;
  }

  ProductType::ProductType(const vector<const Type*>& members):
    Type(vector_hash(members)), _members(members) {}

  u32 ProductType::count() const {
    return _members.size();
  }

  const Type* ProductType::member(u32 i) const {
    return _members[i];
  }
	
	bool ProductType::concrete() const {
		for (const Type* t : _members) if (!t->concrete()) return false;
		return true;
	}

	const Type* ProductType::concretify() const {
		vector<const Type*> ts = _members;
		for (const Type*& t : ts) t = t->concretify();
		return find<ProductType>(ts);
	}

  TypeKind ProductType::kind() const {
    return KIND_PRODUCT;
  }

  bool ProductType::operator==(const Type& other) const {
    if (other.kind() != kind()) return false;
    const ProductType& p = (const ProductType&)other;
    if (p.count() != count()) return false;
    for (u32 i = 0; i < count(); i ++) {
      if (!(*p.member(i) == *member(i))) return false;
    }
    return true;
  }

  bool ProductType::coerces_to(const Type* other) const {
    if (Type::coerces_to(other)) return true;
    
    // if (other.kind() == KIND_ARRAY 
    //     && ((const ArrayType&)other).element() == element() 
    //     && !((const ArrayType&)other).fixed()) return true;

    if (other == TYPE) {
      for (u32 i = 0; i < count(); i ++)
        if (member(i) != TYPE) return false;
      return true;
    }

    if (other->kind() == KIND_ARRAY && ((const ArrayType*)other)->fixed()
        && ((const ArrayType*)other)->count() == count()) {
      for (u32 i = 0; i < count(); i ++)
        if (member(i) != ((const ArrayType*)other)->element()) return false;
      return true;
    }

    return false;
  }

  void ProductType::format(stream& io) const {
    write(io, "(");
    bool first = true;
    for (const Type* t : _members) {
      write(io, first ? "" : " * ", t);
      first = false;
    }
    write(io, ")");
  }

  FunctionType::FunctionType(const Type* arg, const Type* ret):
    Type(arg->hash() ^ ret->hash() ^ 17623206604232272301ul),
    _arg(arg), _ret(ret) {}

  const Type* FunctionType::arg() const {
    return _arg;
  }

  const Type* FunctionType::ret() const {
    return _ret;
  }
  
  u32 FunctionType::arity() const {
    return _arg->kind() == KIND_PRODUCT ? 
      ((const ProductType*)_arg)->count() : 1;
  }
	
	bool FunctionType::concrete() const {
		return _arg->concrete() && _ret->concrete();
	}

	const Type* FunctionType::concretify() const {
		return find<FunctionType>(_arg->concretify(), _ret->concretify());
	}

  TypeKind FunctionType::kind() const {
    return KIND_FUNCTION;
  }

  bool FunctionType::operator==(const Type& other) const {
    if (other.kind() != kind()) return false;
    const FunctionType& f = (const FunctionType&)other;
    return *f._arg == *_arg && *f._ret == *_ret;
  }

  void FunctionType::format(stream& io) const {
    write(io, "(", _arg, " -> ", _ret, ")");
  }

  AliasType::AliasType():
    Type(9323462044786133851ul) {}

  TypeKind AliasType::kind() const {
    return KIND_ALIAS;
  }

  bool AliasType::operator==(const Type& other) const {
    return other.kind() == kind();
  }

  void AliasType::format(stream& io) const {
    write(io, "alias");
  }

  MacroType::MacroType(u32 arity):
    Type(18254210403858406693ul ^ ::hash(arity)), _arity(arity) {}

  u32 MacroType::arity() const {
    return _arity;
  }

  TypeKind MacroType::kind() const {
    return KIND_MACRO;
  }

  bool MacroType::operator==(const Type& other) const {
    return other.kind() == kind() &&
      ((const MacroType&)other).arity() == arity();
  }

  void MacroType::format(stream& io) const {
    write(io, "macro(", _arity, ")");
  }

	RuntimeType::RuntimeType(const Type* base):
		Type(base->hash() ^ 5857490642180150551ul), _base(base) {}

	const Type* RuntimeType::base() const {
		return _base;
	}

	TypeKind RuntimeType::kind() const {
		return KIND_RUNTIME;
	}

	bool RuntimeType::operator==(const Type& other) const {
		return other.kind() == kind() && 
			*((const RuntimeType&)other).base() == *base();
	}

  bool RuntimeType::coerces_to(const Type* other) const {
    return Type::coerces_to(other) 
      || other->kind() == KIND_RUNTIME && base()->coerces_to(((const RuntimeType*)other)->base());
  }

	void RuntimeType::format(stream& io) const {
    write(io, "runtime<", _base, ">");
	}

	static vector<const Type*> type_variables;
	static vector<string> typevar_names;

	static string next_typevar() {
		buffer b;
		write(b, "'T", type_variables.size());
		string s;
		read(b, s);
		return s;
	}

	static u32 create_typevar() {
		type_variables.push(ANY);
		typevar_names.push(next_typevar());
		return type_variables.size() - 1;
	}

	TypeVariable::TypeVariable(u32 id):
		Type(::hash(id) ^ 3860592187614349697ul), _id(id) {}

	TypeVariable::TypeVariable():
		TypeVariable(create_typevar()) {}

	const Type* TypeVariable::actual() const {
		return type_variables[_id];
	}

	void TypeVariable::bind(const Type* concrete) const {
		type_variables[_id] = concrete;
	}

	bool TypeVariable::concrete() const {
		return type_variables[_id]->concrete();
	}

	const Type* TypeVariable::concretify() const {
		const Type* t = actual();
		if (t == ANY) return this;
		return t->concretify();
	}

	TypeKind TypeVariable::kind() const {
		return KIND_TYPEVAR; 
	}

	bool TypeVariable::operator==(const Type& other) const {
		return other.kind() == kind() && _id == ((const TypeVariable&)other)._id;
	}

  bool TypeVariable::coerces_to(const Type* other) const {
    return Type::coerces_to(other) || !concrete() || actual()->coerces_to(other);
  }

	void TypeVariable::format(stream& io) const {
		write(io, typevar_names[_id]);
		if (type_variables[_id]->concrete()) 
			write(io, "(", type_variables[_id], ")");
	}

  struct TypeBox {
    const Type* t;

    bool operator==(const TypeBox& other) const {
      return *t == *other.t;
    }
  };

  static map<TypeBox, const Type*> TYPEMAP;

  const Type* find_existing_type(const Type* t) {
    auto it = TYPEMAP.find({ t });
    if (it == TYPEMAP.end()) return nullptr;
    return it->second;
  }

  const Type* create_type(const Type* t) {
    TYPEMAP.put({ t }, t);
    return t;
  }

  const Type *INT = find<NumericType>(64, false), 
             *FLOAT = find<NumericType>(64, true),
             *SYMBOL = find<SingletonType>("symbol"), 
             *VOID = find<SingletonType>("void"),
             *ERROR = find<SingletonType>("error"),
             *TYPE = find<SingletonType>("type"),
             *ALIAS = find<AliasType>(),
             *BOOL = find<SingletonType>("bool"),
						 *ANY = find<SingletonType>("any"),
						 *STRING = find<SingletonType>("string"),
             *MODULE = find<SingletonType>("module");

	const Type* unify(const Type* a, const Type* b, bool coercing, bool converting) {
		if (!a || !b) return nullptr; 
		
		if (a == ANY) return b;
		else if (b == ANY) return a;

		if (a->kind() == KIND_TYPEVAR) {
			if (!((const TypeVariable*)a)->actual()->concrete()) {
				if (b->concrete() || b->kind() != KIND_TYPEVAR)
					return ((const TypeVariable*)a)->bind(b), b;
				else if (b > a)
					return ((const TypeVariable*)a)->bind(b), b;
				else return a;
			}
			else a = ((const TypeVariable*)a)->actual();
		}

		if (b->kind() == KIND_TYPEVAR) {
			if (!((const TypeVariable*)b)->actual()->concrete()) {
				if (a->concrete() || a->kind() != KIND_TYPEVAR)
					return ((const TypeVariable*)b)->bind(a), a;
				else if (a > b)
					return ((const TypeVariable*)b)->bind(a), a;
				else return b;
			}
			b = ((const TypeVariable*)b)->actual();
		}

		if (a->kind() == KIND_LIST && b->kind() == KIND_LIST) {
			const Type* elt = unify(((const ListType*)a)->element(),
				((const ListType*)b)->element());
			if (!elt) return nullptr;
			return find<ListType>(elt);
		}

		if (a->kind() == KIND_PRODUCT && b->kind() == KIND_PRODUCT) {
			vector<const Type*> members;
			if (((const ProductType*)a)->count() != ((const ProductType*)b)->count())
				return nullptr;
			for (u32 i = 0; i < ((const ProductType*)a)->count(); i ++) {
				const Type* member = unify(((const ProductType*)a)->member(i),
					((const ProductType*)b)->member(i));
				if (!member) return nullptr;
				members.push(member);
			}
			return find<ProductType>(members);
		}

		if (a->kind() == KIND_FUNCTION && b->kind() == KIND_FUNCTION) {
			const Type* arg = unify(((const FunctionType*)a)->arg(),
				((const FunctionType*)b)->arg());
			const Type* ret = unify(((const FunctionType*)a)->ret(),
				((const FunctionType*)b)->ret());
			if (!arg || !ret) return nullptr;
			return find<FunctionType>(arg, ret);
		}

		if (a == VOID && b->kind() == KIND_LIST) return b;
		if (b == VOID && a->kind() == KIND_LIST) return a;

    if (coercing || converting) {
      if (a->coerces_to(b)) return b;
      if (b->coerces_to(a)) return a;
    }
		
		if (a != b) return nullptr;
		return a;
	}
}

template<>
u64 hash(const basil::Type& t) {
  return t.hash();
}

template<>
u64 hash(const basil::Type* const& t) {
  return t->hash();
}

template<>
u64 hash(const basil::TypeBox& t) {
  return t.t->hash();
}

void write(stream& io, const basil::Type* t) {
  t->format(io);
}