//
// Created by MarshallShuai on 2019/4/24.
//

#include "ff_ffplay_video_state.h"
#include <stdbool.h>

void video_state_set_av_sync_type(VideoState* is, enum AV_SYNC_TYPE av_sync_type) {
    if (!is) {
        return;
    }

    is->av_sync_type = av_sync_type;

    is->audclk.is_ref_clock = is->vidclk.is_ref_clock = true;
    is->extclk.is_ref_clock = av_sync_type == AV_SYNC_EXTERNAL_CLOCK;
}