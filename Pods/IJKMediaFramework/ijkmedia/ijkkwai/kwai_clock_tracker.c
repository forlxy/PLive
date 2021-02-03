//
// Created by 帅龙成 on 2018/5/17.
//

#include "kwai_clock_tracker.h"
#include "ff_ffplay.h"

void ClockTracker_reset(ClockTracker* ct) {
    memset(ct, 0, sizeof(ClockTracker));
}


long ClockTracker_get_current_position_ms(ClockTracker* ct) {
    return ct->is_seeking ? atomic_load(&ct->last_seek_pos_ms) : atomic_load(&ct->last_ref_clock_pos_ms);
}

void ClockTracker_update_is_seeking(ClockTracker* ct, bool seek_req, long msec) {
    ct->is_seeking = seek_req;
    atomic_store(&ct->last_seek_pos_ms, msec);
}

void ClockTracker_on_ref_clock_updated(ClockTracker* ct, long pos_ms) {
    atomic_store(&ct->last_ref_clock_pos_ms, pos_ms);
}
