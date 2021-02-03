//
// Created by yuxin liu on 2019-10-16.
//

#include "vod_adaptive_download_priority_task.h"
#include "hodor_downloader.h"
#include "ac_log.h"
#include "abr/abr_parse_manifest.h"

HODOR_NAMESPACE_START

#define UNKNOWN_DOWNLOAD_LEN 0

VodAdaptiveDownloadPriorityTask::VodAdaptiveDownloadPriorityTask(
    const std::string& manifest_json,
    int64_t preload_bytes,
    int64_t dur_ms,
    VodAdaptiveInit vod_adaptive_init,
    DownloadOpts& opts,
    std::shared_ptr<AwesomeCacheCallback> callback,
    int main_priority,
    int sub_priority)
    : DownloadPriorityStepTask(manifest_json, main_priority, sub_priority) {

    if (!ParseVodAdaptiveManifest(manifest_json, vod_adaptive_init, opts, preload_bytes, dur_ms)) {
        curl_download_task_ = std::make_shared<ScopeDownloadPriorityStepTask>(spec_, opts, callback, main_priority, sub_priority);
    }
}

int VodAdaptiveDownloadPriorityTask::ParseVodAdaptiveManifest(const std::string& manifest_json,
                                                              VodAdaptiveInit vod_adaptive_init,
                                                              DownloadOpts& opts,
                                                              int64_t preload_bytes,
                                                              int64_t dur_ms) {
    int ret = 0;

    shared_ptr<kuaishou::abr::AbrParseManifest> abrParser = shared_ptr<kuaishou::abr::AbrParseManifest>(new kuaishou::abr::AbrParseManifest());
    abrParser->Init(vod_adaptive_init.dev_res_width, vod_adaptive_init.dev_res_heigh,
                    vod_adaptive_init.net_type, vod_adaptive_init.low_device,
                    vod_adaptive_init.signal_strength, vod_adaptive_init.switch_code);
    if (!vod_adaptive_init.rate_config.empty()) {
        ret = abrParser->ParserRateConfig(vod_adaptive_init.rate_config);
        if (ret) {
            LOG_ERROR("[%s] parser rate config failed", __func__);
            return ret;
        }
    }

    ret = abrParser->ParserVodAdaptiveManifest(manifest_json);
    if (ret) {
        LOG_ERROR("[%s] parser manifest failed", __func__);
        return ret;
    }

    int index = abrParser->AbrEngienAdaptInit();
    key_ = abrParser->GetKey(index);
    spec_ = DataSpec().WithUri(abrParser->GetUrl(index)).WithKey(key_);
    std::string host = abrParser->GetHost(index);
    if (!host.empty()) {
        opts.headers = "Host: " + host + "\r\n" + opts.headers;
    }

    int download_len = abrParser->GetDownloadLen(index);
    if (download_len != UNKNOWN_DOWNLOAD_LEN) {
        spec_.WithLength(download_len);
    } else {
        LOG_ERROR("[%s] can't get download len from manifest", __func__);
        if (dur_ms > 0) {
            int avg_bitrate = abrParser->GetAvgBitrate(index);
            download_len = (int)(avg_bitrate * dur_ms / 8);
            spec_.WithLength(download_len);
        } else if (preload_bytes > 0) {
            spec_.WithLength(preload_bytes);
        }
    }

    return ret;
}

AcResultType VodAdaptiveDownloadPriorityTask::StepExecute(int thread_work_id) {
    if (!curl_download_task_) {
        return DownloadPriorityStepTask::StepExecute(thread_work_id);
    }
    AcResultType last_error = curl_download_task_->StepExecute(thread_work_id);
    if (curl_download_task_->GetTaskStatus() == TaskStatus::Completed) {
        MarkComplete();
    }
    return last_error;
}

float VodAdaptiveDownloadPriorityTask::GetProgressPercent() {
    if (!curl_download_task_) {
        return DownloadPriorityStepTask::GetProgressPercent();
    }
    return curl_download_task_->GetProgressPercent();
}

AcResultType VodAdaptiveDownloadPriorityTask::LastError() {
    if (curl_download_task_) {
        return curl_download_task_->LastError();
    } else {
        return DownloadPriorityStepTask::LastError();
    }
}

void VodAdaptiveDownloadPriorityTask::DeleteCache() {
    kuaishou::cache::HodorDownloader::GetInstance()->DeleteCacheByKey(spec_.key, TaskType::Media);
}

HODOR_NAMESPACE_END