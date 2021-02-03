//
// Created by 帅龙成 on 2018/5/17.
//


#ifndef KWAI_CLOCK_TRACKER_H
#define KWAI_CLOCK_TRACKER_H

#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>

typedef struct ClockTracker {
    atomic_long last_ref_clock_pos_ms;
    atomic_long last_seek_pos_ms;
    bool is_seeking;
} ClockTracker ;

typedef struct FFPlayer FFPlayer;
typedef struct Clock Clock;

void ClockTracker_reset(ClockTracker* ct);
long ClockTracker_get_current_position_ms(ClockTracker* ct);

void ClockTracker_update_is_seeking(ClockTracker* ct, bool seek_req, long msec);
void ClockTracker_on_ref_clock_updated(ClockTracker* ct, long pos_ms);


#endif // KWAI_CLOCK_TRACKER_H
