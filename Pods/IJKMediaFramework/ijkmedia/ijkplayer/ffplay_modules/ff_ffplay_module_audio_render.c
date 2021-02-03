//
// Created by MarshallShuai on 2019/4/19.
//

#if defined(CONFIG_KS_AUDIOPROCESS) || defined(__APPLE__)
#include "ijkkwai/c_audio_process.h"
#endif

#include "ff_ffplay_module_audio_render.h"
#include "ff_ffplay_def.h"
#include "ff_ffplay.h"
#include "ff_ffplay_internal.h"
#include "ijksdl/ijksdl_aout.h"
#include "ff_ffinc.h"

#ifdef __ANDROID__
#include "android/kwai_android_jni_util.h"
#endif

/* maximum audio speed change to get correct sync */
#define SAMPLE_CORRECTION_PERCENT_MAX 10

/**
 * only called by sdl_audio_callback
 */
/* copy samples for viewing in editor window */
static void update_sample_display(VideoState* is, short* samples, int samples_size) {
    int size, len;

    size = samples_size / sizeof(short);
    while (size > 0) {
        len = SAMPLE_ARRAY_SIZE - is->sample_array_index;
        if (len > size)
            len = size;
        memcpy(is->sample_array + is->sample_array_index, samples, len * sizeof(short));
        samples += len;
        is->sample_array_index += len;
        if (is->sample_array_index >= SAMPLE_ARRAY_SIZE)
            is->sample_array_index = 0;
        size -= len;
    }
}

/*
 * 使用SoundTouch库实现音频的变速变调播放
*/
static int audio_speed_tone_change_by_sound_touch(VideoState* is, bool enableSpeed, float speed, bool enableTone, int tone, int* data_size) {
    if (!enableSpeed && !enableTone)
        return 0;
    int resampled_data_size = *data_size;
    if (NULL == is->sound_touch) {
        SoundTouchC_init(&is->sound_touch);
    }

    SoundTouchC_setSampleRate(is->sound_touch, is->audio_tgt.freq);
    SoundTouchC_setChannels(is->sound_touch, is->audio_tgt.channels);
    if (enableSpeed)
        SoundTouchC_setTempo(is->sound_touch, speed);
    if (enableTone)
        SoundTouchC_setPitchSemiTones(is->sound_touch, tone);

    int size_per_sample = is->audio_tgt.channels * av_get_bytes_per_sample(is->audio_tgt.fmt);
    char* temp = av_malloc(resampled_data_size);
    memcpy(temp, is->audio_buf, resampled_data_size);

    int target_size = resampled_data_size / speed;
    target_size = (target_size + 0x03) & (~0x03);
    av_fast_malloc(&is->audio_buf1, &is->audio_buf1_size, target_size);

    int got_sample_cnt = SoundTouchC_processData(is->sound_touch, (short*)temp,
                                                 resampled_data_size / size_per_sample,
                                                 (short*)is->audio_buf1,
                                                 is->audio_buf1_size / size_per_sample);
    av_freep(&temp);
    is->audio_buf = is->audio_buf1;
    resampled_data_size = got_sample_cnt * size_per_sample;
    *data_size = resampled_data_size;

    int duration = 1000 * got_sample_cnt / is->audio_tgt.freq;
    return duration;
}

/**
 * only called by audio_decode_frame
 * return the wanted number of samples to get better sync if sync_type is video
 * or external master clock
 * */
