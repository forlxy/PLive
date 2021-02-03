#include <cassert>
#include <sstream>
#include "video_adaptation_v2.h"
#include "ac_log.h"

namespace kuaishou {
namespace abr {

template <typename T>
std::string to_string_with_precision(const T a_value, const int n = 6) {
    std::ostringstream out;
    out.precision(n);
    out << std::fixed << a_value;
    return out.str();
}

VideoAdaptationV2::VideoAdaptationV2(const RateAdaptConfig& rate_adapt_config, const RateAdaptConfigA1& rate_adapt_config_a1)
    : rate_adapt_config_(rate_adapt_config)
    , rate_adapt_config_a1_(rate_adapt_config_a1) {
    memset(&his_state_, 0, sizeof(struct HisState));
    memset(&wifi_his_state_, 0, sizeof(struct HisState));
    wifi_his_state_.net_type = static_cast<uint32_t>(NetworkType::WIFI);
    memset(&foureG_his_state_, 0, sizeof(struct HisState));
    foureG_his_state_.net_type = static_cast<uint32_t>(NetworkType::FOUR_G);
    memset(&threeG_his_state_, 0, sizeof(struct HisState));
    threeG_his_state_.net_type = static_cast<uint32_t>(NetworkType::THREE_G);
    memset(&other_his_state_, 0, sizeof(struct HisState));
    other_his_state_.net_type = static_cast<uint32_t>(NetworkType::UNKNOW);
    is_init_ = false;
    is_avg_bandwidth_zero_ = false;
    switch_reason_ = "";
    bandwidth_computation_process_ = "";
    current_condition_ = "";
    avg_bandwidth_ = -1;
    avg_bandwidth_computer_type = UNINIT_DEFAULT_HEIGHTESt_RESOLUTION;
    memset(detail_switch_reason_, 0, DETAIL_SWITCH_REASON_MAX_LEN + 1);
    manual_auto_state_ = 0;
}

uint32_t VideoAdaptationV2::AdaptPendingNextProfileRepId(BandwidthEstimationAlgorithmInterface* bandwidth_estimate,
                                                         uint32_t duration_ms,
                                                         AdaptionProfile profile) {
    assert(!profile.video_profiles.empty());
    assert(bandwidth_estimate != nullptr);
    manual_auto_state_ = 0;
    if (profile.switch_code != 0) {
        manual_auto_state_ = 2;
        for (auto it = profile.video_profiles.begin(); it != profile.video_profiles.end(); it++) {
            if (it->id == profile.switch_code) {
                manual_auto_state_ = 1;
                return it->representation_id;
            }
        }
    }
    bandwidth_estimate_ = bandwidth_estimate;
    target_video_it_ = profile.video_profiles.begin();
    is_avg_bandwidth_zero_ = false;

    current_condition_ = "";
    if (profile.device_resolution.height <= rate_adapt_config_.device_hight_threshold ||
        profile.device_resolution.width <= rate_adapt_config_.device_width_threshold) {
        current_condition_ = "LowDeviceResolution";
    } else {
        current_condition_ = "HighDeviceResolution";
    }
    if (profile.net_type == NetworkType::WIFI) {
        current_condition_ += "_WIFI";
    } else if (profile.net_type == NetworkType::FOUR_G) {
        current_condition_ += "_4G";
    } else {
        current_condition_ += "_OtherNetType";
    }

    avg_bandwidth_ = -1;
    avg_bandwidth_computer_type = UNINIT_DEFAULT_HEIGHTESt_RESOLUTION;
    /*absolute_low_res_low_device == ABSOLUTE_LOW_DEVICE_LOW_RES: absolutely select the lowest resolution; while under same resolution, select the lowest bitate;
     absolute_low_res_low_device == NET_AWARE_LOW_DEVICE_LOW_RES: select the representation with largest bitrate which is samller than bandwidth under the conditon that the resolution is smaller than the given thresholds;
     */
    if (rate_adapt_config_.absolute_low_res_low_device == ABSOLUTE_LOW_DEVICE_LOW_RES && profile.low_device != 0) {
        is_init_ = true;
        bandwidth_computation_process_ = "Low device and absolutely select lowest resolution";
        avg_bandwidth_computer_type = LOWDEVICE;
        return AbsoluteLowRes(profile);
    } else if (rate_adapt_config_.absolute_low_res_low_device == NET_AWARE_LOW_DEVICE_LOW_RES && profile.low_device != 0) {
        uint32_t avg_bandwidth = BandwidthAmend(profile);
        avg_bandwidth_ = avg_bandwidth;
        if (profile.algorithm_mode == 0) {
            std::sort(profile.video_profiles.begin(), profile.video_profiles.end(), [](const VideoProfile & a, const VideoProfile & b) {
                if (a.video_resolution.width * a.video_resolution.height != b.video_resolution.width * b.video_resolution.height) {
                    return a.video_resolution.width * a.video_resolution.height < b.video_resolution.width * b.video_resolution.height;
                }
                return a.max_bitrate_kbps < b.max_bitrate_kbps;
            });
            auto it = std::upper_bound(profile.video_profiles.begin(), profile.video_profiles.end(), rate_adapt_config_.device_hight_threshold * rate_adapt_config_.device_width_threshold, [this](uint32_t res, const VideoProfile & profile) -> bool {
                return res < profile.video_resolution.width* profile.video_resolution.height;
            });
            if (it != profile.video_profiles.begin()) {
                --it;
            }
            target_video_it_ = it;
            for (auto temp_it = it; temp_it != profile.video_profiles.begin(); temp_it--) {
                if (temp_it->avg_bitrate_kbps <= avg_bandwidth) {
                    target_video_it_ = temp_it;
                    break;
                }
            }
            return target_video_it_->representation_id;
        } else {  //for a1
            std::sort(profile.video_profiles.begin(), profile.video_profiles.end(), [](const VideoProfile & a, const VideoProfile & b) {
                if (a.video_resolution.width * a.video_resolution.height != b.video_resolution.width * b.video_resolution.height) {
                    return a.video_resolution.width * a.video_resolution.height < b.video_resolution.width * b.video_resolution.height;
                }
                return a.max_bitrate_kbps < b.max_bitrate_kbps;
            });
            auto it = std::upper_bound(profile.video_profiles.begin(), profile.video_profiles.end(), std::min(static_cast<uint32_t>(576 * 1024), rate_adapt_config_a1_.max_resolution), [this](uint32_t res, const VideoProfile & profile) -> bool {
                return res < profile.video_resolution.width* profile.video_resolution.height;
            });
            if (it != profile.video_profiles.begin()) {
                --it;
            }
            target_video_it_ = it;
            for (auto temp_it = it; temp_it != profile.video_profiles.begin(); temp_it--) {
                if (temp_it->avg_bitrate_kbps <= avg_bandwidth) {
                    target_video_it_ = temp_it;
                    break;
                }
            }
            return target_video_it_->representation_id;
        }
    }

    if (profile.algorithm_mode != 0) {
        return AdaptA1(profile);
    }
    switch (profile.net_type) {
        case NetworkType::WIFI:
            return AdaptWIFI(profile);
        case NetworkType::FOUR_G:
            return AdaptFourG(profile);
        default:
            return AdaptOtherNet(profile);
    }
}

uint32_t VideoAdaptationV2::AdaptInit(AdaptionProfile& profile) {
    std::sort(profile.video_profiles.begin(), profile.video_profiles.end(), [](const VideoProfile & a, const VideoProfile & b) {
        if (a.video_resolution.width * a.video_resolution.height != b.video_resolution.width * b.video_resolution.height) {
            return a.video_resolution.width * a.video_resolution.height < b.video_resolution.width * b.video_resolution.height;
        }
        return a.avg_bitrate_kbps < b.avg_bitrate_kbps;
    });

    if (rate_adapt_config_.bitrate_init_level <= 0) {
        switch (profile.net_type) {
            case NetworkType::WIFI:
                his_state_ = wifi_his_state_;
                break;
            case NetworkType::FOUR_G:
                his_state_ = foureG_his_state_;
                break;
            case NetworkType::THREE_G:
                his_state_ = threeG_his_state_;
            default:
                his_state_ = other_his_state_;
                break;
        }
        if (his_state_.avg_video_bitrate == 0 || his_state_.max_video_bitrate == 0 || his_state_.video_width == 0 || his_state_.video_height == 0) {
            avg_bandwidth_computer_type = UNINIT_NO_HISTORY_LOW_RESOLUTION;
            UpdateHisState(profile.net_type, profile.video_profiles.begin());
            LOG_INFO("Initialization from storage with storage is empty.");
            return profile.video_profiles.at(0).representation_id;
        }
        auto it = std::upper_bound(profile.video_profiles.begin(), profile.video_profiles.end(), his_state_.video_width * his_state_.video_height, [this](uint32_t res, const VideoProfile & profile) -> bool {
            return res < profile.video_resolution.width* profile.video_resolution.height;
        });
        if (it != profile.video_profiles.begin()) {
            --it;
        }
        avg_bandwidth_computer_type = UNINIT_DEPEND_ON_HISTORY_LOW_RESOLUTION;
        LOG_INFO("Initialization from storage with storage is available.");
        UpdateHisState(profile.net_type, it);
        return it->representation_id;
    } else if (rate_adapt_config_.bitrate_init_level >= profile.video_profiles.size()) {
        // highest res
        avg_bandwidth_computer_type = UNINIT_DEFAULT_HEIGHTESt_RESOLUTION;
        UpdateHisState(profile.net_type, profile.video_profiles.end() - 1);
        return profile.video_profiles.at(profile.video_profiles.size() - 1).representation_id;
    } else {
        avg_bandwidth_computer_type = UNINIT_SPECIFIED_RESOLUTION;
        UpdateHisState(profile.net_type, profile.video_profiles.begin() + rate_adapt_config_.bitrate_init_level - 1);
        return profile.video_profiles.at(rate_adapt_config_.bitrate_init_level - 1).representation_id;
    }

    return 0;
}

uint32_t VideoAdaptationV2::AdaptInitA1(AdaptionProfile& profile) {
    std::sort(profile.video_profiles.begin(), profile.video_profiles.end(), [](const VideoProfile & a, const VideoProfile & b) {
        if (a.video_resolution.width * a.video_resolution.height != b.video_resolution.width * b.video_resolution.height) {
            return a.video_resolution.width * a.video_resolution.height < b.video_resolution.width * b.video_resolution.height;
        }
        return a.avg_bitrate_kbps < b.avg_bitrate_kbps;
    });
    target_video_it_ = profile.video_profiles.begin();
    if (rate_adapt_config_a1_.bitrate_init_level >= 10) {
        switch (profile.net_type) {
            case NetworkType::WIFI:
                his_state_ = wifi_his_state_;
                break;
            case NetworkType::FOUR_G:
                his_state_ = foureG_his_state_;
                break;
            case NetworkType::THREE_G:
                his_state_ = threeG_his_state_;
            default:
                his_state_ = other_his_state_;
                break;
        }
        if (his_state_.avg_video_bitrate == 0 || his_state_.max_video_bitrate == 0 || his_state_.video_width == 0 || his_state_.video_height == 0) {
            avg_bandwidth_computer_type = UNINIT_NO_HISTORY_LOW_RESOLUTION;
            LOG_INFO("Initialization from storage with storage is empty.");
            int index = rate_adapt_config_a1_.bitrate_init_level % 10;
            if (index >= profile.video_profiles.size() - 1) {
                for (auto temp_it = profile.video_profiles.end() - 1; temp_it != profile.video_profiles.begin(); temp_it--) {
                    if (temp_it->video_resolution.width * temp_it->video_resolution.height <= rate_adapt_config_a1_.max_resolution) {
                        target_video_it_ = temp_it;
                        break;
                    }
                }
                UpdateHisState(profile.net_type, target_video_it_);
                return target_video_it_->representation_id;
            } else {
                for (auto temp_it = profile.video_profiles.begin() + index; temp_it != profile.video_profiles.begin(); temp_it--) {
                    if (temp_it->video_resolution.width * temp_it->video_resolution.height <= rate_adapt_config_a1_.max_resolution) {
                        target_video_it_ = temp_it;
                        break;
                    }
                }
                UpdateHisState(profile.net_type, target_video_it_);
                return target_video_it_->representation_id;
            }
        } else {
            auto it = std::upper_bound(profile.video_profiles.begin(), profile.video_profiles.end(), his_state_.video_width * his_state_.video_height, [this](uint32_t res, const VideoProfile & profile) -> bool {
                return res < profile.video_resolution.width* profile.video_resolution.height;
            });
            if (it != profile.video_profiles.begin()) {
                --it;
            }
            for (auto temp_it = it; temp_it != profile.video_profiles.begin(); temp_it--) {
                if (temp_it->video_resolution.width * temp_it->video_resolution.height <= rate_adapt_config_a1_.max_resolution) {
                    target_video_it_ = temp_it;
                    break;
                }
            }
            avg_bandwidth_computer_type = UNINIT_DEPEND_ON_HISTORY_LOW_RESOLUTION;
            LOG_INFO("Initialization from storage with storage is available.");
            UpdateHisState(profile.net_type, target_video_it_);
            return target_video_it_->representation_id;
        }
    } else if (rate_adapt_config_a1_.bitrate_init_level >= (profile.video_profiles.size() - 1)) {
        avg_bandwidth_computer_type = UNINIT_DEFAULT_HEIGHTESt_RESOLUTION;
        for (auto temp_it = profile.video_profiles.end() - 1; temp_it != profile.video_profiles.begin(); temp_it--) {
            if (temp_it->video_resolution.width * temp_it->video_resolution.height <= rate_adapt_config_a1_.max_resolution) {
                target_video_it_ = temp_it;
                break;
            }
        }
        UpdateHisState(profile.net_type, target_video_it_);
        return target_video_it_->representation_id;
    } else {
        avg_bandwidth_computer_type = UNINIT_SPECIFIED_RESOLUTION;
        for (auto temp_it = profile.video_profiles.begin() + rate_adapt_config_a1_.bitrate_init_level; temp_it != profile.video_profiles.begin(); temp_it--) {
            if (temp_it->video_resolution.width * temp_it->video_resolution.height <= rate_adapt_config_a1_.max_resolution) {
                target_video_it_ = temp_it;
                break;
            }
        }
        UpdateHisState(profile.net_type, target_video_it_);
        return target_video_it_->representation_id;
    }
}

uint32_t VideoAdaptationV2::AdaptA1(AdaptionProfile& profile) {
    if (!is_init_) {
        is_init_ = true;
        bandwidth_computation_process_ = "Bitrate initialization";
        return AdaptInitA1(profile);
    }

    uint32_t avg_bandwidth = BandwidthAmend(profile);
    avg_bandwidth_ = avg_bandwidth;
    if (avg_bandwidth == 0) {
        is_avg_bandwidth_zero_ = true;
        return AdaptInitA1(profile);
    } else {
        std::sort(profile.video_profiles.begin(), profile.video_profiles.end(), [](const VideoProfile & a, const VideoProfile & b) {
            if (a.equivalent_bitrate_kbps != b.equivalent_bitrate_kbps) {
                return a.equivalent_bitrate_kbps < b.equivalent_bitrate_kbps;
            }
            return a.max_bitrate_kbps < b.max_bitrate_kbps;
        });
        auto it = std::upper_bound(profile.video_profiles.begin(), profile.video_profiles.end(), avg_bandwidth, [this](uint32_t bw, const VideoProfile & profile) -> bool {
            return bw < profile.equivalent_bitrate_kbps;
        });
        if (it != profile.video_profiles.begin()) {
            --it;
        }
        target_video_it_ = it;
        avg_bandwidth_computer_type = ADAP_DYNAMIC_RESOLUTION_BITRATE_QUALITY;
        target_video_it_ = ResolutionPriority(profile, it);
        target_video_it_ = BitratePriority(profile, it, target_video_it_->video_resolution);
        target_video_it_ = QualityPriority(profile, it, target_video_it_->avg_bitrate_kbps, target_video_it_->video_resolution);
    }
    auto target_video_it = profile.video_profiles.begin();
    for (auto temp_it = target_video_it_; temp_it != profile.video_profiles.begin(); temp_it--) {
        if (temp_it->video_resolution.width * temp_it->video_resolution.height <= rate_adapt_config_a1_.max_resolution) {
            target_video_it = temp_it;
            break;
        }
    }
    target_video_it_ = target_video_it;
    UpdateHisState(profile.net_type, target_video_it_);
    return target_video_it_->representation_id;
}

uint32_t VideoAdaptationV2::AdaptFourG(AdaptionProfile& profile) {
    if (rate_adapt_config_.absolute_low_rate_4G) {
        bandwidth_computation_process_ = "Absolutely select lowest rate by config";
        avg_bandwidth_computer_type = ADAP_FOURG_LOWEST_RATE;
        return AbsoluteLowRate(profile);
    }
    if (rate_adapt_config_.absolute_low_res_4G) {
        bandwidth_computation_process_ = "Absolutely select lowest resolution by config";
        avg_bandwidth_computer_type = ADAP_FOURG_LOWEST_RESOLUTION;
        return AbsoluteLowRes(profile);
    }

    if (!(rate_adapt_config_.adapt_under_4G)) {
        //highest res
        bandwidth_computation_process_ = "Absolutely select highest resolution by config";
        avg_bandwidth_computer_type = ADAP_FOURG_HEIGHTEST_RESOLUTION;
        return AbsoluteHighRes(profile);
    } else {
        if (!is_init_) {
            is_init_ = true;
            bandwidth_computation_process_ = "Bitrate initialization";
            return AdaptInit(profile);
        }
        return DynamicAdapt(profile);
    }
}

uint32_t VideoAdaptationV2::AdaptWIFI(AdaptionProfile& profile) {
    if (rate_adapt_config_.absolute_low_rate_wifi) {
        bandwidth_computation_process_ = "Absolutely select lowest rate by config";
        avg_bandwidth_computer_type = ADAP_WIFI_LOWEST_RATE;
        return AbsoluteLowRate(profile);
    }
    if (rate_adapt_config_.absolute_low_res_wifi) {
        bandwidth_computation_process_ = "Absolutely select lowest resolution by config";
        avg_bandwidth_computer_type = ADAP_WIFI_LOWEST_RESOLUTION;
        return AbsoluteLowRes(profile);
    }

    if (!(rate_adapt_config_.adapt_under_wifi)) {
        //highest res
        bandwidth_computation_process_ = "Absolutely select highest resolution by config";
        avg_bandwidth_computer_type = ADAP_WIFI_HEIGHTEST_RESOLUTION;
        return AbsoluteMaxbitrate(profile);
    } else {
        if (!is_init_) {
            is_init_ = true;
            bandwidth_computation_process_ = "Bitrate initialization";
            return AdaptInit(profile);
        }
        return DynamicAdapt(profile);
    }
}

uint32_t VideoAdaptationV2::AdaptOtherNet(AdaptionProfile& profile) {
    if (rate_adapt_config_.adapt_under_other_net) {
        return AdaptFourG(profile);
    } else {
        avg_bandwidth_computer_type = ADAP_OTHER_LOWEST_RESOLUTION;
        return AbsoluteLowRes(profile);
    }
}

uint32_t VideoAdaptationV2::AbsoluteLowRate(AdaptionProfile& profile) {
    std::sort(profile.video_profiles.begin(), profile.video_profiles.end(), [](const VideoProfile & a, const VideoProfile & b) {
        if (a.avg_bitrate_kbps != b.avg_bitrate_kbps) {
            return a.avg_bitrate_kbps < b.avg_bitrate_kbps;
        }
        return a.max_bitrate_kbps < b.max_bitrate_kbps;
    });
    UpdateHisState(profile.net_type, profile.video_profiles.begin());
    return profile.video_profiles.at(0).representation_id;
}

uint32_t VideoAdaptationV2::AbsoluteLowRes(AdaptionProfile& profile) {
    std::sort(profile.video_profiles.begin(), profile.video_profiles.end(), [](const VideoProfile & a, const VideoProfile & b) {
        if (a.video_resolution.width * a.video_resolution.height != b.video_resolution.width * b.video_resolution.height) {
            return a.video_resolution.width * a.video_resolution.height < b.video_resolution.width * b.video_resolution.height;
        }
        return a.avg_bitrate_kbps < b.avg_bitrate_kbps;
    });
    UpdateHisState(profile.net_type, profile.video_profiles.begin());
    return profile.video_profiles.at(0).representation_id;
}

uint32_t VideoAdaptationV2::AbsoluteHighRes(AdaptionProfile& profile) {
    std::sort(profile.video_profiles.begin(), profile.video_profiles.end(), [](const VideoProfile & a, const VideoProfile & b) {
        if (a.video_resolution.width * a.video_resolution.height != b.video_resolution.width * b.video_resolution.height) {
            return a.video_resolution.width * a.video_resolution.height < b.video_resolution.width * b.video_resolution.height;
        }
        return a.avg_bitrate_kbps < b.avg_bitrate_kbps;
    });
    UpdateHisState(profile.net_type, profile.video_profiles.end() - 1);
    return profile.video_profiles.at(profile.video_profiles.size() - 1).representation_id;
}

uint32_t VideoAdaptationV2::AbsoluteMaxbitrate(AdaptionProfile& profile) {
    std::sort(profile.video_profiles.begin(), profile.video_profiles.end(), [](const VideoProfile & a, const VideoProfile & b) {
        if (a.max_bitrate_kbps != b.max_bitrate_kbps) {
            return a.max_bitrate_kbps < b.max_bitrate_kbps;
        }
        return a.video_resolution.width * a.video_resolution.height < b.video_resolution.width * b.video_resolution.height;
    });
    UpdateHisState(profile.net_type, profile.video_profiles.end() - 1);
    return profile.video_profiles.at(profile.video_profiles.size() - 1).representation_id;
}

uint32_t VideoAdaptationV2::DynamicAdapt(AdaptionProfile& profile) {
    uint32_t avg_bandwidth = BandwidthAmend(profile);
    avg_bandwidth_ = avg_bandwidth;
    if (avg_bandwidth == 0) {
        is_avg_bandwidth_zero_ = true;
        return AdaptInit(profile);
    } else {
        std::sort(profile.video_profiles.begin(), profile.video_profiles.end(), [](const VideoProfile & a, const VideoProfile & b) {
            if (a.avg_bitrate_kbps != b.avg_bitrate_kbps) {
                return a.avg_bitrate_kbps < b.avg_bitrate_kbps;
            }
            return a.max_bitrate_kbps < b.max_bitrate_kbps;
        });
        auto it = std::upper_bound(profile.video_profiles.begin(), profile.video_profiles.end(), avg_bandwidth, [this](uint32_t bw, const VideoProfile & profile) -> bool {
            return bw < profile.avg_bitrate_kbps;
        });
        if (it != profile.video_profiles.begin()) {
            --it;
        }
        target_video_it_ = it;

        switch (rate_adapt_config_.priority_policy) {
            case 1:  //resolution->bitrate->quality
                avg_bandwidth_computer_type = ADAP_DYNAMIC_RESOLUTION_BITRATE_QUALITY;
                target_video_it_ = ResolutionPriority(profile, it);
                target_video_it_ = BitratePriority(profile, it, target_video_it_->video_resolution);
                target_video_it_ = QualityPriority(profile, it, target_video_it_->avg_bitrate_kbps, target_video_it_->video_resolution);
                break;
            case 2:  //resolution->quality->bitrate
                avg_bandwidth_computer_type = ADAP_DYNAMIC_RESOLUTION_QUALITY_BITRATE;
                target_video_it_ = ResolutionPriority(profile, it);
                target_video_it_ = QualityPriority(profile, it, target_video_it_->video_resolution);
                target_video_it_ = BitratePriority(profile, it, target_video_it_->video_resolution, target_video_it_->quality);
                break;
            case 3:  //bitrate->resolution->quality
                avg_bandwidth_computer_type = ADAP_DYNAMIC_BITRATE_RESOLUTION_QUALITY;
                target_video_it_ = BitratePriority(profile, it);
                target_video_it_ = ResolutionPriority(profile, it, target_video_it_->avg_bitrate_kbps);
                target_video_it_ = QualityPriority(profile, it, target_video_it_->avg_bitrate_kbps, target_video_it_->video_resolution);
                break;
            case 4:  //bitrate->quality->resolution
                avg_bandwidth_computer_type = ADAP_DYNAMIC_BITRATE_QUALITY_RESOLUTION;
                target_video_it_ = BitratePriority(profile, it);
                target_video_it_ = QualityPriority(profile, it, target_video_it_->avg_bitrate_kbps);
                target_video_it_ = ResolutionPriority(profile, it, target_video_it_->avg_bitrate_kbps, target_video_it_->quality);
                break;
            case 5:  //quality->resolution->bitrate
                avg_bandwidth_computer_type = ADAP_DYNAMIC_QUALITY_RESOLUTION_BITRATE;
                target_video_it_ = QualityPriority(profile, it);
                target_video_it_ = ResolutionPriority(profile, it, target_video_it_->quality);
                target_video_it_ = BitratePriority(profile, it, target_video_it_->video_resolution, target_video_it_->quality);
                break;
            case 6:  //quality->bitrate->resolution
                avg_bandwidth_computer_type = ADAP_DYNAMIC_QUALITY_BITRATE_RESOLUTION;
                target_video_it_ = QualityPriority(profile, it);
                target_video_it_ = BitratePriority(profile, it, target_video_it_->quality);
                target_video_it_ = ResolutionPriority(profile, it, target_video_it_->avg_bitrate_kbps, target_video_it_->quality);
                break;
            default:  //resolution->bitrate->quality
                avg_bandwidth_computer_type = ADAP_DYNAMIC_RESOLUTION_BITRATE_QUALITY;
                target_video_it_ = ResolutionPriority(profile, it);
                target_video_it_ = BitratePriority(profile, it, target_video_it_->video_resolution);
                target_video_it_ = QualityPriority(profile, it, target_video_it_->avg_bitrate_kbps, target_video_it_->video_resolution);
                break;
        }
        UpdateHisState(profile.net_type, target_video_it_);
        return target_video_it_->representation_id;
    }
}

std::vector<kuaishou::abr::VideoProfile>::iterator VideoAdaptationV2::ResolutionPriority(AdaptionProfile& profile, std::vector<kuaishou::abr::VideoProfile>::iterator it) {
    for (auto temp_it = profile.video_profiles.begin(); temp_it != it; temp_it++) {
        if ((temp_it->video_resolution.width * temp_it->video_resolution.height) > (target_video_it_->video_resolution.width * target_video_it_->video_resolution.height)) {
            target_video_it_ = temp_it;
        }
    }
    return target_video_it_;
}

std::vector<kuaishou::abr::VideoProfile>::iterator VideoAdaptationV2::ResolutionPriority(AdaptionProfile& profile, std::vector<kuaishou::abr::VideoProfile>::iterator it, uint32_t target_bitrate) {
    for (auto temp_it = profile.video_profiles.begin(); temp_it != it; temp_it++) {
        if ((temp_it->avg_bitrate_kbps == target_bitrate) && ((temp_it->video_resolution.width * temp_it->video_resolution.height) > (target_video_it_->video_resolution.width * target_video_it_->video_resolution.height))) {
            target_video_it_ = temp_it;
        }
    }
    return target_video_it_;
}

std::vector<kuaishou::abr::VideoProfile>::iterator VideoAdaptationV2::ResolutionPriority(AdaptionProfile& profile, std::vector<kuaishou::abr::VideoProfile>::iterator it, float target_quality) {
    for (auto temp_it = profile.video_profiles.begin(); temp_it != it; temp_it++) {
        if ((temp_it->quality == target_quality) && ((temp_it->video_resolution.width * temp_it->video_resolution.height) > (target_video_it_->video_resolution.width * target_video_it_->video_resolution.height))) {
            target_video_it_ = temp_it;
        }
    }
    return target_video_it_;
}

std::vector<kuaishou::abr::VideoProfile>::iterator VideoAdaptationV2::ResolutionPriority(AdaptionProfile& profile, std::vector<kuaishou::abr::VideoProfile>::iterator it, uint32_t target_bitrate, float target_quality) {
    for (auto temp_it = profile.video_profiles.begin(); temp_it != it; temp_it++) {
        if ((temp_it->avg_bitrate_kbps == target_bitrate) && (temp_it->quality == target_quality) && ((temp_it->video_resolution.width * temp_it->video_resolution.height) > (target_video_it_->video_resolution.width * target_video_it_->video_resolution.height))) {
            target_video_it_ = temp_it;
        }
    }
    return target_video_it_;
}

std::vector<kuaishou::abr::VideoProfile>::iterator VideoAdaptationV2::BitratePriority(AdaptionProfile& profile, std::vector<kuaishou::abr::VideoProfile>::iterator it) {
    for (auto temp_it = profile.video_profiles.begin(); temp_it != it; temp_it++) {
        if (temp_it->avg_bitrate_kbps > target_video_it_->avg_bitrate_kbps) {
            target_video_it_ = temp_it;
        }
    }
    return target_video_it_;
}

std::vector<kuaishou::abr::VideoProfile>::iterator VideoAdaptationV2::BitratePriority(AdaptionProfile& profile, std::vector<kuaishou::abr::VideoProfile>::iterator it, VideoResolution target_res) {
    for (auto temp_it = profile.video_profiles.begin(); temp_it != it; temp_it++) {
        if (((temp_it->video_resolution.width * temp_it->video_resolution.height) == (target_res.width * target_res.height)) && (temp_it->avg_bitrate_kbps > target_video_it_->avg_bitrate_kbps)) {
            target_video_it_ = temp_it;
        }
    }
    return target_video_it_;
}

std::vector<kuaishou::abr::VideoProfile>::iterator VideoAdaptationV2::BitratePriority(AdaptionProfile& profile, std::vector<kuaishou::abr::VideoProfile>::iterator it, float target_quality) {
    for (auto temp_it = profile.video_profiles.begin(); temp_it != it; temp_it++) {
        if ((temp_it->quality == target_quality) && (temp_it->avg_bitrate_kbps > target_video_it_->avg_bitrate_kbps)) {
            target_video_it_ = temp_it;
        }
    }
    return target_video_it_;
}

std::vector<kuaishou::abr::VideoProfile>::iterator VideoAdaptationV2::BitratePriority(AdaptionProfile& profile, std::vector<kuaishou::abr::VideoProfile>::iterator it, VideoResolution target_res, float target_quality) {
    for (auto temp_it = profile.video_profiles.begin(); temp_it != it; temp_it++) {
        if (((temp_it->video_resolution.width * temp_it->video_resolution.height) == (target_res.width * target_res.height)) && (temp_it->quality == target_quality) && (temp_it->avg_bitrate_kbps > target_video_it_->avg_bitrate_kbps)) {
            target_video_it_ = temp_it;
        }
    }
    return target_video_it_;
}

std::vector<kuaishou::abr::VideoProfile>::iterator VideoAdaptationV2::QualityPriority(AdaptionProfile& profile, std::vector<kuaishou::abr::VideoProfile>::iterator it) {
    for (auto temp_it = profile.video_profiles.begin(); temp_it != it; temp_it++) {
        if (temp_it->quality > target_video_it_->quality) {
            target_video_it_ = temp_it;
        }
    }
    return target_video_it_;
}

std::vector<kuaishou::abr::VideoProfile>::iterator VideoAdaptationV2::QualityPriority(AdaptionProfile& profile, std::vector<kuaishou::abr::VideoProfile>::iterator it, uint32_t target_bitrate) {
    for (auto temp_it = profile.video_profiles.begin(); temp_it != it; temp_it++) {
        if ((temp_it->avg_bitrate_kbps == target_bitrate) && (temp_it->quality > target_video_it_->quality)) {
            target_video_it_ = temp_it;
        }
    }
    return target_video_it_;
}

std::vector<kuaishou::abr::VideoProfile>::iterator VideoAdaptationV2::QualityPriority(AdaptionProfile& profile, std::vector<kuaishou::abr::VideoProfile>::iterator it, VideoResolution target_res) {
    for (auto temp_it = profile.video_profiles.begin(); temp_it != it; temp_it++) {
        if (((temp_it->video_resolution.width * temp_it->video_resolution.height) == (target_res.width * target_res.height)) && (temp_it->quality > target_video_it_->quality)) {
            target_video_it_ = temp_it;
        }
    }
    return target_video_it_;
}

std::vector<kuaishou::abr::VideoProfile>::iterator VideoAdaptationV2::QualityPriority(AdaptionProfile& profile, std::vector<kuaishou::abr::VideoProfile>::iterator it, uint32_t target_bitrate, VideoResolution target_res) {
    for (auto temp_it = profile.video_profiles.begin(); temp_it != it; temp_it++) {
        if ((temp_it->avg_bitrate_kbps == target_bitrate) && ((temp_it->video_resolution.width * temp_it->video_resolution.height) == (target_res.width * target_res.height)) && (temp_it->quality > target_video_it_->quality)) {
            target_video_it_ = temp_it;
        }
    }
    return target_video_it_;
}

VideoAdaptationAlgoType VideoAdaptationV2::GetVideoAdaptationAlgoType() const {
    return VideoAdaptationAlgoType::kBandwidthBased;
}

uint32_t VideoAdaptationV2::BandwidthAmend(AdaptionProfile& profile) {
    uint32_t short_bandwidth_kbps = bandwidth_estimate_->ShortTermBandwidthEstimate(profile.algorithm_mode);
    uint32_t long_bandwidth_kbps = bandwidth_estimate_->LongTermBandwidthEstimate(profile.algorithm_mode);
    uint32_t avg_bandwidth_kbps = 0;
    if (short_bandwidth_kbps != 0) {
        avg_bandwidth_kbps = short_bandwidth_kbps;
        bandwidth_computation_process_ =  "final_bandwidth = " + std::to_string(avg_bandwidth_kbps) + "[short-term bandwidth] ";
    } else {
        avg_bandwidth_kbps = long_bandwidth_kbps;
        bandwidth_computation_process_ =  "final_bandwidth = " + std::to_string(avg_bandwidth_kbps) + "[long-term bandwidth] ";
    }
    if (profile.net_type == NetworkType::WIFI) {
        avg_bandwidth_kbps = (uint32_t)(avg_bandwidth_kbps * rate_adapt_config_.wifi_amend);
        bandwidth_computation_process_ = bandwidth_computation_process_ + " * " + to_string_with_precision(rate_adapt_config_.wifi_amend, 3) + "[WIFI discount]";
    } else {
        avg_bandwidth_kbps = (uint32_t)(avg_bandwidth_kbps * rate_adapt_config_.fourG_amend);
        bandwidth_computation_process_ = bandwidth_computation_process_ + " * " + to_string_with_precision(rate_adapt_config_.fourG_amend, 3) + "[4G discount]";
    }

    if (profile.device_resolution.height * profile.device_resolution.width <= rate_adapt_config_.device_hight_threshold * rate_adapt_config_.device_width_threshold) {
        avg_bandwidth_kbps = (uint32_t)(avg_bandwidth_kbps * rate_adapt_config_.resolution_amend);
        bandwidth_computation_process_ = bandwidth_computation_process_ + " * " + to_string_with_precision(rate_adapt_config_.resolution_amend, 3) + "[low resolution discount]";
    }
    bandwidth_computation_process_ = bandwidth_computation_process_ + " = " + std::to_string(avg_bandwidth_kbps) + "kbps";
    return avg_bandwidth_kbps;
}

std::string VideoAdaptationV2::GetSwitchReason() {
    switch_reason_ = "";
    switch_reason_ += current_condition();
    switch_reason_ += ";";
    switch_reason_ += bandwidth_computation_process();
    return switch_reason_;
}

void VideoAdaptationV2::CreateSwitchReason(const char* fmt, ...) {
    va_list vl;
    va_start(vl, fmt);

    vsnprintf(detail_switch_reason_, DETAIL_SWITCH_REASON_MAX_LEN, fmt, vl);

    va_end(vl);
}

const char* VideoAdaptationV2::GetDetailSwitchReason() {
    memset(detail_switch_reason_, 0, DETAIL_SWITCH_REASON_MAX_LEN + 1);
    switch (avg_bandwidth_computer_type) {
        case UNINIT_NO_HISTORY_LOW_RESOLUTION:
            if (!is_avg_bandwidth_zero_) {
                CreateSwitchReason("码率信息还没有初始化，未读取到永久缓存，默认使用低分辨率视频");
            } else {
                CreateSwitchReason("估计带宽为零，且未读取到永久缓存，默认使用低分辨率视频");
            }
            break;
        case UNINIT_DEPEND_ON_HISTORY_LOW_RESOLUTION:
            if (!is_avg_bandwidth_zero_) {
                CreateSwitchReason("读取到永久缓存，默认选择不高于缓存分辨率(%d,%d)的最大分辨率.",
                                   his_state_.video_width,
                                   his_state_.video_height);
            } else {
                CreateSwitchReason("估计带宽为零，且读取到永久缓存，默认选择不高于缓存分辨率(%d,%d)的最大分辨率",
                                   his_state_.video_width,
                                   his_state_.video_height);
            }
            break;
        case UNINIT_DEFAULT_HEIGHTESt_RESOLUTION:
            if (!is_avg_bandwidth_zero_) {
                CreateSwitchReason("码率信息还没有初始化，默认使用高分辨率");
            } else {
                CreateSwitchReason("估计带宽为零,默认使用高分辨率视频");
            }
            break;
        case UNINIT_SPECIFIED_RESOLUTION:
            if (!is_avg_bandwidth_zero_) {
                CreateSwitchReason("依据分辨率升序排序，分辨率相同依据avgbitrate升序排序，使用bitrateinitLevel指定位置的分辨率");
            } else {
                CreateSwitchReason("估计带宽为零，依据分辨率升序排序，分辨率相同依据avgbitrate升序排序，使用bitrateinitLevel指定位置的分辨率");
            }
            break;
        case LOWDEVICE:
            CreateSwitchReason("低端机型，使用最低分辨率");
            break;
        case ADAP_WIFI_LOWEST_RATE:
            CreateSwitchReason("wifi下根据下发配置，固定选择最低码率视频");
            break;
        case ADAP_WIFI_LOWEST_RESOLUTION:
            CreateSwitchReason("wifi下根据下发配置，固定选择最低分辨率视频");
            break;
        case ADAP_WIFI_HEIGHTEST_RESOLUTION:
            CreateSwitchReason("wifi下根据下发配置，固定选择最高分辨率视频");
            break;
        case ADAP_FOURG_LOWEST_RATE:
            CreateSwitchReason("非wifi下根据下发配置，固定选择最低码率视频");
            break;
        case ADAP_FOURG_LOWEST_RESOLUTION:
            CreateSwitchReason("非wifi下根据下发配置，固定选择最低分辨率视频");
            break;
        case ADAP_FOURG_HEIGHTEST_RESOLUTION:
            CreateSwitchReason("非wifi下根据下发配置，固定选择最高分辨率视频");
            break;
        case ADAP_OTHER_LOWEST_RESOLUTION:
            CreateSwitchReason("非wifi非4G下根据下发配置,固定选择最低分辨率视频");
            break;
        case ADAP_DYNAMIC_RESOLUTION_BITRATE_QUALITY:
            CreateSwitchReason("根据估算带宽:%d,按照(分辨率>码率>清晰度)的顺序选择视频", avg_bandwidth_);
            break;
        case ADAP_DYNAMIC_RESOLUTION_QUALITY_BITRATE:
            CreateSwitchReason("根据估算带宽:%d,按照(分辨率>清晰度>码率)的顺序选择视频", avg_bandwidth_);
            break;
        case ADAP_DYNAMIC_BITRATE_RESOLUTION_QUALITY:
            CreateSwitchReason("根据估算带宽:%d,按照(码率>分辨率>清晰度)的顺序选择视频", avg_bandwidth_);
            break;
        case ADAP_DYNAMIC_BITRATE_QUALITY_RESOLUTION:
            CreateSwitchReason("根据估算带宽:%d,按照(码率>清晰度>分辨率)的顺序选择视频", avg_bandwidth_);
            break;
        case ADAP_DYNAMIC_QUALITY_RESOLUTION_BITRATE:
            CreateSwitchReason("根据估算带宽:%d,按照(清晰度>分辨率>码率)的顺序选择视频", avg_bandwidth_);
            break;
        case ADAP_DYNAMIC_QUALITY_BITRATE_RESOLUTION:
            CreateSwitchReason("根据估算带宽:%d,,按照(清晰度>码率>分辨率>)的顺序选择视频", avg_bandwidth_);
            break;
        default:
            CreateSwitchReason(detail_switch_reason_, DETAIL_SWITCH_REASON_MAX_LEN, "unknow");
            break;
    }
    return detail_switch_reason_;
}

void VideoAdaptationV2::UpdateConfig(RateAdaptConfig& rate_config, RateAdaptConfigA1& rate_config_a1) {
    rate_adapt_config_ = rate_config;
    rate_adapt_config_a1_ = rate_config_a1;
}

void VideoAdaptationV2::UpdateHisState(NetworkType net_type, std::vector<kuaishou::abr::VideoProfile>::iterator video_it) {
    switch (net_type) {
        case NetworkType::WIFI:
            wifi_his_state_.net_type = static_cast<uint32_t>(NetworkType::WIFI);
            wifi_his_state_.update_time = 0;
            wifi_his_state_.video_width = video_it->video_resolution.width;
            wifi_his_state_.video_height = video_it->video_resolution.height;
            wifi_his_state_.avg_video_bitrate = video_it->avg_bitrate_kbps;
            wifi_his_state_.max_video_bitrate = video_it->max_bitrate_kbps;
            wifi_his_state_.short_term_throughput = bandwidth_estimate_->ShortTermBandwidthEstimate();
            wifi_his_state_.long_term_throughput = bandwidth_estimate_->LongTermBandwidthEstimate();
            DumpHistoryData(wifi_his_state_, "Update history");
            break;
        case NetworkType::FOUR_G:
            foureG_his_state_.net_type = static_cast<uint32_t>(NetworkType::FOUR_G);
            foureG_his_state_.update_time = 0;
            foureG_his_state_.video_width = video_it->video_resolution.width;
            foureG_his_state_.video_height = video_it->video_resolution.height;
            foureG_his_state_.avg_video_bitrate = video_it->avg_bitrate_kbps;
            foureG_his_state_.max_video_bitrate = video_it->max_bitrate_kbps;
            foureG_his_state_.short_term_throughput = bandwidth_estimate_->ShortTermBandwidthEstimate();
            foureG_his_state_.long_term_throughput = bandwidth_estimate_->LongTermBandwidthEstimate();
            DumpHistoryData(foureG_his_state_, "Update history");
            break;
        case NetworkType::THREE_G:
            threeG_his_state_.net_type = static_cast<uint32_t>(NetworkType::THREE_G);
            threeG_his_state_.update_time = 0;
            threeG_his_state_.video_width = video_it->video_resolution.width;
            threeG_his_state_.video_height = video_it->video_resolution.height;
            threeG_his_state_.avg_video_bitrate = video_it->avg_bitrate_kbps;
            threeG_his_state_.max_video_bitrate = video_it->max_bitrate_kbps;
            threeG_his_state_.short_term_throughput = bandwidth_estimate_->ShortTermBandwidthEstimate();
            threeG_his_state_.long_term_throughput = bandwidth_estimate_->LongTermBandwidthEstimate();
            DumpHistoryData(threeG_his_state_, "Update history");
            break;
        default:
            other_his_state_.net_type = static_cast<uint32_t>(NetworkType::UNKNOW);
            other_his_state_.update_time = 0;
            other_his_state_.video_width = video_it->video_resolution.width;
            other_his_state_.video_height = video_it->video_resolution.height;
            other_his_state_.avg_video_bitrate = video_it->avg_bitrate_kbps;
            other_his_state_.max_video_bitrate = video_it->max_bitrate_kbps;
            other_his_state_.short_term_throughput = bandwidth_estimate_->ShortTermBandwidthEstimate();
            other_his_state_.long_term_throughput = bandwidth_estimate_->LongTermBandwidthEstimate();
            DumpHistoryData(other_his_state_, "Update history");
            break;
    }
}

void VideoAdaptationV2::DumpHistoryData(HisState& his_state, std::string log_tag) {
    std::string net_type;
    switch (his_state.net_type) {
        case static_cast<uint32_t>(NetworkType::WIFI):
            net_type = "WIFI";
            break;
        case static_cast<uint32_t>(NetworkType::FOUR_G):
            net_type = "FOUR_G";
            break;
        case static_cast<uint32_t>(NetworkType::THREE_G):
            net_type = "THREE_G";
            break;
        default:
            net_type = "OTHER";
            break;
    }

    LOG_INFO("%s: net_type: %s, update_time: %lld, video_width: %d, video_height: %d, "
             "avg_bitrate: %d, max_bitrate: %d, short_term_throughput: %d, long_term_throughput: %d. \n",
             log_tag.c_str(), net_type.c_str(), wifi_his_state_.update_time,
             his_state.video_width, his_state.video_height,
             his_state.avg_video_bitrate, his_state.max_video_bitrate,
             his_state.short_term_throughput, his_state.long_term_throughput);
}

void VideoAdaptationV2::SetHistoryData(const std::string& history_data) {
    cJSON* root = cJSON_Parse(const_cast<char*>(history_data.c_str()));
    if (!root) {
        LOG_ERROR("VideoAdaptationV2: history_data parse error!")
        return;
    }
    LOG_INFO("History data list. %s", history_data.c_str());

    if (cJSON_Object == root->type) {
        cJSON* history_data_array = cJSON_GetObjectItem(root, "data_list");
        int len = cJSON_GetArraySize(history_data_array);
        for (int i = 0; i <  len; i++) {
            uint32_t net_type = 0;
            cJSON* array_item = cJSON_GetArrayItem(history_data_array, i);
            net_type = cJSON_GetObjectItem(array_item, "net_type")->valueint;
            switch (net_type) {
                case static_cast<uint32_t>(NetworkType::WIFI):
                    wifi_his_state_.net_type = cJSON_GetObjectItem(array_item, "net_type")->valueint;
                    wifi_his_state_.update_time = cJSON_GetObjectItem(array_item, "update_time")->valueint;
                    wifi_his_state_.video_width = cJSON_GetObjectItem(array_item, "width")->valueint;
                    wifi_his_state_.video_height = cJSON_GetObjectItem(array_item, "height")->valueint;
                    wifi_his_state_.avg_video_bitrate = cJSON_GetObjectItem(array_item, "avg_rate")->valueint;
                    wifi_his_state_.max_video_bitrate = cJSON_GetObjectItem(array_item, "max_rate")->valueint;
                    wifi_his_state_.short_term_throughput = cJSON_GetObjectItem(array_item, "short_bw")->valueint;
                    wifi_his_state_.long_term_throughput = cJSON_GetObjectItem(array_item, "long_bw")->valueint;
                    break;
                case static_cast<uint32_t>(NetworkType::FOUR_G):
                    foureG_his_state_.net_type = cJSON_GetObjectItem(array_item, "net_type")->valueint;
                    foureG_his_state_.update_time = cJSON_GetObjectItem(array_item, "update_time")->valueint;
                    foureG_his_state_.video_width = cJSON_GetObjectItem(array_item, "width")->valueint;
                    foureG_his_state_.video_height = cJSON_GetObjectItem(array_item, "height")->valueint;
                    foureG_his_state_.avg_video_bitrate = cJSON_GetObjectItem(array_item, "avg_rate")->valueint;
                    foureG_his_state_.max_video_bitrate = cJSON_GetObjectItem(array_item, "max_rate")->valueint;
                    foureG_his_state_.short_term_throughput = cJSON_GetObjectItem(array_item, "short_bw")->valueint;
                    foureG_his_state_.long_term_throughput = cJSON_GetObjectItem(array_item, "long_bw")->valueint;
                    break;
                case static_cast<uint32_t>(NetworkType::THREE_G):
                    threeG_his_state_.net_type = cJSON_GetObjectItem(array_item, "net_type")->valueint;
                    threeG_his_state_.update_time = cJSON_GetObjectItem(array_item, "update_time")->valueint;
                    threeG_his_state_.video_width = cJSON_GetObjectItem(array_item, "width")->valueint;
                    threeG_his_state_.video_height = cJSON_GetObjectItem(array_item, "height")->valueint;
                    threeG_his_state_.avg_video_bitrate = cJSON_GetObjectItem(array_item, "avg_rate")->valueint;
                    threeG_his_state_.max_video_bitrate = cJSON_GetObjectItem(array_item, "max_rate")->valueint;
                    threeG_his_state_.short_term_throughput = cJSON_GetObjectItem(array_item, "short_bw")->valueint;
                    threeG_his_state_.long_term_throughput = cJSON_GetObjectItem(array_item, "long_bw")->valueint;
                    break;
                default:
                    other_his_state_.net_type = cJSON_GetObjectItem(array_item, "net_type")->valueint;
                    other_his_state_.update_time = cJSON_GetObjectItem(array_item, "update_time")->valueint;
                    other_his_state_.video_width = cJSON_GetObjectItem(array_item, "width")->valueint;
                    other_his_state_.video_height = cJSON_GetObjectItem(array_item, "height")->valueint;
                    other_his_state_.avg_video_bitrate = cJSON_GetObjectItem(array_item, "avg_rate")->valueint;
                    other_his_state_.max_video_bitrate = cJSON_GetObjectItem(array_item, "max_rate")->valueint;
                    other_his_state_.short_term_throughput = cJSON_GetObjectItem(array_item, "short_bw")->valueint;
                    other_his_state_.long_term_throughput = cJSON_GetObjectItem(array_item, "long_bw")->valueint;
                    break;
            }
        }

        DumpHistoryData(wifi_his_state_, "History data list");
        DumpHistoryData(foureG_his_state_, "History data list");
        DumpHistoryData(threeG_his_state_, "History data list");
        DumpHistoryData(other_his_state_, "History data list");
    }
}

std::string VideoAdaptationV2::GetHistoryData() {
    cJSON* root = cJSON_CreateObject();
    cJSON* array_json = cJSON_CreateArray();
    cJSON* wifi_item = cJSON_CreateObject();
    cJSON* fourG_itme = cJSON_CreateObject();
    cJSON* threeG_item = cJSON_CreateObject();
    cJSON* other_item = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "data_list", array_json);

