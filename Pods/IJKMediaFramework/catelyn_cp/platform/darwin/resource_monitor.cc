#include <sys/sysctl.h>
#include <sys/types.h>
#include <mach/mach.h>
#include <mach/processor_info.h>
#include <mach/mach_host.h>
#include "../../resource_monitor.h"

namespace kuaishou {
namespace kpbase {

ResourceMonitor::ResourceMonitor()
  : runloop_(new Runloop("resource_monitor"))
  , system_cpu_use_time_(0)
  , system_cpu_last_use_time_(0)
  , system_cpu_total_time_(0)
  , system_cpu_last_total_time_(0)
  , system_cpu_usage_(0)
  , process_cpu_usage_(0)
  , process_memory_size_kb_(0)
  , is_monotonic_mode_(false)
  , theory_thres_(0)
  , last_ok_idle_(0)
  , total_factor_(0)
  , use_factor_(0) {
  // clear thread cpu usage
  threads_use_time_.clear();
  threads_cpu_usage_.clear();
  threads_last_use_time_.clear();
  // Get active processor number of device
  unsigned int ncpu;
  size_t len = sizeof(ncpu);
  sysctlbyname("hw.activecpu", &ncpu, &len, NULL, 0);
  if (ncpu <= 0) {
    ncpu = 1;
  }
  processor_number_ = static_cast<uint32_t>(ncpu);
  runloop_->Post([ = ] {
    timer_id_ = runloop_->AddTimer([ = ] {
      UpdateSystemCpuUsageLocal();
      UpdateProcessCpuUsageLocal();
      UpdateProcessMemorySizeKBLocal();
    }, kResourceMonitorInterval, true);
  });
}

ResourceMonitor::~ResourceMonitor() {
  runloop_->Post([ = ] {
    runloop_->RemoveTimer(timer_id_);
    // clear thread cpu usage
    threads_use_time_.clear();
    threads_cpu_usage_.clear();
    threads_last_use_time_.clear();
  });
  runloop_->Stop();
}

void ResourceMonitor::UpdateSystemCpuUsageLocal() {
  processor_info_array_t processor_info;
  mach_msg_type_number_t num_processor_info;
  unsigned int num_processors;

  kern_return_t err = host_processor_info(mach_host_self(), PROCESSOR_CPU_LOAD_INFO,
                                          &num_processors, &processor_info, &num_processor_info);
  if (KERN_SUCCESS != err) {
    return;
  }
  system_cpu_use_time_ = system_cpu_total_time_ = 0;
  for (size_t i = 0; i < num_processors; i++) {
    int64_t use_time_per_processor = processor_info[(CPU_STATE_MAX * i) + CPU_STATE_USER]
                                     + processor_info[(CPU_STATE_MAX * i) + CPU_STATE_SYSTEM]
                                     + processor_info[(CPU_STATE_MAX * i) + CPU_STATE_NICE];

    system_cpu_use_time_ += use_time_per_processor;
    system_cpu_total_time_ += (use_time_per_processor + processor_info[(CPU_STATE_MAX * i) + CPU_STATE_IDLE]);
  }
  vm_deallocate(mach_task_self(), (vm_address_t)processor_info, num_processor_info);

  int64_t total_time = system_cpu_total_time_ - system_cpu_last_total_time_;
  int64_t use_time = system_cpu_use_time_ - system_cpu_last_use_time_;
  if (total_time > 0 && use_time > 0) {
    system_cpu_usage_ = static_cast<uint32_t>(100 * use_time / total_time);
  }
  system_cpu_last_use_time_ = system_cpu_use_time_;
  system_cpu_last_total_time_ = system_cpu_total_time_;
}

void ResourceMonitor::UpdateThreadsCpuUsageLocal() {

}

void ResourceMonitor::UpdateProcessCpuUsageLocal() {
  thread_array_t thread_list;
  mach_msg_type_name_t thread_count;
  //processor_info_array_t processor_info;
  //mach_msg_type_number_t num_processor_info;
  kern_return_t kr;
  //unsigned int num_processors;

  if (task_threads(mach_task_self(), &thread_list, &thread_count) != KERN_SUCCESS) {
    return;
  }

  //if (host_processor_info(mach_host_self(), PROCESSOR_CPU_LOAD_INFO,&num_processors,
  //                        &processor_info, &num_processor_info) != KERN_SUCCESS) {
  //  return;
  //}

  int total_thread_cpu = 0;
  bool first = true;
  for (int i = 0; i < thread_count; i++) {
    thread_info_data_t thinfo;
    mach_msg_type_number_t thread_info_count = THREAD_INFO_MAX;
    kr = thread_info(thread_list[i], THREAD_EXTENDED_INFO, (thread_info_t)thinfo, &thread_info_count);

    if (kr != KERN_SUCCESS) {
      vm_deallocate(mach_task_self(), (vm_offset_t)thread_list, thread_count * sizeof(thread_t));
      return;
    }

    thread_extended_info_t extend_info_th = (thread_extended_info_t)thinfo;

    if (!(extend_info_th->pth_flags & TH_FLAGS_IDLE)) {
      char thread_name[32];
      total_thread_cpu += extend_info_th->pth_cpu_usage;
      if (first) {
        threads_use_time_.clear();
        first = false;
      }
      if (strlen(extend_info_th->pth_name) <= 0) {
        snprintf(thread_name, 32, "%u", thread_list[i]);
      } else {
        snprintf(thread_name, 32, "%s", extend_info_th->pth_name);
      }
      auto iter = find(threads_use_time_.begin(), threads_use_time_.end(), thread_name);
      if (iter == threads_use_time_.end()) {
        ThreadUseTime time {string(thread_name), extend_info_th->pth_cpu_usage};
        threads_use_time_.push_back(time);
      } else {
        iter->usage += extend_info_th->pth_cpu_usage;
      }
    }
  }

  vm_deallocate(mach_task_self(), (vm_offset_t)thread_list, thread_count * sizeof(thread_t));
  //vm_deallocate(mach_task_self(), (vm_address_t)processor_info, num_processor_info);

  process_cpu_usage_ = 100 * total_thread_cpu / (processor_number_ * TH_USAGE_SCALE);
  // calculate thread cpu usage of current process
  std::lock_guard<std::mutex> lg(thread_usage_mutex_);
  threads_cpu_usage_.clear();
  for (auto& m : threads_use_time_) {
    uint32_t usage = 0;
    if (total_thread_cpu > 0) {
      usage = static_cast<uint32_t>((m.usage * 100) / total_thread_cpu);
    }
    threads_cpu_usage_.push_back({m.name, usage});
  }
  sort(threads_cpu_usage_.begin(), threads_cpu_usage_.end(), CmpByValue());
}

void ResourceMonitor::UpdateProcessMemorySizeKBLocal() {
  mach_task_basic_info_data_t info;
  mach_msg_type_number_t size = MACH_TASK_BASIC_INFO_COUNT;
  kern_return_t kerr = task_info(mach_task_self(),
                                 MACH_TASK_BASIC_INFO,
                                 (task_info_t)&info,
                                 &size);
  if (kerr == KERN_SUCCESS) {
    process_memory_size_kb_ = static_cast<uint32_t>(info.resident_size / 1024);
  }
}
}
}
