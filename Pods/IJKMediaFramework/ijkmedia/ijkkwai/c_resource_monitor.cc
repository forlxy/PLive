#include "c_resource_monitor.h"
#include <memory>
#include <cstdlib>
#include "xlog/xlog.h"
#include "catelyn_cp/resource_monitor.h"

using namespace kuaishou::kpbase;

uint32_t get_system_cpu_usage() {
    return kuaishou::kpbase::ResourceMonitor::GetInstance().GetSystemCpuUsage();
}

uint32_t get_process_cpu_usage() {
    return kuaishou::kpbase::ResourceMonitor::GetInstance().GetProcessCpuUsage();
}

uint32_t get_process_memory_size_kb() {
    return kuaishou::kpbase::ResourceMonitor::GetInstance().GetProcessMemorySizeKB();
}

uint32_t get_process_cpu_num() {
    return kuaishou::kpbase::ResourceMonitor::GetInstance().GetProcessorNum();
}


