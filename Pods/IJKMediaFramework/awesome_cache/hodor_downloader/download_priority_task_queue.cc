//
// Created by MarshallShuai on 2019-08-02.
//

#include "download_priority_task_queue.h"
#include <utility>
#include "ac_log.h"

HODOR_NAMESPACE_START

using TaskStatus = DownloadPriorityStepTask::TaskStatus;

static const bool kVerbose = true;
static const bool kPauseResumeVerbose = false;
static const bool kApiCalledVerbose = false;
static const bool kOperateTaskVerbose = false;

int DownloadPriorityTaskQueue_interrupt_cb(void* opaque) {
    if (opaque) {
        auto* thiz = static_cast<DownloadPriorityTaskQueue*>(opaque);
        return thiz->IsQueuePaused();
    }
    return 0;
}

DownloadPriorityTaskQueue::DownloadPriorityTaskQueue() : queue_paused_(false), queue_paused_flag_(kStatusFlagResumed) {
    interrupt_cb_.opaque = this;
    interrupt_cb_.callback = &DownloadPriorityTaskQueue_interrupt_cb;
}

size_t DownloadPriorityTaskQueue::Submit(std::shared_ptr<DownloadPriorityStepTask> new_task) {
    std::lock_guard<std::mutex> lg(queue_mutex_);

    if (waiting_queue_task_keytype_set.find(new_task->GetKeyTypeDigest()) != waiting_queue_task_keytype_set.end()) {
        LOG_INFO("[DownloadPriorityTaskQueue::Submit]new_task already exist in waiting_queue, to UpdateTaskPriorityInWaitingQueue, key:%s, type:%d",
                 new_task->GetCacheKey().c_str(), new_task->GetTaskType());
        UpdateTaskPriorityInWaitingQueue(new_task);
        return waiting_task_queue_.size();
    }

    // 判断新进的task是否要pause
    bool task_to_pause = false;
    if (ShouldPause(new_task)) {
        task_to_pause = true;
        new_task->Pause();
    }

    AddTaskToWaitingQueueNonLock(new_task);

    if (!task_to_pause) {
        // 加进来的任务如果其实不是pause的状态才需要通知队列更新
        NotifyTaskQueueUpdated();
    }

    return waiting_task_queue_.size();
}

size_t DownloadPriorityTaskQueue::PushBack(std::shared_ptr<DownloadPriorityStepTask> task) {
    std::lock_guard<std::mutex> lg(queue_mutex_);

    // 如果存在working队列中，则要移除
    std::list<std::shared_ptr<DownloadPriorityStepTask>>::iterator iter;
    for (iter = working_task_queue_.begin(); iter != working_task_queue_.end(); iter++) {
        if (iter->get()->GetKeyTypeDigest() == task->GetKeyTypeDigest()) {
            break;
        }
    }
    if (iter == working_task_queue_.end()) {
        LOG_WARN("[DownloadPriorityTaskQueue::PushBack] task not in working_task_queue_, this can only be allowed in unit test");
        return waiting_task_queue_.size();
    } else {
        working_task_queue_.erase(iter);
    }

    // 以下情况直接丢弃任务
    // 1.已经取消
    // 2.完成
    // 3.出错
    // 4.waiting队列已经有的任务
    if ((task->GetTaskStatus() == TaskStatus::Cancelled)
        || (task->GetTaskStatus() == TaskStatus::Completed)
        || (task->LastError() != kResultOK)
        || (waiting_queue_task_keytype_set.find(task->GetKeyTypeDigest()) != waiting_queue_task_keytype_set.end())) {
        if (kVerbose && (task->LastError() != kResultOK)) {
            // 出错的情况单独打印一下日志
            LOG_WARN("[DownloadPriorityTaskQueue::PushBack]task error happened, drop it, error:%d", task->LastError());
        }
        // do nothing to abort the task
    } else {
        if (ShouldPause(task)) {
            task->Pause();
        }
        AddTaskToWaitingQueueNonLock(std::move(task));
    }

    NotifyTaskQueueUpdated();
    return waiting_task_queue_.size();
}

