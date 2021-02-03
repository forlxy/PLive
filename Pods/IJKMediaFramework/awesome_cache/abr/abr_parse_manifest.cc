
#include <include/awesome_cache_c.h>
#include "abr_parse_manifest.h"
#include "ac_log.h"
#include "c_abr_engine.h"
#include "abr_engine.h"

namespace kuaishou {
namespace abr {

AbrParseManifest::AbrParseManifest() {}

int AbrParseManifest::AbrEngienAdaptInit() {
    if (AbrEngine::GetInstance()->IsInit()) {
        AbrEngine::GetInstance()->UpdateConfig(rate_config_);
    } else {
        AbrEngine::GetInstance()->Init(rate_config_);
    }

    std::pair<bool, uint32_t> ret = AbrEngine::GetInstance()->AdaptPendingNextProfileRepId(vod_playlist_.duration, adaption_profile_);

    return ret.first ? ret.second : 0;;
}

char* AbrParseManifest::GetUrl(int index) {
    return vod_playlist_.rep[index].url;
}

char* AbrParseManifest::GetHost(int index) {
    return vod_playlist_.rep[index].host;
}

char* AbrParseManifest::GetKey(int index) {
    return vod_playlist_.rep[index].key;
}

int AbrParseManifest::GetDownloadLen(int index) {
    return vod_playlist_.rep[index].download_len;
}

int AbrParseManifest::GetAvgBitrate(int index) {
    return vod_playlist_.rep[index].avg_bitrate_kbps;
}

void AbrParseManifest::Init(uint32_t dev_res_width, uint32_t dev_res_heigh,
                            uint32_t net_type, uint32_t low_device,
                            uint32_t signal_strength, uint32_t switch_code) {
    vod_playlist_.device_resolution.width = dev_res_width;
    vod_playlist_.device_resolution.height = dev_res_heigh;
    vod_playlist_.net_type = (NetworkType)net_type;
    vod_playlist_.low_device = low_device;
    vod_playlist_.signal_strength = signal_strength;
    vod_playlist_.switch_code = switch_code;
    InitRateConfig();
}


int AbrParseManifest::ParserVodAdaptiveManifest(const std::string& url) {
    cJSON* root = cJSON_Parse(const_cast<char*>(url.c_str()));
    if (!root) {
        LOG_ERROR("ParserRateConfig: config is bad data!")
        return -1;
    }
    LOG_INFO("url: %s", url.c_str());

    if (cJSON_Object == root->type) {
        cJSON* adaptation_array = cJSON_GetObjectItem(root, "adaptationSet");
        int adaptation_len = cJSON_GetArraySize(adaptation_array);
        for (int i = 0; i < adaptation_len; i++) {
            cJSON* adaptation_item = cJSON_GetArrayItem(adaptation_array, i);
            vod_playlist_.adaptation_id = GetItemValueInt(adaptation_item, "adaptationId", 0);
            char duration[64];
            if (GetItemValueStr(adaptation_item, "duration", duration) != 0) {
                return -1;
            }
            vod_playlist_.duration = atoi(duration);

            cJSON* representation_array = cJSON_GetObjectItem(adaptation_item, "representation");
            int representation_len =  cJSON_GetArraySize(representation_array);
            vod_playlist_.rep_count = representation_len;
            for (int j = 0; j < representation_len; j++) {
                vod_playlist_.rep[j].representation_id = j;
                cJSON* representation_item = cJSON_GetArrayItem(representation_array, j);
                vod_playlist_.rep[j].max_bitrate_kbps = (int)GetItemValueDouble(representation_item, "maxBitrate", 0.0);
                vod_playlist_.rep[j].avg_bitrate_kbps = (int)GetItemValueDouble(representation_item, "avgBitrate", 0.0);
                vod_playlist_.rep[j].video_resolution.height = (float)GetItemValueDouble(representation_item, "height", 0.0);
                vod_playlist_.rep[j].video_resolution.width = (float)GetItemValueDouble(representation_item, "width", 0.0);
                vod_playlist_.rep[j].quality = (float)GetItemValueDouble(representation_item, "quality", 0.0);
                vod_playlist_.rep[j].download_len = GetItemValueInt(representation_item, "downloadLen", 0);

                if (GetItemValueStr(representation_item, "url", vod_playlist_.rep[j].url) != 0) {
                    return -1;
                }

                if (GetItemValueStr(representation_item, "host", vod_playlist_.rep[j].host) != 0) {
                    return -1;
                }
                if (GetItemValueStr(representation_item, "key", vod_playlist_.rep[j].key) != 0) {
                    return -1;
                }
            }
        }
    }
    cJSON_Delete(root);

    CopyToAdaptProfiles();
    return 0;
}

int AbrParseManifest::ParserRateConfig(const std::string& config) {
    cJSON* root = cJSON_Parse(const_cast<char*>(config.c_str()));
    if (!root) {
        LOG_ERROR("ParserRateConfig: config is bad data!")
        return -1;
    }
    LOG_INFO("config: %s", config.c_str());

    if (cJSON_Object == root->type) {

        rate_config_.rate_addapt_type = (VideoAdaptationAlgoType)GetItemValueInt(root, "rate_adapt_type", 0);
        rate_config_.bandwidth_estimation_type = (BandwidthEstimationAlgoType)GetItemValueInt(root, "bandwidth_estimation_type", 0);
        rate_config_.device_width_threshold = GetItemValueInt(root, "device_width_threshold", 0);
        rate_config_.device_hight_threshold = GetItemValueInt(root, "device_hight_threshold", 0);
        rate_config_.absolute_low_res_low_device = GetItemValueInt(root, "absolute_low_res_low_device", 0);
        rate_config_.adapt_under_4G = GetItemValueInt(root, "adapt_under_4G", 0);
        rate_config_.adapt_under_wifi = GetItemValueInt(root, "adapt_under_wifi", 0);
        rate_config_.adapt_under_other_net = GetItemValueInt(root, "adapt_under_other_net", 0);
        rate_config_.absolute_low_rate_4G = GetItemValueInt(root, "absolute_low_rate_4G", 0);
        rate_config_.absolute_low_rate_wifi = GetItemValueInt(root, "absolute_low_rate_wifi", 0);
        rate_config_.absolute_low_res_4G = GetItemValueInt(root, "absolute_low_res_4G", 0);
        rate_config_.absolute_low_res_wifi = GetItemValueInt(root, "absolute_low_res_wifi", 0);
        rate_config_.short_keep_interval = GetItemValueInt(root, "short_keep_interval", 0);
        rate_config_.long_keep_interval = GetItemValueInt(root, "long_keep_interval", 0);
        rate_config_.bitrate_init_level = GetItemValueInt(root, "bitrate_init_level", 0);
        rate_config_.block_affected_interval = GetItemValueInt(root, "block_affected_interval", 0);
        rate_config_.priority_policy = GetItemValueInt(root, "priority_policy", 0);

        rate_config_.default_weight = GetItemValueDouble(root, "default_weight", 0.0);
        rate_config_.wifi_amend = GetItemValueDouble(root, "wifi_amend", 0.0);
        rate_config_.fourG_amend = GetItemValueDouble(root, "fourG_amend", 0.0);
        rate_config_.resolution_amend = GetItemValueDouble(root, "resolution_amend", 0.0);
    }

    cJSON_Delete(root);
    return 0;
}

int AbrParseManifest::GetItemValueInt(cJSON* root, const char* name, int default_value) {
    cJSON* item = cJSON_GetObjectItem(root, name);
    if (item) {
        return item->valueint;
    } else {
        return default_value;
    }
}

double AbrParseManifest::GetItemValueDouble(cJSON* root, const char* name, double default_value) {
    cJSON* item = cJSON_GetObjectItem(root, name);
    if (item) {
        return item->valuedouble;
    } else {
        return default_value;
    }
}

int AbrParseManifest::GetItemValueStr(cJSON* root, const char* name, char* dst) {
    cJSON* item = cJSON_GetObjectItem(root, name);
    if (item) {
        strcpy(dst, item->valuestring);
        return 0;
    }
    return -1;
}

int AbrParseManifest::GetCachedIndex() {
    if (vod_playlist_.rep_count <= 0) {
        return -1;
    }
    for (int i = 0; i < vod_playlist_.rep_count; i++) {
        if (ac_is_fully_cached(vod_playlist_.rep[i].url, vod_playlist_.rep[i].key)) {
            return i;
        }
    }
    return -1;
}


void AbrParseManifest::CopyToAdaptProfiles() {
    if (vod_playlist_.rep_count <= 0) {
        return;
    }

    for (int i = 0; i < vod_playlist_.rep_count; i++) {
        VideoProfile video_profile;
        video_profile.avg_bitrate_kbps = vod_playlist_.rep[i].avg_bitrate_kbps;
        video_profile.max_bitrate_kbps = vod_playlist_.rep[i].max_bitrate_kbps;
        video_profile.representation_id = vod_playlist_.rep[i].representation_id;
        video_profile.video_resolution.width = vod_playlist_.rep[i].video_resolution.width;
        video_profile.video_resolution.height = vod_playlist_.rep[i].video_resolution.height;
        video_profile.quality = vod_playlist_.rep[i].quality;
        video_profile.download_len = vod_playlist_.rep[i].download_len;
        adaption_profile_.video_profiles.push_back(video_profile);
    }
    adaption_profile_.net_type = vod_playlist_.net_type;
    adaption_profile_.device_resolution.width = vod_playlist_.device_resolution.width;
    adaption_profile_.device_resolution.height = vod_playlist_.device_resolution.height;
    adaption_profile_.low_device = vod_playlist_.low_device;
    adaption_profile_.adaptation_id = vod_playlist_.adaptation_id;

    return;
}

AdaptionProfile& AbrParseManifest::GetAdaptionProfile() {
    return adaption_profile_;
}

void AbrParseManifest::InitRateConfig() {
    rate_config_.rate_addapt_type = kBandwidthBased;
    rate_config_.bandwidth_estimation_type = kMovingAverage;
    rate_config_.absolute_low_res_low_device = 0;
    rate_config_.adapt_under_4G = 1;
    rate_config_.adapt_under_wifi = 0;
    rate_config_.adapt_under_other_net = 0;
    rate_config_.absolute_low_rate_4G = 0;
    rate_config_.absolute_low_rate_wifi = 0;
    rate_config_.absolute_low_res_4G = 0;
    rate_config_.absolute_low_res_wifi = 0;
    rate_config_.short_keep_interval = 60000;
    rate_config_.long_keep_interval = 600000;
    rate_config_.bitrate_init_level = 2;
    rate_config_.default_weight = 1.0;
    rate_config_.block_affected_interval = 10000;
    rate_config_.wifi_amend = 0.7;
    rate_config_.fourG_amend = 0.3;
    rate_config_.resolution_amend = 0.6;
    rate_config_.device_width_threshold = 720;
    rate_config_.device_hight_threshold = 960;  //720p is defined as 720*960
    rate_config_.priority_policy = 1;
}

RateAdaptConfig& AbrParseManifest::GetConfigRate() {
    return rate_config_;
}

}
}
