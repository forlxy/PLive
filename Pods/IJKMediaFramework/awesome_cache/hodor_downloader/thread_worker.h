//
// Created by MarshallShuai on 2019-08-02.
//


#pragma once

#include <atomic>
#include <future>
#include <memory>
#include <mutex>
#include "download_priority_task_queue.h"
#include <utils/macro_util.h>

HODOR_NAMESPACE_START

class ThreadWorker : public QueueTaskListener {
  public:
    /**
     * 结束线程，异步返回
     */
    static void ReleaseAsync(std::shared_ptr<ThreadWorker> worker);

    /**
     * 主要给单元测试和检测线程泄漏的目的使用
     * @return 当前还活着的线程数
     */
    static int GetAliveWorkerThreadCount();

    ThreadWorker(std::shared_ptr<DownloadPriorityTaskQueue> task_queue);

    ~ThreadWorker();

    int id() const {
        return id_;
    }

    virtual void OnTaskQueueUpdated() override;
  private:
    void DoWork();
    /**
     * 想退出线程，此调用是同步函数
     */
    void Quit();

  private:
    static atomic<int> s_workder_id_index_;
    static atomic<int> s_alive_worker_thread_count_;

    int id_;

  private:
    std::mutex mutex_;
    std::condition_variable cond_;
    bool stopped_;

    // 这个不能删，不然会踩到async的坑。future必须用成员变量保存，如果用栈变量保存的时候，则会在future析构的时候
    // 等async线程结束，进而造成卡死当前开启async线程的线程
    std::future<void> work_future_;

    std::shared_ptr<DownloadPriorityTaskQueue> task_queue_;

};

HODOR_NAMESPACE_END