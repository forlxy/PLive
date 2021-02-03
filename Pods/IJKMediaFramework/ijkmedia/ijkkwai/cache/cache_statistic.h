//
// Created by MarshallShuai on 2018/10/31.
//
#pragma once

#include <stdint.h>
#include "awesome_cache_runtime_info_c.h"

typedef struct {
    int adapter_error;
    const char* error_msg;
    int read_cost_ms;

    int seek_size_cnt;
    int seek_set_cnt;
    int seek_cur_cnt;
    int seek_end_cnt;

    // 从ffmpeg adapter输出的总字节数
    int64_t total_read_bytes;
} FfmpegAdapterQos;
void FfmpegAdapterQos_init(FfmpegAdapterQos* qos);
void FfmpegAdapterQos_release(FfmpegAdapterQos* qos);

/**
 * 这个类负责记录一个Cache的生命周期中的 config/统计数据
 */
typedef struct {
    FfmpegAdapterQos ffmpeg_adapter_qos;
    AwesomeCacheRuntimeInfo ac_runtime_info;
} CacheStatistic;

void CacheStatistic_init(CacheStatistic* self);
void CacheStatistic_release(CacheStatistic* self);
