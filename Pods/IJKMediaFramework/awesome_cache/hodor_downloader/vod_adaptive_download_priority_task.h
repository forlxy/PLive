//
// Created by yuxin liu on 2019-10-16.
//

#pragma once

#include "utils/macro_util.h"
#include "download_priority_step_task.h"
#include "scope_download_priority_step_task.h"

HODOR_NAMESPACE_START

struct VodAdaptiveDataSpec {
    std::string manifestJson;
    std::string headers;
    int32_t priority;
    int64_t preload_bytes;
    int64_t dur_ms;
};

struct VodAdaptiveInit {
    std::string rate_config;
    int32_t dev_res_width;
    int32_t dev_res_heigh;
    int32_t net_type;
    int32_t low_device;
    int32_t signal_strength;
    int32_t switch_code;
};

class VodAdaptiveDownloadPriorityTask : public DownloadPriorityStepTask {
  public:
    VodAdaptiveDownloadPriorityTask(const std::string& manifest_json,
                                    int64_t preload_bytes,
                                    int64_t dur_ms,
                                    VodAdaptiveInit vod_adaptive_init,
                                    DownloadOpts& opts,
                                    std::shared_ptr<AwesomeCacheCallback> = nullptr,
                                    int main_priority = Priority_HIGH,
                                    int sub_priority = 0);
    VodAdaptiveDownloadPriorityTask(): DownloadPriorityStepTask("", Priority_HIGH, 0) {}

    virtual AcResultType StepExecute(int thread_work_id) override;

    virtual float GetProgressPercent() override;

    virtual AcResultType LastError() override;

    void DeleteCache();

  private:
    int ParseVodAdaptiveManifest(const std::string& manifest_json,
                                 VodAdaptiveInit vod_adaptive_init,
                                 DownloadOpts& opts,
                                 int64_t preload_bytes,
                                 int64_t dur_ms);
    DataSpec spec_;
    std::shared_ptr<ScopeDownloadPriorityStepTask> curl_download_task_;
};

HODOR_NAMESPACE_END