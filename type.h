#ifndef BASIL_TYPE_H
#define BASIL_TYPE_H

#include "defs.h"
#include "str.h"
#include "hash.h"
#include "io.h"
#include "vec.h"

namespace basil {
  const u8 GC_KIND_FLAG = 128;

  enum TypeKind : u8 {
    KIND_SINGLETON = 0, 
    KIND_LIST = GC_KIND_FLAG | 0, 
    KIND_SUM = GC_KIND_FLAG | 1,
    KIND_PRODUCT = GC_KIND_FLAG | 2,
    KIND_FUNCTION = GC_KIND_FLAG | 3,
    KIND_ALIAS = GC_KIND_FLAG | 4,
    KIND_MACRO = GC_KIND_FLAG | 5
  };

  class Type {
    u64 _hash;
  protected:
    Type(u64 hash);
  
  public:
    virtual TypeKind kind() const = 0;
    u64 hash() const;
    virtual bool operator==(const Type& other) const = 0;
    virtual void format(stream& io) const = 0;
  };

  class SingletonType : public Type {
    string _repr;
  public:
    SingletonType(const string& repr);

    TypeKind kind() const override;
    bool operator==(const Type& other) const override;
    void format(stream& io) const override;
  };

  class ListType : public Type {
    const Type* _element;
  public:
    ListType(const Type* element);

    const Type* element() const;
    TypeKind kind() const override;
    bool operator==(const Type& other) const override;
    void format(stream& io) const override;
  };

  class SumType : public Type {
    set<const Type*> _members;
  public:
    SumType(const set<const Type*>& members);

    bool has(const Type* member) const;
    TypeKind kind() const override;
    bool operator==(const Type& other) const override;
    void format(stream& io) const override;
  };

  class ProductType : public Type {
    vector<const Type*> _members;
  public:
    ProductType(const vector<const Type*>& members);

    u32 count() const;
    const Type* member(u32 i) const;
    TypeKind kind() const override;
    bool operator==(const Type& other) const override;
    void format(stream& io) const override;
  };

  class FunctionType : public Type {
    const Type *_arg, *_ret;
  public:
    FunctionType(const Type* arg, const Type* ret);

    const Type* arg() const;
    const Type* ret() const;
    u32 arity() const;
    TypeKind kind() const override;
    bool operator==(const Type& other) const override;
    void format(stream& io) const override;
  };

  class AliasType : public Type {
  public:
    AliasType();
    
    TypeKind kind() const override;
    bool operator==(const Type& other) const override;
    void format(stream& io) const override;
  };

  class MacroType : public Type {
    u32 _arity;
  public:
    MacroType(u32 arity);
  
    u32 arity() const;

    TypeKind kind() const override;
    bool operator==(const Type& other) const override;
    void format(stream& io) const override;
  };

  const Type* find_existing_type(const Type* t);
  const Type* create_type(const Type* t);

  template<typename T, typename ...Args>
  const Type* find(Args... args) {
    T t(args...);
    const Type* e = find_existing_type(&t);

    if (e) return e;
    return create_type(new T(t));
  }

  extern const Type *INT, *SYMBOL, *VOID, *ERROR, *TYPE, 
                    *ALIAS, *BOOL;
}

template<>
u64 hash(const basil::Type& t);

template<>
u64 hash(const basil::Type* const& t);

void write(stream& io, const basil::Type* t);

#endif