    cJSON_AddItemToArray(array_json, wifi_item);
    cJSON_AddNumberToObject(wifi_item, "net_type", wifi_his_state_.net_type);
    cJSON_AddNumberToObject(wifi_item, "update_time", wifi_his_state_.update_time);
    cJSON_AddNumberToObject(wifi_item, "width", wifi_his_state_.video_width);
    cJSON_AddNumberToObject(wifi_item, "height", wifi_his_state_.video_height);
    cJSON_AddNumberToObject(wifi_item, "avg_rate", wifi_his_state_.avg_video_bitrate);
    cJSON_AddNumberToObject(wifi_item, "max_rate", wifi_his_state_.max_video_bitrate);
    cJSON_AddNumberToObject(wifi_item, "short_bw", wifi_his_state_.short_term_throughput);
    cJSON_AddNumberToObject(wifi_item, "long_bw", wifi_his_state_.long_term_throughput);
    cJSON_AddItemToArray(array_json, fourG_itme);
    cJSON_AddNumberToObject(fourG_itme, "net_type", foureG_his_state_.net_type);
    cJSON_AddNumberToObject(fourG_itme, "update_time", foureG_his_state_.update_time);
    cJSON_AddNumberToObject(fourG_itme, "width", foureG_his_state_.video_width);
    cJSON_AddNumberToObject(fourG_itme, "height", foureG_his_state_.video_height);
    cJSON_AddNumberToObject(fourG_itme, "avg_rate", foureG_his_state_.avg_video_bitrate);
    cJSON_AddNumberToObject(fourG_itme, "max_rate", foureG_his_state_.max_video_bitrate);
    cJSON_AddNumberToObject(fourG_itme, "short_bw", foureG_his_state_.short_term_throughput);
    cJSON_AddNumberToObject(fourG_itme, "long_bw", foureG_his_state_.long_term_throughput);
    cJSON_AddItemToArray(array_json, threeG_item);
    cJSON_AddNumberToObject(threeG_item, "net_type", threeG_his_state_.net_type);
    cJSON_AddNumberToObject(threeG_item, "update_time", threeG_his_state_.update_time);
    cJSON_AddNumberToObject(threeG_item, "width", threeG_his_state_.video_width);
    cJSON_AddNumberToObject(threeG_item, "height", threeG_his_state_.video_height);
    cJSON_AddNumberToObject(threeG_item, "avg_rate", threeG_his_state_.avg_video_bitrate);
    cJSON_AddNumberToObject(threeG_item, "max_rate", threeG_his_state_.max_video_bitrate);
    cJSON_AddNumberToObject(threeG_item, "short_bw", threeG_his_state_.short_term_throughput);
    cJSON_AddNumberToObject(threeG_item, "long_bw", threeG_his_state_.long_term_throughput);
    cJSON_AddItemToArray(array_json, other_item);
    cJSON_AddNumberToObject(other_item, "net_type", other_his_state_.net_type);
    cJSON_AddNumberToObject(other_item, "update_time", other_his_state_.update_time);
    cJSON_AddNumberToObject(other_item, "width", other_his_state_.video_width);
    cJSON_AddNumberToObject(other_item, "height", other_his_state_.video_height);
    cJSON_AddNumberToObject(other_item, "avg_rate", other_his_state_.avg_video_bitrate);
    cJSON_AddNumberToObject(other_item, "max_rate", other_his_state_.max_video_bitrate);
    cJSON_AddNumberToObject(other_item, "short_bw", other_his_state_.short_term_throughput);
    cJSON_AddNumberToObject(other_item, "long_bw", other_his_state_.long_term_throughput);
    std::string str =  cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    LOG_DEBUG("Update stored data: %s", str.c_str());
    return str;
}

uint32_t VideoAdaptationV2::GetManualAutoState() {
    return manual_auto_state_;
}

}
}
