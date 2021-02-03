#ifndef C_RESOURCE_MONITOR_H
#define C_RESOURCE_MONITOR_H

#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

/**
* resource monitor
*/
uint32_t get_system_cpu_usage();
uint32_t get_process_cpu_usage();
uint32_t get_process_memory_size_kb();
uint32_t get_process_cpu_num();

#ifdef __cplusplus
}
#endif
#endif // C_RESOURCE_MONITOR_H
