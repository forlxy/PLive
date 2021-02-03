//
//  kwai_audio_volume_progress.h
//  IJKMediaFramework
//  处理开播的时候音频渐入
//
//  Created by 帅龙成 on 2018/6/4.
//  Copyright © 2018 kuaishou. All rights reserved.
//

#ifndef kwai_audio_volume_progress_h
#define kwai_audio_volume_progress_h

#include <stdint.h>
#include <stdbool.h>
#include "ff_frame_queue.h"

typedef struct {
    int64_t start_pts_ms;
    int64_t end_pts_ms;

    double start_vol;
    double end_vol;

    bool enable;
    bool ended;

    void (*scale_samples)(uint8_t* dst, const uint8_t* src, int nb_samples,
                          int volume);
    int samples_align;
} AudioVolumeProgress;

void AudioVolumeProgress_init(AudioVolumeProgress* avp);

/**
 *
 * @param start_pts_ms 起始pts毫秒
 * @param end_pts_ms 结束pts毫秒
 * @param start_vol 取值范围 (0, 1]
 * @param end_vol 取值范围 (0, 1]
 */
void AudioVolumeProgress_enable(AudioVolumeProgress* avp, int64_t start_pts_ms, int64_t end_pts_ms, float start_vol, float end_vol);

void AudioVolumeProgress_adujust_volume(AudioVolumeProgress* avp, Frame* af);

void AudioVolumeProgress_adujust_volume_data(AudioVolumeProgress* avp, uint8_t* audio_data, int audio_data_len, int64_t pts_ms) ;
#endif /* kwai_audio_volume_progress_h */
