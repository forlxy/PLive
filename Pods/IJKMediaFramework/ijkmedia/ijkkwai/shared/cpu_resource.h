#ifndef __CPU_SROUCE_H__
#define __CPU_SROUCE_H__

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __APPLE__

int getProcessCpuUsage(float* cpu_usage);
int getProcessMemorySize();  // KB

#endif

#ifdef __cplusplus
}
#endif

#endif  // __CPU_SROUCE_H__
