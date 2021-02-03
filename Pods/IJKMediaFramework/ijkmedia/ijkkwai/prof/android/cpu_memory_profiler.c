//
// Created by MarshallShuai on 2019-05-31.
// 参考：
// https://android.googlesource.com/platform/system/core/+/392744175c4de67dc98e72da6745e6351118c985/toolbox/top.c
// http://www.voidcn.com/article/p-oesjkqaq-bnv.html
//

#include <stdio.h>
#include <unistd.h>
#include <dirent.h>
#include <ctype.h>
#include <stdlib.h>
#include <memory.h>

#include "cpu_memory_profiler.h"

#include "ijksdl_log.h"

#define MAX_LINE 256

struct cpu_info {
    long unsigned utime, ntime, stime, itime;
    long unsigned iowtime, irqtime, sirqtime;
};

typedef struct proc_info {
    struct proc_info* next;
    pid_t pid;
    pid_t tid;
    uid_t uid;
    gid_t gid;
    char name[PROC_NAME_LEN];
    char tname[THREAD_NAME_LEN];
    char state;
    long unsigned utime;
    long unsigned stime;
    long unsigned delta_utime;
    long unsigned delta_stime;
    long unsigned delta_time;
    long vss;
    long rss;
    int num_threads;
    char policy[32];
} proc_info;

typedef struct proc_list {
    struct proc_info* array;
    int cur_num;
    int limit;

    const char* name;
} proc_list;

#define INIT_PROC_LIST_LIMIT 32


inline static void proc_list_init(struct proc_list* list, const char* name) {
    list->limit = INIT_PROC_LIST_LIMIT;
    list->array = malloc(INIT_PROC_LIST_LIMIT * sizeof(struct proc_info));
    memset(list->array, 0, INIT_PROC_LIST_LIMIT * sizeof(struct proc_info));
    list->cur_num = 0;
    list->name = name;
}

// 所有线程的proc_info, 遍历 /proc/pid/task/tid/stat
struct proc_list _thread_stat_list1, _thread_stat_list2;
struct proc_list* g_thread_stat_old_list, *g_thread_stat_new_list;

// app进程总体的proc_info, 读取 /proc/pid/stat
proc_info* g_app_proc_old, *g_app_proc_new;

// 总的系统cpu消耗
static struct cpu_info g_old_cpu, g_new_cpu;

// 第二次以上才能开始计算差值,后续重新做成单独线程实现后可以去掉这个参数
static int g_tick_index;

static profiler_key_result g_key_result;

static void read_procs();

// 从proc_list申请一个位置写入proc的信息，如果proc_list内部负责在空间不够的时候自动扩展空间(realloc)
static struct proc_info* get_next_writable_proc(struct proc_list* list);

static int read_stat(char* filename, struct proc_info* proc);

static void calculate_key_result();

static struct proc_info* find_old_proc(proc_list* list, pid_t pid, pid_t tid);


void cpu_memory_profiler_init() {
    proc_list_init(&_thread_stat_list1, "list_1");
    proc_list_init(&_thread_stat_list2, "list_2");
    g_thread_stat_old_list = &_thread_stat_list1;
    g_thread_stat_new_list = &_thread_stat_list2;

    g_app_proc_old = malloc(sizeof(proc_info));
    g_app_proc_new = malloc(sizeof(proc_info));
    memset(g_app_proc_old, 0, sizeof(g_new_cpu));
    memset(g_app_proc_new, 0, sizeof(g_new_cpu));

    memset(&g_old_cpu, 0, sizeof(g_old_cpu));
    memset(&g_new_cpu, 0, sizeof(g_new_cpu));

    memset(&g_key_result, 0, sizeof(g_key_result));
    g_key_result.pagesize = getpagesize();
    g_key_result.cpu_total_cnt = sysconf(_SC_NPROCESSORS_CONF);
    g_key_result.should_record_thread_cpu = 0;

    g_tick_index = 0;
}


