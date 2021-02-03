//
// Created by MarshallShuai on 2019/4/24.
//

#include "kwai_live_delay_stat.h"

#include <libavkwai/cJSON.h>
#include <stdio.h>
#include <assert.h>

#include "ff_ffplay_def.h"

cJSON* DelayStat_gen_live_delay_json(DelayStat* delay_stat) {
    if (!delay_stat) {
        ALOGE("%s: delay_stat null, return\n", __FUNCTION__);
        return NULL;
    }

    static char metric_names[DELAY_STAT_DISTR_DURA_NUM][JSON_NAME_MAX_LEN] = {0};
    static int metric_names_got = 0;
    if (0 == metric_names_got) {
        int i = 0;
        snprintf(metric_names[i++], JSON_NAME_MAX_LEN, "%d~%d", DELAY_STAT_DISTR_DURA_THR_0,
                 DELAY_STAT_DISTR_DURA_THR_0);
        snprintf(metric_names[i++], JSON_NAME_MAX_LEN, "%d~%d", DELAY_STAT_DISTR_DURA_THR_0,
                 DELAY_STAT_DISTR_DURA_THR_1 / 1000);
        snprintf(metric_names[i++], JSON_NAME_MAX_LEN, "%d~%d", DELAY_STAT_DISTR_DURA_THR_1 / 1000,
                 DELAY_STAT_DISTR_DURA_THR_2 / 1000);
        snprintf(metric_names[i++], JSON_NAME_MAX_LEN, "%d~%d", DELAY_STAT_DISTR_DURA_THR_2 / 1000,
                 DELAY_STAT_DISTR_DURA_THR_3 / 1000);
        snprintf(metric_names[i++], JSON_NAME_MAX_LEN, "%d~%d", DELAY_STAT_DISTR_DURA_THR_3 / 1000,
                 DELAY_STAT_DISTR_DURA_THR_4 / 1000);
        snprintf(metric_names[i++], JSON_NAME_MAX_LEN, "%d+", DELAY_STAT_DISTR_DURA_THR_4 / 1000);
        assert(i <= DELAY_STAT_DISTR_DURA_NUM);
        metric_names_got = 1;
    }

    cJSON* delay = cJSON_CreateObject();
    {
        cJSON_AddNumberToObject(delay, "avg", delay_stat->total_avg);
        cJSON* metric = cJSON_CreateObject();
        {
            for (int i = 0; i < DELAY_STAT_DISTR_DURA_NUM; ++i) {
                cJSON_AddNumberToObject(metric, metric_names[i],
                                        delay_stat->distributed_duration[i]);
            }
        }
        cJSON_AddItemToObject(delay, "metric", metric);
    }
    return delay;
}


// QosInfo: delay
void DelayStat_calc_pts_delay(DelayStat* delay_stat, int64_t wall_clock_offset, int64_t pts_offset, int64_t pts) {
    if (!delay_stat) {
        ALOGE("%s: delay_stat null, return\n", __FUNCTION__);
        return;
    }

    int64_t now_ms = av_gettime() / 1000;
    int delay = (int)((now_ms - wall_clock_offset) - (pts + pts_offset));

    delay_stat->period_sum += delay;
    ++delay_stat->period_count;

    if (0 == delay_stat->period_last_calc_time) {
        delay_stat->period_last_calc_time = now_ms;
    } else if (now_ms >= delay_stat->period_last_calc_time + DELAY_STAT_PERIOD_MS) {
        delay_stat->period_avg = delay_stat->period_sum / delay_stat->period_count;
        delay_stat->period_sum = 0;
        delay_stat->period_count = 0;
        delay_stat->period_last_calc_time = now_ms;
    }

    delay_stat->total_sum += delay;
    ++delay_stat->total_count;
    delay_stat->total_avg = (int)(delay_stat->total_sum / delay_stat->total_count);

    if (0 == delay_stat->total_last_calc_time) {
        delay_stat->total_last_calc_time = now_ms;
    } else {
        int duration = (int)(now_ms - delay_stat->total_last_calc_time);
        delay_stat->total_last_calc_time = now_ms;
        if (delay <= DELAY_STAT_DISTR_DURA_THR_0) {
            delay_stat->distributed_duration[0] += duration;
        } else if (delay <= DELAY_STAT_DISTR_DURA_THR_1) {
            delay_stat->distributed_duration[1] += duration;
        } else if (delay <= DELAY_STAT_DISTR_DURA_THR_2) {
            delay_stat->distributed_duration[2] += duration;
        } else if (delay <= DELAY_STAT_DISTR_DURA_THR_3) {
            delay_stat->distributed_duration[3] += duration;
        } else if (delay <= DELAY_STAT_DISTR_DURA_THR_4) {
            delay_stat->distributed_duration[4] += duration;
        } else {
            delay_stat->distributed_duration[5] += duration;
        }
    }
}
