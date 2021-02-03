//
// Created by liuyuxin on 2018/4/2.
//

#include "c_abr_engine.h"
#include "abr_types.h"
#include "abr_engine.h"
#include "kwai_vod_manifest.h"

using namespace kuaishou::abr;

static void c_abr_copy_rate_config(VodRateAdaptConfig* vod_rate_config, RateAdaptConfig* rate_config) {
    rate_config->rate_addapt_type = (VideoAdaptationAlgoType)vod_rate_config->rate_addapt_type;
    rate_config->bandwidth_estimation_type = (BandwidthEstimationAlgoType)vod_rate_config->bandwidth_estimation_type;
    rate_config->absolute_low_res_low_device = vod_rate_config->absolute_low_res_low_device;
    rate_config->adapt_under_4G = vod_rate_config->adapt_under_4G;
    rate_config->adapt_under_wifi = vod_rate_config->adapt_under_wifi;
    rate_config->adapt_under_other_net = vod_rate_config->adapt_under_other_net;
    rate_config->absolute_low_rate_4G = vod_rate_config->absolute_low_rate_4G;
    rate_config->absolute_low_rate_wifi = vod_rate_config->absolute_low_rate_wifi;
    rate_config->absolute_low_res_4G = vod_rate_config->absolute_low_res_4G;
    rate_config->absolute_low_res_wifi = vod_rate_config->absolute_low_res_wifi;
    rate_config->short_keep_interval = vod_rate_config->short_keep_interval;
    rate_config->long_keep_interval = vod_rate_config->long_keep_interval;
    rate_config->bitrate_init_level = vod_rate_config->bitrate_init_level;
    rate_config->default_weight = vod_rate_config->default_weight;
    rate_config->block_affected_interval = vod_rate_config->block_affected_interval;
    rate_config->wifi_amend = vod_rate_config->wifi_amend;
    rate_config->fourG_amend = vod_rate_config->fourG_amend;
    rate_config->resolution_amend = vod_rate_config->resolution_amend;
    rate_config->device_width_threshold = vod_rate_config->device_width_threshold;
    rate_config->device_hight_threshold = vod_rate_config->device_hight_threshold;
    rate_config->priority_policy = vod_rate_config->priority_policy;
    //to do
    return;
}

static void c_abr_copy_rate_config_a1(VodRateAdaptConfigA1* vod_rate_config, RateAdaptConfigA1* rate_config) {
    rate_config->short_keep_interval = vod_rate_config->short_keep_interval;
    rate_config->long_keep_interval = vod_rate_config->long_keep_interval;
    rate_config->bitrate_init_level = vod_rate_config->bitrate_init_level;
    rate_config->max_resolution = vod_rate_config->max_resolution;
    return;
}


int c_abr_engine_adapt_profiles(VodPlayList* pl, char* buf, int len) {
    AdaptionProfile profile;
    std::string resolutions = " ";

    for (int i = 0; i < pl->rep_count; i++) {
        VideoProfile video_profile;
        video_profile.avg_bitrate_kbps = pl->rep[i].avg_bitrate_kbps;
        video_profile.max_bitrate_kbps = pl->rep[i].max_bitrate_kbps;
        video_profile.representation_id = pl->rep[i].representation_id;
        video_profile.video_resolution.width = pl->rep[i].video_resolution.width;
        video_profile.video_resolution.height = pl->rep[i].video_resolution.height;
        video_profile.quality = pl->rep[i].quality;
        video_profile.id = pl->rep[i].id;
        video_profile.cached_bytes = pl->rep[i].cached_bytes;
        int64_t remain_bytes = pl->rep[i].avg_bitrate_kbps * pl->duration - pl->rep[i].cached_bytes;
        if (remain_bytes <= 0 || pl->duration <= 0) {
            video_profile.equivalent_bitrate_kbps = 0;
        } else {
            video_profile.equivalent_bitrate_kbps = static_cast<uint32_t>(remain_bytes / pl->duration);
        }
        resolutions += std::to_string(video_profile.video_resolution.width) + "*" +
                       std::to_string(video_profile.video_resolution.height) + ", max:" +
                       std::to_string(video_profile.max_bitrate_kbps) + ", avg:" +
                       std::to_string(video_profile.avg_bitrate_kbps) + "; ";
        profile.video_profiles.push_back(video_profile);
    }
    strncpy(buf, resolutions.c_str(), len - 1);
    profile.net_type = (kuaishou::abr::NetworkType) pl->net_type;
    profile.device_resolution.width = pl->device_resolution.width;
    profile.device_resolution.height = pl->device_resolution.height;
    profile.low_device = pl->low_device;
    profile.adaptation_id = pl->adaptation_id;
    profile.switch_code = pl->switch_code;
    profile.algorithm_mode = pl->algorithm_mode;

    std::pair<bool, uint32_t> ret = AbrEngine::GetInstance()->AdaptPendingNextProfileRepId(pl->duration, profile);
    return ret.first ? ret.second : 0;
}

