//
//  kwai_packet_queue_strategy.c
//  IJKMediaFramework
//
//  Created by MarshallShuai on 2018/9/29.
//  Copyright © 2018 kuaishou. All rights reserved.
//

#include "kwai_packet_queue_strategy.h"
#include "ff_packet_queue.h"
#include "ff_ffplay_def.h"
#include "kwai_qos.h"
#include "ijkplayer/ffplay_modules/ff_ffplay_internal.h"

static const bool VERBOSE = false;
static const bool APP_DEBUG = false;

static const char* TAG = "KwaiPacketQueueBufferChecker";


inline static  int64_t current_ts_ms() {
    return av_gettime_relative() / 1000;
}


// 对直播/点播都有效
static bool
packet_queue_check_need_buffering(KwaiPacketQueueBufferChecker* ck, FFplayer* ffp, PacketQueue* q) {
#define SAMPLE_BUFFERING_MIN_SIZE 2
#define VIDEO_PICTURE_BUFFERING_MIN_SIZE 1

    bool ret = (ffp->is->audio_stream >= 0
                && q == &(ffp->is->audioq)
                && (frame_queue_nb_remaining(&ffp->is->sampq) <= SAMPLE_BUFFERING_MIN_SIZE)
                && (ffp->first_audio_frame_rendered > 0)
                && (!ffp->audio_render_after_seek_need_notify && !ffp->is->seek_req))
               ||
               (ffp->is->audio_stream < 0
                && q == &(ffp->is->videoq)
                &&
                (frame_queue_nb_remaining(&ffp->is->pictq) <= VIDEO_PICTURE_BUFFERING_MIN_SIZE)
                && (ffp->first_video_frame_rendered > 0)
                && (!ffp->video_rendered_after_seek_need_notify && !ffp->is->seek_req));
    return ret;
}


/**
 *  随时可以起播
 **/
static bool strategy_can_start_play_anytime(KwaiPacketQueueBufferChecker* ck, FFplayer* ffp) {
    if (ck->play_started) {
        return true;
    } else {
        ck->play_started = true;
        ALOGD("[%s] [%s] \n", TAG, __func__);
        if (ck->self_life_cycle_start_ts_ms > 0) {
            ck->self_life_cycle_cost_ms = current_ts_ms() - ck->self_life_cycle_start_ts_ms;
        } else {
            ck->self_life_cycle_cost_ms = 0;
        }
        KwaiQos_onReadyToRender(&ffp->kwai_qos);

        return true;
    }
}

/**
 *  packet_queue要攒够一定时长的数据才能起播
 **/
static bool
strategy_has_enough_buffer_time_to_start_play(KwaiPacketQueueBufferChecker* ck, FFplayer* ffp) {
    if (!ck->ready || !ck->self_life_cycle_started) {
        if (VERBOSE) {
            ALOGD("[%s], strategy_has_enough_buffer_time_to_start_play not ready or started \n", TAG);
        }
        return false;
    }

    if (ck->play_started) {
        return true;
    }

    int cached_ms = ffp_get_total_history_cached_duration_ms(ffp);
    int64_t buffer_cost_ms = current_ts_ms() - ck->self_life_cycle_start_ts_ms;
    if (VERBOSE) {
        ALOGD("[%s][%s] before exam, v.total_dur:%lld, a.total_dur:%lld, v.total_size:%lld, a.total_size:%lld \n",
              TAG, __func__, ffp->is->videoq.history_total_duration,
              ffp->is->audioq.history_total_duration,
              ffp->is->videoq.history_total_size, ffp->is->audioq.history_total_size);
    }

    ck->current_buffer_ms = cached_ms;
    ck->self_life_cycle_cost_ms = buffer_cost_ms;

    if (!ck->enabled
        || cached_ms >= ck->buffer_threshold_ms
        || (ck->self_max_life_cycle_ms >= 0 && buffer_cost_ms >= ck->self_max_life_cycle_ms)
       ) {
        if (APP_DEBUG) {
            ALOGI("[%s][%s][topic:spb] buffer_cost_ms:%lld, max_cost_ms:%lld \n",
                  TAG, __func__, buffer_cost_ms, ck->self_max_life_cycle_ms);
            ALOGI("[%s][%s][topic:spb] "
                  "cached_ms:%d ,ck->buffer_threshold_ms = %d, ck->enabled:%d, "
                  "ck->disable_reason:%d, return true, start playing \n",
                  TAG, __func__, cached_ms, ck->buffer_threshold_ms, ck->enabled,
                  ck->disable_reason);

        }
        ck->play_started = true;
        ALOGD("[%s], strategy_has_enough_buffer_time_to_start_play, self_max_life_cycle_ms: %d\n", TAG, ck->self_max_life_cycle_ms);
        KwaiQos_onReadyToRender(&ffp->kwai_qos);
        return true;
    } else {
        if (VERBOSE) {
            ALOGD("[%s][%s] dur_ms:%d < %d, self_max_life_cycle_ms: %d, return false \n", TAG, __func__, cached_ms,
                  ck->buffer_threshold_ms, ck->self_max_life_cycle_ms);
        }
        return false;
    }
    ALOGE("[%s][%s]should not get here \n", TAG, __func__);
    assert(0);
}

