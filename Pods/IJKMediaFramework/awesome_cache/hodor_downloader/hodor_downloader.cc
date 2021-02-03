//
// Created by MarshallShuai on 2019-08-02.
//

#include "hodor_downloader.h"
#include <utility>
#include <v2/cache/cache_v2_file_manager.h>
#include "hodor_defs.h"
#include "ac_log.h"
#include "v2/cache/cache_content_index_v2.h"

HODOR_NAMESPACE_START

static const bool kVerbose = false;

HodorDownloader* HodorDownloader::instance_;
std::mutex HodorDownloader::instance_mutex_;

HodorDownloader::HodorDownloader(): inited_(false) {
    priority_task_queue_ = std::make_shared<DownloadPriorityTaskQueue>();
    network_monitor_ = std::make_shared<NetworkMonitor>();
    traffic_coordinator_ = std::make_shared<TrafficCoordinator>(network_monitor_.get(), this);
    strategy_never_evictor_ = std::make_shared<StrategyNeverCacheContentEvictor>();
}

HodorDownloader* HodorDownloader::GetInstance() {
    if (instance_ == nullptr) {
        std::lock_guard<std::mutex> lock(instance_mutex_);
        if (instance_ == nullptr) {
            instance_ = new HodorDownloader();
            instance_->SetThreadWorkerCount(kHodorThreadWorkCountInit);
        }
    }
    return instance_;
}

size_t HodorDownloader::SetThreadWorkerCount(int count) {
    if (count > kHodorThreadWorkCountMax || count < kHodorThreadWorkCountMin) {
        LOG_WARN("[HodorDownloader::SetThreadWorkerCount] count invalid:%d, should between:%d~%d",
                 count, kHodorThreadWorkCountMin, kHodorThreadWorkCountMax);
        return thread_worker_vec_.size();
    }
    if (kVerbose) {
        LOG_INFO("[HodorDownloader::SetThreadWorkerCount] count:%d", count);
    }
    return SetThreadWorkerCountWithoutCheck(count);
}

size_t HodorDownloader::SetThreadWorkerCountWithoutCheck(int count) {
    if (count < 0) {
        return thread_worker_vec_.size();
    }

    std::lock_guard<std::mutex> lg(thread_worker_vec_mutex_);
    if (thread_worker_vec_.size() < count) {
        size_t to_add = count - thread_worker_vec_.size();
        for (size_t i = 0; i < to_add; i++) {
            thread_worker_vec_.push_back(std::make_shared<ThreadWorker>(priority_task_queue_));
        }
    } else if (thread_worker_vec_.size() > count) {
        size_t to_delete = thread_worker_vec_.size() - count;
        for (size_t i = 0; i < to_delete; i++) {
            auto worker = *thread_worker_vec_.begin();
            if (kVerbose) {
                LOG_INFO("[HodorDownloader::SetThreadWorkerCount]to delete worker:%d", worker->id())
            }
            ThreadWorker::ReleaseAsync(worker);
            thread_worker_vec_.erase(thread_worker_vec_.begin());
        }
    }
    return thread_worker_vec_.size();
}

void HodorDownloader::ClearThreadWorkers() {
    SetThreadWorkerCountWithoutCheck(0);
}


void HodorDownloader::PauseQueue() {
    priority_task_queue_->FastPauseQueue(DownloadPriorityTaskQueue::kOperatorSourceFlagDownloader);
}

void HodorDownloader::ResumeQueue() {
    priority_task_queue_->FastResumeQueue(DownloadPriorityTaskQueue::kOperatorSourceFlagDownloader);
}

size_t HodorDownloader::GetThreadWorkerCount() {
    return thread_worker_vec_.size();
}

void HodorDownloader::SubmitTask(std::shared_ptr<DownloadPriorityStepTask> task) {
    priority_task_queue_->Submit(std::move(task));
}


size_t HodorDownloader::GetRemainTaskCount() const {
    return priority_task_queue_->GetTaskCountStatus().Total();
}

