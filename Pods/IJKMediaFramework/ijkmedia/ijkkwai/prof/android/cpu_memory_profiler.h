//
// Created by MarshallShuai on 2019-05-31.
//

#pragma once

#include <sys/types.h>

/**
 *
 * /proc/[pid]/status ：可以获取进程的内存/线程数等状态（单独格式）
 *
 * /proc/stat 可以获取总的cpu使用时长（单独格式）
 *
 * /proc/[pid]/stat ：获取进程的stat
 * /proc/[pid]/task/tid/stat：获取线程的stat，格式同进程stat一致
 *
 * 关于cpu占用：
 * process_total_time = utime + stime + cutime + cstime
 */

#define PROC_NAME_LEN 64
#define THREAD_NAME_LEN 32

#define MAX_THREAD_NUM_OF_KEY_RESULT 10
typedef struct profiler_key_result {
    int should_record_thread_cpu;// 统计每个线程cpu的开关
    pid_t pid;

    // 整个系统的 user/system cpu
    float cpu_percent_user;
    float cpu_percent_system;
    float cpu_percent_iow;
    float cpu_percent_irq;

    // cpu的个数，活跃的/总数
    long cpu_alive_cnt;
    long cpu_total_cnt;

    long vss_mb;
    long rss_mb;

    // 本进程总的线程数
    int num_threads_total;

    float this_process_cpu_percent;

    struct {
        float cpu_percent;
        pid_t tid;
        char name[THREAD_NAME_LEN];
    } thread_stat[MAX_THREAD_NUM_OF_KEY_RESULT];
    int num_threads_valid;
    int pagesize;

} profiler_key_result;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * app初始化的时候调用，初始化一些全局变量
 */
void cpu_memory_profiler_init();

/**
 * 刷新并获取当前cpu/memory 指标
 * @return
 */
profiler_key_result* update_and_get_current_key_result();

const char* get_memory_describe_string();

const char* get_cpu_describe_string();

void debug_print_current_key_result();

#ifdef __cplusplus
}
#endif