void c_abr_engine_adapt_init(VodRateAdaptConfig* vod_rate_config, VodRateAdaptConfigA1* vod_rate_config_a1) {
    static bool is_abr_engine_init = false;
    RateAdaptConfig rate_config;
    RateAdaptConfigA1 rate_config_a1;
    if (vod_rate_config) {
        c_abr_copy_rate_config(vod_rate_config, &rate_config);
    }
    if (vod_rate_config_a1) {
        c_abr_copy_rate_config_a1(vod_rate_config_a1, &rate_config_a1);
    }
    if (!is_abr_engine_init) {
        if (vod_rate_config || vod_rate_config_a1) {
            AbrEngine::GetInstance()->Init(rate_config, rate_config_a1);
        } else {
            AbrEngine::GetInstance()->Init();
        }
        is_abr_engine_init = true;
    } else if (vod_rate_config || vod_rate_config_a1) {
        AbrEngine::GetInstance()->UpdateConfig(rate_config, rate_config_a1);
    }
}

uint64_t c_abr_get_idle_last_request_time() {
    return AbrEngine::GetInstance()->get_idle_time_from_last_request();
}

uint32_t c_abr_get_short_throughput_kbps(uint32_t algorithm_mode) {
    return AbrEngine::GetInstance()->get_short_term_throughput(algorithm_mode);
}

uint32_t c_abr_get_long_throughput_kbps(uint32_t algorithm_mode) {
    return AbrEngine::GetInstance()->get_long_term_throughput(algorithm_mode);
}

void c_abr_get_switch_reason(char* buf, int len) {
    strncpy(buf, AbrEngine::GetInstance()->get_switch_reason().c_str(), len);
    return;
}

const char* c_abr_get_detail_switch_reason() {
    return AbrEngine::GetInstance()->get_detail_switch_reason();
}

void c_abr_get_vod_resolution(VodPlayList* pl, char* buf, int len) {
    std::string resolutions = " ";

    for (int i = 0; i < pl->rep_count; i++) {
        VideoProfile video_profile;
        video_profile.avg_bitrate_kbps = pl->rep[i].avg_bitrate_kbps;
        video_profile.max_bitrate_kbps = pl->rep[i].max_bitrate_kbps;
        video_profile.representation_id = pl->rep[i].representation_id;
        video_profile.video_resolution.width = pl->rep[i].video_resolution.width;
        video_profile.video_resolution.height = pl->rep[i].video_resolution.height;
        resolutions += std::to_string(video_profile.video_resolution.width) + "*" +
                       std::to_string(video_profile.video_resolution.height) + ", max:" +
                       std::to_string(video_profile.max_bitrate_kbps) + ", avg:" +
                       std::to_string(video_profile.avg_bitrate_kbps) + "; ";
    }
    strncpy(buf, resolutions.c_str(), len - 1);
}
