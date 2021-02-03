#pragma once
#include <string>
#include "constant.h"

namespace kuaishou {
namespace cache {

struct OfflienCacheDataSpec {
    std::string url;
    DataSourceType cache_mode;
    int64_t pos;
    int64_t len;
    int64_t durMs;
    std::string host;
    std::string key;
    int32_t connect_timeout_ms;
    int32_t read_timeout_ms;
    int32_t socket_buf_size_kb;
    int max_speed_kbps;
    bool enableLimitSpeedWhenCancel;
    int32_t band_width_threshold;
};

struct OfflienCacheVodAdaptiveInit {
    std::string rate_config;
    int32_t dev_res_width;
    int32_t dev_res_heigh;
    int32_t net_type;
    int32_t low_device;
    int32_t signal_strength;
};
}
}