std::shared_ptr<DownloadPriorityStepTask> DownloadPriorityTaskQueue::PollTask(int thread_work_id) {
    std::unique_lock<std::mutex> lock(queue_mutex_);
    if (waiting_task_queue_.empty()) {
        if (kVerbose) {
            LOG_VERBOSE("[worker:%d][DownloadPriorityTaskQueue::PollTask] waiting_task_queue_.empty() return nullptr ", thread_work_id);
        }
        return nullptr;
    } else {
        MultiPriority current_working_priority(-1, -1);
        // 如果当前有更高优先级的任务正在执行，是不会执行低优先级任务的
        if (!working_task_queue_.empty()) {
            current_working_priority = working_task_queue_.front()->multi_priority_;
        }

        // 从 waiting queue里找到满足以下两个条件的任务：
        // 1.优先级 >= current_working_priority
        // 2.没被pause
        for (auto iter = waiting_task_queue_.begin(); iter != waiting_task_queue_.end(); iter++) {
            auto& task = *iter;
            if (current_working_priority > task->multi_priority_) {
                if (kVerbose) {
                    LOG_VERBOSE("[worker:%d][DownloadPriorityTaskQueue::PollTask]有更高优先级的任务在执行，继续等待", thread_work_id);
                }
                return nullptr;
            }

            if (task->multi_priority_.main_priority_ == Priority_UNLIMITED
                || (!queue_paused_ && task->GetTaskStatus() != TaskStatus::Paused)) {
                if (task == nullptr) {
                    LOG_ERROR("[worker:%d][DownloadPriorityTaskQueue::PollTask]Inner error, found_task = null", thread_work_id);
                }
                working_task_queue_.push_back(task);
                OnTaskExitWatingQueue(task);
                waiting_task_queue_.erase(iter);
                return task;
            }
        }

        if (kVerbose) {
            LOG_VERBOSE("[worker:%d][DownloadPriorityTaskQueue::PollTask]没找到符合条件的任务，返回null", thread_work_id);
        }
        return nullptr;
    }
}

QueueResult DownloadPriorityTaskQueue::GetTaskCountStatus() {
    std::unique_lock<std::mutex> lock(queue_mutex_);
    QueueResult ret;
    ret.count_for_waiting_queue_ = waiting_task_queue_.size();
    ret.count_for_working_queue_ = working_task_queue_.size();
    return ret;
};


void DownloadPriorityTaskQueue::NotifyTaskQueueUpdated() {
    std::lock_guard<std::mutex> lg(listeners_mutex_);
    for (auto listener : listener_set_) {
        listener->OnTaskQueueUpdated();
    }
}

int DownloadPriorityTaskQueue::IsQueuePaused() {
    return queue_paused_flag_ != kStatusFlagResumed;

}
void DownloadPriorityTaskQueue::AddTaskToWaitingQueueNonLock(
    const std::shared_ptr<DownloadPriorityStepTask>& task) {
    waiting_task_queue_.push_back(task);
    waiting_task_queue_.sort(PriorityTaskCompare());
    OnTaskIntoWaitingQueue(task);
}

void DownloadPriorityTaskQueue::RegisterListener(QueueTaskListener* listener) {
    std::lock_guard<std::mutex> lg(listeners_mutex_);
    listener_set_.insert(listener);
}

void DownloadPriorityTaskQueue::UnregisterListener(QueueTaskListener* listener) {
    std::lock_guard<std::mutex> lg(listeners_mutex_);
    listener_set_.erase(listener);
}

size_t DownloadPriorityTaskQueue::FastPauseQueue(unsigned int  pause_source_flag) {
    if (kApiCalledVerbose)
        LOG_WARN("[DownloadPriorityTaskQueue::%s]pause_source_flag:%d <<<<< ", __func__, pause_source_flag);
    queue_paused_flag_.store(queue_paused_flag_ | pause_source_flag);
    if (queue_paused_flag_ != kStatusFlagResumed) {
        queue_paused_ = true;
    }
    return working_task_queue_.size();
}

void DownloadPriorityTaskQueue::FastResumeQueue(unsigned int resume_source_flag) {
    if (kApiCalledVerbose)
        LOG_WARN("[DownloadPriorityTaskQueue::%s] >>>>>", __func__);
    queue_paused_flag_.store(queue_paused_flag_ & ~ resume_source_flag);
    if (queue_paused_flag_ == kStatusFlagResumed) {
        queue_paused_ = false;
        NotifyTaskQueueUpdated();
    }
}