profiler_key_result* update_and_get_current_key_result() {
    read_procs();
    return &g_key_result;
}

#define MEMORY_DESC_STRING_MAX_LEN (1024)
static char g_memory_desc_string[MEMORY_DESC_STRING_MAX_LEN];
const char* get_memory_describe_string() {
    size_t cur_offset = 0;
    cur_offset += snprintf(g_memory_desc_string + cur_offset, MEMORY_DESC_STRING_MAX_LEN - cur_offset,
                           "VSS:%5ldM, RSS:%4ldM",
                           g_key_result.vss_mb, g_key_result.rss_mb);

    return g_memory_desc_string;
}

#define CPU_DESC_STRING_MAX_LEN (1024*2)
static char g_cpu_desc_string[CPU_DESC_STRING_MAX_LEN];
const char* get_cpu_describe_string() {
    size_t cur_offset = 0;

    cur_offset += snprintf(g_cpu_desc_string + cur_offset, CPU_DESC_STRING_MAX_LEN - cur_offset,
                           "手机总CPU | User:%3.1f%%, System:%3.1f%%\n",
                           g_key_result.cpu_percent_user,
                           g_key_result.cpu_percent_system);

    cur_offset += snprintf(g_cpu_desc_string + cur_offset, CPU_DESC_STRING_MAX_LEN - cur_offset,
                           "本进程CPU:%3.1f%%, 活跃cpu数:%2ld/%2ld\n",
                           g_key_result.this_process_cpu_percent,
                           g_key_result.cpu_alive_cnt, g_key_result.cpu_total_cnt);

    cur_offset += snprintf(g_cpu_desc_string + cur_offset, CPU_DESC_STRING_MAX_LEN - cur_offset,
                           "线程数:%d | 前%d名线程占用: ⬇️\n",
                           g_key_result.num_threads_total,
                           g_key_result.num_threads_valid);
    if (g_key_result.should_record_thread_cpu) {
        cur_offset += snprintf(
                          g_cpu_desc_string + cur_offset, CPU_DESC_STRING_MAX_LEN -
                          cur_offset, "%8s  %6s %15s\n", "tid", "CPU", "thread_name");
        for (int i = 0; i < g_key_result.num_threads_valid; i++) {
            cur_offset += snprintf(
                              g_cpu_desc_string + cur_offset, CPU_DESC_STRING_MAX_LEN -
                              cur_offset, "%8d %6.1f%%  %-15s\n",
                              g_key_result.thread_stat[i].tid,
                              g_key_result.thread_stat[i].cpu_percent,
                              g_key_result.thread_stat[i].name);
        }
    }

    return g_cpu_desc_string;
}

void debug_print_current_key_result() {
//    ALOGD("[%s] \n%s", get_cpu_describe_string());
    ALOGD("[%s] \n%s", __func__, get_memory_describe_string());
}

/**
 * 读取所有cpu/memory相关的指标，并结算出最终key_result：
 * 总的cpu消耗  proc/stat
 * 每个线程的cpu消耗: proc/pid/task/tid/stat
 * 实际使用内存 proc/pid/status
 */
