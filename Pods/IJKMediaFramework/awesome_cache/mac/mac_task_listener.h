//
// Created by MarshallShuai on 2019/7/12.
//
#pragma once

#include "ac_log.h"
#include "task_listener.h"
#include "event.h"

namespace kuaishou {
namespace catchya {

struct MacTaskListener : public cache::TaskListener {
    MacTaskListener() {
        LOG_VERBOSE(__PRETTY_FUNCTION__);
    }
    ~MacTaskListener() override {
        LOG_VERBOSE(__PRETTY_FUNCTION__);
    }

    void OnTaskSuccessful() override;

    void OnTaskCancelled() override;

    void OnTaskFailed(int32_t fail_reason) override;

    void onTaskProgress(int64_t position, int64_t totalsize) override;

    void onTaskStopped(int64_t download_bytes, int64_t transfer_consume_ms) override;

    void onTaskStopped(int64_t download_bytes, int64_t transfer_consume_ms, const char* sign) override;

    void onTaskStarted(int64_t start_pos, int64_t cached_bytes, int64_t totalsize) override;

    void WaitFinish();



    bool is_successful_ = false;
    bool is_cancel_ = false;
    bool is_failed_ = false;
    int64_t position_ = 0;
    int32_t transfer_consume_ms_ = 0;
    int64_t total_download_bytes_ = 0;
    int32_t on_stop_cnt_ = 0;
    std::string sign_;
    int64_t start_pos_ = 0;
    int64_t cached_bytes_on_task_start_ = 0;
    int64_t totalsize_on_task_start = 0;


    kpbase::Event wait_finish_;

};


}
};