size_t
DownloadPriorityTaskQueue::CancelTask(std::shared_ptr<DownloadPriorityStepTask> cancel_task) {
    if (kApiCalledVerbose)
        LOG_WARN("[DownloadPriorityTaskQueue::%s]", __func__);
    auto ret =  CancelTasksByCriteria([&cancel_task](std::shared_ptr<DownloadPriorityStepTask> task) {
        return task.get() == cancel_task.get();
    });

    if (ret.Total() > 0) {
        NotifyTaskQueueUpdated();
    }
    return ret.Total();
}

size_t DownloadPriorityTaskQueue::CancelTaskByKey(std::string key, TaskType type) {
    if (kApiCalledVerbose)
        LOG_WARN("[DownloadPriorityTaskQueue::%s]", __func__);
    auto ret =  CancelTasksByCriteria([&key, &type](std::shared_ptr<DownloadPriorityStepTask> task) {
        return task->GetCacheKey() == key && task->GetTaskType() == type;
    });

    if (ret.Total() > 0) {
        NotifyTaskQueueUpdated();
    }
    return ret.Total();
}

size_t DownloadPriorityTaskQueue::CancelAllTasksOfMainPriority(int main_priority) {
    if (kApiCalledVerbose)
        LOG_WARN("[DownloadPriorityTaskQueue::%s]", __func__);
    auto ret = CancelTasksByCriteria([&main_priority](std::shared_ptr<DownloadPriorityStepTask> task) {
        return task->multi_priority_.main_priority_ == main_priority;
    });

    if (ret.Total() > 0) {
        NotifyTaskQueueUpdated();
    }
    return ret.Total();
}

void DownloadPriorityTaskQueue::CancelAllTasksOfType(TaskType type) {
    if (kApiCalledVerbose)
        LOG_WARN("[DownloadPriorityTaskQueue::%s]", __func__);
    auto ret = CancelTasksByCriteria([&type](std::shared_ptr<DownloadPriorityStepTask> task) {
        return task->GetTaskType() == type;
    });

    if (ret.Total() > 0) {
        NotifyTaskQueueUpdated();
    }
}

size_t DownloadPriorityTaskQueue::CancelAllTasks() {
    auto ret = CancelTasksByCriteria([](std::shared_ptr<DownloadPriorityStepTask> task) {
        return true;
    });

    if (ret.Total() > 0) {
        NotifyTaskQueueUpdated();
    }
    return ret.Total();
}

void DownloadPriorityTaskQueue::PauseTaskByKey(std::string key, TaskType type) {
    std::unique_lock<std::mutex> lock(queue_mutex_);
    if (kApiCalledVerbose)
        LOG_WARN("[DownloadPriorityTaskQueue::%s]", __func__);
    auto ret = PauseTasksByCriteriaNonLock([&key, &type](std::shared_ptr<DownloadPriorityStepTask> task) {
        return task->GetCacheKey() == key && task->GetTaskType() == type;
    });

    if (ret.Total() > 0) {
        NotifyTaskQueueUpdated();
    }
}

void DownloadPriorityTaskQueue::PauseAllTasksOfMainPriority(int main_priority) {
    std::unique_lock<std::mutex> lock(queue_mutex_);
    if (kApiCalledVerbose)
        LOG_WARN("[DownloadPriorityTaskQueue::%s]", __func__);
    paused_main_priority_set_.insert(main_priority);
    auto ret = PauseTasksByCriteriaNonLock([&main_priority](
    std::shared_ptr<DownloadPriorityStepTask> task) {
        return task->multi_priority_.main_priority_ == main_priority;
    });
    if (ret.Total() > 0) {
        NotifyTaskQueueUpdated();
    }
}

