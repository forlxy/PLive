//
// Created by MarshallShuai on 2019-08-02.
//

#pragma once

#include <vector>
#include <mutex>
#include <utils/macro_util.h>
#include <sstream>
#include "thread_worker.h"
#include "download_priority_task_queue.h"
#include "download_priority_step_task.h"
#include "traffic_coordinator.h"
#include "network_monitor.h"
#include "v2/cache/strategy_never_cache_content_evictor.h"

HODOR_NAMESPACE_START

using TaskType = DownloadPriorityStepTask::TaskType;

/**
 * Hodor下载器，负责thread worker管理。任务进出
 */
class HodorDownloader : public TrafficCoordinator::Listener {
  public:
    static HodorDownloader* GetInstance();

    TrafficCoordinator* GetTrafficCoordinator();
    /**
     * 设置并发数
     * @return 设置之后的并发数
     */
    size_t SetThreadWorkerCount(int count);

    /**
     * 清理ThreadWorker，理论上只有单元测试能用到，因为单元测试做不到对单例模式的Clean fixture，只能手动模拟
     */
    void ClearThreadWorkers();

    void PauseQueue();

    void ResumeQueue();
    /**
     * 无锁调用
     */
    size_t GetThreadWorkerCount();

    /**
     * 提交task
     * 如果这个task已经存在队列或者ThreadWorker正在进行中，则更新这个task的设置
     */
    void SubmitTask(std::shared_ptr<DownloadPriorityStepTask> task);

    /**
     * Cancel某个特定的任务
     * @param task 需要取消的task
     */
    void CancelTask(std::shared_ptr<DownloadPriorityStepTask> task);
    /**
     * Cancel某个特定的任务，原理上只会清理特定的某一个task，因为队列中不会存在key相同的任务
     */
    void CancelTaskByKey(std::string key, TaskType type);
    /**
     * Cancel所有某个特定优先级的任务
     */
    void CancelAllTasksOfMainPriority(int main_priority);
    /**
     * Cancel所有某个类型的任务
     */
    void CancelAllTasksOfType(TaskType type);
    /**
     * Cancel所有任务
     */
    void CancelAllTasks();

    /**
     * Pause某个特定的任务，原理上只会清理特定的某一个task，因为队列中不会存在key相同的任务
     */
    void PauseTaskByKey(std::string key, TaskType type);
    /**
     * Pause所有某个特定优先级的任务
     */
    void PauseAllTasksOfMainPriority(int main_priority);
    /**
     * Pause所有某个类型的任务
     */
    void PauseAllTasksOfType(TaskType type);
    /**
     * Pause所有任务
     */
    void PauseAllTasks();

    /**
     * Resume某个特定的任务，原理上只会清理特定的某一个task，因为队列中不会存在key相同的任务
     */
    void ResumeTaskByKey(std::string key, TaskType type);
    /**
     * Resume所有某个特定优先级的任务
     */
    void ResumeAllTasksOfMainPriority(int main_priority);
    /**
     * Resume所有某个类型的任务
     */
    void ResumeAllTasksOfType(TaskType type);
    /**
     * 恢复任务执行进程
     */
    void ResumeAllTasks();
    /**
     *
     * @return 留在队列中的任务数
     */
    size_t GetRemainTaskCount() const;

    void DeleteCacheByKey(std::string key, TaskType task_type);

    AwesomeCacheInterruptCB GetInterruptCallback();

    /**
     * A1活动临时接口
     * 表示添加一个预热视频到记录中
     */
    void OnStrategyNeverTaskAdded(const std::string& key);
    /**
     *
     * 清理春节相关的预热资源
     * @param clear_all true表示全部清除，false表示只清理不在下发列表中的预热视频
     */
    void PruneStrategyNeverCacheContent(bool clear_all = false);
    /**
     * 获取debugInfo
     */
    void GetStatusForDebugInfo(char* buf, int buf_len);

#pragma mark TrafficCoordinator::Listener
    void ResumePreloadTask() override ;
    void PausePreloadTask() override ;

    NetworkMonitor* GetNetworkMonitor() {
        return network_monitor_.get();
    }
  private:
    HodorDownloader();
    /**
     * 这个不会检查上下限，count是多少就设置多少并发
     * @return 设置之后生效的并发数
     */
    size_t SetThreadWorkerCountWithoutCheck(int count);

  private:
    static HodorDownloader* instance_;
    static std::mutex instance_mutex_;
    bool inited_;

    std::shared_ptr<NetworkMonitor> network_monitor_;

    std::shared_ptr<DownloadPriorityTaskQueue> priority_task_queue_;
    std::mutex thread_worker_vec_mutex_;
    std::vector<std::shared_ptr<ThreadWorker>> thread_worker_vec_;

    std::shared_ptr<TrafficCoordinator> traffic_coordinator_;

    std::shared_ptr<StrategyNeverCacheContentEvictor> strategy_never_evictor_;
};


HODOR_NAMESPACE_END