static void read_procs() {
    DIR* task_dir;
    struct dirent* tid_dir;
    char filename[64];
    FILE* file;
    struct proc_info* proc;
    pid_t pid = getpid(), tid;

    g_tick_index++;

    file = fopen("/proc/stat", "re");
    if (!file) {
        ALOGE("[%s]Could not open /proc/stat.", __func__);
    } else {
        memcpy(&g_old_cpu, &g_new_cpu, sizeof(g_old_cpu));

        fscanf(file, "cpu %lu %lu %lu %lu %lu %lu %lu",
               &g_new_cpu.utime, &g_new_cpu.ntime, &g_new_cpu.stime,
               &g_new_cpu.itime, &g_new_cpu.iowtime, &g_new_cpu.irqtime, &g_new_cpu.sirqtime);
        fclose(file);
    }

    // whole proc stat
    proc_info* tmp_info = g_app_proc_new;
    g_app_proc_new = g_app_proc_old;
    g_app_proc_old = tmp_info;

    sprintf(filename, "/proc/%d/stat", pid);
    read_stat(filename, g_app_proc_new);
    g_app_proc_new->pid = pid;

    // 分线程cpu采集暂时关闭：线上用不到，另外还有崩溃：
    // https://bugly.qq.com/v2/crash-reporting/crashes/900014602/142153743/report?pid=1
    // thread stats
    if (g_key_result.should_record_thread_cpu) {
        proc_list* tmp_list = g_thread_stat_new_list;
        g_thread_stat_new_list = g_thread_stat_old_list;
        g_thread_stat_old_list = tmp_list;

        sprintf(filename, "/proc/%d/task", pid);
        task_dir = opendir(filename);
        if (!task_dir) {
            if (!file) {
                ALOGD("[%s]Could not open %s.", __func__, filename);
            }
            return;
        }

        g_thread_stat_new_list->cur_num = 0;
        while ((tid_dir = readdir(task_dir))) {
            if (!isdigit(tid_dir->d_name[0])) {
                continue;
            }

            tid = atoi(tid_dir->d_name);

            proc = get_next_writable_proc(g_thread_stat_new_list);
            if (!proc) {
                ALOGE("[%s] get_next_writable_proc fail abort", __func__);
                return;
            }

            proc->pid = pid;
            proc->tid = tid;

            sprintf(filename, "/proc/%d/task/%d/stat", pid, tid);
            if (0 != read_stat(filename, proc)) {
                return;
            }
        }
    }

    if (g_tick_index >= 2) {
        calculate_key_result();
    }
}

static int read_stat(char* filename, struct proc_info* proc) {
    FILE* file;
    char buf[MAX_LINE], *open_paren, *close_paren;
    int res, idx;
    file = fopen(filename, "r");
    if (!file)
        return 1;
    fgets(buf, MAX_LINE, file);
    fclose(file);
    /* Split at first '(' and last ')' to get process name. */
    open_paren = strchr(buf, '(');
    close_paren = strrchr(buf, ')');
    if (!open_paren || !close_paren) {
        return 1;
    }
    *open_paren = *close_paren = '\0';
    strncpy(proc->tname, open_paren + 1, THREAD_NAME_LEN);
    proc->tname[THREAD_NAME_LEN - 1] = 0;

    /* Scan rest of string. */
    sscanf(close_paren + 1, " %c %*d %*d %*d %*d %*d %*d %*d %*d %*d %*d "
           "%lu %lu %*d %*d %*d %*d %d %*d %*d %ld %ld",
           &proc->state, &proc->utime, &proc->stime, &proc->num_threads, &proc->vss, &proc->rss);

//    debug_print_current_key_result();
    return 0;
}

static struct proc_info* get_next_writable_proc(struct proc_list* list) {
    if (list->limit > list->cur_num) {
        return &list->array[list->cur_num++];
    } else {
        void* ret = realloc(list->array,
                            2 * list->limit * sizeof(struct proc_info));
        if (!ret) {
            ALOGE("Could not expand procs array.\n");
            return NULL;
        } else {
            list->array = ret;
            list->limit = 2 * list->limit;
            return &list->array[list->cur_num++];
        }
    }
}

static int proc_cmp_desc(proc_info* a, proc_info* b) {
    return (int)(b->delta_utime - a->delta_time);
}

/**
 * 计算出 各个线程cpu占用/进程总的cpu占用
 */
