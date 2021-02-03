#pragma once
#include <stdint.h>
#ifdef _WIN32
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif

#include <functional>
#include <list>
#include <memory>
#include <thread>
#include <vector>
#include <string>

#include <boost/lockfree/queue.hpp>
#include "any.h"
#include "event.h"
using namespace std;

namespace kuaishou {
namespace kpbase {

class RunloopMsgCenter;

class Runloop {
 public:
  Runloop(const std::string& name = "Runloop", shared_ptr<RunloopMsgCenter> msg_center = nullptr);
  virtual ~Runloop();

  bool Post(function<void()> func);
  bool PostAndWait(function<void()> func); // !!!WARNING!!!: will be blocked until func() done

  long AddTimer(function<void()> func, uint32_t interval, bool repeat);
  void RemoveTimer(long tid);
  void RefreshTimer(long tid);
  thread::id thread_id();

  bool AddMsgObserver(const string& msg_key, function<void(const Any& data)> handle);
  void RemoveMsgObserver(const string& msg_key);
  bool PostMsgToObservers(const string& msg_key, const Any& data);

  void Stop();

 protected:
  virtual void WaitFor(uint32_t msec);
  virtual void Wakeup();

  thread thread_;
  atomic<bool> stop_;

 private:
  struct Timer {
    long tid;
    function<void()> func;
    uint32_t interval;
    uint32_t repeat;
    uint64_t next_fire_time;

    bool operator<(const Timer& t) const {
      return next_fire_time < t.next_fire_time;
    }

    bool operator==(long tid_0) const {
      return this->tid == tid_0;
    }
  };

  struct TaskObject {
    function<void()> func;
  };

  void runloopProc(const std::string& name);

  boost::lockfree::queue<function<void()> *, boost::lockfree::capacity<1024>> tasks_;
  Event post_event_;
  list<Timer> timers_;
  atomic_long timer_id_generator_;
  weak_ptr<RunloopMsgCenter> msg_center_;
};

}
}
