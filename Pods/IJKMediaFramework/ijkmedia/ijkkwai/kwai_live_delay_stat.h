//
// Created by MarshallShuai on 2019/4/24.
// 直播中延迟统计相关的逻辑
//

#pragma once

#include <stdint.h>
#include <libavkwai/cJSON.h>

#define JSON_NAME_MAX_LEN 64

#define DELAY_STAT_PERIOD_MS 1000
#define DELAY_STAT_DISTR_DURA_NUM 6
#define DELAY_STAT_DISTR_DURA_THR_0 0
#define DELAY_STAT_DISTR_DURA_THR_1 3000
#define DELAY_STAT_DISTR_DURA_THR_2 8000
#define DELAY_STAT_DISTR_DURA_THR_3 15000
#define DELAY_STAT_DISTR_DURA_THR_4 25000
typedef struct DelayStat_t {
    int64_t period_last_calc_time;
    int period_sum;
    int period_count;
    int period_avg;
    int64_t total_last_calc_time;
    int64_t total_sum;
    int total_count;
    int total_avg;
    int distributed_duration[DELAY_STAT_DISTR_DURA_NUM];
} DelayStat;

cJSON* DelayStat_gen_live_delay_json(DelayStat* delay_stat);

void DelayStat_calc_pts_delay(DelayStat* delay_stat, int64_t wall_clock_offset,
                              int64_t pts_offset, int64_t pts);