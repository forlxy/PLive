#include "event.h"

namespace kuaishou {
namespace kpbase {

Event::Event() : signaled_(false), signaled_count_(0) {}

void Event::Wait() {
  unique_lock<mutex> lock(mutex_);
  condition_.wait(lock, [&] { return signaled_ || signaled_count_ > 0; });

  if (signaled_count_ > 0) {
    --signaled_count_;
  } else {
    signaled_ = false;
  }
}

bool Event::WaitFor(uint16_t msec) {
  unique_lock<mutex> lock(mutex_);
  bool gotSignal = condition_.wait_for(lock, chrono::milliseconds(msec), [&] { return signaled_ || signaled_count_ > 0; });

  if (signaled_count_ > 0) {
    gotSignal = true;
    --signaled_count_;
  } else if (signaled_) {
    gotSignal = true;
    signaled_ = false;
  }

  return gotSignal;
}

void Event::Signal() {
  lock_guard<mutex> lock(mutex_);
  signaled_ = true;
  condition_.notify_one();
}

void Event::SignalMultiple(uint16_t count) {
  lock_guard<mutex> lock(mutex_);
  signaled_count_ += count;
  condition_.notify_all();
}

}
}