static int synchronize_audio(VideoState* is, int nb_samples) {
    int wanted_nb_samples = nb_samples;

    /* if not master, then we try to remove or add samples to correct the clock */
    if (get_master_sync_type(is) != AV_SYNC_AUDIO_MASTER) {
        double diff, avg_diff;
        int min_nb_samples, max_nb_samples;

        diff = get_clock(&is->audclk) - get_master_clock(is);

        if (!isnan(diff) && fabs(diff) < AV_NOSYNC_THRESHOLD) {
            is->audio_diff_cum = diff + is->audio_diff_avg_coef * is->audio_diff_cum;
            if (is->audio_diff_avg_count < AUDIO_DIFF_AVG_NB) {
                /* not enough measures to have a correct estimate */
                is->audio_diff_avg_count++;
            } else {
                /* estimate the A-V difference */
                avg_diff = is->audio_diff_cum * (1.0 - is->audio_diff_avg_coef);

                if (fabs(avg_diff) >= is->audio_diff_threshold) {
                    wanted_nb_samples = nb_samples + (int)(diff * is->audio_src.freq);
                    min_nb_samples = ((nb_samples * (100 - SAMPLE_CORRECTION_PERCENT_MAX) / 100));
                    max_nb_samples = ((nb_samples * (100 + SAMPLE_CORRECTION_PERCENT_MAX) / 100));
                    wanted_nb_samples = av_clip(wanted_nb_samples, min_nb_samples, max_nb_samples);
                }
                av_log(NULL, AV_LOG_TRACE, "diff=%f adiff=%f sample_diff=%d apts=%0.3f %f\n",
                       diff, avg_diff, wanted_nb_samples - nb_samples,
                       is->audio_clock, is->audio_diff_threshold);
            }
        } else {
            /* too big difference : may be initial PTS errors, so
               reset A-V filter */
            is->audio_diff_avg_count = 0;
            is->audio_diff_cum = 0;
        }
    }

    return wanted_nb_samples;
}

//音频无声统计
static inline void audio_silence_monitor(FFplayer* ffp, char* data, int sample_cnt) {
    short* pcmData = (short*)data;
    bool is_silence_sample = true;
    //如果有一个sample绝对值大于1，则认为有声音，退出循环。如果是无声，循环会遍历sample_cnt次，在低端机上测试性能也是ok的
    for (int i = 0; i < sample_cnt; i++) {
        if (FFABS(*(pcmData + i)) > 1) {
            is_silence_sample = false;
            break;
        }
    }

    if (is_silence_sample) {
        KwaiQos_onSilenceSamplePlayed(&ffp->kwai_qos);
    }
}

/**
 * Decode one audio frame and return its uncompressed size.
 *
 * The processed audio frame is decoded, converted if required, and
 * stored in is->audio_buf, with size in bytes given by the return
 * value.
 */