static void calculate_key_result() {
    int i;
    struct proc_info* old_proc, *proc;
    long unsigned total_delta_time;

    float debug_cum_cpu = 0;

    total_delta_time = (g_new_cpu.utime + g_new_cpu.ntime + g_new_cpu.stime + g_new_cpu.itime
                        + g_new_cpu.iowtime + g_new_cpu.irqtime + g_new_cpu.sirqtime)
                       - (g_old_cpu.utime + g_old_cpu.ntime + g_old_cpu.stime + g_old_cpu.itime
                          + g_old_cpu.iowtime + g_old_cpu.irqtime + g_old_cpu.sirqtime);

    if (g_key_result.should_record_thread_cpu) {
        // 每个线程的cpu占用耗时
        for (i = 0; i < g_thread_stat_new_list->cur_num; i++) {
            struct proc_info* proc = &g_thread_stat_new_list->array[i];
            old_proc = find_old_proc(g_thread_stat_old_list, proc->pid, proc->tid);
            if (old_proc) {
                proc->delta_utime = proc->utime - old_proc->utime;
                proc->delta_stime = proc->stime - old_proc->stime;
            } else {
                proc->delta_utime = 0;
                proc->delta_stime = 0;
            }
            proc->delta_time = proc->delta_utime + proc->delta_stime;

            debug_cum_cpu += (proc->delta_time * 1.f * 100 / total_delta_time);
        }
        qsort(g_thread_stat_new_list->array, (size_t) g_thread_stat_new_list->cur_num, sizeof(struct proc_info), proc_cmp_desc);
    }

    // 进程总的cpu占用耗时
    g_app_proc_new->delta_utime = g_app_proc_new->utime - g_app_proc_old->utime;
    g_app_proc_new->delta_stime = g_app_proc_new->stime - g_app_proc_old->stime;
    g_app_proc_new->delta_time = g_app_proc_new->delta_utime + g_app_proc_new->delta_stime;

    proc = g_app_proc_new;
    g_key_result.pid = proc->pid;
    g_key_result.vss_mb = proc->vss / (1024 * 1024);
    g_key_result.rss_mb = proc->rss * g_key_result.pagesize / (1024 * 1024);
    g_key_result.num_threads_total = proc->num_threads;
    g_key_result.this_process_cpu_percent = proc->delta_time * 100.f / total_delta_time;
    for (i = 0; i < g_thread_stat_new_list->cur_num && i < MAX_THREAD_NUM_OF_KEY_RESULT; i++) {
        proc = &g_thread_stat_new_list->array[i];

        g_key_result.thread_stat[i].cpu_percent = proc->delta_time * 100.f / total_delta_time;
        g_key_result.thread_stat[i].tid = proc->tid;
        strcpy(g_key_result.thread_stat[i].name, proc->tname);
        g_key_result.num_threads_valid = i + 1;
    }


    g_key_result.cpu_alive_cnt = sysconf(_SC_NPROCESSORS_ONLN);

    g_key_result.cpu_percent_user =
        ((g_new_cpu.utime + g_new_cpu.ntime) - (g_old_cpu.utime + g_old_cpu.ntime)) * 100.f /
        total_delta_time;
    g_key_result.cpu_percent_system =
        ((g_new_cpu.stime) - (g_old_cpu.stime)) * 100.f / total_delta_time;
    g_key_result.cpu_percent_iow =
        ((g_new_cpu.iowtime) - (g_old_cpu.iowtime)) * 100.f / total_delta_time;
    g_key_result.cpu_percent_irq = ((g_new_cpu.irqtime + g_new_cpu.sirqtime)
                                    - (g_old_cpu.irqtime + g_old_cpu.sirqtime)) * 100.f /
                                   total_delta_time;

    // uncomment this to print debug log
//    debug_print_current_key_result();
}

static struct proc_info* find_old_proc(proc_list* list, pid_t pid, pid_t tid) {
    int i;
    for (i = 0; i < list->cur_num; i++) {
        if ((list->array[i].pid == pid) && (list->array[i].tid == tid)) {
            return &list->array[i];
        }
    }
    return NULL;
}
