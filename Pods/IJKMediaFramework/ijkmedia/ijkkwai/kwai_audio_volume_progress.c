//
//  kwai_audio_volume_progress.c
//  IJKMediaFramework
//
//  Created by 帅龙成 on 2018/6/4.
//  Copyright © 2018 kuaishou. All rights reserved.
//

#include "kwai_audio_volume_progress.h"
#include "ff_frame_queue.h"


void AudioVolumeProgress_init(AudioVolumeProgress* avp) {
    avp->enable = false;
}

void AudioVolumeProgress_enable(AudioVolumeProgress* avp, int64_t start_pts_ms, int64_t end_pts_ms, float start_vol, float end_vol) {
    if (start_pts_ms >= end_pts_ms
        || start_vol >= end_vol
        || start_vol < 0 ||  end_vol <= 0 || end_vol > 1) {
        ALOGW("AudioVolumeProgress_enable, INVALID params, AudioVolumeProgress wont be enabled");
        return;
    }

    avp->start_pts_ms = start_pts_ms;
    avp->end_pts_ms = end_pts_ms;
    avp->start_vol = start_vol;
    avp->end_vol = end_vol;
    avp->enable = true;
    avp->ended = false;
}

static inline void scale_samples_s16(uint8_t* dst, const uint8_t* src,
                                     int nb_samples, int volume) {
    int i;
    int16_t* smp_dst       = (int16_t*)dst;
    const int16_t* smp_src = (const int16_t*)src;
    for (i = 0; i < nb_samples; i++)
        smp_dst[i] = av_clip_int16(((int64_t)smp_src[i] * volume + 128) >> 8);
}


void AudioVolumeProgress_adujust_volume(AudioVolumeProgress* avp, Frame* af) {
    if (!avp->enable || avp->ended || !af || !af->frame || af->frame->linesize[0] <= 0) {
        return;
    }

    int64_t pts_ms = af->pts * 1000;
    if (pts_ms >= avp->end_pts_ms) {
        avp->ended = true;
        return;
    }

    if (pts_ms < avp->start_pts_ms) {
        memset(af->frame->data[0], 0, af->frame->linesize[0]);
        return;
    }

    double ratio = avp->start_vol + 1.0 * (pts_ms - avp->start_pts_ms) / (avp->end_pts_ms - avp->start_pts_ms)
                   * (avp->end_vol - avp->start_vol);

    AVFrame* buf = af->frame;

    if (buf->format != AV_SAMPLE_FMT_S16 && buf->format != AV_SAMPLE_FMT_S16P) {
        // 只处理s16&s16p的情况
        return;
    }

    int plane_samples = 0;
    if (av_sample_fmt_is_planar(buf->format))
        plane_samples = FFALIGN(buf->nb_samples, 1);
    else
        plane_samples = FFALIGN(buf->nb_samples * buf->channels, 1);

    scale_samples_s16(buf->data[0],
                      buf->data[0], plane_samples,
                      ratio * 256);
}


void AudioVolumeProgress_adujust_volume_data(AudioVolumeProgress* avp, uint8_t* audio_data, int audio_data_len, int64_t pts_ms) {
    if (!avp->enable || !audio_data || audio_data_len <= 0) {
        return;
    }

    if (pts_ms >= avp->end_pts_ms) {
        return;
    }

    if (pts_ms < avp->start_pts_ms) {
        memset(audio_data, 0, audio_data_len);
        return;
    }

    double ratio = avp->start_vol + 1.0 * (pts_ms - avp->start_pts_ms) / (avp->end_pts_ms - avp->start_pts_ms)
                   * (avp->end_vol - avp->start_vol);
    uint16_t* buf_s16 = (uint16_t*)audio_data;
    for (int i = 0; i < audio_data_len / 2; i += 2) {
        buf_s16[i] *= ratio;
    }
    ALOGD("AudioVolumeProgress_adujust_volume, ratio:%f pts_ms:%lld \n", ratio, pts_ms);
}
