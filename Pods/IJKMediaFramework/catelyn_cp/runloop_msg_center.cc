#include "runloop_msg_center.h"
#include "runloop.h"

namespace kuaishou {
namespace kpbase {

namespace {
function<void(const Any&)> no_func;
}

bool RunloopMsgCenter::AddMsgObserver(const string& msg_key,
                                      Runloop* runloop,
                                      function<void(const Any& data)> handle) {
  lock_guard<recursive_mutex> lock(map_mutex_);
  if (InsertMsgMap(msg_key, runloop, handle)) {
    if (InsertRunloopMap(msg_key, runloop)) {
      return true;
    } else {
      EraseMsgMap(msg_key, runloop);
    }
  }

  return false;
}

bool RunloopMsgCenter::InsertMsgMap(const string& msg_key, Runloop* runloop, function<void(const Any& data)> handle) {
  auto iter = msg_map_.find(msg_key);
  if (iter == msg_map_.end()) {
    set<MsgHandle> handles = {{runloop, handle}};
    return msg_map_.insert({msg_key, handles}).second;
  } else {
    return iter->second.insert({runloop, handle}).second;
  }
}

bool RunloopMsgCenter::InsertRunloopMap(const string& msg_key, Runloop* runloop) {
  auto iter = runloop_map_.find(runloop);
  if (iter == runloop_map_.end()) {
    set<string> msgs;
    msgs.insert(msg_key);
    return runloop_map_.insert({runloop, msgs}).second;
  } else {
    return iter->second.insert(msg_key).second;
  }
}

void RunloopMsgCenter::EraseMsgMap(const string& msg_key, Runloop* runloop) {
  auto iter = msg_map_.find(msg_key);
  if (iter != msg_map_.end()) {
    if (iter->second.size() == 1)
      msg_map_.erase(iter);
    else
      iter->second.erase({runloop, no_func});
  }
}

void RunloopMsgCenter::EraseRunloopMap(const string& msg_key, Runloop* runloop) {
  auto iter = runloop_map_.find(runloop);
  if (iter != runloop_map_.end()) {
    if (iter->second.size() == 1)
      runloop_map_.erase(iter);
    else
      iter->second.erase(msg_key);
  }
}

void RunloopMsgCenter::RemoveMsgObserver(const string& msg_key, Runloop* runloop) {
  lock_guard<recursive_mutex> lock(map_mutex_);
  EraseMsgMap(msg_key, runloop);
  EraseRunloopMap(msg_key, runloop);
}

void RunloopMsgCenter::RemoveAllObservers(Runloop* runloop) {
  lock_guard<recursive_mutex> lock(map_mutex_);
  auto iter = runloop_map_.find(runloop);
  if (iter != runloop_map_.end()) {
    for (auto& m : iter->second) {
      auto m_iter = msg_map_.find(m);
      if (m_iter != msg_map_.end())
        m_iter->second.erase({runloop, no_func});
    }

    runloop_map_.erase(iter);
  }
}

bool RunloopMsgCenter::PostMsg(const string& msg_key, const Any& data) {
  lock_guard<recursive_mutex> lock(map_mutex_);
  auto iter = msg_map_.find(msg_key);
  if (iter == msg_map_.end())
    return false;

  bool val = true;
  for (auto& h : iter->second) {
    Runloop* r = h.runloop;
    val &= h.runloop->Post([ =, &h] {
      auto f = LockAndFindHandle(msg_key, r);
      if (f.first)
        f.second(data);
    });
  }
  return val;
}

pair<bool, function<void(const Any& data)>> RunloopMsgCenter::LockAndFindHandle(const string& msg_key, Runloop* runloop) {
  lock_guard<recursive_mutex> lock(map_mutex_);
  auto iter = msg_map_.find(msg_key);
  if (iter == msg_map_.end())
    return {false, [](const Any & data) {}};

  auto h_iter = iter->second.find({runloop, [](const Any&) {}});
  if (h_iter == iter->second.end())
    return {false, [](const Any & data) {}};

  return {true, h_iter->func};
}

}
}
