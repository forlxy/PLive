/*
 * ff_buffer_strategy.h
 * 这个类主要控制了两个方面：
 * 1.read_thread里的最大缓冲长度
 * 2.当遇到卡顿的时候，缓冲多少内容后才能恢复播放(high/low water mark)
 */

#ifndef FFPLAY__FF_STRATEGY_H
#define FFPLAY__FF_STRATEGY_H

#include <stdint.h>
#include <stdbool.h>
#include <ijkmedia/ijksdl/ijksdl_log.h>
#include <libavutil/time.h>


#define MIN_QUEUE_DUR_BSP_MS   (1 * 1000)  // 开播前最小缓冲的buffer dur， only for vod for now

#define DEFAULT_MIN_FRAMES  50000
#define DEFAULT_QUEUE_DUR_MS   (2 * 60 * 1000)  // 2min
#define MAX_QUEUE_SIZE (15 * 1024 * 1024)
#define MAX_QUEUE_DUR_MS       (60 * 60 * 1000)  // 1hour

typedef enum VodMaxBufStrategy {
    MaxBufStrategy_None = 0,
    MaxBufStrategy_BeforeStartPlayControl = 1,
    MaxBufStrategy_ProgressToMax = 2,   // max buffer duration that progressively increase after start play
} MaxBufStrategy;
// 每隔 INCREASE_INTERVAL 秒增大 INCREASE_STEP 秒
#define MAX_BUF_STRATEGY_PROGRESS_INCREASE_INTERVAL_MS 1000
#define MAX_BUF_STRATEGY_PROGRESS_INCREASE_STEP_MS 5000

#define DEFAULT_HIGH_WATER_MARK_IN_BYTES        (256 * 1024)

/*
 * START: buffering after prepared/seeked
 * NEXT:  buffering for the second time after START
 * MAX:   ...
 */
#define DEFAULT_FIRST_HIGH_WATER_MARK_IN_MS     (100)
#define DEFAULT_NEXT_HIGH_WATER_MARK_IN_MS      (1 * 1000)
#define DEFAULT_LAST_HIGH_WATER_MARK_IN_MS      (5 * 1000)

#define DEFAULT_BUFFER_INCREMENT_STEP               (-1)
#define DEFAULT_BUFFER_STRATEGY_OLD_INCREMENT_STEP  (100)
#define DEFAULT_BUFFER_STRATEGY_NEW_INCREMENT_STEP  (500)
#define MIN_BUFFER_INCREMENT_STEP                   (-1)
#define MAX_BUFFER_INCREMENT_STEP                   (5000)

#define DEFAULT_BUFFER_SMOOTH_TIME              (20 * 1000)
#define MIN_BUFFER_SMOOTH_TIME                  (1 * 1000)
#define MAX_BUFFER_SMOOTH_TIME                  (60 * 1000)
#define DEFAULT_BUFFER_DECLINE_RATE             (0.875)

#define BUFFER_STRATEGY_OLD 1
#define BUFFER_STRATEGY_NEW 2

typedef struct FFDemuxCacheControl {
    int min_frames;
    int max_buffer_size;
    int max_buffer_dur_ms;
    int max_buffer_dur_bsp_ms; // 开播前使用更小的start_play_buffer可以平衡开播性能(bsp:before start play)
    int high_water_mark_in_bytes;

    // 目前仅vod会使用
    MaxBufStrategy max_buf_dur_strategy;
    int64_t max_buf_bsp_update_last_ts_ms;
    bool max_buf_strategy_finished;

    int first_high_water_mark_in_ms;
    int next_high_water_mark_in_ms;
    int last_high_water_mark_in_ms;
    int current_high_water_mark_in_ms;

    int buffer_strategy;    // live下才用到的water mark相关的strategy
    int buffer_increment_step;
    int buffer_smooth_time;
    int64_t last_buffering_end_time;
} FFDemuxCacheControl;

// 打开FFP_SHOW_HWM， FFDemuxCacheControl_print 才会生效
// #define FFP_SHOW_HWM
void FFDemuxCacheControl_print(FFDemuxCacheControl* dcc);

