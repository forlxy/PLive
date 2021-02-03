#include "abr_engine.h"

namespace kuaishou {
namespace abr {

AbrEngine* AbrEngine::GetInstance() {
    static AbrEngine* engine = new AbrEngine();
    return engine;
}

void AbrEngine::SetHistoryData(const std::string& history_data) {
    history_data_ = history_data;
}

std::string AbrEngine::GetHistoryData() {
    if (video_adaptation_algorithm_ != nullptr) {
        history_data_ = video_adaptation_algorithm_->GetHistoryData();
    }
    return history_data_;
}

uint32_t AbrEngine::get_manual_auto_state() {
    if (video_adaptation_algorithm_ != nullptr) {
        manual_auto_state_ = video_adaptation_algorithm_->GetManualAutoState();
    }
    return manual_auto_state_;
}


void AbrEngine::InitInternale(const RateAdaptConfig& rate_adapt_config, const RateAdaptConfigA1& rate_adapt_config_a1) {
    video_adaptation_algorithm_.reset(VideoAdaptationAlgorithmInterface::Create(rate_adapt_config, rate_adapt_config_a1));
    bandwidth_estimation_algorithm_.reset(BandwidthEstimationAlgorithmInterface::Create(rate_adapt_config, rate_adapt_config_a1));
    video_adaptation_algorithm_->SetHistoryData(history_data_);
    is_inited_ = true;
    last_request_time_ = kpbase::SystemUtil::GetCPUTime();
    idle_time_from_last_request_ = 0;
    manual_auto_state_ = 0;

    switch (rate_adapt_config.rate_addapt_type) {
        case VideoAdaptationAlgoType::kBandwidthBased:
            rate_adaption_algo_ = "BandwidthBased";
            break;
        default:
            rate_adaption_algo_ = "Unknow";
            break;
    }

    switch (rate_adapt_config.bandwidth_estimation_type) {
        case BandwidthEstimationAlgoType::kMovingAverage:
            bandwidth_estimation_algo_ = "MovingAverage";
            break;
        default:
            bandwidth_estimation_algo_ = "Unknow";
            break;
    }
}

void AbrEngine::Init() {
    std::lock_guard<std::mutex> lg(mutex_);
    if (is_inited_) {
        return;
    }
    //only used for test
    RateAdaptConfig rate_adapt_config;
    RateAdaptConfigA1 rate_adapt_config_a1;
    SetDefaultRateAdaptConfig(rate_adapt_config);
    SetDefaultRateAdaptConfigA1(rate_adapt_config_a1);

    InitInternale(rate_adapt_config, rate_adapt_config_a1);
}

bool AbrEngine::IsInit() {
    return is_inited_;
}

void AbrEngine::Init(const RateAdaptConfig& rate_adapt_config) {
    std::lock_guard<std::mutex> lg(mutex_);
    if (is_inited_) {
        return;
    }
    RateAdaptConfigA1 rate_adapt_config_a1;
    SetDefaultRateAdaptConfigA1(rate_adapt_config_a1);
    InitInternale(rate_adapt_config, rate_adapt_config_a1);
}

void AbrEngine::Init(const RateAdaptConfig& rate_adapt_config, const RateAdaptConfigA1& rate_adapt_config_a1) {
    std::lock_guard<std::mutex> lg(mutex_);
    if (is_inited_) {
        return;
    }

    InitInternale(rate_adapt_config, rate_adapt_config_a1);
}

void AbrEngine::UpdateDownloadInfo(DownloadSampleInfo& info) {
    std::lock_guard<std::mutex> lg(mutex_);
    if (!is_inited_) {
        return;
    }
    bandwidth_estimation_algorithm_->UpdateDownloadInfo(info);
#ifdef ABR_DEBUG_QOS
    qos_logger_.UpdateDownloadSample(info);
    qos_logger_.UpdateEstimateBandwidth(bandwidth_estimation_algorithm_->BandwidthEstimate());
    qos_logger_.Report();
#endif
}

std::pair<bool, uint32_t> AbrEngine::AdaptPendingNextProfileRepId(uint32_t duration_ms, AdaptionProfile profile) {
    std::lock_guard<std::mutex> lg(mutex_);
    if (!is_inited_) {
        return {false, 0};
    }
    uint64_t now_ms = kpbase::SystemUtil::GetCPUTime();
    if ((now_ms > last_request_time_) && (now_ms - last_request_time_) < 3600 * 1000) {
        idle_time_from_last_request_ = now_ms - last_request_time_;
    } else {
        idle_time_from_last_request_ = 0;
    }
    last_request_time_ = now_ms;
    uint32_t  rep_id = video_adaptation_algorithm_->AdaptPendingNextProfileRepId(bandwidth_estimation_algorithm_.get(),
                                                                                 duration_ms,
                                                                                 profile);
#ifdef ABR_DEBUG_QOS
    auto it = std::find_if(profiles.begin(), profiles.end(), [&rep_id](const VideoProfile & profile) {
        return profile.representation_id == rep_id;
    });
    if (it != profiles.end()) {
        qos_logger_.UpdateCurrentProfile(*it);
        qos_logger_.UpdateEstimateBandwidth(bandwidth_estimation_algorithm_->BandwidthEstimate());
        qos_logger_.Report();
    }
#endif
    return {true, rep_id};
}

void AbrEngine::UpdateBlockInfo(std::vector<uint64_t>& block_info) {
    std::lock_guard<std::mutex> lg(mutex_);
    if (is_inited_) {
        return;
    }
}

AbrEngine::AbrEngine()
    : is_inited_(false)
    , history_data_("") {}

int AbrEngine::SetDefaultRateAdaptConfig(RateAdaptConfig& rate_config) {
    rate_config.rate_addapt_type = kBandwidthBased;
    rate_config.bandwidth_estimation_type = kMovingAverage;
    rate_config.absolute_low_res_low_device = 0;
    rate_config.adapt_under_4G = 1;
    rate_config.adapt_under_wifi = 0;
    rate_config.adapt_under_other_net = 0;
    rate_config.absolute_low_rate_4G = 0;
    rate_config.absolute_low_rate_wifi = 0;
    rate_config.absolute_low_res_4G = 0;
    rate_config.absolute_low_res_wifi = 0;
    rate_config.short_keep_interval = 60000;
    rate_config.long_keep_interval = 600000;
    rate_config.bitrate_init_level = 2;
    rate_config.default_weight = 1.0;
    rate_config.block_affected_interval = 10000;
    rate_config.wifi_amend = 0.7;
    rate_config.fourG_amend = 0.3;
    rate_config.resolution_amend = 0.6;
    rate_config.device_width_threshold = 720;
    rate_config.device_hight_threshold = 960;
    rate_config.priority_policy = 1;
    return 0;
}

int AbrEngine::SetDefaultRateAdaptConfigA1(RateAdaptConfigA1& rate_config_a1) {
    rate_config_a1.bitrate_init_level = 12;
    rate_config_a1.short_keep_interval = 30000;
    rate_config_a1.long_keep_interval = 180000;
    rate_config_a1.max_resolution = 720 * 960;    //720p is defined as 720*960
    return 0;
}

void AbrEngine::UpdateConfig(RateAdaptConfig& rate_config) {
    std::lock_guard<std::mutex> lg(mutex_);
    if (!is_inited_) {
        return;
    }
    RateAdaptConfigA1 rate_config_a1;
    SetDefaultRateAdaptConfigA1(rate_config_a1);
    bandwidth_estimation_algorithm_->UpdateConfig(rate_config, rate_config_a1);
    video_adaptation_algorithm_->UpdateConfig(rate_config, rate_config_a1);
    return;
}

void AbrEngine::UpdateConfig(RateAdaptConfig& rate_config, RateAdaptConfigA1& rate_config_a1) {
    std::lock_guard<std::mutex> lg(mutex_);
    if (!is_inited_) {
        return;
    }
    bandwidth_estimation_algorithm_->UpdateConfig(rate_config, rate_config_a1);
    video_adaptation_algorithm_->UpdateConfig(rate_config, rate_config_a1);
    return;
}

uint32_t AbrEngine::get_real_time_throughput() {
    std::lock_guard<std::mutex> lg(mutex_);
    if (!is_inited_) {
        return 0;
    }
    return bandwidth_estimation_algorithm_->real_time_throughput();
}

uint32_t AbrEngine::get_short_term_throughput(uint32_t algorithm_mode) {
    std::lock_guard<std::mutex> lg(mutex_);
    if (!is_inited_) {
        return 0;
    }
    return bandwidth_estimation_algorithm_->short_bandwidth_kbps(algorithm_mode);
}

uint32_t AbrEngine::get_long_term_throughput(uint32_t algorithm_mode) {
    std::lock_guard<std::mutex> lg(mutex_);
    if (!is_inited_) {
        return 0;
    }
    return bandwidth_estimation_algorithm_->long_bandwidth_kbps(algorithm_mode);
}

uint64_t AbrEngine::get_idle_time_from_last_request() {
    std::lock_guard<std::mutex> lg(mutex_);
    if (!is_inited_) {
        return 0;
    }
    return idle_time_from_last_request_;
}

std::string AbrEngine::get_switch_reason() {
    std::lock_guard<std::mutex> lg(mutex_);
    if (!is_inited_) {
        return "";
    }
    return video_adaptation_algorithm_->GetSwitchReason();
}

const char* AbrEngine::get_detail_switch_reason() {
    std::lock_guard<std::mutex> lg(mutex_);
    if (!is_inited_) {
        return "";
    }
    return video_adaptation_algorithm_->GetDetailSwitchReason();
}


std::string AbrEngine::get_rate_adaption_algo() {
    std::lock_guard<std::mutex> lg(mutex_);
    if (!is_inited_) {
        return "";
    }
    return rate_adaption_algo_;
}

std::string AbrEngine::get_bandwidth_estimation_algo() {
    std::lock_guard<std::mutex> lg(mutex_);
    if (!is_inited_) {
        return "";
    }
    return bandwidth_estimation_algo_;
}

}
}
