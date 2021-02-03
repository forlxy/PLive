#pragma once
#include <stdint.h>
#include <atomic>
#include <condition_variable>
#include <mutex>
using namespace std;

namespace kuaishou {
namespace kpbase {

class Event {
 public:
  Event();

  void Signal();
  void SignalMultiple(uint16_t count);
  void Wait();
  bool WaitFor(uint16_t msec);

 private:
  bool signaled_;
  uint16_t signaled_count_;
  mutex mutex_;
  condition_variable condition_;
};

}
}
