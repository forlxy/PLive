//
// Created by MarshallShuai on 2019-08-02.
//

#pragma once

#include <string>
#include <mutex>
#include "utils/macro_util.h"
#include "cache_errors.h"
#include "multi_priority.h"

HODOR_NAMESPACE_START


/**
 * 一个分步进行的下载任务，每一次execute会进行一部分task(step of task)，直到多次运行后全部完成
 */
class DownloadPriorityStepTask {
  public:
    typedef enum TaskType {
        Media = 0,
        Resource = 1
    } TaskType;

    virtual TaskType GetTaskType() const {
        return Media;
    };

    /**
     * 此任务的id，随创建的时间戳来单调递增的，越晚创建的任务id越大
     */
    virtual int id() {return id_;};
    /**
     *
     * @param key cacheKey
     * @param main_priority 这里类型不强制要求为TaskPriority，这是最底层实现，可以接受更广泛的范围
     * @param sub_priority 子优先级
     */
    DownloadPriorityStepTask(const std::string& key, int main_priority = Priority_HIGH, int sub_priority = 0);

    DownloadPriorityStepTask(int main_priority = Priority_HIGH, int sub_priority = 0);

    virtual ~DownloadPriorityStepTask() {}

    /**
     * @param thread_work_id 主要用来调试，看当前任务是在哪个线程执行的
     */
    virtual AcResultType StepExecute(int thread_work_id);

    /**
     * @return 返回一个大概完成的百分比，1表示完成
     */
    virtual float GetProgressPercent();

    /**
    * 打断任务，回到队列中继续等待
    */
    virtual void Interrupt() {};

    /**
     * 如果任务正在执行，则会中断任务
     * 如果任务正在等待队列，则直接丢弃该任务
     */
    void Cancel();
    /**
     * 暂停任务，如果任务正在进行，则会中断任务
     * 调用应该是不耗时的
     */
    void Pause();
    /**
     * 设置任务为恢复状态
     * 调用应该是不耗时的
     */
    void Resume();
    /**
     *
     * @return  true 表示该任务complete了，无需再回到等待队列了
     */
    void MarkComplete();

    void CopyPriorityOf(const DownloadPriorityStepTask& other);

    typedef enum TaskStatus {
        Resumed = 0,
        Paused,
        Cancelled,
        Completed,
    } TaskStatus;
    TaskStatus GetTaskStatus();

    const std::string& GetCacheKey();

    // 这个主要给debugInfo使用，所以不要加锁
    const char* GetStatusString();
    /**
     * 一个key 和 type能组成唯一的一个标识，主要是用来确认队列中是否存在同样的任务
     * @return CacheKey_TaskType like string
     */
    std::string GetKeyTypeDigest() const;

    virtual AcResultType LastError() {return kResultOK;}
  public:
    int id_;  // 越晚创建的任务id越大，相对优先级越低
    MultiPriority multi_priority_;

    std::string key_;

  private:
    std::mutex status_mutex_;
    TaskStatus status_;
};

HODOR_NAMESPACE_END
