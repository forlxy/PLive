//
// Created by 帅龙成 on 13/03/2018.
//

#include <string.h>
#include <libavutil/time.h>
#include "kwai_time_recorder.h"



inline static int64_t get_current_time_ms() {
    return av_gettime_relative() / 1000;
}

void TimeRecorder_init(TimeRecoder* recorder) {
    if (recorder) {
        memset(recorder, 0, sizeof(TimeRecoder));
    }
}

/**
 * 分片开始计时，如果连续调用，只从第一次调用时开始计时
 * @param recorder 计时时间
 */
void TimeRecoder_start(TimeRecoder* recorder) {
    if (recorder->last_start_ts_ms == 0) {
        recorder->last_start_ts_ms = get_current_time_ms();
    }
}

/**
 * 分片计时结束
 * @param recorder 计时时间
 */
void TimeRecorder_end(TimeRecoder* recorder) {
    int64_t now = get_current_time_ms();
    if (recorder->last_start_ts_ms != 0 && now >= recorder->last_start_ts_ms) {
        recorder->sum_ms += (now - recorder->last_start_ts_ms);
    }
    recorder->last_start_ts_ms = 0;
}


int64_t TimeRecoder_get_total_duration_ms(TimeRecoder* recorder) {
    if (recorder->last_start_ts_ms == 0) {
        // not in process of timing
        return recorder->sum_ms;
    } else {
        int64_t now = get_current_time_ms();
        if (now >= recorder->last_start_ts_ms) {
            return recorder->sum_ms + (now - recorder->last_start_ts_ms);
        } else {
            return recorder->sum_ms;
        }
    }
}

int64_t TimeRecoder_get_last_start_ts_ms(TimeRecoder* recorder) {
    return recorder->last_start_ts_ms;
}

/**
 * 用src_recorder的开始时间作为dst_recorder的开始时间，计算分片时间
 * @param src_recorder 源开始时间
 * @param dst_recorder 目的开始时间
 */
void TimeRecoder_copy_end(TimeRecoder* src_recorder, TimeRecoder* dst_recorder) {
    dst_recorder->last_start_ts_ms = src_recorder->last_start_ts_ms;
    TimeRecorder_end(dst_recorder);
}

