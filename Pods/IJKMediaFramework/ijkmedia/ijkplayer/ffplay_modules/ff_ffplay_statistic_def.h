//
// Created by MarshallShuai on 2019/4/24.
// 官方代码里的statistic，别的统计数据别往这个文件加了
//
#pragma once

#include <stdint.h>

typedef struct FFTrackCacheStatistic {
    int64_t duration;
    int64_t bytes;
    int64_t packets;
} FFTrackCacheStatistic;

typedef struct FFStatistic {
    int64_t vdec_type;

    float vfps;
    float vdps;
    float vrps;
    float avdelay;
    float avdiff;
    //实时上报，该值为正数，audio超前video的diff最大值
    float maxAvDiffRealTime;
    //实时上报，该值为负数，video超前audio的diff最大值
    float minAvDiffRealTime;
    //汇总上报，该值为正数，audio超前video的diff最大值
    float maxAvDiffTotalTime;
    //汇总上报，该值为负数，video超前audio的diff最大值
    float minAvDiffTotalTime;
    int64_t   bit_rate;
    //连续读取dts的diff最大值
    int64_t max_video_dts_diff_ms;
    int64_t max_audio_dts_diff_ms;

    int32_t speed_changed_cnt;  //倍速切换次数

    FFTrackCacheStatistic video_cache;
    FFTrackCacheStatistic audio_cache;

    int64_t latest_seek_load_duration;
    int64_t byte_count;
} FFStatistic;
