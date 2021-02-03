#pragma once

#include <string>
#include <list>

#include "utils/macro_util.h"
#include "download_priority_step_task.h"
#include "scope_download_priority_step_task.h"
#include "media_parser/hls_playlist_parser.h"

HODOR_NAMESPACE_START

class HlsSegmentDownloadPriorityTask : public DownloadPriorityStepTask {
  public:
    HlsSegmentDownloadPriorityTask(const std::string& manifest_json,
                                   int perfer_bandwidth,
                                   int64_t preload_bytes,
                                   const DownloadOpts& opts,
                                   std::shared_ptr<AwesomeCacheCallback> callback = nullptr,
                                   int main_priority = Priority_HIGH,
                                   int sub_priority = 0);
    HlsSegmentDownloadPriorityTask(): DownloadPriorityStepTask("", Priority_HIGH, 0) {}

    virtual AcResultType StepExecute(int thread_work_id) override;

    virtual float GetProgressPercent() override;

    virtual AcResultType LastError() override;

    void DeleteCache();

  private:
    DataSpec spec_;
    std::list<Segment> segments_;
    list<Segment>::iterator iter_current_segment_;
    list<Segment>::iterator iter_segment_end_;
    std::shared_ptr<ScopeDownloadPriorityStepTask> curl_download_task_;
};

HODOR_NAMESPACE_END

