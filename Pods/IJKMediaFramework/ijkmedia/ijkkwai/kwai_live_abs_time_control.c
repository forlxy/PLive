//
// Created by liuyuxin on 2018/3/7.
//

#include "kwai_live_abs_time_control.h"
#include "ijkplayer/ff_ffplay_def.h"

#define LIVE_CONTROL_ENABLE_THRESHOLD_MS     500   // 0.5 second
#define LIVE_CONTROL_HIGH_SPEED_UP_ADJUST_THRESHOLD_MS   2000
#define LIVE_CONTROL_MAX_WALL_CLOCK_DEVIATION_MS   (-2000)  // 2 second


static int LiveAbsTimeControl_check_wall_clock_deviation(int64_t wall_clock_offset, int64_t abs_time) {
    int64_t now_ms = av_gettime() / 1000;
    int delay = (int)(now_ms - wall_clock_offset - abs_time);

    if (delay < LIVE_CONTROL_MAX_WALL_CLOCK_DEVIATION_MS) {
        ALOGE("[%s:%d] now_ms: %lld, wall_clock_offset: %d, abs_time: %lld, delay: %d\n",
              __FUNCTION__, __LINE__, now_ms, wall_clock_offset, abs_time, delay);
        return -1;
    }
    return 0;
}

int LiveAbsTimeControl_init(LiveAbsTimeControl* lc) {
    lc->max_wall_clock_offset_ms = -1;
    lc->adjusting = 0;

    lc->cur_live_abs_time      = 0;
    lc->last_live_abs_time     = -1;
    lc->last_sys_time          = -1;
    return 0;
}

int64_t LiveAbsTimeControl_cur_abs_time(LiveAbsTimeControl* lc) {
    return lc->cur_live_abs_time;
}

int64_t LiveAbsTimeControl_update_abs_time(LiveAbsTimeControl* lc, int64_t abs_pts) {
    if (abs_pts > 0) {
        lc->cur_live_abs_time = abs_pts;
        lc->last_live_abs_time = abs_pts;
        lc->last_sys_time = av_gettime_relative() / 1000;
    } else if (lc->last_live_abs_time != -1) {
        int64_t duration = av_gettime_relative() / 1000 - lc->last_sys_time;
        lc->cur_live_abs_time = lc->last_live_abs_time + duration;
    }
    return 0;
}

int LiveAbsTimeControl_set(LiveAbsTimeControl* lc, int64_t max_wall_clock_offset_ms) {
    lc->max_wall_clock_offset_ms = max_wall_clock_offset_ms;
    return 0;
}

int LiveAbsTimeControl_is_enable(LiveAbsTimeControl* lc) {
    return lc->adjusting;
}

void LiveAbsTimeControl_control(FFPlayer* ffp, int64_t abs_time, int audio_buf_thr) {
    VideoState* is = ffp->is;
    LiveAbsTimeControl* lc = &ffp->live_abs_time_control;
    int64_t localtime = 0;
    int64_t diff, abs_diff;

    if (lc->max_wall_clock_offset_ms <= 0
        || LiveAbsTimeControl_check_wall_clock_deviation(ffp->wall_clock_offset, abs_time) < 0) {
        if (lc->adjusting) {
            is->audio_speed_percent = KS_AUDIO_PLAY_SPEED_NORMAL;
            lc->adjusting = 0;
        }
        return;
    }

    if (lc->max_wall_clock_offset_ms > is->i_buffer_time_max) {
        av_log(NULL, AV_LOG_ERROR, "[%s:%d] i_buffer_time_max=%d, ffp->i_buffer_time_max=%d, max_wall_clock_offset_ms=%lld\n",
               __FUNCTION__, __LINE__, is->i_buffer_time_max, ffp->i_buffer_time_max, lc->max_wall_clock_offset_ms);
        is->i_buffer_time_max = (int)lc->max_wall_clock_offset_ms;
        ffp->i_buffer_time_max = (int)lc->max_wall_clock_offset_ms;
    }

    localtime = av_gettime() / 1000;
    diff = localtime - ffp->wall_clock_offset - abs_time - lc->max_wall_clock_offset_ms;
    abs_diff = llabs(diff);

    if (abs_diff > LIVE_CONTROL_ENABLE_THRESHOLD_MS) {
        if (diff < 0) {
            is->audio_speed_percent = KS_AUDIO_PLAY_SPEED_DOWN;
            lc->adjusting = 1;
        } else if (ffp->stat.audio_cache.duration > audio_buf_thr) {
            if (abs_diff < LIVE_CONTROL_HIGH_SPEED_UP_ADJUST_THRESHOLD_MS) {
                is->audio_speed_percent = KS_AUDIO_PLAY_SPEED_UP_1;
            } else {
                is->audio_speed_percent = KS_AUDIO_PLAY_SPEED_UP_2;
            }
            lc->adjusting = 1;
        }
    } else if (lc->adjusting) {
        is->audio_speed_percent = KS_AUDIO_PLAY_SPEED_NORMAL;
        lc->adjusting = 0;
    }

    return;
}
