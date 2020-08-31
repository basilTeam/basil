#include "type.h"

namespace basil {

  Type::Type(u64 hash) : _hash(hash) {}
  
  u64 Type::hash() const {
    return _hash;
  }

	bool Type::concrete() const {
		return this != ANY;
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

  ListType::ListType(const Type* element):
    Type(element->hash() ^ 11340086872871314823ul), _element(element) {}

	bool ListType::concrete() const {
		return _element->concrete();
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

  void ListType::format(stream& io) const {
    write(io, "[", _element, "]");
  }

  u64 set_hash(const set<const Type*>& members) {
    u64 h = 6530804687830202173ul;
    for (const Type* t : members) h ^= t->hash();
    return h;
  }

  SumType::SumType(const set<const Type*>& members):
    Type(set_hash(members)), _members(members) {}

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

  void SumType::format(stream& io) const {
    write(io, "(");
    bool first = true;
    for (const Type* t : _members) {
      write(io, first ? "" : " | ", t);
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

	TypeKind TypeVariable::kind() const {
		return KIND_TYPEVAR; 
	}

	bool TypeVariable::operator==(const Type& other) const {
		return other.kind() == kind() && _id == ((const TypeVariable&)other)._id;
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

  const Type *INT = find<SingletonType>("int"), 
             *SYMBOL = find<SingletonType>("symbol"), 
             *VOID = find<SingletonType>("void"),
             *ERROR = find<SingletonType>("error"),
             *TYPE = find<SingletonType>("type"),
             *ALIAS = find<AliasType>(),
             *BOOL = find<SingletonType>("bool"),
						 *ANY = find<SingletonType>("any"),
						 *STRING = find<SingletonType>("string");

	const Type* unify(const Type* a, const Type* b) {
		if (!a || !b) return nullptr; 
		
		if (a == ANY) return b;
		else if (b == ANY) return a;

		if (a->kind() == KIND_TYPEVAR) {
			if (!((const TypeVariable*)a)->actual()->concrete())
				return ((const TypeVariable*)a)->bind(b), b;
			a = ((const TypeVariable*)a)->actual();
		}

		if (b->kind() == KIND_TYPEVAR) {
			if (!((const TypeVariable*)b)->actual()->concrete())
				return ((const TypeVariable*)b)->bind(a), a;
			b = ((const TypeVariable*)b)->actual();
		}

		if (a->kind() == KIND_LIST && b->kind() == KIND_LIST)
			return find<ListType>(unify(((const ListType*)a)->element(),
				((const ListType*)b)->element()));

		if (a == VOID && b->kind() == KIND_LIST) return b;
		if (b == VOID && a->kind() == KIND_LIST) return a;
		
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