static int audio_decode_frame(FFPlayer* ffp) {
    VideoState* is = ffp->is;
    int data_size, resampled_data_size;
    int64_t dec_channel_layout;
    av_unused double audio_clock0;
    int wanted_nb_samples;
    Frame* af;

    if (is->paused || is->step)
        return -1;

    if (ffp->sync_av_start &&                       /* sync enabled */
        is->video_st &&                             /* has video stream */
        !is->viddec.first_frame_decoded &&          /* not hot */
        is->viddec.finished != is->videoq.serial) { /* not finished */
        /* waiting for first video frame */
        Uint64 now = SDL_GetTickHR();
        if (now < is->viddec.first_frame_decoded_time
            || now > is->viddec.first_frame_decoded_time + 2000
            || is->audio_accurate_seek_req) {
            is->viddec.first_frame_decoded = 1;
        } else {
            /* video pipeline is not ready yet */
            return -1;
        }
    }

    do {
#if defined(_WIN32) || defined(__APPLE__)
        while (frame_queue_nb_remaining(&is->sampq) == 0) {
            if ((av_gettime_relative() - ffp->audio_callback_time) > 1000000LL * is->audio_hw_buf_size / is->audio_tgt.bytes_per_sec / 2) {
                return -1;
            }
            av_usleep(1000);
        }
#endif
        if (!(af = frame_queue_peek_readable(&is->sampq))) {
            return -1;
        }

        frame_queue_next(&is->sampq);
        //音频首帧渲染时，丢弃比视频首帧pts小的音频帧
        if (ffp->islive && ffp->use_aligned_pts && !is->is_audio_pts_aligned
            && is->viddec.first_frame_pts != -1
            && !isnan(af->pts) && af->pts < is->viddec.first_frame_pts) {
            continue;
        }
    } while (af->serial != is->audioq.serial);

    if (!is->is_audio_pts_aligned && !isnan(af->pts)) {
        is->is_audio_pts_aligned = true;
    }

    FFDemuxCacheControl_on_av_rendered(&ffp->dcc);
    KwaiQos_onSamplePlayed(&ffp->kwai_qos, af->duration, ffp->start_on_prepared);

    KwaiQos_setBlockInfoStartPeriod(&ffp->kwai_qos);

    data_size = av_samples_get_buffer_size(NULL, av_frame_get_channels(af->frame),
                                           af->frame->nb_samples,
                                           af->frame->format, 1);

    dec_channel_layout =
        (af->frame->channel_layout && av_frame_get_channels(af->frame) ==
         av_get_channel_layout_nb_channels(
             af->frame->channel_layout)) ?
        af->frame->channel_layout : av_get_default_channel_layout(
            av_frame_get_channels(af->frame));
    if (is->i_buffer_time_max != ffp->i_buffer_time_max) {
        sync_chasing_threshold(ffp);
    }
    wanted_nb_samples = synchronize_audio(is, af->frame->nb_samples);
    if (is->chasing_enabled && is->chasing_status) {
        ffp_check_buffering_l(ffp, false);
    }
    if (af->frame->format != is->audio_src.fmt ||
        dec_channel_layout != is->audio_src.channel_layout ||
        af->frame->sample_rate != is->audio_src.freq ||
        (wanted_nb_samples != af->frame->nb_samples && !is->swr_ctx)) {
        swr_free(&is->swr_ctx);
        is->swr_ctx = swr_alloc_set_opts(NULL,
                                         is->audio_tgt.channel_layout, is->audio_tgt.fmt,
                                         is->audio_tgt.freq,
                                         dec_channel_layout, af->frame->format,
                                         af->frame->sample_rate,
                                         0, NULL);
        if (!is->swr_ctx || swr_init(is->swr_ctx) < 0) {
            ALOGE("[%u] Cannot create sample rate converter for conversion of %d Hz %s %d channels to %d Hz %s %d channels!\n",
                  ffp->session_id, af->frame->sample_rate, av_get_sample_fmt_name(af->frame->format), av_frame_get_channels(af->frame),
                  is->audio_tgt.freq, av_get_sample_fmt_name(is->audio_tgt.fmt), is->audio_tgt.channels);
            swr_free(&is->swr_ctx);
            return -1;
        }
        is->audio_src.channel_layout = dec_channel_layout;
        is->audio_src.channels = av_frame_get_channels(af->frame);
        is->audio_src.freq = af->frame->sample_rate;
        is->audio_src.fmt = af->frame->format;
    }

    int per_sample_size = av_get_bytes_per_sample(is->audio_tgt.fmt);
    if (is->swr_ctx) {
        const uint8_t** in = (const uint8_t**) af->frame->extended_data;
        uint8_t** out = &is->audio_buf1;
        int out_count = (int)(
                            (int64_t) wanted_nb_samples * is->audio_tgt.freq / af->frame->sample_rate + 256);
        int out_size = av_samples_get_buffer_size(NULL, is->audio_tgt.channels, out_count,
                                                  is->audio_tgt.fmt, 0);
        int len2;
        if (out_size < 0) {
            ALOGE("[%u] av_samples_get_buffer_size() failed\n", ffp->session_id);
            return -1;
        }
        if (wanted_nb_samples != af->frame->nb_samples) {
            if (swr_set_compensation(is->swr_ctx, (wanted_nb_samples - af->frame->nb_samples) * is->audio_tgt.freq / af->frame->sample_rate,
                                     wanted_nb_samples * is->audio_tgt.freq / af->frame->sample_rate) < 0) {
                ALOGE("[%u] swr_set_compensation() failed\n", ffp->session_id);
                return -1;
            }
        }
        av_fast_malloc(&is->audio_buf1, &is->audio_buf1_size, out_size);
        if (!is->audio_buf1)
            return AVERROR(ENOMEM);
        len2 = swr_convert(is->swr_ctx, out, out_count, in, af->frame->nb_samples);
        if (len2 < 0) {
            ALOGE("[%u] swr_convert() failed\n", ffp->session_id);
            return -1;
        }
        if (len2 == out_count) {
            ALOGW("[%u] audio buffer is probably too small\n", ffp->session_id);
            if (swr_init(is->swr_ctx) < 0)
                swr_free(&is->swr_ctx);
        }
        is->audio_buf = is->audio_buf1;
        resampled_data_size =
            len2 * is->audio_tgt.channels * per_sample_size;
    } else {
        is->audio_buf = af->frame->data[0];
        resampled_data_size = data_size;
    }

    audio_silence_monitor(ffp, (char*)is->audio_buf, resampled_data_size / per_sample_size);

#if defined(CONFIG_KS_AUDIOPROCESS) || defined(__APPLE__)
    float spectrum_volume[AUDIOFFTSIZE] = {0};
    if (ffp->enable_audio_spectrum) {
        AudioSpectrumProcessor_process(&ffp->audio_spectrum_processor, (char*)is->audio_buf, is->audio_tgt.channels,
                                       resampled_data_size / is->audio_tgt.channels /
                                       per_sample_size, spectrum_volume);
//此处目前只对安卓端开启fft数据回调，IOS对应功能集成时会优化此处代码
#ifdef __ANDROID__
        jni_post_fftdata_from_audio_process(ffp->weak_thiz, spectrum_volume);
#endif
    }
#endif

    // speed up/down
    if (ffp->audio_speed_change_enable && is->audio_speed_percent != KS_AUDIO_PLAY_SPEED_NORMAL) {
        float speed = is->audio_speed_percent / 100.0f;
        int duration = audio_speed_tone_change_by_sound_touch(ffp->is, true, speed, false, ffp->pf_playback_tone, &resampled_data_size);
        if (KS_AUDIO_PLAY_SPEED_DOWN == is->audio_speed_percent) {
            ffp->qos_speed_change.down_duration += duration;
        } else if (KS_AUDIO_PLAY_SPEED_UP_1 == is->audio_speed_percent) {
            ffp->qos_speed_change.up_1_duration += duration;
        } else if (KS_AUDIO_PLAY_SPEED_UP_2 == is->audio_speed_percent) {
            ffp->qos_speed_change.up_2_duration += duration;
        }
    }

    audio_clock0 = is->audio_clock;
    /* update the audio clock with the pts */
    if (!isnan(af->pts))
        is->audio_clock = af->pts + (double) af->frame->nb_samples / af->frame->sample_rate;
    else
        is->audio_clock = NAN;
    is->audio_clock_serial = af->serial;
    if (!is->auddec.first_frame_decoded) {
        // ALOGD("[%u] avcodec/Audio: first frame decoded\n", ffp->session_id);
        is->auddec.first_frame_decoded_time = SDL_GetTickHR();
        is->auddec.first_frame_decoded = 1;
    }
    if (!ffp->first_audio_frame_rendered) {
        ffp->first_audio_frame_rendered = 1;
        ffp_notify_msg1(ffp, FFP_MSG_AUDIO_RENDERING_START);
        KwaiQos_onStartPlayer(&ffp->kwai_qos);
    }
    if (ffp->audio_render_after_seek_need_notify && is->seek_pos != -1) {
        if (ffp->enable_accurate_seek) {
            if (!is->audio_accurate_seek_req) {
                ALOGD("[%u] audio_decode_frame, FFP_MSG_AUDIO_RENDERING_START_AFTER_SEEK\n", ffp->session_id);
                ffp->audio_render_after_seek_need_notify = 0;
                ffp_notify_msg1(ffp, FFP_MSG_AUDIO_RENDERING_START_AFTER_SEEK);
            }
        } else {
            ffp->audio_render_after_seek_need_notify = 0;
            ffp_notify_msg1(ffp, FFP_MSG_AUDIO_RENDERING_START_AFTER_SEEK);
        }
    }

    AudioVolumeProgress_adujust_volume(&ffp->audio_vol_progress, af);
    audio_speed_tone_change_by_sound_touch(is, ffp->pf_playback_rate && ffp->pf_playback_rate_is_sound_touch, ffp->pf_playback_rate, ffp->pf_playback_tone_is_sound_touch, ffp->pf_playback_tone, &resampled_data_size);

    //Process PCM
    if (ffp->aout && ffp->should_export_process_audio_pcm) {
        SDL_AoutProcessPCM(ffp->aout, is->audio_buf, &resampled_data_size,
                           af->frame->nb_samples, av_frame_get_channels(af->frame), 0);
    }

    return resampled_data_size;
}

