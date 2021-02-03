//
// Created by 李金海 on 2019/8/20.
//

#include "hls_segment_download_priority_task.h"
#include "hodor_downloader.h"
#include "ac_log.h"

HODOR_NAMESPACE_START

HlsSegmentDownloadPriorityTask::HlsSegmentDownloadPriorityTask(
    const std::string& manifest_json,
    int perfer_bandwidth,
    int64_t preload_bytes,
    const DownloadOpts& opts,
    std::shared_ptr<AwesomeCacheCallback> callback,
    int main_priority, int sub_priority)
    : DownloadPriorityStepTask(manifest_json, main_priority, sub_priority) {

    std::shared_ptr<HlsPlaylistParser> playlist_parser_ = make_shared<HlsPlaylistParser>(manifest_json, perfer_bandwidth);
    segments_ = playlist_parser_->getSegmentList();
    iter_current_segment_ = segments_.begin();
    iter_segment_end_ = segments_.end();
    if (iter_current_segment_ != iter_segment_end_) {
        Segment seg = *iter_current_segment_;
        key_ = playlist_parser_->getSegmentCacheKey(iter_current_segment_->seq_no);
        spec_ = DataSpec().WithUri(iter_current_segment_->url)
                .WithKey(key_);
        if (seg.url_offset > 0) {
            spec_.WithPosition(seg.url_offset);
        }
        if (preload_bytes > 0) {
            spec_.WithLength(preload_bytes < seg.size ? preload_bytes : seg.size);
        } else if (seg.size > 0) {
            spec_.WithLength(seg.size);
        }
        curl_download_task_ = std::make_shared<ScopeDownloadPriorityStepTask>(spec_, opts, callback, main_priority, sub_priority);
    }
}

AcResultType HlsSegmentDownloadPriorityTask::StepExecute(int thread_work_id) {
    if (!curl_download_task_) {
        return DownloadPriorityStepTask::StepExecute(thread_work_id);
    }
    AcResultType last_error = curl_download_task_->StepExecute(thread_work_id);
    if (curl_download_task_->GetTaskStatus() == TaskStatus::Completed) {
        MarkComplete();
    }
    return last_error;
}

float HlsSegmentDownloadPriorityTask::GetProgressPercent() {
    if (!curl_download_task_) {
        return DownloadPriorityStepTask::GetProgressPercent();
    }
    return curl_download_task_->GetProgressPercent();
}

AcResultType HlsSegmentDownloadPriorityTask::LastError() {
    if (curl_download_task_) {
        return curl_download_task_->LastError();
    } else {
        return DownloadPriorityStepTask::LastError();
    }
}

void HlsSegmentDownloadPriorityTask::DeleteCache() {
    kuaishou::cache::HodorDownloader::GetInstance()->DeleteCacheByKey(spec_.key, Media);
}

HODOR_NAMESPACE_END
