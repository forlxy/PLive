//
// Created by MarshallShuai on 2019-08-02.
//

#include <cassert>
#include <ac_log.h>
#include <atomic>
#include "download_priority_step_task.h"

HODOR_NAMESPACE_START

static std::atomic<int> id_index(0);

DownloadPriorityStepTask::DownloadPriorityStepTask(const std::string& key, int main_priority,
                                                   int sub_priority)
    : key_(key), multi_priority_(main_priority, sub_priority), status_(Resumed) {
    id_ = id_index.fetch_add(1);
    assert(!key_.empty());
}

DownloadPriorityStepTask::DownloadPriorityStepTask(int main_priority, int sub_priority)
    : multi_priority_(main_priority, sub_priority), status_(Resumed) {
    id_ = id_index.fetch_add(1);
}

float DownloadPriorityStepTask::GetProgressPercent() {
    return 0.f;
}

AcResultType DownloadPriorityStepTask::StepExecute(int thread_work_id) {
    LOG_ERROR("[DownloadPriorityStepTask::StepExecute] empty implementation");
    return kResultOK;
}


void DownloadPriorityStepTask::Cancel() {
    std::lock_guard<std::mutex> lg(status_mutex_);
    if (status_ == Cancelled) {
        return;
    }
    Interrupt();
    status_ = Cancelled;
}

void DownloadPriorityStepTask::Pause() {
    std::lock_guard<std::mutex> lg(status_mutex_);
    if (status_ == Paused) {
        return;
    }
    Interrupt();
    status_ = Paused;
}

void DownloadPriorityStepTask::Resume() {
    std::lock_guard<std::mutex> lg(status_mutex_);
    if (status_ == Resumed) {
        return;
    }
    status_ = Resumed;
}

void DownloadPriorityStepTask::MarkComplete() {
    std::lock_guard<std::mutex> lg(status_mutex_);
    if (status_ == Completed) {
        return;
    }
    status_ = Completed;
}


void DownloadPriorityStepTask::CopyPriorityOf(const DownloadPriorityStepTask& other) {
    multi_priority_ = other.multi_priority_;
    id_ = other.id_;
}

const std::string& DownloadPriorityStepTask::GetCacheKey() {
    return key_;
}

DownloadPriorityStepTask::TaskStatus DownloadPriorityStepTask::GetTaskStatus() {
    std::lock_guard<std::mutex> lg(status_mutex_);
    return status_;
}

std::string DownloadPriorityStepTask::GetKeyTypeDigest() const {
    auto ret = key_ + "_" + std::to_string((int)GetTaskType());
    return ret;
}

const char* DownloadPriorityStepTask::GetStatusString() {
    switch (status_) {
        case Resumed:
            return "恢复";
        case Paused:
            return "暂停";
        case Cancelled:
            return "取消";
        case Completed:
            return "完成";
        default:
            return "未知？";
    }
}


HODOR_NAMESPACE_END