void FFDemuxCacheControl_increase_buffer_time_live(FFDemuxCacheControl* dcc, int hwm_in_ms);
void FFDemuxCacheControl_decrease_buffer_time_live(FFDemuxCacheControl* dcc);

void FFDemuxCacheControl_increase_buffer_time_vod(FFDemuxCacheControl* dcc, int hwm_in_ms);

#ifndef MIN
#define MIN(a, b) (a > b ? b : a)
#endif

static inline int FFDemuxCacheControl_current_max_buffer_dur_ms(FFDemuxCacheControl* dcc) {
    if (dcc->max_buf_dur_strategy == MaxBufStrategy_None) {
        return dcc->max_buffer_dur_ms;
    } else {
        return MIN(dcc->max_buffer_dur_ms, dcc->max_buffer_dur_bsp_ms);
    }
}

/**
 * 通知FFDemuxCacheControl已经开播了，以方便做一些FFDemuxCacheControl的内部动态调节
 * @param dcc FFDemuxCacheControl
 */
static inline void FFDemuxCacheControl_on_av_rendered(FFDemuxCacheControl* dcc) {
    if (dcc->max_buf_dur_strategy == MaxBufStrategy_None || dcc->max_buf_strategy_finished) {
        return;
    }

    if (dcc->max_buf_dur_strategy == MaxBufStrategy_ProgressToMax) {

        int64_t cur_ms = av_gettime_relative() / 1000;
        // 每1秒递增10秒
        if (cur_ms - dcc->max_buf_bsp_update_last_ts_ms > MAX_BUF_STRATEGY_PROGRESS_INCREASE_INTERVAL_MS) {
            dcc->max_buffer_dur_bsp_ms += MAX_BUF_STRATEGY_PROGRESS_INCREASE_STEP_MS;
            dcc->max_buf_bsp_update_last_ts_ms = cur_ms;
            if (dcc->max_buffer_dur_bsp_ms >= dcc->max_buffer_dur_ms) {
                dcc->max_buffer_dur_bsp_ms = dcc->max_buffer_dur_ms;
                dcc->max_buf_strategy_finished = true;
            }
        }

        return;
    } else if (dcc->max_buf_dur_strategy == MaxBufStrategy_BeforeStartPlayControl) {
        dcc->max_buf_strategy_finished = true;
        dcc->max_buffer_dur_bsp_ms = dcc->max_buffer_dur_ms;
        return;
    } else {
        ALOGE("[%s], invalid stratege:%d", __func__, dcc->max_buf_dur_strategy);
    }

    // 开播后max_buffer_dur_bsp_ms就置为和max_buffer_dur_ms一致
}


inline static void FFDemuxCacheControl_reset(FFDemuxCacheControl* dcc) {
    dcc->min_frames                = DEFAULT_MIN_FRAMES;
    dcc->max_buffer_size           = MAX_QUEUE_SIZE;
    dcc->max_buffer_dur_ms         = DEFAULT_QUEUE_DUR_MS;
    dcc->max_buffer_dur_bsp_ms     = DEFAULT_QUEUE_DUR_MS;
    dcc->high_water_mark_in_bytes  = DEFAULT_HIGH_WATER_MARK_IN_BYTES;

    dcc->max_buf_dur_strategy  = MaxBufStrategy_None;
    dcc->max_buf_strategy_finished = false;
    dcc->max_buf_bsp_update_last_ts_ms = 0;

    dcc->first_high_water_mark_in_ms    = DEFAULT_FIRST_HIGH_WATER_MARK_IN_MS;
    dcc->next_high_water_mark_in_ms     = DEFAULT_NEXT_HIGH_WATER_MARK_IN_MS;
    dcc->last_high_water_mark_in_ms     = DEFAULT_LAST_HIGH_WATER_MARK_IN_MS;
    dcc->current_high_water_mark_in_ms  = DEFAULT_FIRST_HIGH_WATER_MARK_IN_MS;

    dcc->buffer_strategy                = BUFFER_STRATEGY_OLD;
    dcc->buffer_increment_step          = DEFAULT_BUFFER_STRATEGY_OLD_INCREMENT_STEP;
    dcc->buffer_smooth_time             = DEFAULT_BUFFER_SMOOTH_TIME;
}

#endif
