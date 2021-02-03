#include "http_task_progress_helper.h"
#include "abr/abr_engine.h"
#include "dcc_algorithm_c.h"
#include "hodor_downloader/hodor_downloader.h"

using namespace kuaishou::cache;

HttpTaskProgressHelper::HttpTaskProgressHelper(int id, const DownloadOpts& options,
                                               ConnectionInfoV2* connection_info,
                                               AwesomeCacheRuntimeInfo* ac_rt_info):
    id_(id), options_(options), connection_info_(connection_info), ac_rt_info_(ac_rt_info) {
    speed_cal_ = std::make_shared<SpeedCalculator>();
}


void HttpTaskProgressHelper::OnStart() {
    start_ts_ms_ = kpbase::SystemUtil::GetCPUTime();
}

void HttpTaskProgressHelper::OnProgress(int64_t received_bytes) {
    connection_info_->downloaded_bytes_from_curl = received_bytes;
    if (ac_rt_info_) {
        speed_cal_->Update(received_bytes);
        ac_rt_info_->download_task.speed_cal_current_speed_kbps = speed_cal_->GetCurrentSpeedKbps();
    }
}

void HttpTaskProgressHelper::OnFinish() {
    end_ts_ms_ = kpbase::SystemUtil::GetCPUTime();
    connection_info_->transfer_consume_ms = static_cast<int32_t>(end_ts_ms_ - start_ts_ms_);

    if (ac_rt_info_) {
        ac_rt_info_->download_task.speed_cal_avg_speed_kbps =
            static_cast<int>(connection_info_->GetAvgDownloadSpeedkbps());
    }
    // 带宽控制
    if (connection_info_->downloaded_bytes_from_curl > 0) {
        DccAlgorithm_update_speed_mark(static_cast<int>(connection_info_->GetAvgDownloadSpeedkbps()));
    }
    // 短视频多码率
    if (options_.enable_vod_adaptive && connection_info_->downloaded_bytes_from_curl > 0) {
        // AbrUpdateDownloadInfo
        kuaishou::abr::DownloadSampleInfo last_sample_info;

        last_sample_info.begin_timestamp = start_ts_ms_;
        last_sample_info.end_timestamp = end_ts_ms_;
        last_sample_info.total_bytes = static_cast<uint64_t>(connection_info_->downloaded_bytes_from_curl);
        kuaishou::abr::AbrEngine::GetInstance()->UpdateDownloadInfo(last_sample_info);

        LOG_DEBUG("[%d][id:%d][HttpTaskProgressHelper] start_ts:%llu, end_ts:%llu, diff:%llu, bytes_transferred:%llu \n",
                  options_.context_id, id_,
                  start_ts_ms_, end_ts_ms_, end_ts_ms_ - start_ts_ms_, connection_info_->downloaded_bytes_from_curl);
        if (ac_rt_info_) {
            ac_rt_info_->vod_adaptive.real_time_throughput_kbps = kuaishou::abr::AbrEngine::GetInstance()->get_real_time_throughput();
            ac_rt_info_->vod_adaptive.consumed_download_time_ms += (end_ts_ms_ - start_ts_ms_);
            ac_rt_info_->vod_adaptive.actual_video_size_byte += connection_info_->downloaded_bytes_from_curl;
        }
    }

    // HodorDownloader的带宽更新
    HodorDownloader::GetInstance()->GetNetworkMonitor()->AddSpeedSample(connection_info_->downloaded_bytes_from_curl, connection_info_->transfer_consume_ms);
    HodorDownloader::GetInstance()->GetTrafficCoordinator()->OnNetWorkSpeedUpdated();
}
