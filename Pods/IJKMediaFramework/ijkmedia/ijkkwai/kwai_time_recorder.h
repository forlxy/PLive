//
// Created by 帅龙成 on 13/03/2018.
//

#ifndef IJKPLAYER_KWAI_TIME_RECORDER_H
#define IJKPLAYER_KWAI_TIME_RECORDER_H

#include <stdint.h>

typedef struct TimeRecoder {
    int64_t sum_ms;
    int64_t last_start_ts_ms;
} TimeRecoder;

void TimeRecorder_init(TimeRecoder* recorder);
/**
 * 分片开始计时
 * @param recorder timer
 */
void TimeRecoder_start(TimeRecoder* recorder);

/**
 * 分片计时结束
 * @param recorder timer
 */
void TimeRecorder_end(TimeRecoder* recorder);

int64_t TimeRecoder_get_total_duration_ms(TimeRecoder* recorder);

int64_t TimeRecoder_get_last_start_ts_ms(TimeRecoder* recorder);

void TimeRecoder_copy_end(TimeRecoder* src_recorder, TimeRecoder* dst_recorder);

#endif //IJKPLAYER_KWAI_TIME_RECORDER_H