void DownloadPriorityTaskQueue::PauseAllTasksOfType(TaskType type) {
    std::unique_lock<std::mutex> lock(queue_mutex_);
    if (kApiCalledVerbose)
        LOG_WARN("[DownloadPriorityTaskQueue::%s]", __func__);
    paused_task_type_set_.insert(type);
    auto ret = PauseTasksByCriteriaNonLock([&type](std::shared_ptr<DownloadPriorityStepTask> task) {
        if (kApiCalledVerbose)
            LOG_WARN("[DownloadPriorityTaskQueue::%s]task->GetTaskType():%d, type:%d", __func__, task->GetTaskType(), type);
        return task->GetTaskType() == type;
    });
    if (ret.Total() > 0) {
        NotifyTaskQueueUpdated();
    }
}

void DownloadPriorityTaskQueue::PauseAllTasks() {
    std::unique_lock<std::mutex> lock(queue_mutex_);
    if (kApiCalledVerbose)
        LOG_WARN("[DownloadPriorityTaskQueue::%s]", __func__);
    pause_all_tasks_ = true;
    PauseTasksByCriteriaNonLock([](std::shared_ptr<DownloadPriorityStepTask> task) {
        return true;
    });
    // 这个不需要 NotifyTaskQueueUpdated,因为肯定没任务可以执行了
}

void DownloadPriorityTaskQueue::ResumeTaskByKey(std::string key, TaskType type) {
    std::unique_lock<std::mutex> lock(queue_mutex_);
    if (kApiCalledVerbose)
        LOG_WARN("[DownloadPriorityTaskQueue::%s]", __func__);
    auto ret = ResumeTasksByCriteriaNonLock([&key, &type](std::shared_ptr<DownloadPriorityStepTask> task) {
        return task->GetCacheKey() == key && task->GetTaskType() == type;
    });
    if (ret.Total() > 0) {
        NotifyTaskQueueUpdated();
    }
}

void DownloadPriorityTaskQueue::ResumeAllTasksOfMainPriority(int main_priority) {
    std::unique_lock<std::mutex> lock(queue_mutex_);
    if (kApiCalledVerbose)
        LOG_WARN("[DownloadPriorityTaskQueue::%s]", __func__);
    paused_main_priority_set_.erase(main_priority);
    auto ret = ResumeTasksByCriteriaNonLock([&main_priority](std::shared_ptr<DownloadPriorityStepTask> task) {
        return task->multi_priority_.main_priority_ == main_priority;
    });
    if (ret.Total() > 0) {
        NotifyTaskQueueUpdated();
    }
}

void DownloadPriorityTaskQueue::ResumeAllTasksOfType(TaskType type) {
    std::unique_lock<std::mutex> lock(queue_mutex_);
    if (kApiCalledVerbose)
        LOG_WARN("[DownloadPriorityTaskQueue::%s]", __func__);
    paused_task_type_set_.erase(type);
    auto ret = ResumeTasksByCriteriaNonLock([&type](std::shared_ptr<DownloadPriorityStepTask> task) {
        return task->GetTaskType() == type;
    });
    if (ret.Total() > 0) {
        NotifyTaskQueueUpdated();
    }
}

void DownloadPriorityTaskQueue::ResumeAllTasks() {
    std::unique_lock<std::mutex> lock(queue_mutex_);
    if (kApiCalledVerbose)
        LOG_WARN("[DownloadPriorityTaskQueue::%s]", __func__);
    pause_all_tasks_ = false;
    auto ret = ResumeTasksByCriteriaNonLock([](std::shared_ptr<DownloadPriorityStepTask> task) {
        return true;
    });
    if (ret.Total() > 0) {
        NotifyTaskQueueUpdated();
    }
}

QueueResult DownloadPriorityTaskQueue::CancelTasksByCriteria(
    const std::function<bool(std::shared_ptr<DownloadPriorityStepTask>)>& criteria) {
    std::unique_lock<std::mutex> lock(queue_mutex_);
    QueueResult cancel_ret;

    // working queue里的任务等PushBack的时候处理
    for (auto& task : working_task_queue_) {
        if (criteria(task)) {
            if (kOperateTaskVerbose) {
                LOG_INFO("[DownloadPriorityTaskQueue::CancelTaskByKey] key:%s found, int working_task_queue_, mark it as cancel", task->GetCacheKey().c_str());
            }
            task->Cancel();
            cancel_ret.count_for_working_queue_++;
        }
    }

    // waiting queue里的任务直接删除
    auto iter = waiting_task_queue_.begin();
    while (iter != waiting_task_queue_.end()) {
        if (criteria(*iter)) {
            if (kOperateTaskVerbose) {
                LOG_INFO("[DownloadPriorityTaskQueue::CancelTaskByKey] key:%s found in waiting_task_queue_, delete it", (*iter)->GetCacheKey().c_str());
            }
            OnTaskExitWatingQueue(*iter);
            iter = waiting_task_queue_.erase(iter);
            cancel_ret.count_for_waiting_queue_++;
        } else {
            iter++;
        }
    }
    return cancel_ret;
}

