//
// Created by MarshallShuai on 2019-08-02.
//
#pragma once

#include <memory>
#include <utils/macro_util.h>
#include <set>
#include <mutex>
#include <list>
#include "event.h"
#include "download_priority_step_task.h"
#include "ac_log.h"
#include <sstream>
#include <atomic>
#include "awesome_cache_interrupt_cb_c.h"

HODOR_NAMESPACE_START

using TaskType = DownloadPriorityStepTask::TaskType;

class QueueTaskListener {
  public:
    virtual ~QueueTaskListener() {};
    /**
     * 这个通知的语义是，当有新的任务加入，或者是旧的任务执行完一次StepExecute的被pushBack回队列的时候。通知
     * ThreadWorker来来重新尝试 PollTask
     */
    virtual void OnTaskQueueUpdated() = 0;
};

/**
 * 这个类用来反映查询/cancel的时候返回的结果，内部字段反映了具体的每个queue受影响到/统计到的task个数
 */
struct QueueResult {
    size_t Total() const {
        return count_for_waiting_queue_ + count_for_working_queue_;
    }

    size_t count_for_waiting_queue_ = 0;
    size_t count_for_working_queue_ = 0;
};

class DownloadPriorityTaskQueue {
  public:
    static constexpr unsigned int  kOperatorSourceFlagDownloader = 1 << 0;
    static constexpr unsigned int  kOperatorSourceFlagPlayer = 1 << 1;

    DownloadPriorityTaskQueue();
    /**
     * submitTask
     * 如果submit同样cacheKey + type 的task，并且之前这个task已经存在，那么submit将会失败
     * @return current queue size
     */
    size_t Submit(std::shared_ptr<DownloadPriorityStepTask> task);

    /**
     * threadWorker取出的任务执行了一个时间分片后，如果没执行完，会push回来
     * 如果队列里已有相同任务，则会被丢弃，以队列中已有的任务信息为准
     * @return current waiting queue size
     */
    size_t PushBack(std::shared_ptr<DownloadPriorityStepTask> task);

    /**
     * ThreadWorker 取任务的接口
     */
    std::shared_ptr<DownloadPriorityStepTask> PollTask(int thread_work_id = -1);

    /**
     * 当前队列中的任务数
     */
    QueueResult GetTaskCountStatus();


    void RegisterListener(QueueTaskListener* listener);

    void UnregisterListener(QueueTaskListener* listener);

    /**
     * 让整个下载队列处于暂停状态，但是不会改改变task的Pause/Resume状态，如果任务正在执行中，则会Interrupt该任务
     * 这是个无锁快速的方法
     * @param pause_source_flag 表示是从哪个源pause了队列的
     * @return 有多少个任务因pause被interrupt了
     */
    size_t FastPauseQueue(unsigned int pause_source_flag);

    /**
     * 让整个下载队列处于恢复下载的状态,无锁快速resume的方法
     * @param resume_source_flag 表示是从哪个源resume了队列的
     */
    void FastResumeQueue(unsigned int resume_source_flag);

    /**
     * Cancel某个特定的任务
     * @param task 需要取消的task
     */
    size_t CancelTask(std::shared_ptr<DownloadPriorityStepTask> task);

    /**
     * Cancel某个特定的任务，原理上只会清理特定的某一个task，因为队列中不会存在key相同的任务
     * @return 实际cancel的个数,理论上只会返回一个，（记得让单元测试保证）
     */
    size_t CancelTaskByKey(std::string key, TaskType type);

    /**
     * Cancel所有某个特定优先级的任务，一般清理的任务可能不止1个
     */
    size_t CancelAllTasksOfMainPriority(int main_priority);

    /**
     * Cancel所有某个类型的任务
     */
    void CancelAllTasksOfType(TaskType type);

    /**
     * Cancel所有的任务
     */
    size_t CancelAllTasks();

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
     * 因为单元测试原因，下面3个函数给与public访问权限
     * 按某个原则去cancel/pause/resume任务
     * @param criteria 具体的原则实现
     * @return 每个队列影响到的个数
     */
    QueueResult CancelTasksByCriteria(
        const std::function<bool(std::shared_ptr<DownloadPriorityStepTask>)>& criteria);
    QueueResult PauseTasksByCriteriaNonLock(
        const std::function<bool(std::shared_ptr<DownloadPriorityStepTask>)>& criteria);
    QueueResult ResumeTasksByCriteriaNonLock(
        const std::function<bool(std::shared_ptr<DownloadPriorityStepTask>)>& criteria);

    AwesomeCacheInterruptCB GetInterruptCallback();

    void GetStatusForDebugInfo(size_t thread_worker_cnt, size_t current_player_task_cnt,  char* buf, int buf_len);
  private:
    inline bool ShouldPause(std::shared_ptr<DownloadPriorityStepTask>& task);

    inline void UpdateTaskPriorityInWaitingQueue(const std::shared_ptr<DownloadPriorityStepTask>& task);
    /**
     * 一个任务进入这个task queue的时候调用，会做一些比如备案keyTypeId的事情
     */
    inline void OnTaskIntoWaitingQueue(const std::shared_ptr<DownloadPriorityStepTask>& task);
    /**
     * 一个任务从本queue中退出，不再维护的时候调用
     */
    inline void OnTaskExitWatingQueue(const std::shared_ptr<DownloadPriorityStepTask>& task);
    //
    void NotifyTaskQueueUpdated();

    int IsQueuePaused();

    friend int DownloadPriorityTaskQueue_interrupt_cb(void* opaque);
    /**
     * 无锁操作
     * @return 添加完任务后队列里的任务数
     */
    void AddTaskToWaitingQueueNonLock(const std::shared_ptr<DownloadPriorityStepTask>& task);

    /**
     * 按优先级在queue中排序，优先级大的排在前面(front)
     */
    struct PriorityTaskCompare {
        bool operator()(std::shared_ptr<DownloadPriorityStepTask> const& t1, std::shared_ptr<DownloadPriorityStepTask> const& t2) {
            if (t1->multi_priority_ != t2->multi_priority_) {
                return t1->multi_priority_ > t2->multi_priority_;
            }
            // 如果优先级完全一样，则id小的，优先级高，排在前面，这样能保证集中先完成一个任务，而不是完全乱序
            return t1->id() < t2->id();
        }
    };

    std::mutex listeners_mutex_;
    std::set<QueueTaskListener*> listener_set_;

    // 为了能快速check同样 key+type的task是否存在队列中，队列中waiting queue里相同的keyTypeDigest的任务只会存在一个
    std::set<std::string> waiting_queue_task_keytype_set;
    // 已经被pause的task type
    std::set<TaskType> paused_task_type_set_;
    // 已经被pause的priority
    std::set<int> paused_main_priority_set_;
    bool pause_all_tasks_ = false;

    std::mutex queue_mutex_;

    // 这个队列里的表示正在排队的task，需按优先级排列
    std::list<std::shared_ptr<DownloadPriorityStepTask>> waiting_task_queue_;
    // 这个队列里的表示正在执行的task，无需按优先级排列
    std::list<std::shared_ptr<DownloadPriorityStepTask>> working_task_queue_;

    // 队列的暂停状态和task的本身的暂停状态是分开管理的，只有队列是resume状态并且 waiting_taks_queue_里有任务，ThreadWorker才能获取到任务去执行
    bool queue_paused_;
    atomic<unsigned int> queue_paused_flag_;
    static constexpr unsigned int kStatusFlagResumed = 0; // 表示当前queue是resume状态的

    AwesomeCacheInterruptCB interrupt_cb_{};
};

HODOR_NAMESPACE_END
