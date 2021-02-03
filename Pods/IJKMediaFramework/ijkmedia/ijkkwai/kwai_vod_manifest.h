//
// Created by liuyuxin on 2018/4/2.
//

#ifndef IJKPLAYER_KWAI_VOD_MANIFEST_H
#define IJKPLAYER_KWAI_VOD_MANIFEST_H

#include <stdint.h>

#define MAX_INFO_SIZE  4096
#define MAX_REPRSENTATION_COUNT  16

typedef struct AVDictionary AVDictionary;
typedef struct _KwaiQos KwaiQos;

enum NetworkType {
    UNKNOW,
    WIFI,
    FOUR_G,
    THREE_G,
    TWO_G,
};

typedef struct VodRateAdaptConfig {
    uint32_t rate_addapt_type;
    uint32_t bandwidth_estimation_type;
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
} VodRateAdaptConfig;

typedef struct VodRateAdaptConfigA1 {
    uint32_t short_keep_interval;
    uint32_t long_keep_interval;
    uint32_t bitrate_init_level;
    uint32_t max_resolution;
} VodRateAdaptConfigA1;

typedef struct VideoResolution {
    uint32_t height;
    uint32_t width;
} VideoResolution;

typedef struct DeviceResolution {
    uint32_t height;
    uint32_t width;
} DeviceResolution;

typedef struct Representation {
    char url[MAX_INFO_SIZE + 1];
    char host[MAX_INFO_SIZE + 1];
    char key[MAX_INFO_SIZE + 1];
    char quality_show[MAX_INFO_SIZE + 1];
    int feature_p2sp;  // 表示这个url是否可以走p2sp。打开后，将会使用p2sp upstream type
    uint32_t representation_id;     // unique id used to identify each video profile
    uint32_t max_bitrate_kbps;      // max bitrate in kbps
    uint32_t avg_bitrate_kbps;      // average bitrate in kbps
    float quality;
    uint32_t id;
    VideoResolution video_resolution;
    int64_t cached_bytes;
} Representation;

typedef struct VodPlayList {
    // param from manifest
    int adaptation_id;
    int duration;
    int rep_count;
    Representation rep[MAX_REPRSENTATION_COUNT];
    // param from app
    enum NetworkType net_type;
    DeviceResolution device_resolution;
    int cached;
    int32_t low_device;
    int32_t signal_strength;
    int32_t switch_code;
    // for A1
    int32_t algorithm_mode; // 0: 线上方式， 1: A1方式
} VodPlayList;

int kwai_vod_manifest_init(KwaiQos* qos, AVDictionary* fmt_options, VodPlayList* playlist, const char* file_name);


#endif //IJKPLAYER_KWAI_VOD_MANIFEST_H
