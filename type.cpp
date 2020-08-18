#include "type.h"

namespace basil {

  Type::Type(u64 hash) : _hash(hash) {}
  
  u64 Type::hash() const {
    return _hash;
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
             *TYPE = find<SingletonType>("type");
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