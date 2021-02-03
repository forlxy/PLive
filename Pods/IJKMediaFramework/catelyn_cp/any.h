#pragma once
#include <cassert>
#include <memory>
#include <typeinfo>
using namespace std;

namespace kuaishou {
namespace kpbase {

namespace internal {

struct AnyValue {
  virtual ~AnyValue() {}
  virtual const type_info& GetType() const = 0;
  virtual shared_ptr<AnyValue> Clone() const = 0;
};

template<class T>
struct SomeValue : public AnyValue {
  SomeValue(const T& o) : obj(o) {}
  SomeValue(const SomeValue& o) : obj(o.obj) {}
  virtual ~SomeValue() {}

  virtual const type_info& GetType() const { return typeid(T); }
  virtual shared_ptr<AnyValue> Clone() const { return shared_ptr<AnyValue>(new SomeValue<T>(obj)); }

  T obj;
};

}

class Any {
 public:
  Any() {}

  template<class T>
  Any(const T& o) : ptr(new internal::SomeValue<T>(o)) {}

  Any(const Any& o) : ptr(o.ptr == nullptr ? nullptr : o.ptr->Clone()) {}

  Any(Any&& o) : ptr(o.ptr) {}

  Any& operator=(const Any& o) {
    ptr = o.ptr->Clone();
    return *this;
  }

  void Reset() {
    ptr = nullptr;
  }

  template<class T>
  const T& Cast() const {
    auto real_ptr = dynamic_pointer_cast<internal::SomeValue<T> >(ptr);
    assert(real_ptr != nullptr);
    return real_ptr->obj;
  }

  const type_info& Type() const {
    assert(ptr != nullptr);
    return ptr->GetType();
  }

  bool HasValue() const {
    return ptr != nullptr;
  }

 private:
  shared_ptr<internal::AnyValue> ptr;
};

}
}
