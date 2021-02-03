//
//  kwai_ab_loop.c
//  IJKMediaFramework
//
//  Created by 帅龙成 on 08/03/2018.
//  Copyright © 2018 kuaishou. All rights reserved.
//

#include "kwai_ab_loop.h"
#include "ijksdl/ijksdl_log.h"
#include "ff_ffplay.h"
#include <libavformat/avformat.h>

void AbLoop_init(AbLoop* loop) {
    loop->enable = 0;
    loop->a_pts_ms = loop->b_pts_ms = -1;
}

void AbLoop_set_ab(AbLoop* loop, int64_t a_pts_ms, int64_t b_pts_ms) {
    loop->a_pts_ms = a_pts_ms > 0 ? a_pts_ms : 0;
    loop->b_pts_ms = b_pts_ms;
}

void AbLoop_on_play_start(AbLoop* loop, struct FFPlayer* ffp) {
    if (loop->a_pts_ms < 0
        || loop->b_pts_ms <= 0
        || loop->b_pts_ms <= loop->a_pts_ms
        || loop->a_pts_ms >= ffp_get_duration_l(ffp)) {
        loop->enable = 0;
    } else {
        loop->enable = 1;
        ffp->seek_at_start = loop->a_pts_ms;
    }
}

void AbLoop_on_frame_rendered(AbLoop* loop, struct FFPlayer* ffp) {
    if (!loop->enable) {
        return;
    }

    if (ffp_get_current_position_l(ffp) >= loop->b_pts_ms) {
        ffp_seek_to_l(ffp, loop->a_pts_ms);
    }
}

void BufferLoop_init(BufferLoop* loop) {
    loop->enable = 0;
    loop->buffer_start_percent = loop->buffer_end_percent = -1;
    loop->buffer_start_pos = loop->buffer_end_pos = -1;
    loop->loop_begin_pos = -1;
    loop->loop_buffer_pos = -1;
}

void BufferLoop_update_pos(BufferLoop* loop, int64_t duration) {
    if (!loop->enable || duration <= 0) {
        return;
    }
    loop->buffer_start_pos = loop->buffer_start_percent >= 0 ? (int64_t)(loop->buffer_start_percent * duration / 100) : 0;
    loop->buffer_end_pos = loop->buffer_end_percent >= 0 ? (int64_t)(loop->buffer_end_percent * duration / 100) : duration;
}

void BufferLoop_enable(BufferLoop* loop, int buffer_start_precent, int buffer_end_precent, int64_t loop_begin) {
    if (buffer_start_precent > buffer_end_precent && buffer_end_precent > 0) {
        return;
    }
    loop->enable = 1;
    loop->buffer_start_percent = buffer_start_precent;
    loop->buffer_end_percent = buffer_end_precent;
    loop->loop_begin_pos = loop_begin < 0 ? 0 : loop_begin;
}

void BufferLoop_on_frame_rendered(BufferLoop* loop, struct FFPlayer* ffp) {
    if (!loop->enable || loop->loop_buffer_pos <= 0) {
        return;
    }
    if (ffp_get_current_position_l(ffp) >= loop->loop_buffer_pos) {
        ffp_seek_to_l(ffp, loop->loop_begin_pos);
    }
}

int BufferLoop_loop_on_buffer(BufferLoop* loop, struct FFPlayer* ffp) {
    if (!loop->enable) {
        return 0;
    }
    int64_t pos = ffp_get_current_position_l(ffp);
    if (pos >= loop->buffer_start_pos && pos <= loop->buffer_end_pos) {
        if (pos > loop->loop_begin_pos) {
            loop->loop_buffer_pos = pos;
            ffp_seek_to_l(ffp, loop->loop_begin_pos);
        }
        return 1;
    }
    return 0;
}
