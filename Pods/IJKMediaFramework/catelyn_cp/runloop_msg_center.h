#pragma once
#include <functional>
#include <mutex>
#include <set>
#include <string>
#include <unordered_map>
#include "any.h"
using namespace std;

namespace kuaishou {
namespace kpbase {

class Runloop;

class RunloopMsgCenter {
 public:
  bool AddMsgObserver(const string& msg_key, Runloop* runloop, function<void(const Any& data)> handle);
  void RemoveMsgObserver(const string& msg_key, Runloop* runloop);
  void RemoveAllObservers(Runloop* runloop);

  bool PostMsg(const string& msg_key, const Any& data);

 private:
  struct MsgHandle {
    Runloop* runloop;
    function <void(const Any& data)> func;
    bool operator<(const MsgHandle& o) const { return runloop < o.runloop; }
  };

  bool InsertMsgMap(const string& msg_key, Runloop* runloop, function<void(const Any& data)> handle);
  bool InsertRunloopMap(const string& msg_key, Runloop* runloop);
  void EraseMsgMap(const string& msg_key, Runloop* runloop);
  void EraseRunloopMap(const string& msg_key, Runloop* runloop);
  pair<bool, function<void(const Any& data)>> LockAndFindHandle(const string& msg_key, Runloop* runloop);

  recursive_mutex map_mutex_;
  unordered_map<string, set<MsgHandle> > msg_map_;
  unordered_map<Runloop*, set<string> > runloop_map_;
};

}
}