void on_read_frame_error(KwaiPacketQueueBufferChecker* ck, FFplayer* ffp) {
    // 强制开始
    if (!ck->play_started) {
        ck->play_started = true;
        int64_t buffer_cost_ms = current_ts_ms() - ck->self_life_cycle_start_ts_ms;
        ALOGD("[%s], on_read_frame_error \n", TAG);
        ck->self_life_cycle_cost_ms = buffer_cost_ms;
        KwaiQos_onReadyToRender(&ffp->kwai_qos);
    }
}

void KwaiPacketQueueBufferChecker_init(KwaiPacketQueueBufferChecker* ck) {
    ck->play_started = false;

    ck->on_read_frame_error = &on_read_frame_error;
    ck->func_check_pkt_q_need_buffering = &packet_queue_check_need_buffering;

    ck->used_strategy = kStrategyStartPlayBlockByNone;
    ck->func_check_can_start_play = &strategy_can_start_play_anytime;

    ck->buffer_threshold_bytes = MIN_START_PLAY_BUFFER_BYTES_DEFAULT;
    ck->buffer_threshold_ms = MIN_START_PLAY_BUFFER_MS_DEFAULT;

    ck->self_max_life_cycle_ms = START_PLAY_MAX_COST_MS_DEFAULT;
    ck->self_life_cycle_started = false;
    ck->self_life_cycle_start_ts_ms = -1;
    ck->self_life_cycle_cost_ms = 0;

    ck->enabled = false;

    ck->ready = false;
}

void KwaiPacketQueueBufferChecker_use_strategy(KwaiPacketQueueBufferChecker* ck,
                                               KwaiPacketQueueBufferCheckerStrategy strategy) {
    switch (strategy) {
        case kStrategyStartPlayBlockByTimeMs:
            ck->func_check_can_start_play = &strategy_has_enough_buffer_time_to_start_play;
            ck->used_strategy = strategy;
            break;
        default:
            ck->func_check_can_start_play = &strategy_can_start_play_anytime;
            ck->used_strategy = kStrategyStartPlayBlockByNone;
            break;
    }
}


void KwaiPacketQueueBufferChecker_on_lifecycle_start(KwaiPacketQueueBufferChecker* ck) {
    ck->self_life_cycle_start_ts_ms = current_ts_ms();
    ck->self_life_cycle_started = true;
    if (VERBOSE) {
        ALOGD("[%s], self_life_cycle_start_ts_ms:%lld \n", __func__, ck->self_life_cycle_start_ts_ms);
    }
}

void KwaiPacketQueueBufferChecker_set_start_play_max_buffer_cost_ms(KwaiPacketQueueBufferChecker* ck, int max_buffer_cost_ms) {
    if (max_buffer_cost_ms > START_PLAY_MAX_COST_MS_MAX) {
        return;
    }
    ck->self_max_life_cycle_ms = max_buffer_cost_ms;
}

void KwaiPacketQueueBufferChecker_set_start_play_buffer_ms(KwaiPacketQueueBufferChecker* ck,
                                                           int block_buffer_ms) {
    if (block_buffer_ms <= 0 || block_buffer_ms > MIN_START_PLAY_BUFFER_MS_MAX) {
        return;
    }
    ck->buffer_threshold_ms = block_buffer_ms;
}

void KwaiPacketQueueBufferChecker_set_enable(KwaiPacketQueueBufferChecker* ck,
                                             bool enable,
                                             KwaiPacketQueueBufferCheckerDisableReason reason) {
    ck->enabled = enable;
    if (!ck->enabled) {
        ck->disable_reason = reason;
    }
    ck->ready = true;
}
