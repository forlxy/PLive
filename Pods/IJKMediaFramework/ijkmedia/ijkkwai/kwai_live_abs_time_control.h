//
// Created by liuyuxin on 2018/3/7.
//

#ifndef IJKPLAYER_KWAI_LIVE_ABS_TIME_CONTROL_H
#define IJKPLAYER_KWAI_LIVE_ABS_TIME_CONTROL_H

#include <stdint.h>

typedef struct FFPlayer FFPlayer;

typedef struct LiveAbsTimeControl {
    int adjusting;
    int64_t max_wall_clock_offset_ms;

    int64_t cur_live_abs_time;
    int64_t last_live_abs_time;
    int64_t last_sys_time;
} LiveAbsTimeControl;

int LiveAbsTimeControl_init(LiveAbsTimeControl* lc);
int LiveAbsTimeControl_set(LiveAbsTimeControl* lc, int64_t max_wall_clock_offset_ms);
int LiveAbsTimeControl_is_enable(LiveAbsTimeControl* lc);
void LiveAbsTimeControl_control(FFPlayer* ffp, int64_t abs_time, int audio_buf_thr);

int64_t LiveAbsTimeControl_cur_abs_time(LiveAbsTimeControl* lc);
int64_t LiveAbsTimeControl_update_abs_time(LiveAbsTimeControl* lc, int64_t abs_pts);

#endif //IJKPLAYER_KWAI_LIVE_ABS_TIME_CONTROL_H
