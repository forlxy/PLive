#pragma once
#include <cstdint>
#include <vector>

namespace kuaishou {
namespace abr {


enum NetworkType {
    UNKNOW,
    WIFI,
    FOUR_G,
    THREE_G,
    TWO_G,
};

enum BandwidthComputerType {
    UNINIT_NO_HISTORY_LOW_RESOLUTION,
    UNINIT_DEPEND_ON_HISTORY_LOW_RESOLUTION,
    UNINIT_DEFAULT_HEIGHTESt_RESOLUTION,
    UNINIT_SPECIFIED_RESOLUTION,
    LOWDEVICE,
    ADAP_WIFI_LOWEST_RATE,
    ADAP_WIFI_LOWEST_RESOLUTION,
    ADAP_WIFI_HEIGHTEST_RESOLUTION,
    ADAP_FOURG_LOWEST_RATE,
    ADAP_FOURG_LOWEST_RESOLUTION,
    ADAP_FOURG_HEIGHTEST_RESOLUTION,
    ADAP_OTHER_LOWEST_RESOLUTION,
    ADAP_DYNAMIC_RESOLUTION_BITRATE_QUALITY,
    ADAP_DYNAMIC_RESOLUTION_QUALITY_BITRATE,
    ADAP_DYNAMIC_BITRATE_RESOLUTION_QUALITY,
    ADAP_DYNAMIC_BITRATE_QUALITY_RESOLUTION,
    ADAP_DYNAMIC_QUALITY_RESOLUTION_BITRATE,
    ADAP_DYNAMIC_QUALITY_BITRATE_RESOLUTION,
};

typedef struct VideoResolution {
    uint32_t height;
    uint32_t width;
} VideoResolution;

typedef struct DeviceResolution {
    uint32_t height;
    uint32_t width;
} DeviceResolution;

typedef struct VideoProfile {
    // unique id used to identify each video profile
    uint32_t representation_id;
    // average bitrate in kbps
    uint32_t avg_bitrate_kbps;
    // max bitrate in kbps
    uint32_t max_bitrate_kbps;
    VideoResolution video_resolution;
    float quality;
    int download_len;
    uint32_t id;
    int64_t cached_bytes;
    uint32_t equivalent_bitrate_kbps;
} VideoProfile;

typedef struct AdaptionProfile {
    std::vector<VideoProfile> video_profiles;
    NetworkType net_type;
    DeviceResolution device_resolution;
    int adaptation_id;
    int32_t low_device = 0;
    int32_t switch_code = 0;
    int32_t algorithm_mode; // 0: 线上方式， 1: A1方式
} AdaptionProfile;

typedef struct DownloadSampleInfo {
    // timestamp in ms just before downloading began
    uint64_t begin_timestamp;
    // timestamp in ms just after downloading ended
    uint64_t end_timestamp;
    // total downloaded bytes between the time interval
    uint64_t total_bytes;
} DownloadSampleInfo;

enum BandwidthEstimationAlgoType {
    // simple moving average algorithm to estimate the bandwidth
    kMovingAverage,
};

enum VideoAdaptationAlgoType {
    // choose profile based on bandwidth
    kBandwidthBased,
};

const uint32_t kMinTransmittedSize = 10000;
const uint32_t kMinTransmittedTime = 5;

typedef struct RateAdaptConfig {
    VideoAdaptationAlgoType rate_addapt_type;
    BandwidthEstimationAlgoType bandwidth_estimation_type;
    uint32_t absolute_low_res_low_device;
    uint32_t adapt_under_4G;
    uint32_t adapt_under_wifi;
    uint32_t adapt_under_other_net;
    uint32_t absolute_low_rate_4G;
    uint32_t absolute_low_rate_wifi;
    uint32_t absolute_low_res_4G;
    uint32_t absolute_low_res_wifi;
    uint32_t short_keep_interval;
    uint32_t long_keep_interval;
    uint32_t bitrate_init_level;
    float default_weight;
    uint32_t block_affected_interval;
    float wifi_amend;
    float fourG_amend;
    float resolution_amend;
    uint32_t device_width_threshold;
    uint32_t device_hight_threshold;
    uint32_t priority_policy;
} RateAdaptConfig;

typedef struct RateAdaptConfigA1 {
    uint32_t short_keep_interval;
    uint32_t long_keep_interval;
    uint32_t bitrate_init_level;
    uint32_t max_resolution;
} RateAdaptConfigA1;

typedef struct HisState {
    uint32_t net_type;
    uint64_t update_time;
    uint32_t video_width;
    uint32_t video_height;
    uint32_t avg_video_bitrate;
    uint32_t max_video_bitrate;
    uint32_t short_term_throughput;
    uint32_t long_term_throughput;
} HisState;

}
}
