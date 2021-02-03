#include "cpu_resource.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "shared.h"
#include <sys/time.h>
#include <stdio.h>
#ifdef __APPLE__
#include <mach/mach.h>
#include <mach/processor_info.h>
#endif

#ifdef __cplusplus
}
#endif

#ifdef __APPLE__
int getProcessCpuUsage(float* cpu_usage) {
    thread_array_t thread_list;
    mach_msg_type_name_t thread_count;
    processor_info_array_t processor_info;
    mach_msg_type_number_t num_processor_info;
    kern_return_t kr;
    unsigned int num_processors;

    if (task_threads(mach_task_self(), &thread_list, &thread_count) != KERN_SUCCESS) {
        return -1;
    }

    if (host_processor_info(mach_host_self(), PROCESSOR_CPU_LOAD_INFO, &num_processors, &processor_info, &num_processor_info) != KERN_SUCCESS) {
        return -1;
    }

    int total_thread_cpu = 0;

    for (int i = 0; i < thread_count; i++) {
        thread_info_data_t thinfo;
        mach_msg_type_number_t thread_info_count = THREAD_INFO_MAX;
        kr = thread_info(thread_list[i], THREAD_BASIC_INFO, (thread_info_t)thinfo, &thread_info_count);

        if (kr != KERN_SUCCESS) {
            vm_deallocate(mach_task_self(), (vm_offset_t)thread_list, thread_count * sizeof(thread_t));
            return -1;
        }

        thread_basic_info_t basic_info_th = (thread_basic_info_t)thinfo;

        if (!(basic_info_th->flags & TH_FLAGS_IDLE)) {
            total_thread_cpu += basic_info_th->cpu_usage;
        }
    }

    vm_deallocate(mach_task_self(), (vm_offset_t)thread_list, thread_count * sizeof(thread_t));
    vm_deallocate(mach_task_self(), (vm_address_t)processor_info, num_processor_info);

    *cpu_usage = total_thread_cpu * 1.0f / TH_USAGE_SCALE / num_processors;
    return 1;
}

int getProcessMemorySize() {
    int memSize = 0;
    struct task_basic_info info;
    mach_msg_type_number_t size = sizeof(info);
    kern_return_t kerr = task_info(mach_task_self(),
                                   TASK_BASIC_INFO,
                                   (task_info_t)&info,
                                   &size);
    if (kerr == KERN_SUCCESS) {
        memSize = ((float)info.resident_size) / (1024);
    }
    return memSize;
}
#endif
