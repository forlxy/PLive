#include "ff_buffer_strategy.h"

#include "ijksdl_timer.h"

/**
 * @file
 * buffer strategy
 */
// FIXME 这块实现应该是在实现buffer strategy过程中抽象函数时候改的和原逻辑不一样了，
// 现在对live仍然采用当前逻辑，以后有空再单独研究这块的影响,这一块原来是做过AB test的
static void buffer_time_limit_live(FFDemuxCacheControl* dcc) {
    int hwm_in_ms = dcc->current_high_water_mark_in_ms;
    if (hwm_in_ms < dcc->next_high_water_mark_in_ms) {
        hwm_in_ms = dcc->next_high_water_mark_in_ms;
    } else if (hwm_in_ms > dcc->last_high_water_mark_in_ms) {
        hwm_in_ms = dcc->last_high_water_mark_in_ms;
    }
    dcc->current_high_water_mark_in_ms = hwm_in_ms;
}

void FFDemuxCacheControl_increase_buffer_time_live(FFDemuxCacheControl* dcc, int hwm_in_ms) {
    if (dcc->buffer_strategy == BUFFER_STRATEGY_OLD) {
        if (dcc->buffer_increment_step == DEFAULT_BUFFER_INCREMENT_STEP) {
            dcc->buffer_increment_step = DEFAULT_BUFFER_STRATEGY_OLD_INCREMENT_STEP;
        }
        hwm_in_ms += dcc->buffer_increment_step;
    } else {
        if (dcc->buffer_increment_step == DEFAULT_BUFFER_INCREMENT_STEP) {
            dcc->buffer_increment_step = DEFAULT_BUFFER_STRATEGY_NEW_INCREMENT_STEP;
        }
        if (hwm_in_ms >= 0 && hwm_in_ms < 1000) {
            hwm_in_ms += dcc->buffer_increment_step;
        } else if (hwm_in_ms >= 1000 && hwm_in_ms < 2000) {
            hwm_in_ms += dcc->buffer_increment_step * 0.8;
        } else if (hwm_in_ms >= 2000 && hwm_in_ms < 3000) {
            hwm_in_ms += dcc->buffer_increment_step * 0.6;
        } else if (hwm_in_ms >= 3000 && hwm_in_ms < 4000) {
            hwm_in_ms += dcc->buffer_increment_step * 0.4;
        } else if (hwm_in_ms >= 4000 && hwm_in_ms < 5000) {
            hwm_in_ms += dcc->buffer_increment_step * 0.2;
        }
    }
    dcc->current_high_water_mark_in_ms = hwm_in_ms;
    buffer_time_limit_live(dcc);
}

void FFDemuxCacheControl_decrease_buffer_time_live(FFDemuxCacheControl* dcc) {
    if (dcc->buffer_strategy != BUFFER_STRATEGY_OLD) {
        int64_t curr_time = SDL_GetTickHR();
        if (dcc->last_buffering_end_time != 0
            && curr_time - dcc->last_buffering_end_time >= dcc->buffer_smooth_time) {
            dcc->current_high_water_mark_in_ms *= DEFAULT_BUFFER_DECLINE_RATE;
            buffer_time_limit_live(dcc);
        }
    }
}

void FFDemuxCacheControl_print(FFDemuxCacheControl* dcc) {
#ifdef FFP_SHOW_HWM
    av_log(ffp, AV_LOG_DEBUG, "buffer_strategy: %d\n", ffp->dcc.buffer_strategy);
    av_log(ffp, AV_LOG_DEBUG, "first_high_water_mark_in_ms: %d\n", ffp->dcc.first_high_water_mark_in_ms);
    av_log(ffp, AV_LOG_DEBUG, "next_high_water_mark_in_ms: %d\n", ffp->dcc.next_high_water_mark_in_ms);
    av_log(ffp, AV_LOG_DEBUG, "last_high_water_mark_in_ms: %d\n", ffp->dcc.last_high_water_mark_in_ms);
    av_log(ffp, AV_LOG_DEBUG, "current_high_water_mark_in_ms: %d\n", ffp->dcc.current_high_water_mark_in_ms);
    av_log(ffp, AV_LOG_DEBUG, "buffer_increment_step: %d\n", ffp->dcc.buffer_increment_step);
    av_log(ffp, AV_LOG_DEBUG, "buffer_smooth_time: %d\n", ffp->dcc.buffer_smooth_time);
#endif
}


void FFDemuxCacheControl_increase_buffer_time_vod(FFDemuxCacheControl* dcc, int hwm_in_ms) {
    if (hwm_in_ms < dcc->next_high_water_mark_in_ms) {
        hwm_in_ms = dcc->next_high_water_mark_in_ms;
    } else {
        hwm_in_ms *= 2;
    }

    if (hwm_in_ms > dcc->last_high_water_mark_in_ms)
        hwm_in_ms = dcc->last_high_water_mark_in_ms;

    dcc->current_high_water_mark_in_ms = hwm_in_ms;
}