QueueResult DownloadPriorityTaskQueue::PauseTasksByCriteriaNonLock(
    const std::function<bool(std::shared_ptr<DownloadPriorityStepTask>)>& criteria) {
    // 把working_queue和waiting_queue里的任务加到paused_queue里
    QueueResult pause_ret;

    // working queue里满足条件的任务 调用Pause，等PushBack的时候会自动加到 pause queue 里
    for (auto& task : working_task_queue_) {
        if (criteria(task)) {
            if (kOperateTaskVerbose) {
                LOG_INFO("[DownloadPriorityTaskQueue::PauseTasksByCriteriaNonLock] key:%s found, int working_task_queue_, to Pause",
                         task->GetCacheKey().c_str());
            }
            // pause后，task再次被PushBack回来的时候会被加入到paused_queue里
            task->Pause();
            pause_ret.count_for_working_queue_++;
        }
    }

    // waiting queue 里的满足条件的任务移到 pause queue
    for (auto& task : waiting_task_queue_) {
        if (criteria(task)) {
            if (kOperateTaskVerbose) {
                LOG_INFO("[DownloadPriorityTaskQueue::PauseTasksByCriteriaNonLock] key:%s found in waiting_task_queue_, to Pause",
                         task->GetCacheKey().c_str());
            }
            // 直接移入paused_queue
            task->Pause();
            pause_ret.count_for_waiting_queue_++;
        }
    }

    return pause_ret;
}

QueueResult DownloadPriorityTaskQueue::ResumeTasksByCriteriaNonLock(
    const std::function<bool(std::shared_ptr<DownloadPriorityStepTask>)>& criteria) {
    QueueResult resume_ret;

    // paused queue 里的满足条件的任务移到 waiting queue里
    for (auto& task : waiting_task_queue_) {
        if (criteria(task)) {
            if (kOperateTaskVerbose) {
                LOG_INFO("[DownloadPriorityTaskQueue::ResumeTasksByCriteriaNonLock] key:%s found, int working_task_queue_, to Resume", task->GetCacheKey().c_str());
            }
            task->Resume();
            resume_ret.count_for_waiting_queue_++;
        }
    }
    // working queue里满足条件的任务 调用Pause，等PushBack的时候会自动加到 pause queue 里
    for (auto& task : working_task_queue_) {
        if (criteria(task)) {
            if (kOperateTaskVerbose) {
                LOG_INFO("[DownloadPriorityTaskQueue::PauseTasksByCriteriaNonLock] key:%s found, int working_task_queue_, to Resume it",
                         task->GetCacheKey().c_str());
            }
            // pause后，task再次被PushBack回来的时候会被加入到paused_queue里
            task->Resume();
            resume_ret.count_for_working_queue_++;
        }
    }

    return resume_ret;
}

