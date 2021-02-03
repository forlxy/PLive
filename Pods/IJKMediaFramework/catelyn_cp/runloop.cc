#include "runloop.h"
#include <cassert>
#include <utility>
#include "runloop_msg_center.h"
#include "utility.h"
#include "thread_function_wrapper.h"

#ifdef _WIN32
#include "platform/windows/util.h"
#endif // _WIN32

using namespace std;

namespace kuaishou {
namespace kpbase {

Runloop::Runloop(const std::string& name, shared_ptr<RunloopMsgCenter> msg_center) :
  stop_(false),
  timer_id_generator_(0),
  msg_center_(msg_center) {
  thread_ = thread(&Runloop::runloopProc, this, name);
}

Runloop::~Runloop() {
  auto ptr = msg_center_.lock();
  if (ptr != nullptr) {
    ptr->RemoveAllObservers(this);
  }

  Stop();

  function<void()>* t = nullptr;
  while (tasks_.pop(t)) {
    delete t;
  }
}

void Runloop::Stop() {
  if (stop_) {
    return;
  }

  stop_ = true;

  post_event_.Signal();

  if (thread_.joinable()) {
    thread_.join();
  }
}

bool Runloop::Post(function<void()> func) {
  if (stop_) {
    return false;
  }

  auto newTask = new function<void()>(func);
  auto ret = tasks_.push(newTask);

  if (!ret) {
    delete newTask;
    return false;
  }

  Wakeup();

  return ret;
}

bool Runloop::PostAndWait(function<void()> func) {
  if (stop_) {
    return false;
  }

  Event task_done;
  function<void()>* newTask = new function<void()>([func, &task_done] {
    func();
    task_done.Signal();
  });

  auto ret = tasks_.push(newTask);

  if (!ret) {
    delete newTask;
    return false;
  }

  Wakeup();

  task_done.Wait();

  return ret;
}

long Runloop::AddTimer(function<void()> func, uint32_t interval, bool repeat) {
  if (thread_.get_id() != this_thread::get_id()) {
    //throw "addTimer() should only be invoked in runloop thread";
    assert(false);
  }

  if (stop_) {
    return -1;
  }

  long tid = ++timer_id_generator_;
  uint64_t now = SystemUtil::GetCPUTime();
  Timer newTimer {tid, func, interval, repeat, now + interval};
  timers_.push_back(newTimer);
  timers_.sort();
  Wakeup();
  return tid;
}

void Runloop::RemoveTimer(long tid) {
  if (thread_.get_id() != this_thread::get_id()) {
    //throw "removeTimer() should only be invoked in runloop thread";
    assert(false);
  }

  if (stop_) {
    return;
  }

  auto iter = find(timers_.begin(), timers_.end(), tid);

  if (iter == timers_.end()) {
    return;
  }

  timers_.erase(iter);
  Wakeup();
}

void Runloop::RefreshTimer(long tid) {
  if (thread_.get_id() != this_thread::get_id()) {
    //throw "refreshTimer() should only be invoked in runloop thread";
    assert(false);
  }

  if (stop_) {
    return;
  }

  auto iter = find(timers_.begin(), timers_.end(), tid);

  if (iter == timers_.end()) {
    return;
  }

  iter->next_fire_time = SystemUtil::GetCPUTime() + iter->interval;
  Wakeup();
}

void Runloop::WaitFor(uint32_t msec) {
  if (msec > 0) {
    post_event_.WaitFor(msec);
  } else {
    post_event_.Wait();
  }
}

void Runloop::Wakeup() {
  post_event_.Signal();
}

void Runloop::runloopProc(const std::string& name) {
  RunWithPlatformThreadWrapper([ = ] {
#ifdef __APPLE__
    pthread_setname_np(name.c_str());
#elif _WIN32
    set_thread_name(name.c_str());
#else
    pthread_setname_np(pthread_self(), name.c_str());
#endif
    while (!stop_) {
      if (timers_.empty()) {
        WaitFor(-1);
      } else {
        uint64_t now = SystemUtil::GetCPUTime();
        uint64_t nextTimer = timers_.front().next_fire_time;

        if (now < nextTimer) {
          WaitFor(static_cast<uint32_t>(nextTimer - now));
        }
      }

      if (stop_) {
        break;
      }

      uint64_t now = SystemUtil::GetCPUTime();

      while (!stop_ && !timers_.empty() && now >= timers_.front().next_fire_time) {
        auto t = timers_.front();
        t.func();

        if (t.repeat) {
          timers_.begin()->next_fire_time += t.interval;
          timers_.sort();
        } else {
          timers_.pop_front();
        }
      }

      function<void()>* t = nullptr;

      while (!stop_ && tasks_.pop(t)) {
        (*t)();
        delete t;
      }
    }
  });
}

thread::id Runloop::thread_id() {
  return thread_.get_id();
}

bool Runloop::AddMsgObserver(const string& msg_key, function<void(const Any& data)> handle) {
  auto ptr = msg_center_.lock();
  if (!ptr)
    return false;

  return ptr->AddMsgObserver(msg_key, this, handle);
}

void Runloop::RemoveMsgObserver(const string& msg_key) {
  auto ptr = msg_center_.lock();
  if (ptr)
    ptr->RemoveMsgObserver(msg_key, this);
}

bool Runloop::PostMsgToObservers(const string& msg_key, const Any& data) {
  auto ptr = msg_center_.lock();
  if (!ptr)
    return false;

  return ptr->PostMsg(msg_key, data);
}

}
}