void HodorDownloader::DeleteCacheByKey(std::string key, TaskType task_type) {
    if (task_type == TaskType::Media) {
        auto content_with_scope = CacheV2FileManager::GetMediaDirManager()->Index()->GetCacheContent("", key, false);
        content_with_scope->DeleteScopeFiles();
    } else if (task_type == TaskType::Resource) {
        auto content_non_scope = CacheV2FileManager::GetResourceDirManager()->Index()->GetCacheContent("", key, false);
        content_non_scope->DeleteCacheContentFile(true);
    } else {
        LOG_WARN("[HodorDownloader::DeleteCacheByKey] illegal task_type :%d for key:%s", task_type, key.c_str());
    }
}

void HodorDownloader::GetStatusForDebugInfo(char* buf, int buf_len) {
    priority_task_queue_->GetStatusForDebugInfo(thread_worker_vec_.size(),
                                                traffic_coordinator_->CurrentPlayerTaskCount(), buf, buf_len);
}

TrafficCoordinator* HodorDownloader::GetTrafficCoordinator() {
    return traffic_coordinator_.get();
}


void HodorDownloader::ResumePreloadTask() {
    priority_task_queue_->FastResumeQueue(DownloadPriorityTaskQueue::kOperatorSourceFlagPlayer);
}

void HodorDownloader::PausePreloadTask() {
    priority_task_queue_->FastPauseQueue(DownloadPriorityTaskQueue::kOperatorSourceFlagPlayer);
}

void HodorDownloader::CancelTask(std::shared_ptr<DownloadPriorityStepTask> task) {
    priority_task_queue_->CancelTask(task);
}


void HodorDownloader::CancelTaskByKey(std::string key, TaskType type) {
    priority_task_queue_->CancelTaskByKey(std::move(key), type);
}

void HodorDownloader::CancelAllTasksOfMainPriority(int main_priority) {
    priority_task_queue_->CancelAllTasksOfMainPriority(main_priority);
}

void HodorDownloader::CancelAllTasksOfType(TaskType type) {
    priority_task_queue_->CancelAllTasksOfType(type);
}

void HodorDownloader::CancelAllTasks() {
    priority_task_queue_->CancelAllTasks();
}

void HodorDownloader::PauseTaskByKey(std::string key, TaskType type) {
    priority_task_queue_->PauseTaskByKey(key, type);
}

void HodorDownloader::PauseAllTasksOfMainPriority(int main_priority) {
    priority_task_queue_->PauseAllTasksOfMainPriority(main_priority);
}

void HodorDownloader::PauseAllTasksOfType(TaskType type) {
    priority_task_queue_->PauseAllTasksOfType(type);
}

void HodorDownloader::PauseAllTasks() {
    priority_task_queue_->PauseAllTasks();
}

void HodorDownloader::ResumeTaskByKey(std::string key, TaskType type) {
    priority_task_queue_->ResumeTaskByKey(key, type);
}

void HodorDownloader::ResumeAllTasksOfMainPriority(int main_priority) {
    priority_task_queue_->ResumeAllTasksOfMainPriority(main_priority);
}

void HodorDownloader::ResumeAllTasksOfType(TaskType type) {
    priority_task_queue_->ResumeAllTasksOfType(type);
}

void HodorDownloader::ResumeAllTasks() {
    priority_task_queue_->ResumeAllTasks();
}

AwesomeCacheInterruptCB HodorDownloader::GetInterruptCallback() {
    return priority_task_queue_->GetInterruptCallback();
}

void HodorDownloader::OnStrategyNeverTaskAdded(const std::string& key) {
    strategy_never_evictor_->OnStrategyNeverTaskAdded(key);
}

void HodorDownloader::PruneStrategyNeverCacheContent(bool clear_all) {
    strategy_never_evictor_->PruneStrategyNeverCacheContent(clear_all);
}


HODOR_NAMESPACE_END
