//
// Created by MarshallShuai on 2019/4/22.
//
#pragma once

#include <stdbool.h>

typedef struct Clock {
    const char* name;
    double pts;           /* clock base */
    double pts_drift;     /* clock base minus time at which we updated the clock */
    double origin_pts;
    double origin_pts_drift;
    double last_updated;
    double speed;
    int serial;           /* clock is based on a packet with this serial */
    int paused;
    int* queue_serial;    /* pointer to the current packet queue serial, used for obsolete clock detection */

    bool is_ref_clock; /* 给ClockTracker参考的,为true表示此时钟的时间是可以通过ijkmp_current_positions提供给外部的*/
} Clock;

struct VideoState;

double get_clock(Clock* c);

double get_position_clock(Clock* c);

void set_clock_at(Clock* c, double pts, double origin_pts, int serial, double time);

void set_clock(Clock* c, double pts, int serial);

void set_clock_speed(Clock* c, double speed);

void init_clock(Clock* c, int* queue_serial, const char* name);

void sync_clock_to_slave(Clock* c, Clock* slave);

int get_master_sync_type(struct VideoState* is);

/* get the current master clock value */
double get_master_clock(struct VideoState* is);

/*
 * 该函数由get_current_position调用，便于应用层获知当前的播放进度。
 * 对于音频是master的情况，修复了SDL_AoutGetLatencySeconds(ffp->aout)造成的误差。
 */
double get_master_position_clock(struct VideoState* is);