#pragma once
#include <mutex>
#include <set>
#include <functional>
using namespace std;

namespace kuaishou {
namespace kpbase {

template<class T, bool thread_safe = true>
class Listener {
 public:
  void AddListener(T* l) {
    if (thread_safe) {
      lock_guard<mutex> lock(mutex_);
      listener_set_.insert(l);
    } else {
      listener_set_.insert(l);
    }

  }

  void RemoveListener(T* l) {
    if (thread_safe) {
      lock_guard<mutex> lock(mutex_);
      listener_set_.erase(l);
    } else {
      listener_set_.erase(l);
    }
  }

  void NotifyListeners(const function<void(T*)>& func) {
    if (thread_safe) {
      lock_guard<mutex> lock(mutex_);
      for (auto& l : listener_set_) {
        func(l);
      }
    } else {
      for (auto& l : listener_set_) {
        func(l);
      }
    }
  }

 private:
  mutex mutex_;
  set<T*> listener_set_;
};

}
}
