//
// Created by MarshallShuai on 2019-09-25.
//

#pragma once

#include "utils/macro_util.h"
#include "download_priority_step_task.h"
#include "scope_download_priority_step_task.h"
#include "v2/cache/cache_content_v2_non_scope.h"

HODOR_NAMESPACE_START

/**
 * 主要给静态资源下载场景使用，如果没有指定目标路径，则下载到默认的cache_v2/resource目录，如果调用方指定了，则下载到指定目录
 */

class SingleFileDownloadPriorityStepTask : public DownloadPriorityStepTask, ScopeTaskListener {
  public:
    SingleFileDownloadPriorityStepTask(const DataSpec& spec, const DownloadOpts& opts,
                                       std::shared_ptr<AwesomeCacheCallback> callback = nullptr,
                                       int main_priority = Priority_MEDIUM, int sub_priority = 0);
    ~SingleFileDownloadPriorityStepTask();

    virtual TaskType GetTaskType() const override {
        return Resource;
    }

    virtual AcResultType StepExecute(int thread_work_id) override;

    virtual void Interrupt() override ;

    virtual float GetProgressPercent() override;

    virtual AcResultType LastError() override;

#pragma mark ScopeTaskListener
    virtual void OnConnectionInfoParsed(const ConnectionInfoV2& info) override;
    virtual void OnReceiveData(uint8_t* data_buf, int64_t data_len) override;
    virtual void OnDownloadComplete(int32_t error, int32_t stop_reason) override;

    /**
     * 设置每次分片下载会下载的字节数，默认为1MB
     */
    void SetStepDownloadBytes(int64_t bytes);
  private:
    /**
     * 下载一个scope，如果下载完后，总任务完成了，则会标记complete_=true
     * @return 错误码
     */
    AcResultType DownloadScope(int64_t start_pos, int64_t expect_download_bytes_, int thread_work_id);

    void AppendDataToFile();

    /**
     * 带时间阈值过滤的通知进度
     */
    void ThrottleNotifyProgressToCallbacks(int64_t total_progress_bytes);

  private:

    std::shared_ptr<CacheContentV2NonScope> cache_content_;
    kpbase::File cache_content_file_;
    DataSpec spec_;
    DownloadOpts download_opts_;


    /**
     * 用ScopeDataSource来完成download scope的功能
     */
    std::shared_ptr<ScopeTask> scope_download_task_;
    AcResultType last_error_;
    bool is_abort_by_interrupt_;

    std::shared_ptr<AcCallbackInfo> ac_cb_info_;
    std::shared_ptr<AwesomeCacheCallback> ac_callback_;
    AwesomeCacheRuntimeInfo* ac_rt_info_{};
    int64_t last_notify_progress_ts_ms_;
    int progress_cb_interval_ms_;


    int64_t step_download_bytes_ = 1 * MB;
    uint8_t* data_buf_{};
    int64_t recv_data_len_{};
    // 每次下载scope对应的开始位置和期待下载的长度
    bool content_length_unknown_on_open_;
    int64_t start_download_position_{};
    int64_t expect_download_bytes_{};

    int64_t total_cached_bytes_so_far_{};
};

HODOR_NAMESPACE_END
