//
// Created by MarshallShuai on 2019-08-02.
//

#include "thread_worker.h"
#include <utility>
#include <unistd.h>
#include "ac_log.h"
#include "ac_utils.h"
#include "hodor_defs.h"

#if LOG_OVERALL_DOWNLOAD_STATUS
#include "hodor_downloader.h"
#endif

HODOR_NAMESPACE_START

static const bool kVerbose = false;
atomic<int> ThreadWorker::s_workder_id_index_(0);
atomic<int> ThreadWorker::s_alive_worker_thread_count_(0);

ThreadWorker::ThreadWorker(std::shared_ptr<DownloadPriorityTaskQueue> task_queue)
    : task_queue_(std::move(task_queue)), stopped_(false) {
    id_ = s_workder_id_index_.fetch_add(1);
    work_future_ = std::async(std::launch::async, [&]() { DoWork(); });
}

int ThreadWorker::GetAliveWorkerThreadCount() {
    return ThreadWorker::s_alive_worker_thread_count_.load();
}

void ThreadWorker::ReleaseAsync(std::shared_ptr<ThreadWorker> worker) {
    std::thread([worker]() {
        worker->Quit();
    }).detach();
}

void ThreadWorker::DoWork() {
    if (kVerbose) {
        LOG_WARN("[worker:%d][ThreadWorker::DoWork]thread start", id_);
    }
    s_alive_worker_thread_count_.fetch_add(1);
    task_queue_->RegisterListener(this);

    char thread_name[HODOR_THREAD_NAME_MAX_LEN];
    snprintf(thread_name, HODOR_THREAD_NAME_MAX_LEN, "[Hodor:ThreadWorker_%d]", id());
    AcUtils::SetThreadName(thread_name);
    while (!stopped_) {
        if (kVerbose) {
            LOG_VERBOSE("[worker:%d][ThreadWorker::DoWork]to PollTask", id_);
        }
        std::shared_ptr<DownloadPriorityStepTask> task;

        {
            std::unique_lock<std::mutex> lock(mutex_);
            task = task_queue_->PollTask(id_);
            if (task == nullptr) {
                if (kVerbose) {
                    LOG_WARN("[worker:%d][ThreadWorker::DoWork]task == nullptr, to wait", id_);
                }
                cond_.wait(lock);
                continue;
            }
        }

        task->StepExecute(id_);
        if (kVerbose) {
            LOG_VERBOSE("[worker:%d][ThreadWorker::DoWork]to PushBack", id_);
        }
        task_queue_->PushBack(task);

#if LOG_OVERALL_DOWNLOAD_STATUS
        LOG_INFO("[worker:%2d][ThreadWorker::DoWork] after task->StepExecute, thread_worker_count:%3d, "
                 "GetRemainTaskCount:%3d, finish task->StepExecute(%.2f) key:%-12s, main:%2d, sub:%2d",
                 id_, HodorDownloader::GetInstance()->GetThreadWorkerCount(),
                 HodorDownloader::GetInstance()->GetRemainTaskCount(),
                 task->GetProgressPercent(),
                 task->GetCacheKey().c_str(), task->multi_priority_.main_priority_, task->multi_priority_.sub_priority_);
#endif
    }

    task_queue_->UnregisterListener(this);
    if (kVerbose) {
        LOG_WARN("[worker:%d][ThreadWorker::DoWork]thread exit", id_);
    }
    s_alive_worker_thread_count_.fetch_sub(1);
}

void ThreadWorker::Quit() {
    std::lock_guard<std::mutex> lg(mutex_);
    stopped_ = true;
    cond_.notify_all();
    if (kVerbose) {
        LOG_WARN("[worker:%d][ThreadWorker::Quit] set stopped_ = true", id_);
    }
}

ThreadWorker::~ThreadWorker() {
    LOG_DEBUG("[worker:%d[ThreadWorker::~ThreadWorker]ThreadWorker::~ThreadWorker", id_);
}

void ThreadWorker::OnTaskQueueUpdated() {
    cond_.notify_all();
}


HODOR_NAMESPACE_END
