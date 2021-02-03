//
// Created by MarshallShuai on 2019/4/22.
//

#include "ff_ffplay_clock.h"

#include <math.h>
#include <ijkmedia/ijkplayer/ff_ffinc.h>
#include "ff_ffplay_def.h"

double get_clock(Clock* c) {
    if (*c->queue_serial != c->serial)
        return NAN;
    if (c->paused) {
        return c->pts;
    } else {
        double time = av_gettime_relative() / 1000000.0;
        return c->pts_drift + time - (time - c->last_updated) * (1.0 - c->speed);
    }
}

double get_position_clock(Clock* c) {
    if (*c->queue_serial != c->serial)
        return NAN;
    if (c->paused) {
        return c->origin_pts;
    } else {
        double time = av_gettime_relative() / 1000000.0;
        return c->origin_pts_drift + time - (time - c->last_updated) * (1.0 - c->speed);
    }
}


void set_clock_at(Clock* c, double pts, double origin_pts, int serial, double time) {
    c->pts = pts;
    c->origin_pts = origin_pts;
    c->last_updated = time;
    c->pts_drift = c->pts - time;
    c->origin_pts_drift = c->origin_pts - time;
    c->serial = serial;
}

void set_clock(Clock* c, double pts, int serial) {
    double time = av_gettime_relative() / 1000000.0;
    set_clock_at(c, pts, pts, serial, time);
}

void set_clock_speed(Clock* c, double speed) {
    set_clock(c, get_clock(c), c->serial);
    c->speed = speed;
}

void init_clock(Clock* c, int* queue_serial, const char* name) {
    c->name = name;
    c->speed = 1.0;
    c->paused = 0;
    c->queue_serial = queue_serial;
    set_clock(c, NAN, -1);
}

void sync_clock_to_slave(Clock* c, Clock* slave) {
    double clock = get_clock(c);
    double slave_clock = get_clock(slave);
    if (!isnan(slave_clock) && (isnan(clock) || fabs(clock - slave_clock) > AV_NOSYNC_THRESHOLD))
        set_clock(c, slave_clock, slave->serial);
}

int get_master_sync_type(VideoState* is) {
    if (is->av_sync_type == AV_SYNC_VIDEO_MASTER) {
        if (is->video_st)
            return AV_SYNC_VIDEO_MASTER;
        else
            return AV_SYNC_AUDIO_MASTER;
    } else if (is->av_sync_type == AV_SYNC_AUDIO_MASTER) {
        if (is->audio_st && is->av_aligned)
            return AV_SYNC_AUDIO_MASTER;
        else
            return AV_SYNC_EXTERNAL_CLOCK;
    } else {
        return AV_SYNC_EXTERNAL_CLOCK;
    }
}

/* get the current master clock value */
double get_master_clock(VideoState* is) {
    double val;

    switch (get_master_sync_type(is)) {
        case AV_SYNC_VIDEO_MASTER:
            val = get_clock(&is->vidclk);
            break;
        case AV_SYNC_AUDIO_MASTER:
            val = get_clock(&is->audclk);
            break;
        default:
            val = get_clock(&is->extclk);
            break;
    }
    return val;
}

/*
 * 该函数由get_current_position调用，便于应用层获知当前的播放进度。
 * 对于音频是master的情况，修复了SDL_AoutGetLatencySeconds(ffp->aout)造成的误差。
 */
double get_master_position_clock(VideoState* is) {
    double val;

    switch (get_master_sync_type(is)) {
        case AV_SYNC_VIDEO_MASTER:
            val = get_position_clock(&is->vidclk);
            break;
        case AV_SYNC_AUDIO_MASTER:
            val = get_position_clock(&is->audclk);
            break;
        default:
            val = get_position_clock(&is->extclk);
            break;
    }
    return val;
}