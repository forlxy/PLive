#include <algorithm>
#include "simple_priority_download_manager.h"
#include <algorithm>
#include "ac_log.h"

namespace kuaishou {
namespace cache {

SimplePriorityDownloadManager::SimplePriorityDownloadManager() {
}

void SimplePriorityDownloadManager::OnDestroy() {
    for (auto task : high_priority_queue_) {
        task->Resume();
    }
    for (auto task : normal_priority_queue_) {
        task->Resume();
    }
    for (auto task : low_priority_queue_) {
        task->Resume();
    }
}

SimplePriorityDownloadManager::~SimplePriorityDownloadManager() {
    // there shouldn't be any ptrs left in the queue, if there is, resume them all.
    OnDestroy();
}

void SimplePriorityDownloadManager::OnConnectionOpen(DownloadTaskWithPriority* task,
                                                     uint64_t position, const ConnectionInfo& info) {
    if (info.error_code != 0)  {
        return;
    }
    std::lock_guard<std::mutex> lg(mutex_);
    std::vector<DownloadTaskWithPriority*>* queue;
    switch (task->priority()) {
        case DownloadPriority::kPriorityLow:
            queue = &low_priority_queue_;
            break;
        case DownloadPriority::kPriorityDefault:
            queue = &normal_priority_queue_;
            break;
        case DownloadPriority::kPriorityHigh:
            queue = &high_priority_queue_;
            break;
    }
    Enqueue(queue, task);
    RePriorization();
}

void SimplePriorityDownloadManager::OnConnectionClosed(DownloadTaskWithPriority* task, const ConnectionInfo& info, DownloadStopReason reason, uint64_t, uint64_t) {
    std::lock_guard<std::mutex> lg(mutex_);
    std::vector<DownloadTaskWithPriority*>* queue;
    switch (task->priority()) {
        case DownloadPriority::kPriorityLow:
            queue = &low_priority_queue_;
            break;
        case DownloadPriority::kPriorityDefault:
            queue = &normal_priority_queue_;
            break;
        case DownloadPriority::kPriorityHigh:
            queue = &high_priority_queue_;
            break;
    }
    Dequeue(queue, task);
    RePriorization();
}

void SimplePriorityDownloadManager::Enqueue(std::vector<DownloadTaskWithPriority*>* queue,
                                            DownloadTaskWithPriority* task) {
    if (std::find(queue->begin(), queue->end(), task) == queue->end()) {
        queue->push_back(task);
    }
}

void SimplePriorityDownloadManager::Dequeue(std::vector<DownloadTaskWithPriority*>* queue,
                                            DownloadTaskWithPriority* task) {
    auto it = std::find(queue->begin(), queue->end(), task);
    if (it != queue->end()) {
        queue->erase(it);
    }
}

void SimplePriorityDownloadManager::RePriorization() {
    if (!high_priority_queue_.empty()) {
        ResumeAll(&high_priority_queue_);
        PauseAll(&normal_priority_queue_);
        PauseAll(&low_priority_queue_);
    } else if (!normal_priority_queue_.empty()) {
        ResumeAll(&normal_priority_queue_);
        PauseAll(&low_priority_queue_);
    } else if (!low_priority_queue_.empty()) {
        ResumeAll(&low_priority_queue_);
    }
}

void SimplePriorityDownloadManager::ResumeAll(std::vector<DownloadTaskWithPriority*>* queue) {
    for (auto task : *queue) {
        task->Resume();
    }
}

void SimplePriorityDownloadManager::PauseAll(std::vector<DownloadTaskWithPriority*>* queue) {
    for (auto task : *queue) {
        task->Pause();
    }
}

} // namespace cache
} // namespace kuaishou