void DownloadPriorityTaskQueue::GetStatusForDebugInfo(size_t thread_worker_cnt,
                                                      size_t current_player_task_cnt, char* buf,
                                                      int buf_len) {
    std::unique_lock<std::mutex> lock(queue_mutex_);

    int offset = 0;
    offset += snprintf(buf, static_cast<size_t>(buf_len), "-------------------------------------- \n"
                       "Hodor: %s(flag:%d) | 播放器任务数: %zu\n [并发:%d][下载中:%d][排队数:%d]\n",
                       queue_paused_ ? "已暂停" : "下载中", queue_paused_flag_.load(),
                       current_player_task_cnt, (int) thread_worker_cnt,
                       (int) working_task_queue_.size(), (int) waiting_task_queue_.size());

    int to_show_detail_task_cnt = 5;
    for (const auto& task : working_task_queue_) {
        offset += snprintf(
                      buf + offset, buf_len - offset, "[下载中][priority: %d|%d][progress:%d%%][key:%s][last_error:%d]\n",
                      task->multi_priority_.main_priority_, task->multi_priority_.sub_priority_, (int)(
                          task->GetProgressPercent() * 100), task->GetCacheKey().c_str(), task->LastError());
        if (--to_show_detail_task_cnt <= 0) {
            break;
        }
    }

    for (const auto& task : waiting_task_queue_) {
        offset += snprintf(
                      buf + offset, buf_len - offset, "[排队中][priority: %d|%d][progress:%d%%][key:%s][last_error:%d][%s]\n",
                      task->multi_priority_.main_priority_, task->multi_priority_.sub_priority_,
                      (int)(task->GetProgressPercent() * 100), task->GetCacheKey().c_str(),
                      task->LastError(), task->GetStatusString());
        if (--to_show_detail_task_cnt <= 0) {
            break;
        }
    }

    // 这种方式暂时留着，后续再删
    //    ss_output << "Hodor: " << (queue_paused_flag_ ? "已暂停" : "下载中") << endl
    //              << "[并发:" << thread_worker_cnt << "]"
    //              << "[排队数:" << waiting_task_queue_.size() << "]"
    //              << "[下载中数:" << working_task_queue_.size() << "]"
    //              << endl;
    //
    //    int to_show_detail_task_cnt = 5;
    //    for (const auto& task : working_task_queue_) {
    //        ss_output << "[下载中][priority: " << task->multi_priority_.main_priority_ << "|" << task->multi_priority_.sub_priority_ << "]"
    //                  << "[progress:" << (int)(task->GetProgressPercent() * 100) << "%]"
    //                  << "[key:" << task->key_ << "]"
    //                  << "[last_error:" << task->LastError() << "]"
    //                  << endl;
    //        if (--to_show_detail_task_cnt <= 0) {
    //            break;
    //        }
    //    }
    //
    //    for (const auto& task : waiting_task_queue_) {
    //        ss_output << "[排队中][priority: " << task->multi_priority_.main_priority_ << "|" << task->multi_priority_.sub_priority_ << "]"
    //                  << "[progress: " << (int)(task->GetProgressPercent() * 100) << "%]"
    //                  << "[key:" << task->key_ << "]"
    //                  << "[last_error:" << task->LastError() << "]" << endl;
    //        if (--to_show_detail_task_cnt <= 0) {
    //            break;
    //        }
    //    }
}


void DownloadPriorityTaskQueue::UpdateTaskPriorityInWaitingQueue(
    const std::shared_ptr<DownloadPriorityStepTask>& new_task) {
    for (auto& task : waiting_task_queue_) {
        if (task->GetKeyTypeDigest() == new_task->GetKeyTypeDigest()) {
            task->CopyPriorityOf(*new_task);
            break;
        }
    }

    // 如果在waiting 队列则需要立马重新排列
    waiting_task_queue_.sort(PriorityTaskCompare());

}

void DownloadPriorityTaskQueue::OnTaskIntoWaitingQueue(const std::shared_ptr<DownloadPriorityStepTask>& task) {
    waiting_queue_task_keytype_set.insert(task->GetKeyTypeDigest());
}

void DownloadPriorityTaskQueue::OnTaskExitWatingQueue(const std::shared_ptr<DownloadPriorityStepTask>& task) {
    waiting_queue_task_keytype_set.erase(task->GetKeyTypeDigest());
}

bool DownloadPriorityTaskQueue::ShouldPause(std::shared_ptr<DownloadPriorityStepTask>& task) {
    bool ret = pause_all_tasks_ ||
               (paused_main_priority_set_.find(task->multi_priority_.main_priority_) !=
                paused_main_priority_set_.end()) ||
               (paused_task_type_set_.find(task->GetTaskType()) != paused_task_type_set_.end());
    return ret;
}

AwesomeCacheInterruptCB DownloadPriorityTaskQueue::GetInterruptCallback() {
    return interrupt_cb_;
}



HODOR_NAMESPACE_END
