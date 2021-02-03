#pragma once

#include <atomic>
#include "utility.h"
#include "runloop.h"

namespace kuaishou {
namespace kpbase {

static const uint32_t kResourceMonitorInterval = 2000;

class ResourceMonitor : public NonCopyable {
 public:
  typedef pair<string, int> PAIR;
  struct CmpByValue {
    bool operator()(const PAIR& lhs, const PAIR& rhs) {
      return lhs.second > rhs.second;
    }
  };
  static ResourceMonitor& GetInstance() {
    static ResourceMonitor resource_monitor_;
    return resource_monitor_;
  }

  const vector<PAIR> GetThreadsCpuUsage() {
    std::lock_guard<std::mutex> lg(thread_usage_mutex_);
    return threads_cpu_usage_;
  }
  uint32_t GetProcessorNum() const {return processor_number_;}
  uint32_t GetSystemCpuUsage() const { return system_cpu_usage_; }
  uint32_t GetProcessCpuUsage() const { return process_cpu_usage_; }
  uint32_t GetProcessMemorySizeKB() const { return process_memory_size_kb_; }

 private:
  struct ThreadUseTime {
    string name;
    int64_t usage;
    bool operator<(const ThreadUseTime& t) const {
      return usage < t.usage;
    }
    bool operator==(string name_0) const {
      return (this->name.compare(name_0) == 0);
    }
  };
  ResourceMonitor();
  ~ResourceMonitor();

  void UpdateSystemCpuUsageLocal();
  void UpdateProcessCpuUsageLocal();
  void UpdateThreadsCpuUsageLocal();
  void UpdateProcessMemorySizeKBLocal();

  unique_ptr<Runloop> runloop_;
  long timer_id_;

  mutex thread_usage_mutex_;
  vector<PAIR> threads_cpu_usage_;
  atomic<uint32_t> processor_number_;
  atomic<uint32_t> system_cpu_usage_;
  atomic<uint32_t> process_cpu_usage_;
  atomic<uint32_t> process_memory_size_kb_;

  int64_t system_cpu_use_time_;
  int64_t system_cpu_last_use_time_;
  int64_t system_cpu_total_time_;
  int64_t system_cpu_last_total_time_;
  int64_t process_cpu_use_time_;
  int64_t process_cpu_last_use_time_;
  // Thread usage for android or linux
  list<ThreadUseTime> threads_use_time_;
  list<ThreadUseTime> threads_last_use_time_;
  // Only for android or linux
  bool is_monotonic_mode_;
  int64_t theory_thres_;
  int64_t last_ok_idle_;
  int32_t total_factor_;
  int32_t use_factor_;
};

}
}