/* prepare a new audio buffer */
void sdl_audio_callback(void* opaque, Uint8* stream, int len) {
    FFPlayer* ffp = opaque;
    bool is_silence = false;
    if (!ffp || !ffp->is) {
        memset(stream, 0, (size_t) len);
        return;
    }
    VideoState* is = ffp->is;
    int audio_size, len1;
    SDL_Aout* aout = ffp->aout;


    ffp->audio_callback_time = av_gettime_relative();

    if (ffp->pf_playback_rate_changed && !ffp->pf_playback_rate_is_sound_touch) {
        ffp->pf_playback_rate_changed = 0;
        SDL_AoutSetPlaybackRate(ffp->aout, ffp->pf_playback_rate);
    }

    while (len > 0) {
        if (is->audio_buf_index >= is->audio_buf_size) {
            audio_size = audio_decode_frame(ffp);
            if (audio_size < 0) {
                /* if error, just output silence */
                is->audio_buf      = is->silence_buf;
                if (is->audio_tgt.frame_size == 0) {
                    is->audio_tgt.frame_size = av_samples_get_buffer_size(NULL, is->audio_tgt.channels, 1,
                                                                          is->audio_tgt.fmt, 1);
                }
                if (is->audio_tgt.frame_size > 0) {
                    is->audio_buf_size = sizeof(is->silence_buf) / is->audio_tgt.frame_size * is->audio_tgt.frame_size;
                } else {
                    is->audio_buf_size = sizeof(is->silence_buf);
                }
                SDL_Aout_Qos_onSilentBuffer(aout, is->audio_buf_size);
                is_silence = true;
            } else if (0 == audio_size) {
                continue;
            } else {
                if (is->show_mode != SHOW_MODE_VIDEO) {
                    update_sample_display(is, (int16_t*) is->audio_buf, audio_size);
                }
                is_silence = false;
                is->audio_buf_size = (unsigned int) audio_size;
            }
            is->audio_buf_index = 0;
        }
        if (is->auddec.pkt_serial != is->audioq.serial) {
            is->audio_buf_index = is->audio_buf_size;
            memset(stream, 0, len);
            SDL_AoutFlushAudio(ffp->aout);
            break;
        }
        len1 = is->audio_buf_size - is->audio_buf_index;
        if (len1 > len)
            len1 = len;
        if (ffp->volumes[AUDIO_VOLUME_LEFT] == 1.0f && ffp->volumes[AUDIO_VOLUME_RIGHT] == 1.0f)
            memcpy(stream, is->audio_buf + is->audio_buf_index, len1);
        else {
            memset(stream, is->silence_buf[0], len1);
            LOCK(ffp->volude_mutex);
            SDL_MixAudio(stream, is->audio_buf + is->audio_buf_index, len1, ffp->volumes);
            UNLOCK(ffp->volude_mutex);

        }
        len -= len1;
        stream += len1;
        is->audio_buf_index += len1;
    }
    is->audio_write_buf_size = is->audio_buf_size - is->audio_buf_index;
    /* Let's assume the audio driver that is used by SDL has two periods. */
    double clock =
        is->audio_clock - (double)(is->audio_write_buf_size) / is->audio_tgt.bytes_per_sec;
    double latency = SDL_AoutGetLatencySeconds(ffp->aout);
    double clock_late = clock - latency;

    ffp->exported_pcm_ts_ms = (int64_t)(clock * 1000);
#if defined(__APPLE__)
    // 解决ios段部分视频无法loop的问题。
    // 原因：视频文件的音频比视频的长度短，这个时候播放器需要插入空包，由于音频没有新的更新，
    // 一直使用最后的包的pts设置音频的clock，造成整个播放被卡死。
    // 解决：在音频已经到尾部的时候，不要用最后一个音频包更新clock.
    //bool is_disable_set_clock = (is->auddec.finished == is->audioq.serial) & is_silence;
    if (!isnan(is->audio_clock) && !((is->auddec.finished == is->audioq.serial) && is_silence)) {
#else
    if (!isnan(is->audio_clock)) {
#endif
        if (ffp->islive && is->vc_queue.nb_comment > 0) {
            int64_t vc_time = 0;
            bool peeked = live_voice_comment_queue_peek(&is->vc_queue, fftime_to_milliseconds((int64_t)(is->audio_clock * 1000 * 1000)), &vc_time);
            while (peeked) {
                ALOGI("[%u] voice_com, matched vc_time=%lld, audio_clk=%lld\n", ffp->session_id, vc_time, fftime_to_milliseconds(is->audio_clock * 1000 * 1000));
                int vc_time_high32 = (int)((0xFFFFFFFF00000000 & vc_time) >> 32);
                int vc_time_low32  = (int)(0x00000000FFFFFFFF & vc_time);
                ffp_notify_msg3(ffp, FFP_MSG_LIVE_VOICE_COMMENT_CHANGE, vc_time_high32, vc_time_low32);

                peeked = live_voice_comment_queue_peek(&is->vc_queue, fftime_to_milliseconds((int64_t)(is->audio_clock * 1000 * 1000)), &vc_time);
            }
        }

        if (ffp->islive && is->event_queue.nb_event > 0) {
            int64_t event_time = 0;
            bool peeked = live_event_queue_peek(&is->event_queue, fftime_to_milliseconds((int64_t)(is->audio_clock * 1000 * 1000)), &event_time);
            while (peeked) {
                ALOGI("[%u] live event peeked, matched time=%lld, audio_clk=%lld\n", ffp->session_id, event_time, fftime_to_milliseconds(is->audio_clock * 1000 * 1000));
                LiveEvent event;
                memset(&event, 0, sizeof(LiveEvent));
                if (0 == live_event_queue_get(&is->event_queue, &event, event_time)) {
                    SDL_AoutProcessAacLiveEvent(ffp->aout, event.content, event.content_len);
                }

                peeked = live_event_queue_peek(&is->event_queue, fftime_to_milliseconds((int64_t)(is->audio_clock * 1000 * 1000)), &event_time);
            }
        }

        set_clock_at(&is->audclk, clock_late, is->audio_clock, is->audio_clock_serial,
                     ffp->audio_callback_time / 1000000.0);
        ffp_on_clock_changed(ffp, &is->audclk);
        sync_clock_to_slave(&is->extclk, &is->audclk);
        ffp_on_clock_changed(ffp, &is->extclk);
    }

    // QosInfo: delay
    if (ffp->qos_pts_offset_got && ffp->wall_clock_updated && ffp->first_audio_frame_rendered) {
        DelayStat_calc_pts_delay(&ffp->qos_delay_audio_render, ffp->wall_clock_offset,
                                 ffp->qos_pts_offset, (int64_t)(is->audclk.pts * 1000));
    }
}
