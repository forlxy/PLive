/*
 * Copyright (c) 2003 Fabrice Bellard
 * Copyright (c) 2013 Zhang Rui <bbcallen@gmail.com>
 *
 * This file is part of ijkPlayer.
 *
 * ijkPlayer is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * ijkPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with ijkPlayer; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "ff_ffplay.h"
#include "awesome_cache_c.h"
#include "ffmpeg_adapter.h"

/**
 * @file
 * simple media player based on the FFmpeg libraries
 */

#include "config.h"
#include <inttypes.h>
#include <math.h>
#include <limits.h>
#include <signal.h>
#include <stdint.h>

#include "libavutil/avstring.h"
#include "libavutil/eval.h"
#include "libavutil/mathematics.h"
#include "libavutil/pixdesc.h"
#include "libavutil/imgutils.h"
#include "libavutil/dict.h"
#include "libavutil/parseutils.h"
#include "libavutil/samplefmt.h"
#include "libavutil/avassert.h"
#include "libavutil/time.h"
#include "libavutil/timestamp.h"
#include "libavformat/avformat.h"
#include "libavformat/url.h"
#include "ijkkwai/kwai_error_code_manager.h"

#if CONFIG_AVDEVICE
#include "libavdevice/avdevice.h"
#endif

#include "libswscale/swscale.h"
#include "libavutil/opt.h"
#include "libavcodec/avfft.h"
#include "libswresample/swresample.h"
#include <libavkwai/cJSON.h>
#include "ijkavformat/ijkdatasource.h"
#include "ijkavformat/ijk_index_content.h"
#if CONFIG_AVFILTER

# include "libavfilter/avfilter.h"
# include "libavfilter/buffersink.h"
# include "libavfilter/buffersrc.h"

#endif

#include "ijksdl/ijksdl_log.h"
#include "ijkavformat/ijkavformat.h"
#include "ff_cmdutils.h"
#include "ff_fferror.h"
#include "ff_ffpipeline.h"
#include "ff_ffpipenode.h"
#include "ff_ffplay_debug.h"
#include "version.h"
#include "ijkmeta.h"

#include "kwai_priv_nal_c.h"

#include <stdatomic.h>
#include <awesome_cache/include/dcc_algorithm_c.h>
#include <awesome_cache/ac_log.h>
#include <awesome_cache/include/awesome_cache_callback_c.h>
#include "ijkkwai/ff_buffer_strategy.h"
#include "ijkkwai/kwai_audio_gain.h"
#include "ijkkwai/kwai_qos.h"
#include "ijkkwai/kwai_ab_loop.h"
#include "ijkkwai/kwai_vod_manifest.h"
#include "ijkkwai/kwai_error_code_manager_ff_convert.h"
#include "ijkkwai/kwai_priv_aac_parser.h"
#include "ijkkwai/kwaiplayer_lifecycle.h"

#include "ffplay_modules/ff_ffplay_internal.h"
#include "ffplay_modules/ff_ffplay_clock.h"
#include "ffplay_modules/ff_ffplay_module_read_thread.h"
#include "ffplay_modules/ff_ffplay_module_audio_decode.h"
#include "ffplay_modules/ff_ffplay_module_audio_render.h"
#include "ffplay_modules/ff_ffplay_module_video_decode.h"
#include "ffplay_modules/ff_ffplay_module_video_render.h"

#ifndef AV_CODEC_FLAG2_FAST
#define AV_CODEC_FLAG2_FAST CODEC_FLAG2_FAST
#endif

#ifndef AV_CODEC_CAP_DR1
#define AV_CODEC_CAP_DR1 CODEC_CAP_DR1
#endif

// isnan() may not recognize some double NAN, so we test both double and float
#if defined(__ANDROID__)
#ifdef isnan
#undef isnan
#endif
#define isnan(x) (isnan((double)(x)) || isnanf((float)(x)))
#endif

#if defined(__ANDROID__)
#define printf(...) ALOGD(__VA_ARGS__)
#endif

#define FFP_IO_STAT_STEP (50 * 1024)

#define FFP_BUF_MSG_PERIOD (3)

#define MAX_LIVE_AV_PTS_GAP_THRESHOLD (0.8)

// static const AVOption ffp_context_options[] = ...
#include "ff_ffplay_options.h"

#if defined(CONFIG_KS_AUDIOPROCESS) || defined(__APPLE__)
#include "ijkkwai/c_audio_process.h"
#endif

#include "ff_ffplay_def.h"

#if CONFIG_AVFILTER
// FFP_MERGE: opt_add_vfilter
#endif


static volatile unsigned global_session_id = 0;

// FFP_MERGE: fill_rectangle
// FFP_MERGE: fill_border
// FFP_MERGE: ALPHA_BLEND
// FFP_MERGE: RGBA_IN
// FFP_MERGE: YUVA_IN
// FFP_MERGE: YUVA_OUT
// FFP_MERGE: BPP
// FFP_MERGE: blend_subrect

void set_buffersize(FFPlayer* ffp, int size) {
    if (size > 10 && size < 100)
        ffp->dcc.max_buffer_size = size * 1024 * 1024;
    else
        ALOGE("[%u] [%s:%d]wrong size = %d M.\n", ffp->session_id, __FUNCTION__, __LINE__, size);
}

void set_timeout(FFPlayer* ffp, int timeout) {
    if (timeout > 0)
        ffp->timeout = (int64_t)timeout * 1000000;
    else
        ALOGE("[%u] [%s:%d]wrong timeout = %d s.\n", ffp->session_id, __FUNCTION__, __LINE__, timeout);
}
void set_codecflag(FFPlayer* ffp, int flag) {
    if (NULL == ffp)
        return;

    if (flag & FLAG_KWAI_USE_MEDIACODEC_ALL) {
        ffp->mediacodec_all_videos = 1;
        ffp->mediacodec_auto_rotate = 1;
    }

    if (flag & FLAG_KWAI_USE_MEDIACODEC_H265) {
        ffp->mediacodec_hevc = 1;
        ffp->mediacodec_auto_rotate = 1;
    }

    if (flag & FLAG_KWAI_USE_MEDIACODEC_H264) {
        ffp->mediacodec_avc = 1;
        ffp->mediacodec_auto_rotate = 1;
    }
}
// FFP_MERGE: calculate_display_rect
// FFP_MERGE: video_image_display

// FFP_MERGE: compute_mod
// FFP_MERGE: video_audio_display

static void stream_component_close(FFPlayer* ffp, int stream_index) {
    VideoState* is = ffp->is;
    AVFormatContext* ic = is->ic;
    AVCodecContext* avctx;

    if (stream_index < 0 || !ic || stream_index >= ic->nb_streams)
        return;
    avctx = ic->streams[stream_index]->codec;

    switch (avctx->codec_type) {
        case AVMEDIA_TYPE_AUDIO:
            decoder_abort(&is->auddec, &is->sampq);
            SDL_AoutForceStop(ffp->aout);
            SDL_AoutCloseAudio(ffp->aout);

            decoder_destroy(&is->auddec);
            swr_free(&is->swr_ctx);
            av_freep(&is->audio_buf1);
            is->audio_buf1_size = 0;
            is->audio_buf = NULL;

#ifdef FFP_MERGE
            if (is->rdft) {
                av_rdft_end(is->rdft);
                av_freep(&is->rdft_data);
                is->rdft = NULL;
                is->rdft_bits = 0;
            }
#endif
            break;
        case AVMEDIA_TYPE_VIDEO:
            decoder_abort(&is->viddec, &is->pictq);
            decoder_destroy(&is->viddec);
            break;
        // FFP_MERGE: case AVMEDIA_TYPE_SUBTITLE:
        default:
            break;
    }

    ic->streams[stream_index]->discard = AVDISCARD_ALL;
    avcodec_close(avctx);
    switch (avctx->codec_type) {
        case AVMEDIA_TYPE_AUDIO:
            is->audio_st = NULL;
            is->audio_stream = -1;
            break;
        case AVMEDIA_TYPE_VIDEO:
            is->video_st = NULL;
            is->video_stream = -1;
            break;
        // FFP_MERGE: case AVMEDIA_TYPE_SUBTITLE:
        default:
            break;
    }
}

static void stream_close(FFPlayer* ffp) {
    VideoState* is = ffp->is;
    /* XXX: use a special url_shutdown call to abort parse cleanly */
    ffp_AwesomeCache_AVIOContext_abort(ffp);
    is->abort_request = 1;
    packet_queue_abort(&is->videoq);
    packet_queue_abort(&is->audioq);

    /* should wait audio_read_thread firstly, OR may got multi-thread conditions */
    if (is->audio_read_tid) {
        ALOGD("[%u][stream_close] to SDL_WaitThread(is->audio_read_tid) is->ic:%p \n", ffp->session_id,
              is->ic);
        SDL_CondSignal(is->continue_audio_read_thread);
        SDL_CondSignal(is->continue_read_thread);
        SDL_WaitThread(is->audio_read_tid, NULL);
        is->audio_read_tid = NULL;
    }

    if (is->read_tid) {
        ALOGD("[%u][stream_close] to SDL_WaitThread(is->read_tid) is->ic:%p \n", ffp->session_id,
              is->ic);
        SDL_WaitThread(is->read_tid, NULL);
        is->read_tid = NULL;
    }

    if (is->video_read_tid) {
        SDL_CondSignal(is->continue_video_read_thread);
        SDL_CondSignal(is->continue_read_thread);
        SDL_WaitThread(is->video_read_tid, NULL);
        is->video_read_tid = NULL;
        ffp->is_video_reloaded = false;
        ffp->first_reloaded_v_frame_rendered = 0;
        ffp->reloaded_video_first_pts = AV_NOPTS_VALUE;
    }

    /* close each stream */
    if (is->audio_stream >= 0) {
        stream_component_close(ffp, is->audio_stream);
    }
    if (is->video_stream >= 0) {
        stream_component_close(ffp, is->video_stream);
    }
#ifdef FFP_MERGE
    if (is->subtitle_stream >= 0)
        stream_component_close(ffp, is->subtitle_stream);
#endif

    if (is->ic) {
        ALOGI("[%u][stream_close] ffp_avformat_close_input is->ic: %p", ffp->session_id, is->ic);
        ffp_avformat_close_input(ffp, &is->ic);
    }
    ffp_close_release_AwesomeCache_AVIOContext(ffp);

    if (is->video_refresh_tid) {
        ALOGD("[%u][stream_close] SDL_WaitThread(is->video_refresh_tid) \n", ffp->session_id);
        SDL_WaitThread(is->video_refresh_tid, NULL);
        is->video_refresh_tid = NULL;
    }

    if (ffp->is_live_manifest && is->live_manifest_tid) {
        ALOGD("[stream_close] SDL_WaitThread(is->live_manifest_tid) \n");
        SDL_WaitThread(is->live_manifest_tid, NULL);
        is->live_manifest_tid = NULL;
    }

    if (ffp->islive) {
        live_voice_comment_queue_destroy(&is->vc_queue);
        live_event_queue_destroy(&is->event_queue);
        live_type_queue_destroy(&is->live_type_queue);
    }

    packet_queue_destroy(&is->videoq);
    packet_queue_destroy(&is->audioq);
#ifdef FFP_MERGE
    packet_queue_destroy(&is->subtitleq);
#endif

    /* free all pictures */
    frame_queue_destory(&is->pictq);
    frame_queue_destory(&is->sampq);
#ifdef FFP_MERGE
    frame_queue_destory(&is->subpq);
#endif
    SDL_DestroyCond(is->audio_accurate_seek_cond);
    SDL_DestroyCond(is->video_accurate_seek_cond);
    SDL_DestroyCond(is->continue_read_thread);
    SDL_DestroyCond(is->continue_audio_read_thread);
    SDL_DestroyCond(is->continue_video_read_thread);
    SDL_DestroyCond(is->continue_kflv_thread);
    SDL_DestroyMutex(is->accurate_seek_mutex);
    SDL_DestroyMutex(is->cached_seek_mutex);
    SDL_DestroyMutex(is->play_mutex);
#if !CONFIG_AVFILTER
    sws_freeContext(is->img_convert_ctx);
#endif
#ifdef FFP_MERGE
    sws_freeContext(is->sub_convert_ctx);
#endif
    av_freep(&is->filename);
    av_freep(&is->server_ip);
    av_freep(&is->http_headers);

    if (is->sound_touch) {
        SoundTouchC_free(is->sound_touch);
        is->sound_touch = NULL;
    }

#if defined(CONFIG_KS_AUDIOPROCESS) || defined(__APPLE__)
    if (ffp->audio_gain.audio_processor) {
        AudioProcessor_releasep(&ffp->audio_gain.audio_processor);
    }

    if (ffp->audio_gain.audio_compress_processor) {
        AudioCompressProcessor_releasep(&ffp->audio_gain.audio_compress_processor);
    }

    if (ffp->audio_spectrum_processor) {
        AudioSpectrumProcessor_releasep(&ffp->audio_spectrum_processor);
    }
#endif

    av_freep(&is);
}

static void read_stream_close(FFPlayer* ffp, bool is_flush) {

    VideoState* is = ffp->is;
    ALOGD("[%u] wait for read_tid in read_stream_close\n", ffp->session_id);
    if (0 == is->read_abort_request) {
        ffp_AwesomeCache_AVIOContext_abort(ffp);
        is->read_abort_request = 1;
    }

    /* should wait audio_read_thread firstly, OR may got multi-thread conditions */
    if (is->audio_read_tid) {
        SDL_CondSignal(is->continue_audio_read_thread);
        SDL_CondSignal(is->continue_read_thread);
        SDL_WaitThread(is->audio_read_tid, NULL);
        is->audio_read_tid = NULL;
    }

    if (is->read_tid) {
        SDL_CondSignal(is->continue_read_thread);
        SDL_WaitThread(is->read_tid, NULL);
        is->read_tid = NULL;
    }

    if (is->video_read_tid) {
        SDL_CondSignal(is->continue_video_read_thread);
        SDL_CondSignal(is->continue_read_thread);
        SDL_WaitThread(is->video_read_tid, NULL);
        is->video_read_tid = NULL;
        ffp->is_video_reloaded = false;
        ffp->first_reloaded_v_frame_rendered = 0;
        ffp->reloaded_video_first_pts = AV_NOPTS_VALUE;
    }

    if (is_flush) {
        packet_queue_flush(&is->videoq);
        packet_queue_put(&is->videoq, &flush_pkt, NULL);
        packet_queue_flush(&is->audioq);
        packet_queue_put(&is->audioq, &flush_pkt, NULL);
    }

    /* close each stream */
    if (is->audio_stream >= 0)
        stream_component_close(ffp, is->audio_stream);
    if (is->video_stream >= 0) {
        stream_component_close(ffp, is->video_stream);
        if (ffp && ffp->node_vdec) {
            ffpipenode_flush(ffp->node_vdec);
            ffpipenode_free_p(&ffp->node_vdec);
        }

    }
#ifdef FFP_MERGE
    if (is->subtitle_stream >= 0)
        stream_component_close(ffp, is->subtitle_stream);
#endif

    SDL_AoutFreeP(&ffp->aout);


    ALOGI("[%u][read_stream_close]  ffp_avformat_close_input is->ic: %p", ffp->session_id, is->ic);
    ffp_avformat_close_input(ffp, &is->ic);
    ffp_close_release_AwesomeCache_AVIOContext(ffp);


#ifdef FFP_MERGE
    packet_queue_flush(&is->subtitleq);
#endif
}

// FFP_MERGE: do_exit
// FFP_MERGE: sigterm_handler
// FFP_MERGE: video_open
// FFP_MERGE: video_display



#ifdef FFP_MERGE
static void duplicate_right_border_pixels(SDL_Overlay* bmp) {
    int i, width, height;
    Uint8* p, *maxp;
    for (i = 0; i < 3; i++) {
        width  = bmp->w;
        height = bmp->h;
        if (i > 0) {
            width  >>= 1;
            height >>= 1;
        }
        if (bmp->pitches[i] > width) {
            maxp = bmp->pixels[i] + bmp->pitches[i] * height - 1;
            for (p = bmp->pixels[i] + width - 1; p < maxp; p += bmp->pitches[i])
                * (p + 1) = *p;
        }
    }
}
#endif


// FFP_MERGE: subtitle_thread




//reference wrap_timestamp function in utils.c
//static int64_t rewrap_timestamp_base(AVStream* st, int64_t pts) {
//    if (st->pts_wrap_behavior != AV_PTS_WRAP_IGNORE && st->pts_wrap_reference != AV_NOPTS_VALUE &&
//        pts != AV_NOPTS_VALUE) {
//        if (st->pts_wrap_behavior == AV_PTS_WRAP_ADD_OFFSET && pts > (1ULL << st->pts_wrap_bits) &&
//            pts - (1ULL << st->pts_wrap_bits) < st->pts_wrap_reference)
//            return -(1ULL << st->pts_wrap_bits);
//        else if (st->pts_wrap_behavior == AV_PTS_WRAP_SUB_OFFSET && pts < st->pts_wrap_reference &&
//                 pts + (1ULL << st->pts_wrap_bits) >= st->pts_wrap_reference)
//            return (1ULL << st->pts_wrap_bits);
//    }
//    return 0;
//}

static int kflv_stat_thread(void* arg) {
    int ret = 0;
    FFPlayer* ffp = arg;
    VideoState* is = ffp->is;
    SDL_mutex* wait_mutex = SDL_CreateMutex();

    if (!wait_mutex) {
        ret = AVERROR(ENOMEM);
        ffp->kwai_error_code = convert_to_kwai_error_code(ret);
        goto fail;
    }

    // fix me: set by adapt_configuration later
    ffp->kflv_player_statistic.init_index = 0;
    KFlvPlayerStatistic_collect_initial_info(&ffp->kflv_player_statistic, ffp->is->filename);

    for (;;) {
        if (is->interrupt_exit) {
            ret = AVERROR(ETIMEDOUT);
            ffp->kwai_error_code = convert_to_kwai_error_code(ret);
            break;
        }

        if (is->abort_request || is->read_abort_request)
            break;

        if (!is->ic) {
            SDL_LockMutex(wait_mutex);
            SDL_CondWaitTimeout(is->continue_kflv_thread, wait_mutex, 100);
            SDL_UnlockMutex(wait_mutex);
            ALOGI("[%u] kflv_stat_thread, wait 100ms, is->ic=%p\n", ffp->session_id, is->ic);
            continue;
        }

        av_dict_set_int(&(is->ic->metadata), "block_duration", KwaiQos_getBufferTotalDurationMs(&ffp->kwai_qos), 0);

        KFlvPlayerStatistic_collect_playing_info(&ffp->kflv_player_statistic, is->ic);

//        is->bytes_read = ffp->kflv_player_statistic.kflv_stat.total_bytes_read;

        if (ffp->i_buffer_time_max_live_manifest != ffp->kflv_player_statistic.kflv_stat.speed_up_threshold) {
            ffp->i_buffer_time_max_live_manifest = ffp->kflv_player_statistic.kflv_stat.speed_up_threshold;
        }

        SDL_Delay(20);
    }

fail:
    if (!is->read_abort_request && !is->abort_request) {
        ALOGE("[%u][kflv_stat_thread] ffp->kwai_error_code:%d(%s)\n",
              ffp->session_id, ffp->kwai_error_code, ffp_get_error_string(ret));
        KwaiQos_onError(&ffp->kwai_qos, ffp->kwai_error_code);
        toggle_pause(ffp, 1);
        ffp_notify_msg3(ffp, FFP_MSG_ERROR, ffp->kwai_error_code, 0);
    }

    SDL_DestroyMutex(wait_mutex);
    ALOGI("[%u][kflv_stat_thread] EXIT", ffp->session_id);
    return ret;
}



void ffp_debug_show_queue_status(const char* tag, FFPlayer* ffp) {
    static int i = 0;
    if (i++ % 100 == 0) {
        ALOGD("[preDecode] [%s], audio_packets:%d(%lld), video_packets:%d(%lld), audio_samples:%d, video_frames:%d \n",
              tag,
              ffp->is->audioq.nb_packets, ffp->stat.audio_cache.duration,
              ffp->is->videoq.nb_packets, ffp->stat.video_cache.duration,
              ffp->is->sampq.size, ffp->is->pictq.size);
    }
}

void ffp_kwai_collect_dts_info(FFplayer* ffp, AVPacket* pkt, int audio_stream, int video_stream, AVStream* st) {
    VideoState* is = ffp->is;

    if (pkt->stream_index == audio_stream) {
        int64_t dtsMs = av_rescale_q(pkt->dts, st->time_base, (AVRational) {1, 1000});

        if (pkt->dts != AV_NOPTS_VALUE) {
            int64_t dts_diff = dtsMs - is->last_audio_dts_ms;
            if (is->last_audio_dts_ms != -1 &&
                llabs(ffp->stat.max_audio_dts_diff_ms) < llabs(dts_diff)) {
                ffp->stat.max_audio_dts_diff_ms = dts_diff;
            }
            is->last_audio_dts_ms = dtsMs;
        }
        if (!is->is_audio_first_dts_got) {
            is->is_audio_first_dts_got = true;
            is->audio_first_dts = dtsMs;
            KwaiQos_setAudioFirstDts(&ffp->kwai_qos, is->audio_first_dts);
            if (is->is_video_first_dts_got) {
                is->first_dts =
                    is->audio_first_dts < is->video_first_dts ? is->audio_first_dts
                    : is->video_first_dts;
            } else {
                is->first_dts = is->audio_first_dts;
            }

        }
        ffp->qos_dts_duration = dtsMs - is->first_dts;
#ifdef FFP_SHOW_DTS
        ALOGD("[%u] audio dts: %lld, audio_first_dts: %lld\n", ffp->session_id, dtsMs, is->audio_first_dts);
#endif
    } else if (pkt->stream_index == video_stream) {
        int64_t dtsMs = av_rescale_q(pkt->dts, st->time_base, (AVRational) {1, 1000});

        if (pkt->dts != AV_NOPTS_VALUE) {
            int64_t dts_diff = dtsMs - is->last_video_dts_ms;
            if (is->last_video_dts_ms != -1 && llabs(ffp->stat.max_video_dts_diff_ms) < llabs(dts_diff)) {
                ffp->stat.max_video_dts_diff_ms = dts_diff;
            }
            is->last_video_dts_ms = dtsMs;
        }
        if (!is->is_video_first_dts_got) {
            is->is_video_first_dts_got = true;
            is->video_first_dts = dtsMs;
            KwaiQos_setVideoFirstDts(&ffp->kwai_qos, is->video_first_dts);
            if (is->is_audio_first_dts_got) {
                is->first_dts =
                    is->audio_first_dts < is->video_first_dts ? is->audio_first_dts : is->video_first_dts;
            } else {
                is->first_dts = is->video_first_dts;
            }
        }

        ffp->qos_dts_duration = dtsMs - is->first_dts;
#ifdef FFP_SHOW_DTS
        ALOGD("[%u] video dts: %lld, video_first_dts: %lld\n", ffp->session_id, dtsMs, is->video_first_dts);
#endif
    }
}

static VideoState*
stream_reopen(FFPlayer* ffp, const char* filename, AVInputFormat* iformat, bool is_flush) {
    VideoState* is;
    is = ffp->is;
    if (NULL == is)
        return NULL;
    if (is->read_tid)
        read_stream_close(ffp, is_flush);

    is->read_tid = NULL;
    is->iformat = iformat;
    is->read_abort_request = 0;
    is->read_start_time = 0;
    is->interrupt_exit = 0;
#ifdef QY_CHASING
    is->i_buffer_time_max = KSY_SLOW_CHASING_THRESHOLD_DEFAULT + KSY_FAST_CHASING_DELTA;
    is->chasing_enabled = 1;
    is->chasing_status = 0;
#endif
    is->audio_speed_percent = KS_AUDIO_PLAY_SPEED_NORMAL;
    toggle_pause(ffp, 0);


    if (filename) {
        av_freep(&is->filename);
        is->filename = av_strdup(filename);
    }
    if (is->server_ip != NULL) {
        av_freep(&is->server_ip);
    }

    if (is->http_headers != NULL) {
        av_freep(&is->http_headers);
    }
    if (!is->filename)
        return NULL;
    if (!ffp->aout) {
        ffp->aout = ffpipeline_open_audio_output(ffp->pipeline, ffp);
        if (!ffp->aout)
            return NULL;
    }

    is->read_tid = SDL_CreateThreadEx(&is->_read_tid, read_thread, ffp, "stream_reopen");
    if (!is->read_tid)
        ALOGE("[%u] SDL_CreateThread(stream_reopen): %s\n", ffp->session_id, SDL_GetError());
    return is;
}

static VideoState* stream_open(FFPlayer* ffp, const char* filename, AVInputFormat* iformat) {
    assert(!ffp->is);

    VideoState* is;

    is = av_mallocz(sizeof(VideoState));
    if (!is)
        return NULL;
    is->filename = av_strdup(filename);
    is->http_headers = NULL;
    if (!is->filename)
        goto fail;
    is->iformat = iformat;
    is->ytop = 0;
    is->xleft = 0;
    is->show_frame_count = 0;
    is->probe_fps = 0.0;
    is->seek_cached_pos = -1;
    is->seek_pos = -1;
    is->video_first_pts_ms = -1;
    is->is_audio_pts_aligned = false;

    is->is_audio_first_dts_got = false;
    is->is_video_first_dts_got = false;
    is->last_audio_dts_ms = -1;
    is->last_video_dts_ms = -1;
    is->video_first_dts = 0;
    is->audio_first_dts = 0;
    is->first_dts = 0;

    is->prev_keyframe_dts = 0;

    is->video_refresh_abort_request = 0;

    if (frame_queue_init(&is->pictq, &is->videoq, ffp->pictq_size, 1) < 0) {
        ALOGE("[%u][%s] frame_queue_init(pictq) fail", ffp->session_id, __func__);
        goto fail;
    }
#ifdef FFP_MERGE
    if (frame_queue_init(&is->subpq, &is->subtitleq, SUBPICTURE_QUEUE_SIZE, 0) < 0)
        goto fail;
#endif
    if (frame_queue_init(&is->sampq, &is->audioq, SAMPLE_QUEUE_SIZE, 1) < 0) {
        ALOGE("[%u][%s] frame_queue_init(sampq) fail", ffp->session_id, __func__);
        goto fail;
    }

    if (packet_queue_init_with_name(&is->videoq, "videoq") < 0 ||
        packet_queue_init_with_name(&is->audioq, "audioq") < 0 ||
#ifdef FFP_MERGE
        packet_queue_init(&is->subtitleq) < 0)
#else
        0)
#endif
        goto fail;

    if (ffp->islive) {
        if (live_voice_comment_queue_init(&is->vc_queue) < 0)
            goto fail;

        if (live_event_queue_init(&is->event_queue) < 0)
            goto fail;

        if (live_type_queue_init(&is->live_type_queue) < 0)
            goto fail;
    }

    if (!(is->continue_read_thread = SDL_CreateCond())) {
        ALOGE("[%u] SDL_CreateCond(continue_read_thread): %s\n", ffp->session_id, SDL_GetError());
        goto fail;
    }

    if (!(is->continue_audio_read_thread = SDL_CreateCond())) {
        ALOGE("[%u] SDL_CreateCond(continue_audio_read_thread): %s\n", ffp->session_id, SDL_GetError());
        goto fail;
    }

    if (!(is->continue_video_read_thread = SDL_CreateCond())) {
        ALOGE("[%u] SDL_CreateCond(continue_video_read_thread): %s\n", ffp->session_id, SDL_GetError());
        goto fail;
    }

    if (!(is->continue_kflv_thread = SDL_CreateCond())) {
        ALOGE("[%u] SDL_CreateCond(continue_kflv_thread): %s\n", ffp->session_id, SDL_GetError());
        goto fail;
    }

    if (!(is->video_accurate_seek_cond = SDL_CreateCond())) {
        ALOGE("[%u] SDL_CreateCond(video_accurate_seek_cond): %s\n", ffp->session_id, SDL_GetError());
        ffp->enable_accurate_seek = 0;
    }

    if (!(is->audio_accurate_seek_cond = SDL_CreateCond())) {
        ALOGE("[%u] SDL_CreateCond(audio_accurate_seek_cond): %s\n", ffp->session_id, SDL_GetError());
        ffp->enable_accurate_seek = 0;
    }

    init_clock(&is->vidclk, &is->videoq.serial, "vidclk");
    init_clock(&is->audclk, &is->audioq.serial, "audclk");
    init_clock(&is->extclk, &is->extclk.serial, "extclk");
    is->audio_clock_serial = -1;
    is->audio_volume = SDL_MIX_MAXVOLUME;
    is->muted = ffp->muted;
    video_state_set_av_sync_type(is, ffp->av_sync_type);

    is->play_mutex = SDL_CreateMutex();
    is->accurate_seek_mutex = SDL_CreateMutex();
    is->cached_seek_mutex = SDL_CreateMutex();

    ffp->is = is;
    is->pause_req = !ffp->start_on_prepared;

    is->video_refresh_tid = SDL_CreateThreadEx(&is->_video_refresh_tid, video_refresh_thread, ffp,
                                               "ff_vout");
    if (!is->video_refresh_tid) {
        ALOGE("[%u][%s] create video_refresh_tid fail", ffp->session_id, __func__);
        av_freep(&ffp->is);
        return NULL;
    }

    is->audio_speed_percent = KS_AUDIO_PLAY_SPEED_NORMAL;
    if (ffp->audio_speed_change_enable) {
        SoundTouchC_init(&is->sound_touch);
    }

    if (ffp->is_live_manifest) {
        is->live_manifest_tid = SDL_CreateThreadEx(&is->_live_manifest_tid, kflv_stat_thread, ffp, "kflv_stat_thread");
        if (!is->live_manifest_tid) {
            ALOGE("[%u] SDL_CreateThread(kflv_stat_thread): %s\n", ffp->session_id, SDL_GetError());
            av_freep(&ffp->is);
            return NULL;
        }
    }

    is->read_tid = SDL_CreateThreadEx(&is->_read_tid, read_thread, ffp, "ff_read");
    if (!is->read_tid) {
        ALOGE("[%u] SDL_CreateThread(ff_read): %s\n", ffp->session_id, SDL_GetError());
fail:
        is->abort_request = true;
        if (is->video_refresh_tid) {
            SDL_WaitThread(is->video_refresh_tid, NULL);
            is->video_refresh_tid = NULL;
        }

        if (ffp->is_live_manifest && is->live_manifest_tid) {
            SDL_WaitThread(is->live_manifest_tid, NULL);
            is->live_manifest_tid = NULL;
        }
        stream_close(ffp);
        ffp->is = NULL;
        return NULL;
    }

    is->i_buffer_time_max = 2000;
    is->chasing_enabled = 1;
    is->chasing_status = 0;
    is->bytes_read = 0;
    is->av_aligned = 1;
    is->video_duration = AV_NOPTS_VALUE;
    is->audio_duration = AV_NOPTS_VALUE;
    is->dts_of_last_frame = AV_NOPTS_VALUE;

    return is;
}


static int lockmgr(void** mtx, enum AVLockOp op) {
    switch (op) {
        case AV_LOCK_CREATE:
            *mtx = SDL_CreateMutex();
            if (!*mtx) {
                av_log(NULL, AV_LOG_FATAL, "SDL_CreateMutex(): %s\n", SDL_GetError());
                return 1;
            }
            return 0;
        case AV_LOCK_OBTAIN:
            return !!SDL_LockMutex(*mtx);
        case AV_LOCK_RELEASE:
            return !!SDL_UnlockMutex(*mtx);
        case AV_LOCK_DESTROY:
            SDL_DestroyMutex(*mtx);
            return 0;
    }
    return 1;
}

// FFP_MERGE: main

/*****************************************************************************
 * end last line in ffplay.c
 ****************************************************************************/

static pthread_mutex_t g_init_mutex = PTHREAD_MUTEX_INITIALIZER;
static bool g_ffmpeg_global_inited = false;

inline static int log_level_av_to_ijk(int av_level) {
    int ijk_level = IJK_LOG_VERBOSE;
    if (av_level <= AV_LOG_PANIC)
        ijk_level = IJK_LOG_FATAL;
    else if (av_level <= AV_LOG_FATAL)
        ijk_level = IJK_LOG_FATAL;
    else if (av_level <= AV_LOG_ERROR)
        ijk_level = IJK_LOG_ERROR;
    else if (av_level <= AV_LOG_WARNING)
        ijk_level = IJK_LOG_WARN;
    else if (av_level <= AV_LOG_INFO)
        ijk_level = IJK_LOG_INFO;
    // AV_LOG_VERBOSE means detailed info
    else if (av_level <= AV_LOG_VERBOSE)
        ijk_level = IJK_LOG_INFO;
    else if (av_level <= AV_LOG_DEBUG)
        ijk_level = IJK_LOG_DEBUG;
    else if (av_level <= AV_LOG_TRACE)
        ijk_level = IJK_LOG_VERBOSE;
    else
        ijk_level = IJK_LOG_VERBOSE;
    return ijk_level;
}

inline static int log_level_ijk_to_av(int ijk_level) {
    int av_level = IJK_LOG_VERBOSE;
    if (ijk_level >= IJK_LOG_SILENT)
        av_level = AV_LOG_QUIET;
    else if (ijk_level >= IJK_LOG_FATAL)
        av_level = AV_LOG_FATAL;
    else if (ijk_level >= IJK_LOG_ERROR)
        av_level = AV_LOG_ERROR;
    else if (ijk_level >= IJK_LOG_WARN)
        av_level = AV_LOG_WARNING;
    else if (ijk_level >= IJK_LOG_INFO)
        av_level = AV_LOG_INFO;
    // AV_LOG_VERBOSE means detailed info
    else if (ijk_level >= IJK_LOG_DEBUG)
        av_level = AV_LOG_DEBUG;
    else if (ijk_level >= IJK_LOG_VERBOSE)
        av_level = AV_LOG_TRACE;
    else if (ijk_level >= IJK_LOG_DEFAULT)
        av_level = AV_LOG_TRACE;
    else if (ijk_level >= IJK_LOG_UNKNOWN)
        av_level = AV_LOG_TRACE;
    else
        av_level = AV_LOG_TRACE;
    return av_level;
}

static void ffp_log_callback_brief(void* ptr, int level, const char* fmt, va_list vl) {
    if (level > av_log_get_level())
        return;

    int ffplv __unused = log_level_av_to_ijk(level);
    VLOG(ffplv, IJK_LOG_TAG, fmt, vl);
}

static void ffp_log_callback_report(void* ptr, int level, const char* fmt, va_list vl) {
    if (level > av_log_get_level())
        return;

    int ffplv __unused = log_level_av_to_ijk(level);

    va_list vl2;
    char line[1024];
    static int print_prefix = 1;

    va_copy(vl2, vl);
    av_log_format_line(ptr, level, fmt, vl2, line, sizeof(line), &print_prefix);
    va_end(vl2);

    ALOG(ffplv, IJK_LOG_TAG, "%s", line);
}

static void ffp_global_init_l() {
    if (g_ffmpeg_global_inited)
        return;

    global_session_id = 0;

    avcodec_register_all();
#if CONFIG_AVDEVICE
    avdevice_register_all();
#endif
#if CONFIG_AVFILTER
    avfilter_register_all();
#endif
    av_register_all();

    ijkav_register_all();

    avformat_network_init();

    av_lockmgr_register(lockmgr);
    av_log_set_callback(ffp_log_callback_brief);

    av_init_packet(&flush_pkt);
    flush_pkt.data = (uint8_t*) &flush_pkt;

    set_native_cache_log_callback(VLOG_KWAI);

    g_ffmpeg_global_inited = true;
}

void ffp_global_init() {
    pthread_mutex_lock(&g_init_mutex);
    ffp_global_init_l();
    pthread_mutex_unlock(&g_init_mutex);
}

void ffp_global_uninit() {
    // no need to lock g_init_mutex here, only called by Android JNI_OnUnload
    if (!g_ffmpeg_global_inited)
        return;

    av_lockmgr_register(NULL);

    avformat_network_deinit();

    g_ffmpeg_global_inited = false;
}

void ffp_global_set_log_report(int use_report) {
    if (use_report) {
        av_log_set_callback(ffp_log_callback_report);
    } else {
        av_log_set_callback(ffp_log_callback_brief);
    }
}

void ffp_global_set_log_level(int log_level) {
    int av_level = log_level_ijk_to_av(log_level);
    av_log_set_level(av_level);
}

void ffp_global_set_inject_callback(ijk_inject_callback cb) {
    ijkav_register_inject_callback(cb);
}

void ffp_io_stat_register(void (*cb)(const char* url, int type, int bytes)) {
    // avijk_io_stat_register(cb);
}

void ffp_io_stat_complete_register(void (*cb)(const char* url,
                                              int64_t read_bytes, int64_t total_size,
                                              int64_t elpased_time, int64_t total_duration)) {
    // avijk_io_stat_complete_register(cb);
}

static const char* ffp_context_to_name(void* ptr) {
    return "FFPlayer";
}


static void* ffp_context_child_next(void* obj, void* prev) {
    return NULL;
}

static const AVClass* ffp_context_child_class_next(const AVClass* prev) {
    return NULL;
}

const AVClass ffp_context_class = {
    .class_name       = "FFPlayer",
    .item_name        = ffp_context_to_name,
    .option           = ffp_context_options,
    .version          = LIBAVUTIL_VERSION_INT,
    .child_next       = ffp_context_child_next,
    .child_class_next = ffp_context_child_class_next,
};

/**
 * 只有在ffp_create/ffp_destroy的时候调用
 */
inline static void ffp_reset_internal(FFPlayer* ffp) {
    /* ffp->is closed in stream_close() */
    av_opt_free(ffp);

    /* format/codec options */
    av_dict_free(&ffp->format_opts);
    av_dict_free(&ffp->codec_opts);
    av_dict_free(&ffp->sws_dict);
    av_dict_free(&ffp->player_opts);
    av_dict_free(&ffp->swr_opts);

    /* ffplay options specified by the user */
    av_freep(&ffp->input_filename);
    av_freep(&ffp->cache_key);
    av_freep(&ffp->reload_audio_filename);
    ffp->audio_disable          = 0;
    ffp->video_disable          = 0;
    memset(ffp->wanted_stream_spec, 0, sizeof(ffp->wanted_stream_spec));
    ffp->seek_by_bytes          = -1;
    ffp->display_disable        = 0;
    ffp->show_status            = 0;
    ffp->av_sync_type           = AV_SYNC_AUDIO_MASTER;
    ffp->start_time             = AV_NOPTS_VALUE;
    ffp->duration               = AV_NOPTS_VALUE;
    ffp->fast                   = 1;
    ffp->genpts                 = 0;
    ffp->lowres                 = 0;
    ffp->decoder_reorder_pts    = -1;
    ffp->autoexit               = 0;
    ffp->loop                   = 1;
    ffp->enable_loop_on_error   = 0;
    ffp->exit_on_dec_error      = 0;
    ffp->framedrop              = 0; // option
    ffp->last_packet_drop       = 0;
    ffp->seek_at_start          = 0;
    ffp->infinite_buffer        = -1;
    ffp->enable_segment_cache   = 0;
    ffp->show_mode              = SHOW_MODE_NONE;
    av_freep(&ffp->audio_codec_name);
    av_freep(&ffp->video_codec_name);
    av_freep(&ffp->preferred_hevc_codec_name);
    av_freep(&ffp->http_redirect_info);
    ffp->rdftspeed              = 0.02;
#if CONFIG_AVFILTER
    av_freep(&ffp->vfilters_list);
    ffp->nb_vfilters            = 0;
    ffp->afilters               = NULL;
    ffp->vfilter0               = NULL;
#endif
    ffp->autorotate             = 1;

    ffp->sws_flags              = SWS_FAST_BILINEAR;

    /* current context */
    ffp->audio_callback_time    = 0;

    /* extra fields */
    ffp->aout                   = NULL; /* reset outside */
    ffp->vout                   = NULL; /* reset outside */
    ffp->pipeline               = NULL;
    ffp->node_vdec              = NULL;
    ffp->sar_num                = 0;
    ffp->sar_den                = 0;

    ffp->live_event_callback    = NULL;

    av_freep(&ffp->video_codec_info);
    av_freep(&ffp->audio_codec_info);
    ffp->overlay_format         = SDL_FCC_I420;

    ffp->kwai_error_code                = 0;
    ffp->prepared               = 0;
    ffp->auto_resume            = 0;
    ffp->error                  = 0;
    ffp->error_count            = 0;
    ffp->start_on_prepared      = 1;
    ffp->first_video_frame_rendered = 0;
    ffp->first_audio_frame_rendered = 0;
    ffp->sync_av_start          = 1;
    ffp->enable_accurate_seek   = 0;
    ffp->accurate_seek_timeout  = MAX_ACCURATE_SEEK_TIMEOUT;
    ffp->enable_cache_seek      = 0;

    ffp->enable_seek_forward_offset     = 1;
    ffp->playable_duration_ms           = 0;
    ffp->audio_invalid_duration         = 0;
    ffp->video_invalid_duration         = 0;
    ffp->audio_pts_invalid              = true;
    ffp->video_pts_invalid              = true;

    ffp->packet_buffering               = 1;
    ffp->pictq_size                     = VIDEO_PICTURE_QUEUE_SIZE_DEFAULT; // option
    ffp->max_fps                        = 31; // option

    ffp->enable_modify_block            = 0;

    ffp->aac_libfdk                     = 1; // option
    ffp->use_aligned_pts                = 1; // option
    ffp->vtb_max_frame_width            = 0; // option
    ffp->vtb_async                      = 0; // option
    ffp->vtb_wait_async                 = 0; // option
    ffp->vtb_h264                       = 0; // option
    ffp->vtb_h265                       = 0; // option
    ffp->vtb_auto_rotate                = 0; // option

    ffp->mediacodec_all_videos          = 0; // option
    ffp->mediacodec_avc                 = 0; // option
    ffp->mediacodec_hevc                = 0; // option
    ffp->mediacodec_mpeg2               = 0; // option
    ffp->mediacodec_auto_rotate         = 0; // option
    ffp->mediacodec_max_cnt             = 1; // option
    ffp->use_mediacodec_bytebuffer      = 0; // option

    ffp->opensles                       = 0; // option

    ffp->iformat_name                   = NULL; // option

    ffp->no_time_adjust                 = 0; // option

    ffp->muted                          = 0;
    ffp->volumes[AUDIO_VOLUME_LEFT]     = 1.0f;
    ffp->volumes[AUDIO_VOLUME_RIGHT]     = 1.0f;


    ijkmeta_reset(ffp->meta);

    SDL_SpeedSamplerReset(&ffp->vfps_sampler);
    SDL_SpeedSamplerReset(&ffp->vdps_sampler);
    SDL_SpeedSamplerReset(&ffp->vrps_sampler);

    /* filters */
    ffp->vf_changed                     = 0;
    ffp->af_changed                     = 0;
    ffp->pf_playback_rate               = DEFAULT_PLAYBACK_RATE;
    ffp->pf_playback_tone               = 0;
    ffp->i_buffer_time_max              = 2000;
    ffp->i_buffer_time_max_live_manifest = TIME_SPEED_UP_THRESHOLD_DEFAULT_MS;

    ffp->i_video_decoded_size           = 0;
    ffp->i_audio_decoded_size           = 0;
    ffp->timeout = 30 * 1000000;

    ffp->pf_playback_rate_changed       = 0;
    ffp->pf_playback_rate_is_sound_touch    = false;
    ffp->pf_playback_tone_is_sound_touch    = false;

    msg_queue_flush(&ffp->msg_queue);

    memset(&ffp->stat, 0, sizeof(ffp->stat));
    FFDemuxCacheControl_reset(&ffp->dcc);


    ffp->should_export_video_raw = false;
    ffp->should_export_process_audio_pcm = false;
    ffp->java_process_pcm_byte_buffer = NULL;
    ffp->weak_thiz = NULL;

    ffp->wall_clock_updated = false;
    ffp->wall_clock_offset = 0;
    LiveAbsTimeControl_init(&ffp->live_abs_time_control);
    ffp->qos_pts_offset = 0;
    ffp->qos_pts_offset_got = false;
    ffp->qos_dts_duration = 0;
    memset(&ffp->qos_delay_audio_render, 0, sizeof(ffp->qos_delay_audio_render));
    memset(&ffp->qos_delay_video_recv, 0, sizeof(ffp->qos_delay_video_recv));
    memset(&ffp->qos_delay_video_before_dec, 0, sizeof(ffp->qos_delay_video_before_dec));
    memset(&ffp->qos_delay_video_after_dec, 0, sizeof(ffp->qos_delay_video_after_dec));
    memset(&ffp->qos_delay_video_render, 0, sizeof(ffp->qos_delay_video_render));
    memset(&ffp->qos_speed_change, 0, sizeof(ffp->qos_speed_change));

    ffp->audio_speed_change_enable = 0;

    ffp->live_low_delay_buffer_time_max = 2000;   // low_delay default hurry-up threshold

    memset(&ffp->kflv_player_statistic, 0, sizeof(ffp->kflv_player_statistic));
    ClockTracker_reset(&ffp->clock_tracker);

    ffp->is_live_manifest = 0;
    ffp->live_manifest_last_decoding_flv_index = -1;
    ffp->live_manifest_switch_mode = -1;
    ffp->enable_vod_manifest = 0;
    ffp->vod_adaptive_rep_id = 0;
    ffp->audio_render_after_seek_need_notify = 0;
    ffp->video_rendered_after_seek_need_notify = 0;
    ffp->block_start_cnt = 0;

    ffp->is_audio_reloaded = false;
    ffp->last_audio_pts = AV_NOPTS_VALUE;

    ffp->is_video_reloaded = false;
    ffp->last_only_audio_pts = AV_NOPTS_VALUE;
    ffp->last_only_audio_pts_updated = false;
    ffp->reloaded_video_first_pts = AV_NOPTS_VALUE;
    ffp->last_vp_pts_before_audio_only = AV_NOPTS_VALUE;
    ffp->first_reloaded_v_frame_rendered = 0;
    ffp->last_audio_pts_updated = false;

    ffp->mix_type = -1;
    ffp->source_device_type = 0;
    ffp->live_voice_comment_time = 0;
    ffp->stat.maxAvDiffRealTime = 0;
    ffp->stat.minAvDiffRealTime = 0;
    ffp->stat.maxAvDiffTotalTime = 0;
    ffp->stat.minAvDiffTotalTime = 0;
    ffp->stat.max_video_dts_diff_ms = 0;
    ffp->stat.max_audio_dts_diff_ms = 0;
    ffp->stat.speed_changed_cnt = 0;
    ffp->is_loop_seek = false;

    ffp->enable_audio_spectrum = 0;
    ffp->audio_spectrum_processor = NULL;

    // kwai http redirect info
    av_freep(&ffp->http_redirect_info);

    AudioGain_reset(&(ffp->audio_gain));
    ac_player_statistic_destroy(&ffp->player_statistic);

    memset(ffp->st_index, -1, sizeof(ffp->st_index));
    ffp->async_stream_component_open = 0;

    C_DataSourceOptions_release(&ffp->data_source_opt);

    ffp->fade_in_end_time_ms = 0;
    ffp->is_hdr = false;
}

FFPlayer* ffp_create() {

    FFPlayer* ffp = (FFPlayer*) av_mallocz(sizeof(FFPlayer));
    if (!ffp)
        return NULL;

    msg_queue_init(&ffp->msg_queue);
    ffp->af_mutex = SDL_CreateMutex();
    ffp->vf_mutex = SDL_CreateMutex();
    ffp->volude_mutex = SDL_CreateMutex();
    ffp->pcm_process_mutex = SDL_CreateMutex();
    ffp->cache_avio_overrall_mutex = SDL_CreateMutex_Name("cache_avio_overrall_mutex");
    ffp->cache_avio_exist_mutex = SDL_CreateMutex_Name("cache_avio_exist_mutex");

    ffp_reset_internal(ffp);
    ffp->av_class = &ffp_context_class;
    ffp->meta = ijkmeta_create();

    av_opt_set_defaults(ffp);

    // kwai logic init
    global_session_id++;
    ffp->session_id = global_session_id;

    KwaiQos_init(&ffp->kwai_qos);
    CacheStatistic_init(&ffp->cache_stat);
    KwaiRotateControl_init(&ffp->kwai_rotate_control);
    AbLoop_init(&ffp->ab_loop);
    BufferLoop_init(&ffp->buffer_loop);
    AudioVolumeProgress_init(&ffp->audio_vol_progress);
    ffp->expect_use_cache = 0;
    ffp->cache_actually_used = false;
    KwaiPacketQueueBufferChecker_init(&ffp->kwai_packet_buffer_checker);
    DccAlgorithm_init(&ffp->dcc_algorithm);
    KwaiIoQueueObserver_init(&ffp->kwai_io_queue_observer);
    ffp->player_statistic = ac_player_statistic_create();

    return ffp;
}

void ffp_destroy(FFPlayer* ffp) {
    if (!ffp)
        return;

    if (ffp->is) {
        ALOGW("[%u] ffp_destroy_ffplayer: force stream_close()", ffp->session_id);
        stream_close(ffp);
        ffp->is = NULL;
    }
    if (ffp->cache_callback) {
        AwesomeCacheCallback_Opaque_delete(ffp->cache_callback);
        ffp->cache_callback = NULL;
    }

    SDL_VoutFreeP(&ffp->vout);
    SDL_AoutFreeP(&ffp->aout);
    ffpipenode_free_p(&ffp->node_vdec);
    ffpipeline_free_p(&ffp->pipeline);
    ijkmeta_destroy_p(&ffp->meta);

    ffp_reset_internal(ffp);

    KwaiQos_close(&ffp->kwai_qos);
    CacheStatistic_release(&ffp->cache_stat);

    SDL_DestroyMutexP(&ffp->af_mutex);
    SDL_DestroyMutexP(&ffp->vf_mutex);
    SDL_DestroyMutexP(&ffp->volude_mutex);
    msg_queue_destroy(&ffp->msg_queue);
    SDL_DestroyMutexP(&ffp->cache_avio_overrall_mutex);
    SDL_DestroyMutexP(&ffp->cache_avio_exist_mutex);
    SDL_DestroyMutexP(&ffp->pcm_process_mutex);

    PreDemux_destroy_p(&ffp->pre_demux);
    ac_player_statistic_destroy(&ffp->player_statistic);

    av_freep(&ffp);
}

void ffp_destroy_p(FFPlayer** pffp) {
    if (!pffp)
        return;

    ffp_destroy(*pffp);
    *pffp = NULL;
}

static AVDictionary** ffp_get_opt_dict(FFPlayer* ffp, int opt_category) {
    assert(ffp);

    switch (opt_category) {
        case FFP_OPT_CATEGORY_FORMAT:
            return &ffp->format_opts;
        case FFP_OPT_CATEGORY_CODEC:
            return &ffp->codec_opts;
        case FFP_OPT_CATEGORY_SWS:
            return &ffp->sws_dict;
        case FFP_OPT_CATEGORY_PLAYER:
            return &ffp->player_opts;
        case FFP_OPT_CATEGORY_SWR:
            return &ffp->swr_opts;
        default:
            ALOGE("[%u] unknown option category %d\n", ffp->session_id, opt_category);
            return NULL;
    }
}

void ffp_set_option(FFPlayer* ffp, int opt_category, const char* name, const char* value) {
    if (!ffp)
        return;

    AVDictionary** dict = ffp_get_opt_dict(ffp, opt_category);
    av_dict_set(dict, name, value, 0);
}

void ffp_set_option_int(FFPlayer* ffp, int opt_category, const char* name, int64_t value) {
    if (!ffp)
        return;

    AVDictionary** dict = ffp_get_opt_dict(ffp, opt_category);

    av_dict_set_int(dict, name, value, 0);
}

void ffp_set_overlay_format(FFPlayer* ffp, int chroma_fourcc) {
    switch (chroma_fourcc) {
        case SDL_FCC__GLES2:
        case SDL_FCC_I420:
        case SDL_FCC_YV12:
        case SDL_FCC_RV16:
        case SDL_FCC_RV24:
        case SDL_FCC_RV32:
        case SDL_FCC_NV21:
            ffp->overlay_format = chroma_fourcc;
            break;
#ifdef __APPLE__
        case SDL_FCC_I444P10LE:
            ffp->overlay_format = chroma_fourcc;
            break;
#endif
        default:
            ALOGE("[%u] ffp_set_overlay_format: unknown chroma fourcc: %d\n", ffp->session_id, chroma_fourcc);
            break;
    }
}

int ffp_get_video_codec_info(FFPlayer* ffp, char** codec_info) {
    if (!codec_info)
        return -1;

    // FIXME: not thread-safe
    if (ffp->video_codec_info) {
        *codec_info = strdup(ffp->video_codec_info);
    } else {
        *codec_info = NULL;
    }
    return 0;
}

int ffp_get_audio_codec_info(FFPlayer* ffp, char** codec_info) {
    if (!codec_info)
        return -1;

    // FIXME: not thread-safe
    if (ffp->audio_codec_info) {
        *codec_info = strdup(ffp->audio_codec_info);
    } else {
        *codec_info = NULL;
    }
    return 0;
}

static void ffp_show_dict(FFPlayer* ffp, const char* tag, AVDictionary* dict) {
    AVDictionaryEntry* t = NULL;

    while ((t = av_dict_get(dict, "", t, AV_DICT_IGNORE_SUFFIX))) {
        ALOGI("[%u] %-*s: %-*s = %s\n", ffp->session_id, 12, tag, 28, t->key, t->value);
    }
}

#define FFP_VERSION_MODULE_NAME_LENGTH 13
//static void ffp_show_version_str(FFPlayer* ffp, const char* module, const char* version) {
//    ALOGI("[%u] %-*s: %s\n", ffp->session_id, FFP_VERSION_MODULE_NAME_LENGTH, module, version);
//}

static void ffp_show_version_int(FFPlayer* ffp, const char* module, unsigned version) {
    ALOGI("[%u] %-*s: %u.%u.%u\n", ffp->session_id,
          FFP_VERSION_MODULE_NAME_LENGTH, module,
          (unsigned int)IJKVERSION_GET_MAJOR(version),
          (unsigned int)IJKVERSION_GET_MINOR(version),
          (unsigned int)IJKVERSION_GET_MICRO(version));
}

static void ffp_update_live_low_delay_max_buffer_time(FFPlayer* ffp) {
    if (ffp && ffp->live_low_delay_buffer_time_max > 0
        && ffp->live_low_delay_buffer_time_max != ffp->i_buffer_time_max) {
        ffp->i_buffer_time_max = ffp->live_low_delay_buffer_time_max;
    }
}

int ffp_reprepare_async_l(FFPlayer* ffp, const char* file_name, bool is_flush) {
    assert(ffp);
    assert(file_name);

    VideoState* is = stream_reopen(ffp, file_name, NULL, is_flush);
    if (!is) {
        ALOGW("[%u] ffp_reprepare_async_l: stream_reopen failed OOM", ffp->session_id);
        return EIJK_OUT_OF_MEMORY;
    }
    if (ffp->input_filename) {
        av_freep(&ffp->input_filename);
    }

    ffp->input_filename = av_strdup(file_name);
    is->abort_request = 0;
    is->read_abort_request = 0;
    return 0;
}

/**
 * 解析出url里转码类型
 * @return true表示解析出来pattern，false表示没找到pattern
 */

static bool parse_transcode_type_from_filename(FFPlayer* ffp, const char* filename) {
    if (!filename || strlen(filename) <= 0) {
        return false;
    }
    const char* pattern = "_(([a-zA-Z0-9]*)?)[_]";
    regex_t reg;

    bool found = false;
    int reg_c = regcomp(&reg, pattern, REG_EXTENDED);
    if (reg_c) {
        // error
        ALOGW("[%s] fail to regcomp", __func__);
    } else {
        regmatch_t pmatch[1];
        int ret = regexec(&reg, filename, 1, pmatch, 0);
        if (!ret) {
            // match!
            long long match_len = pmatch[0].rm_eo - pmatch[0].rm_so - 1;
            if (match_len > 1 && match_len < TRANSCODE_TYPE_MAX_LEN) {
                snprintf(ffp->trancode_type, match_len,
                         "%s", filename + pmatch->rm_so + 1);
                if (!ffp->islive) {
                    KwaiQos_setTranscodeType(&ffp->kwai_qos, ffp->trancode_type);
                }
                found = true;
            } else {
                // no need to log warning right now
                // ALOGW("[%s] match_len [%lld] <= 1 || >= %d, fail to find transcode type with regex for url:%s\n",
                //      __func__, match_len, TRANSCODE_TYPE_MAX_LEN, filename);
            }

        } else if (ret == REG_NOMATCH) {
            ALOGW("[%s] fail to find transcode type with regex for url:%s\n", __func__, filename);
        }
    }
    regfree(&reg);
    return found;
}

int ffp_prepare_async_l(FFPlayer* ffp, const char* file_name) {
    assert(ffp);
    assert(!ffp->is);
    assert(file_name);

    av_log(NULL, AV_LOG_INFO, "===== player options =====\n");
    ffp_show_dict(ffp, "player-opts", ffp->player_opts);
    av_log(NULL, AV_LOG_INFO, "===================\n");
    av_opt_set_dict(ffp, &ffp->player_opts);

    if (ffp->enable_vod_manifest) {
        int index = kwai_vod_manifest_init(&ffp->kwai_qos, ffp->format_opts, &ffp->vodplaylist, file_name);
        if (index < 0) {
            ALOGE("invalid vod manifest");
            return -1;
        }
        file_name = ffp->vodplaylist.rep[index].url;
        if (ffp->host) {
            av_freep(&ffp->host);
        }
        ffp->host = av_strdup(ffp->vodplaylist.rep[index].host);
        if (ffp->cache_key) {
            av_freep(&ffp->cache_key);
        }
        ffp->cache_key = av_strdup(ffp->vodplaylist.rep[index].key);
        av_dict_set(&ffp->format_opts, "enable_vod_adaptive", "1", 0);

        if (ffp->vodplaylist.rep[index].feature_p2sp) {
            av_dict_set_int(&ffp->format_opts, "cache-upstream-type", kP2spHttpDataSource, 0);
        }

        ffp->vod_adaptive_rep_id = ffp->vodplaylist.rep[index].id;

        char* headers = av_asprintf("Host: %s", ffp->host);
        av_dict_set(&ffp->format_opts, "headers", headers, 0);
        av_freep(&headers);
    }
    KwaiQos_onStartAlivePlayer(&ffp->kwai_qos);

    if (av_stristart(file_name, "rtmp", NULL) ||
        av_stristart(file_name, "rtsp", NULL)) {
        // There is total different meaning for 'timeout' option in rtmp
        ALOGW("[%u] remove 'timeout' option for rtmp.\n", ffp->session_id);
        av_dict_set(&ffp->format_opts, "timeout", NULL, 0);
    } else {
        if (NULL == av_dict_get(ffp->format_opts, "timeout", NULL, 0)) {
            av_dict_set_int(&ffp->format_opts, "timeout", ffp->timeout, 0);
        }
    }

    if (strlen(file_name) + 1 > 1024) {
        ALOGI("[%u] %s too long url\n", ffp->session_id, __FUNCTION__);
        if (avio_find_protocol_name("ijklongurl:")) {
            av_dict_set(&ffp->format_opts, "ijklongurl-url", file_name, 0);
            file_name = "ijklongurl:";
        }
    }

    ALOGI("[%u] ===== versions =====\n", ffp->session_id);
    // ffp_show_version_str(ffp, "FFmpeg",         av_version_info());
    ffp_show_version_int(ffp, "libavutil", avutil_version());
    ffp_show_version_int(ffp, "libavcodec", avcodec_version());
    ffp_show_version_int(ffp, "libavformat", avformat_version());
    ffp_show_version_int(ffp, "libswscale", swscale_version());
    ffp_show_version_int(ffp, "libswresample", swresample_version());
    ALOGI("[%u] ===== options =====\n", ffp->session_id);
    ffp_show_dict(ffp, "player-opts", ffp->player_opts);
    ffp_show_dict(ffp, "format-opts", ffp->format_opts);
    ffp_show_dict(ffp, "codec-opts ", ffp->codec_opts);
    ffp_show_dict(ffp, "sws-opts   ", ffp->sws_dict);
    ffp_show_dict(ffp, "swr-opts   ", ffp->swr_opts);
    ALOGI("[%u] ===================\n", ffp->session_id);


    av_opt_set_dict(ffp, &ffp->player_opts);

    bool found_transcode_type = parse_transcode_type_from_filename(ffp, file_name);

    // 春节视频的pattern：20sf720，20sf576，20sf540
    // 这段代码会一直存在这里，春节之后可以删掉这段代码
    if (found_transcode_type && (0 == strncmp("20sf", ffp->trancode_type, 4))) {
        av_dict_set_int(&ffp->format_opts, "is-sf2020-encrypt-source", 1, 0);
    }

    KwaiQos_collectPlayerStaticConfig(ffp, file_name);

    if (!ffp->aout) {
        ffp->aout = ffpipeline_open_audio_output(ffp->pipeline, ffp);
        if (!ffp->aout)
            return -1;
        SDL_AoutMuteAudio(ffp->aout, ffp->muted);
    }

#if CONFIG_AVFILTER
    if (ffp->vfilter0) {
        GROW_ARRAY(ffp->vfilters_list, ffp->nb_vfilters);
        ffp->vfilters_list[ffp->nb_vfilters - 1] = ffp->vfilter0;
    }
#endif

    // live low_delay status check
    if (ffp->islive && strstr(file_name, "lowDelay=1")) {
        ffp_update_live_low_delay_max_buffer_time(ffp);
    }

    AVDictionaryEntry* t = NULL;
    VideoState* is = NULL;
    if (ffp->is_live_manifest && !strcmp(file_name, "ijklongurl:") && (t = av_dict_get(ffp->format_opts, "ijklongurl-url", NULL, 0))) {
        ALOGI("[%u] %s, Live Manifest longurl: %s\n", ffp->session_id, __FUNCTION__, t->value);
        is = stream_open(ffp, t->value, NULL);
    } else {
        is = stream_open(ffp, file_name, NULL);
    }

    if (!is) {
        ALOGE("[%u] ffp_prepare_async_l: stream_open failed OOM, alive player cnt:%d",
              ffp->session_id, KwaiPlayerLifeCycle_get_current_alive_cnt_unsafe());
        return EIJK_OUT_OF_MEMORY;
    }

    ffp->is = is;
    ffp->input_filename = av_strdup(file_name);
    return 0;
}

int ffp_start_from_l(FFPlayer* ffp, long msec) {
    assert(ffp);

    VideoState* is = ffp->is;
    if (!is)
        return EIJK_NULL_IS_PTR;

    ffp->auto_resume = 1;
    ffp_toggle_buffering(ffp, 1, 0);
    ffp_seek_to_l(ffp, msec);
    return 0;
}

int ffp_start_l(FFPlayer* ffp) {
    assert(ffp);

    VideoState* is = ffp->is;
    if (!is)
        return EIJK_NULL_IS_PTR;

    toggle_pause(ffp, 0);
    KwaiQos_onAppCallStart(&ffp->kwai_qos, ffp);
    return 0;
}

int ffp_pause_l(FFPlayer* ffp) {
    assert(ffp);

    VideoState* is = ffp->is;
    if (!is)
        return EIJK_NULL_IS_PTR;

    toggle_pause(ffp, 1);
    KwaiQos_onAppCallPause(&ffp->kwai_qos);
    return 0;
}

int ffp_step_frame_l(FFPlayer* ffp) {
    assert(ffp);

    VideoState* is = ffp->is;
    if (!is)
        return EIJK_NULL_IS_PTR;

    step_to_next_frame(ffp);
    return 0;
}

int ffp_is_paused_l(FFPlayer* ffp) {
    assert(ffp);

    VideoState* is = ffp->is;
    if (!is)
        return 1;

    return is->paused;
}

int ffp_read_stop_l(FFPlayer* ffp) {
    assert(ffp);

    VideoState* is = ffp->is;
    if (is) {
        ffp_AwesomeCache_AVIOContext_abort(ffp);
        is->read_abort_request = 1;
    }
    ffp->start_time = AV_NOPTS_VALUE;

    return 0;
}

int ffp_reload_video_l(FFPlayer* ffp) {
    assert(ffp);

    VideoState* is = ffp->is;

    // proxy p2sp cases
    if (ffp->input_filename && strstr(ffp->input_filename, "http://127.0.0.1")) {
        ALOGI("[%d] Video reload not support P2SP \n", ffp->session_id);
        return -1;
    }

    if (!is)
        return -1;

    // 1.非live，直接退出
    // 2.如果当前不是audio_only播放，直接退出
    // 3.如果当前是audio_only并且同时是video_reload状态，
    //   说明是在等待audio或者video下载线程退出（弱网频繁切换前后台）,
    //   此时需要再次抛出RELOAD_VIDEO请求后再退出，确保切换回前台video
    //   播放命令不会丢掉
    // 4.如果is->read_tid非NULL，ffp->is_audio_reloaded为true的情
    //   况下说明此时audio_read_thread在等待ff_read thread结束，再次
    //   抛出RELOAD_VIDEO
    if (!ffp->islive || !ffp->is_audio_reloaded || ffp->is_video_reloaded || is->read_tid) {
        ALOGI("[%d] ffp_reload_video_l, islive=%d, is_audio_reloaded=%d, is_video_reloaded=%d, is->read_tid=%p\n",
              ffp->session_id, ffp->islive, ffp->is_audio_reloaded, ffp->is_video_reloaded, is->read_tid);
        if (ffp->islive) {
            if (!ffp->is_audio_reloaded) {
                ALOGI("[%d] ffp_reload_video_l, not audio only\n", ffp->session_id);
            } else {
                ALOGI("[%d] ffp_reload_video_l, in waiting, notify again in 10ms\n", ffp->session_id);
                av_usleep(10000);
                ffp_notify_msg1(ffp, FFP_REQ_LIVE_RELOAD_VIDEO);
            }
        }
        return -1;
    }

    ffp->is_video_reloaded = true;

    if (!ffp->input_filename) {
        return -1;
    }

    is->video_read_tid = SDL_CreateThreadEx(&is->_video_read_tid, video_read_thread, ffp, "video_reload");
    if (!is->video_read_tid) {
        ALOGE("[%d] SDL_CreateThread(video_reload): %s\n", ffp->session_id, SDL_GetError());
        // fixme: should let app do retry here
        return EIJK_OUT_OF_MEMORY;
    }

    return 0;
}

int ffp_reload_audio_l(FFPlayer* ffp) {
    assert(ffp);

    VideoState* is = ffp->is;

    // proxy p2sp cases
    if (ffp->input_filename && strstr(ffp->input_filename, "http://127.0.0.1")) {
        ALOGI("[%d] Audio Only not support P2SP \n", ffp->session_id);
        return -1;
    }

    if (!is)
        return -1;

    if (!ffp->islive || ffp->is_audio_reloaded) {
        ALOGI("[%d] %s \n", ffp->session_id,
              ffp->islive ? "Already reloaded audio" : "Non-Live not support reload audio");
        return -1;
    }

    ffp->is_audio_reloaded = true;

    if (ffp->reload_audio_filename)
        av_freep(&ffp->reload_audio_filename);

    if (ffp->input_filename) {
        ffp->reload_audio_filename = av_asprintf("%s&onlyaudio=yes", ffp->input_filename);
    } else {
        return -1;
    }

    is->audio_read_tid = SDL_CreateThreadEx(&is->_audio_read_tid, audio_read_thread, ffp, "audio_reload");
    if (!is->audio_read_tid) {
        ALOGE("[%d] SDL_CreateThread(audio_reload): %s\n", ffp->session_id, SDL_GetError());

        is->abort_request = true;
        if (is->video_refresh_tid) {
            SDL_WaitThread(is->video_refresh_tid, NULL);
            is->video_refresh_tid = NULL;
        }

        stream_close(ffp);
        ffp->is = NULL;
        return -1;
    }

    return 0;
}


int ffp_stop_l(FFPlayer* ffp) {
    assert(ffp);

    VideoState* is = ffp->is;
    if (is) {
        is->abort_request = 1;
        toggle_pause(ffp, 1);
    }

    ffp_AwesomeCache_AVIOContext_abort(ffp);
    ffp_interrupt_pre_demux_l(ffp);

    msg_queue_abort(&ffp->msg_queue);
    ffp->start_time = 0;

    if (ffp->enable_accurate_seek && is && is->accurate_seek_mutex
        && is->audio_accurate_seek_cond && is->video_accurate_seek_cond) {
        SDL_LockMutex(is->accurate_seek_mutex);
        is->audio_accurate_seek_req = 0;
        is->video_accurate_seek_req = 0;
        SDL_CondSignal(is->audio_accurate_seek_cond);
        SDL_CondSignal(is->video_accurate_seek_cond);
        SDL_UnlockMutex(is->accurate_seek_mutex);
    }

    KwaiQos_onStopAlivePlayer(&ffp->kwai_qos);

    return 0;
}

int ffp_wait_stop_l(FFPlayer* ffp, KwaiPlayerResultQos* result_qos) {
    assert(ffp);


    if (ffp->is) {
        ffp_stop_l(ffp);
        stream_close(ffp);
        if (result_qos != NULL) {
            KwaiPlayerResultQos_collect_result_qos(result_qos, ffp);
        }
        ffp->is = NULL;
    }
    return 0;
}

int ffp_seek_to_l(FFPlayer* ffp, long msec) {
    assert(ffp);

    VideoState* is = ffp->is;
    int64_t start_time = 0;
    int64_t seek_pos = milliseconds_to_fftime(msec);
    int64_t duration = milliseconds_to_fftime(ffp_get_duration_l(ffp));

    if (!is || !is->ic) {
        ALOGW("[%u] %s is || is->ic is null\n", ffp->session_id, __FUNCTION__);
        return EIJK_NULL_IS_PTR;
    }

    if (duration > 0 && seek_pos >= duration) {
        if (ffp->loop != 1 && (!ffp->loop || --ffp->loop)) {
            ALOGD("[%u] seek to End, stream_seek from start_time(%"PRId64")\n",
                  ffp->session_id, ffp->start_time != AV_NOPTS_VALUE ? ffp->start_time : 0);
            stream_seek(is, ffp->start_time != AV_NOPTS_VALUE ? ffp->start_time : 0, 0, 0);
        } else {
            toggle_pause(ffp, 1);
            ffp_notify_msg1(ffp, FFP_MSG_COMPLETED);
        }
        return 0;
    }

    start_time = is->ic->start_time;
    if (start_time > 0 && start_time != AV_NOPTS_VALUE)
        seek_pos += start_time;

    ALOGD("[%u] stream_seek %"PRId64"(%d) + %"PRId64", \n", ffp->session_id, seek_pos, (int)msec, start_time);
    stream_seek(is, seek_pos, 0, 0);
    ffp->is_loop_seek = false;
    return 0;
}

float ffp_get_probe_fps_l(FFPlayer* ffp) {
    if (!ffp)
        return 0.0;

    VideoState* is = ffp->is;
    if (!is)
        return 0.0;
    return is->probe_fps;
}

void  ffp_on_clock_changed(FFPlayer* ffp, Clock* clock) {
    if (!ffp || !clock) {
        return;
    }
    if (clock->is_ref_clock) {
        ClockTracker_on_ref_clock_updated(&ffp->clock_tracker, ffp_get_current_position_l(ffp));
    }
}

long ffp_get_current_position_l(FFPlayer* ffp) {
    assert(ffp);

    VideoState* is = ffp->is;
    if (!is || !is->ic)
        return 0;

    int64_t start_time = is->ic->start_time;
    int64_t start_diff = 0;
    if (start_time > 0 && start_time != AV_NOPTS_VALUE)
        start_diff = fftime_to_milliseconds(start_time);

    int64_t pos = 0;
    double pos_clock = get_master_clock(is);
    if (isnan(pos_clock)) {
        pos = fftime_to_milliseconds(is->seek_pos);
    } else {
        pos = pos_clock * 1000;
    }

    // If using REAL time and not ajusted, then return the real pos as calculated from the stream
    // the use case for this is primarily when using a custom non-seekable data source that starts
    // with a buffer that is NOT the start of the stream.  We want the get_current_position to
    // return the time in the stream, and not the player's internal clock.
    if (ffp->no_time_adjust) {
        return (long) pos;
    }

    if (pos < 0 || pos < start_diff)
        return 0;

    int64_t adjust_pos = pos - start_diff;
    return (long) adjust_pos;
}

long ffp_get_duration_l(FFPlayer* ffp) {
    assert(ffp);

    VideoState* is = ffp->is;
    if (!is || !is->ic)
        return 0;

    // 因为mp4有editlist，转码后的音频前两帧(100ms左右)会被丢弃，导致视频描述的duration和真实播放的duration不一致
    // 之前的逻辑使用audio_invalid_duration，这个要经过demux之后才能实时获得，导致客户端获取的这个值不是稳定的
    // 因此使用video_duration和audio_duration(已经减去discard的音频)的最小值，提供给客户端一个稳定的duration
    // 针对某些视频（如hls）无法获取audio_duration 和 video_duration 直接使用ic->duration

    int64_t duration = 0;
    if (is->video_duration > 0 || is->audio_duration > 0) {
        duration = fftime_to_milliseconds(FFMIN(is->ic->duration, FFMAX(is->video_duration, is->audio_duration)));
    } else {
        duration = fftime_to_milliseconds(is->ic->duration);
    }

    // ffplay原生逻辑
//    if (is->video_duration > is->audio_duration)
//        duration = fftime_to_milliseconds(is->ic->duration - ffp->video_invalid_duration);
//    else
//        duration = fftime_to_milliseconds(is->ic->duration - ffp->audio_invalid_duration);

    if (duration < 0)
        return 0;

    return (long) duration;
}

long ffp_get_playable_duration_l(FFPlayer* ffp) {
    if (!ffp)
        return 0;

    return (long) ffp->playable_duration_ms;
}

bool ffp_is_hw_l(FFPlayer* ffp) {
    if (!ffp)
        return 0;

    return ffp->hardware_vdec;
}

void ffp_set_loop(FFPlayer* ffp, int loop) {
    if (!ffp)
        return;
    ffp->loop = loop;
}

int ffp_get_loop(FFPlayer* ffp) {
    if (!ffp)
        return 1;
    return ffp->loop;
}

int ffp_packet_queue_init(PacketQueue* q) {
    return packet_queue_init(q);
}

void ffp_packet_queue_destroy(PacketQueue* q) {
    return packet_queue_destroy(q);
}

void ffp_packet_queue_abort(PacketQueue* q) {
    return packet_queue_abort(q);
}

void ffp_packet_queue_start(PacketQueue* q) {
    return packet_queue_start(q);
}

void ffp_packet_queue_flush(PacketQueue* q) {
    return packet_queue_flush(q);
}

int ffp_packet_queue_get(PacketQueue* q, AVPacket* pkt, int block, int* serial) {
    return packet_queue_get(q, pkt, block, serial, NULL);
}

int ffp_packet_queue_get_with_abs_time(FFPlayer* ffp, PacketQueue* q, AVPacket* pkt, int* serial, int* finished, int64_t* abs_time) {
    AVPacketTime packet_time;
    int ret = -1;
    ret = packet_queue_get_or_buffering(ffp, q, pkt, serial, &packet_time, finished);
    *abs_time = packet_time.abs_pts;
    return ret;
}

int ffp_packet_queue_get_or_buffering(FFPlayer* ffp, PacketQueue* q, AVPacket* pkt, int* serial, int* finished) {
    return packet_queue_get_or_buffering(ffp, q, pkt, serial, NULL, finished);
}

int ffp_packet_queue_put(PacketQueue* q, AVPacket* pkt) {
    return packet_queue_put(q, pkt, NULL);
}

bool ffp_is_flush_packet(AVPacket* pkt) {
    if (!pkt)
        return false;

    return pkt->data == flush_pkt.data;
}

Frame* ffp_frame_queue_peek_writable(FrameQueue* f) {
    return frame_queue_peek_writable(f);
}

void ffp_frame_queue_push(FrameQueue* f) {
    return frame_queue_push(f);
}

int ffp_queue_picture_with_abs_time(FFPlayer* ffp, AVFrame* src_frame, double pts, double duration, int64_t pos, int serial, int64_t abs_time) {
    AVPacketTime packet_time;
    packet_time.abs_pts = abs_time;
    return queue_picture(ffp, src_frame, pts, duration, pos, serial, &packet_time);
}

int ffp_queue_picture(FFPlayer* ffp, AVFrame* src_frame, double pts, double duration, int64_t pos, int serial) {
    return queue_picture(ffp, src_frame, pts, duration, pos, serial, NULL);
}

int ffp_get_master_sync_type(VideoState* is) {
    return get_master_sync_type(is);
}

double ffp_get_master_clock(VideoState* is) {
    return get_master_clock(is);
}

void ffp_toggle_buffering_l(FFPlayer* ffp, int buffering_on, int is_block) {
    if (!ffp->packet_buffering)
        return;

    VideoState* is = ffp->is;
    if (buffering_on && !is->buffering_on) {
        ALOGD("[%u] ffp_toggle_buffering_l: start\n", ffp->session_id);
        is->buffering_on = 1;
        stream_update_pause_l(ffp);
        if (is_block) {
            ffp_notify_msg1(ffp, FFP_MSG_BUFFERING_START);
            ffp->block_start_cnt++;
        }

        if (is->realtime) {
            FFDemuxCacheControl_decrease_buffer_time_live(&ffp->dcc);
        }

        KwaiQos_onBufferingStart(ffp, is_block);
    } else if (!buffering_on && is->buffering_on) {
        ALOGD("[%u] ffp_toggle_buffering_l: end\n", ffp->session_id);
        is->buffering_on = 0;
        stream_update_pause_l(ffp);
        if (is_block && ffp->block_start_cnt) {
            ffp_notify_msg1(ffp, FFP_MSG_BUFFERING_END);
            ffp->block_start_cnt--;
        }
        // update last_buffering_end_time
        ffp->dcc.last_buffering_end_time = SDL_GetTickHR();

        KwaiQos_onBufferingEnd(ffp, is_block);
    }
}

/**
 *
 * @param ffp 播放器实例
 * @param start_buffering 0: 开始缓冲 1: 结束缓冲
 * @param is_block 表示这次是和卡顿有关的toggle_buffering
 */
void ffp_toggle_buffering(FFPlayer* ffp, int start_buffering, int is_block) {
    SDL_LockMutex(ffp->is->play_mutex);
    ffp_toggle_buffering_l(ffp, start_buffering, is_block);
    SDL_UnlockMutex(ffp->is->play_mutex);
}

void
ffp_track_statistic_l(FFPlayer* ffp, AVStream* st, PacketQueue* q, FFTrackCacheStatistic* cache) {
    assert(cache);

    if (q) {
        cache->bytes = q->size;
        cache->packets = q->nb_packets;
    }

    if (q && st && st->time_base.den > 0 && st->time_base.num > 0) {
        cache->duration = q->duration * av_q2d(st->time_base) * 1000;
    }
}

static void ffp_audio_statistic_l_with_update(FFPlayer* ffp, int do_update) {
    VideoState* is = ffp->is;
    ffp_track_statistic_l(ffp, is->audio_st, &is->audioq, &ffp->stat.audio_cache);

    if (is->buffering_on || !is->audio_st || !ffp->first_audio_frame_rendered) {
        // The buffer size is used by the datasource to determine if current reading data is urgent.
        // If the player is buffering or havn't started playing,
        // we should treat it as urgent.
        ac_player_statistic_set_audio_buffer(ffp->player_statistic, 0, 0);
    } else {
        ac_player_statistic_set_audio_buffer(ffp->player_statistic,
                                             ffp->stat.audio_cache.duration, ffp->stat.audio_cache.bytes);
    }
    if (do_update) {
        ac_player_statistic_update(ffp->player_statistic);
    }

}

static void ffp_video_statistic_l_with_update(FFPlayer* ffp, int do_update) {
    VideoState* is = ffp->is;
    ffp_track_statistic_l(ffp, is->video_st, &is->videoq, &ffp->stat.video_cache);

    // see ffp_audio_statistic_l
    if (is->buffering_on || !is->video_st || !ffp->first_video_frame_rendered) {
        ac_player_statistic_set_video_buffer(ffp->player_statistic, 0, 0);
    } else {
        ac_player_statistic_set_video_buffer(ffp->player_statistic,
                                             ffp->stat.video_cache.duration, ffp->stat.video_cache.bytes);
    }

    if (do_update) {
        ac_player_statistic_update(ffp->player_statistic);
    }
}

inline static void ac_player_statistic_collect_read_position(FFPlayer* ffp) {
    SDL_LockMutex(ffp->cache_avio_exist_mutex);
    if (ffp->cache_avio_context) {
        ac_player_statistic_set_read_position(
            ffp->player_statistic,
            avio_tell(ffp->cache_avio_context));
    }
    SDL_UnlockMutex(ffp->cache_avio_exist_mutex);
}

void ffp_audio_statistic_l(FFPlayer* ffp) {
    ffp_audio_statistic_l_with_update(ffp, 1);
}

void ffp_video_statistic_l(FFPlayer* ffp) {
    ffp_video_statistic_l_with_update(ffp, 1);
}

void ffp_statistic_l(FFPlayer* ffp) {
    ffp_audio_statistic_l_with_update(ffp, 0);
    ffp_video_statistic_l_with_update(ffp, 0);
    ac_player_statistic_collect_read_position(ffp);

    ac_player_statistic_update(ffp->player_statistic);
}

/**
 * used by audio render thread and io thread
 * 1.判断是否满足开播条件，终止buffering
 * 2.计算当前的buffer percent和playableDuration percent，并post_msg给上层 bufferUpdateListener
 * @param ffp FFPlayer
 * @param is_eof 一般是read_thread线程读到eof的时候会用到，其他时候填false即可
 */
void ffp_check_buffering_l(FFPlayer* ffp, bool is_eof) {
    VideoState* is = ffp->is;
    int hwm_in_ms = ffp->dcc.current_high_water_mark_in_ms; // use fast water mark for first loading
    int buf_size_percent = -1;
    int buf_time_percent = -1;
    int hwm_in_bytes = ffp->dcc.high_water_mark_in_bytes;
    int need_end_buffering = 0;
    int audio_time_base_valid = 0;
    int video_time_base_valid = 0;
    int64_t buf_time_position = -1;

    if (is->audio_st)
        audio_time_base_valid = is->audio_st->time_base.den > 0 && is->audio_st->time_base.num > 0;
    if (is->video_st)
        video_time_base_valid = is->video_st->time_base.den > 0 && is->video_st->time_base.num > 0;

    if (hwm_in_ms > 0) {
        int cached_duration_in_ms = -1;
        int64_t audio_cached_duration = -1;
        int64_t video_cached_duration = -1;

        if (is->audio_st && audio_time_base_valid) {
            audio_cached_duration = ffp->stat.audio_cache.duration;
#ifdef FFP_SHOW_DEMUX_CACHE
            int audio_cached_percent = (int)av_rescale(audio_cached_duration, 1005, hwm_in_ms * 10);
            ALOGD("[%u] audio cache=%%%d milli:(%d/%d) bytes:(%d/%d) packet:(%d/%d)\n",
                  ffp->session_id, audio_cached_percent,
                  (int)audio_cached_duration, hwm_in_ms,
                  is->audioq.size, hwm_in_bytes,
                  is->audioq.nb_packets, MIN_FRAMES);
#endif
        }

        if (is->video_st && video_time_base_valid) {
            video_cached_duration = ffp->stat.video_cache.duration;
#ifdef FFP_SHOW_DEMUX_CACHE
            int video_cached_percent = (int)av_rescale(video_cached_duration, 1005, hwm_in_ms * 10);
            ALOGD("[%u] video cache=%%%d milli:(%d/%d) bytes:(%d/%d) packet:(%d/%d)\n",
                  ffp->session_id, video_cached_percent,
                  (int)video_cached_duration, hwm_in_ms,
                  is->videoq.size, hwm_in_bytes,
                  is->videoq.nb_packets, MIN_FRAMES);
#endif
        }

        if (video_cached_duration > 0 && audio_cached_duration > 0) {
            cached_duration_in_ms = (int) IJKMIN(video_cached_duration, audio_cached_duration);
        } else if (video_cached_duration > 0) {
            cached_duration_in_ms = (int) video_cached_duration;
        } else if (audio_cached_duration > 0) {
            cached_duration_in_ms = (int) audio_cached_duration;
        }

        if (cached_duration_in_ms >= 0) {
            buf_time_position = ffp_get_current_position_l(ffp) + cached_duration_in_ms;
            ffp->playable_duration_ms = buf_time_position;

            buf_time_percent = (int) av_rescale(cached_duration_in_ms, 1005, hwm_in_ms * 10);
#ifdef FFP_SHOW_DEMUX_CACHE
            ALOGD("[%u] time cache=%%%d (%d/%d)\n", ffp->session_id, buf_time_percent, cached_duration_in_ms, hwm_in_ms);
#endif
#ifdef FFP_NOTIFY_BUF_TIME
            ffp_notify_msg3(ffp, FFP_MSG_BUFFERING_TIME_UPDATE, cached_duration_in_ms, hwm_in_ms);
#endif
        }
    }

    int cached_size = is->audioq.size + is->videoq.size;
    if (hwm_in_bytes > 0) {
        buf_size_percent = (int) av_rescale(cached_size, 1005, hwm_in_bytes * 10);
#ifdef FFP_SHOW_DEMUX_CACHE
        ALOGD("[%u] size cache=%%%d (%d/%d)\n", ffp->session_id, buf_size_percent, cached_size, hwm_in_bytes);
#endif
#ifdef FFP_NOTIFY_BUF_BYTES
        ffp_notify_msg3(ffp, FFP_MSG_BUFFERING_BYTES_UPDATE, cached_size, hwm_in_bytes);
#endif
    }

    int buf_percent = -1;
    if (buf_time_percent >= 0) {
        if (buf_time_percent >= 100)
            need_end_buffering = 1;
        buf_percent = buf_time_percent;
    } else {
        if (buf_size_percent >= 100)
            need_end_buffering = 1;
        buf_percent = buf_size_percent;
    }

    if (buf_time_percent >= 0 && buf_size_percent >= 0) {
        buf_percent = FFMIN(buf_time_percent, buf_size_percent);
    }

    if (buf_percent) {
#if FFP_SHOW_BUF_POS
        ALOGD("[%u][%s] buf_time_position=%"PRId64", buf_percent: %d%\n",
              ffp->session_id, __func__, buf_time_position, buf_percent);
#endif
        if ((buf_time_position != ffp->buffer_update_control.last_report_buf_time_position
             && buf_percent != ffp->buffer_update_control.last_report_buf_hwm_percent)
            || is_eof
           ) {
            // 计算percent
            long percent = 0;
            long duration = ffp_get_duration_l(ffp);
            if (duration > 0) {
                percent = (long)buf_time_position * 100 / duration;
            }

            // 兼容逻辑，一般很少有视频可以缓冲到100%，有两个原因：
            // 1.视频和音频跟ic->duration的长度不会完全一致
            // 2.frameQueue里有一次些信息，而目前的playableDuration是基于 current_postion+packet_buffer的
            if (percent > 90 && is_eof) {
                // 大于90才能才能认为是正常读到结尾的eof，不然也可能是出错导致的，虽然这个回调对出错并不在意
                percent = 100;
            }

            percent = percent >= 100 ? 100 : percent;

            ffp_notify_msg3(ffp, FFP_MSG_BUFFERING_UPDATE, percent, buf_percent);

            ffp->buffer_update_control.last_report_buf_time_position = buf_time_position;
            ffp->buffer_update_control.last_report_buf_hwm_percent = buf_percent;

#if FFP_SHOW_BUF_POS
            ALOGD("[%u][%s] percent:%d, buf_percent:%d, buf_time_position:%lld, duration:%ld, duration-buf_time_position:%lld \n",
                  ffp->session_id, __func__, percent, buf_percent, buf_time_position, duration, duration - buf_time_position);
#endif
        }
    }

    if (is->realtime) {
        if (ffp->dcc.last_high_water_mark_in_ms > ffp->i_buffer_time_max - 300)
            ffp->dcc.last_high_water_mark_in_ms = ffp->i_buffer_time_max - 300;
        if (is->i_buffer_time_max != ffp->i_buffer_time_max)
            sync_chasing_threshold(ffp);
    }

    if (need_end_buffering
        && ffp->kwai_packet_buffer_checker.func_check_can_start_play(&ffp->kwai_packet_buffer_checker, ffp)
       ) {
        if (ffp->is->realtime) {
            FFDemuxCacheControl_increase_buffer_time_live(&ffp->dcc, hwm_in_ms);
        } else {
            FFDemuxCacheControl_increase_buffer_time_vod(&ffp->dcc, hwm_in_ms);
        }

        FFDemuxCacheControl_print(&ffp->dcc);
        if (is->buffer_indicator_queue && is->buffer_indicator_queue->nb_packets > 0) {
            // 和IJK官方逻辑（注释部分）的不同点（暂时不改，以后有机会用ab调研这块的影响）：
            // 1.audio && video满足条件，我们是||
            // 2. nb_packets >= MIN_MIN_FRAMES,我们是 >
//            if ((is->audioq.nb_packets >= MIN_MIN_FRAMES || is->audio_stream < 0 || is->audioq.abort_request)
//                && (is->videoq.nb_packets >= MIN_MIN_FRAMES || is->video_stream < 0 || is->videoq.abort_request)) {
//                ffp_toggle_buffering(ffp, 0, 1);
//            }
            if ((is->audioq.nb_packets > MIN_MIN_FRAMES || is->audio_stream < 0 || is->audioq.abort_request)
                || (is->videoq.nb_packets > MIN_MIN_FRAMES || is->video_stream < 0 || is->videoq.abort_request)) {
                ffp_toggle_buffering(ffp, 0, 1);
            }
        }
    }
}

int ffp_video_thread(FFPlayer* ffp) {
    return ffplay_video_thread(ffp);
}

void ffp_set_video_codec_info(FFPlayer* ffp, const char* codec, const char* decoder) {
    av_freep(&ffp->video_codec_info);
    ffp->video_codec_info = av_asprintf("%s, %s%s", decoder ? decoder : "", codec ? codec : "",
                                        ffp->is_hdr ? ", hdr" : "");
    ALOGI("[%u] VideoCodec: %s\n", ffp->session_id, ffp->video_codec_info);
    KwaiQos_setVideoCodecInfo(&ffp->kwai_qos, ffp->video_codec_info);
}

void ffp_set_audio_codec_info(FFPlayer* ffp, const char* codec, const char* decoder) {
    av_freep(&ffp->audio_codec_info);
    ffp->audio_codec_info = av_asprintf("%s, %s", decoder ? decoder : "", codec ? codec : "");
    ALOGI("[%u] AudioCodec: %s\n", ffp->session_id, ffp->audio_codec_info);
    KwaiQos_setAudioCodecInfo(&ffp->kwai_qos, ffp->audio_codec_info);
}

void ffp_set_bufferTimeMax(FFPlayer* ffp, float max) {
    if (!ffp)
        return;
    if (max <= 0)
        max = 0;
    ffp->i_buffer_time_max = (int)(max * 1000);
}
void ffp_set_playback_rate(FFPlayer* ffp, float rate, bool is_sound_touch) {
    if (!ffp)
        return;
    if (abs(ffp->pf_playback_rate - rate) >= 0.01) {
        ffp->stat.speed_changed_cnt++;
    }
    ffp->pf_playback_rate = rate;
    ffp->pf_playback_rate_changed = 1;
    ffp->pf_playback_rate_is_sound_touch = is_sound_touch;
}

void ffp_set_playback_tone(FFPlayer* ffp, int tone) {
    if (!ffp)
        return;

    ffp->pf_playback_tone = tone;
    ffp->pf_playback_tone_is_sound_touch = true;
}


void ffp_set_live_manifest_switch_mode(FFPlayer* ffp, int mode) {
    if (!ffp)
        return;

    ffp->live_manifest_switch_mode = mode;
    KwaiQos_onSetLiveManifestSwitchMode(&ffp->kwai_qos, mode);
}

int ffp_get_video_rotate_degrees(FFPlayer* ffp) {
    VideoState* is = ffp->is;
    if (!is)
        return 0;

    int theta = abs((int)((int64_t) round(fabs(get_rotation(is->video_st))) % 360));
    switch (theta) {
        case 0:
        case 90:
        case 180:
        case 270:
            break;
        case 360:
            theta = 0;
            break;
        default:
            ALOGW("[%u] Unknown rotate degress: %d\n", ffp->session_id, theta);
            theta = 0;
            break;
    }

    return theta;
}

int ffp_set_stream_selected(FFPlayer* ffp, int stream, int selected) {
    VideoState* is = ffp->is;
    AVFormatContext* ic = NULL;
    AVCodecContext* avctx = NULL;
    if (!is)
        return -1;
    ic = is->ic;
    if (!ic)
        return -1;

    if (stream < 0 || stream >= ic->nb_streams) {
        ALOGE("[%u] invalid stream index %d >= stream number (%d)\n",
              ffp->session_id, stream, ic->nb_streams);
        return -1;
    }

    avctx = ic->streams[stream]->codec;

    if (selected) {
        switch (avctx->codec_type) {
            case AVMEDIA_TYPE_VIDEO:
                if (stream != is->video_stream && is->video_stream >= 0)
                    stream_component_close(ffp, is->video_stream);
                break;
            case AVMEDIA_TYPE_AUDIO:
                if (stream != is->audio_stream && is->audio_stream >= 0)
                    stream_component_close(ffp, is->audio_stream);
                break;
            default:
                ALOGE("[%u] select invalid stream %d of video type %d\n", ffp->session_id, stream, avctx->codec_type);
                return -1;
        }
        return stream_component_open(ffp, stream);
    } else {
        switch (avctx->codec_type) {
            case AVMEDIA_TYPE_VIDEO:
                if (stream == is->video_stream)
                    stream_component_close(ffp, is->video_stream);
                break;
            case AVMEDIA_TYPE_AUDIO:
                if (stream == is->audio_stream)
                    stream_component_close(ffp, is->audio_stream);
                break;
            default:
                ALOGE("[%u] select invalid stream %d of audio type %d\n", ffp->session_id, stream, avctx->codec_type);
                return -1;
        }
        return 0;
    }
}

float ffp_get_property_float(FFPlayer* ffp, int id, float default_value) {
    switch (id) {
        case FFP_PROP_FLOAT_BUFFERSIZE_MAX:
            return ffp->i_buffer_time_max / 1000.0;
        case FFP_PROP_FLOAT_VIDEO_DECODE_FRAMES_PER_SECOND:
            return ffp ? ffp->stat.vdps : default_value;
        case FFP_PROP_FLOAT_VIDEO_OUTPUT_FRAMES_PER_SECOND:
            return ffp ? ffp->stat.vfps : default_value;
        case FFP_PROP_FLOAT_PLAYBACK_RATE:
            return ffp ? ffp->pf_playback_rate : default_value;
        case FFP_PROP_FLOAT_AVDELAY:
            return ffp ? ffp->stat.avdelay : default_value;
        case FFP_PROP_FLOAT_AVDIFF:
            return ffp ? ffp->stat.avdiff : default_value;
        case FFP_PROP_FLOAT_MAX_AVDIFF_REALTIME:
            if (ffp) {
                float maxAvDiff = ffp->stat.maxAvDiffRealTime;
                ffp->stat.maxAvDiffRealTime = 0;
                return maxAvDiff;
            } else {
                return default_value;
            }
        case FFP_PROP_FLOAT_MIN_AVDIFF_REALTIME:
            if (ffp) {
                float minAvDiff = ffp->stat.minAvDiffRealTime;
                ffp->stat.minAvDiffRealTime = 0;
                return minAvDiff;
            } else {
                return default_value;
            }
        case FFP_PROP_FLOAT_VIDEO_AVG_FPS: {
            float fps = 0.0f;
            if (ffp) {
                fps = ffp->kwai_qos.media_metadata.fps;
            }
            return fps;
        }
        case FFP_PROP_FLOAT_AVERAGE_DISPLAYED_FPS:
            return ffp ? KwaiQos_getAppAverageFps(&ffp->kwai_qos) : default_value;
        default:
            return default_value;
    }
}

void ffp_set_property_float(FFPlayer* ffp, int id, float value) {
    switch (id) {
        case FFP_PROP_FLOAT_BUFFERSIZE_MAX:
            ffp_set_bufferTimeMax(ffp, value);
            break;
        default:
            return;
    }
}

int64_t ffp_get_property_int64(FFPlayer* ffp, int id, int64_t default_value) {
    switch (id) {
        case FFP_PROP_INT64_SELECTED_VIDEO_STREAM:
            if (!ffp || !ffp->is)
                return default_value;
            return ffp->is->video_stream;
        case FFP_PROP_INT64_SELECTED_AUDIO_STREAM:
            if (!ffp || !ffp->is)
                return default_value;
            return ffp->is->audio_stream;
        case FFP_PROP_INT64_VIDEO_DECODER:
            if (!ffp)
                return default_value;
            return get_video_decode_type(ffp);
        case FFP_PROP_INT64_AUDIO_DECODER:
            return FFP_PROPV_DECODER_AVCODEC;
        case FFP_PROP_INT64_VIDEO_CACHED_DURATION:
            if (!ffp)
                return default_value;
            return ffp->stat.video_cache.duration;
        case FFP_PROP_INT64_AUDIO_CACHED_DURATION:
            if (!ffp)
                return default_value;
            return ffp->stat.audio_cache.duration;
        case FFP_PROP_INT64_VIDEO_CACHED_BYTES:
            if (!ffp)
                return default_value;
            return ffp->stat.video_cache.bytes;
        case FFP_PROP_INT64_AUDIO_CACHED_BYTES:
            if (!ffp)
                return default_value;
            return ffp->stat.audio_cache.bytes;
        case FFP_PROP_INT64_VIDEO_CACHED_PACKETS:
            if (!ffp)
                return default_value;
            return ffp->stat.video_cache.packets;
        case FFP_PROP_INT64_AUDIO_CACHED_PACKETS:
            if (!ffp)
                return default_value;
            return ffp->stat.audio_cache.packets;
        case FFP_PROP_INT64_CPU:
            return ffp ? ffp->kwai_qos.system_performance.last_process_cpu : default_value;
        case FFP_PROP_INT64_MEMORY:
            return ffp ? ffp->kwai_qos.system_performance.last_process_memory_size_kb : default_value;
        case FFP_PROP_INT64_BUFFERTIME:
            return ffp ? KwaiQos_getBufferTotalDurationMs(&ffp->kwai_qos) : default_value;
        case FFP_PROP_INT64_BLOCKCNT:
            return ffp ? ffp->kwai_qos.runtime_stat.block_cnt : default_value;
        case FFP_PROP_INT64_BIT_RATE:
            return ffp ? ffp->stat.bit_rate : default_value;
        case FFP_PROP_INT64_LATEST_SEEK_LOAD_DURATION:
            return ffp ? ffp->stat.latest_seek_load_duration : default_value;
        case FFP_PROP_INT64_TRAFFIC_STATISTIC_BYTE_COUNT:
            return ffp ? ffp->stat.byte_count : default_value;
        case FFP_PROP_INT64_VIDEO_WIDTH:
            return (ffp && ffp->is && ffp->is->viddec.avctx) ? ffp->is->viddec.avctx->width : 0;
        case FFP_PROP_INT64_VIDEO_HEIGHT:
            return (ffp && ffp->is && ffp->is->viddec.avctx) ? ffp->is->viddec.avctx->height : 0;
        case FFP_PROP_LONG_READ_DATA:
            return ffp ? (ffp->i_video_decoded_size + ffp->i_audio_decoded_size) : default_value;
        case FFP_PROP_LONG_DOWNLOAD_SIZE:
            if (ffp && ffp->is)
                return (ffp->is->bytes_read / 1024);
            else
                return 0;
        case FFP_PROP_INT64_DTS_DURATION:
            return ffp ? ffp->qos_dts_duration : default_value;
        case FFP_PROP_INT64_VIDEO_DEC_ERROR_COUNT:
            return ffp ? ffp->error_count : default_value;
        case FFP_PROP_INT64_DROPPED_DURATION:
            return ffp ? ffp->kwai_qos.runtime_stat.total_dropped_duration : default_value;
        case FFP_PROP_INT64_READ_VIDEO_FRAME_COUNT:
            return ffp ? ffp->kwai_qos.runtime_stat.v_read_frame_count : default_value;
        case FFP_PROP_INT64_DECODED_VIDEO_FRAME_COUNT:
            return ffp ? ffp->kwai_qos.runtime_stat.v_decode_frame_count : default_value;
        case FFP_PROP_INT64_DISPLAYED_FRAME_COUNT:
            return ffp ? ffp->kwai_qos.runtime_stat.render_frame_count : default_value;
        case FFP_PROP_INT64_SOURCE_DEVICE_TYPE:
            return ffp ? ffp->source_device_type : default_value;
        case FFP_PROP_INT64_AUDIO_BUF_SIZE:
            if (ffp && ffp->aout)
                return SDL_GetBufSize(ffp->aout);
        case FFP_PROP_INT64_CURRENT_ABSOLUTE_TIME:
            return ffp ? LiveAbsTimeControl_cur_abs_time(&ffp->live_abs_time_control)
                   : default_value;
        case FFP_PROP_INT64_VOD_ADAPTIVE_REP_ID:
            return ffp ? ffp->vod_adaptive_rep_id : default_value;
            break;
        default:
            return default_value;
    }
}

void ffp_set_property_int64(FFPlayer* ffp, int id, int64_t value) {
    switch (id) {
        default:
            break;
    }
}


char* ffp_get_playing_url(FFPlayer* ffp) {
    if (ffp->is_live_manifest) {
        return KFlvPlayerStatistic_get_playing_url(&ffp->kflv_player_statistic);
    } else if (ffp->is_audio_reloaded) {
        return (ffp->reload_audio_filename ? ffp->reload_audio_filename : "N/A");
    } else {
        return (ffp->input_filename ? ffp->input_filename : "N/A");
    }
}

char* ffp_get_property_string(FFPlayer* ffp, int id) {
    if (NULL == ffp || NULL == ffp->is)
        return "N/A";

    switch (id) {
        case FFP_PROP_STRING_SERVER_IP:
            if (ffp->is_live_manifest && ffp->is->ic) {
                if (ffp->is->server_ip != NULL) {
                    if (ffp->live_manifest_last_decoding_flv_index == ffp->kflv_player_statistic.kflv_stat.cur_decoding_flv_index) {
                        return ffp->is->server_ip;
                    }
                }

                AVDictionaryEntry* server_ip = av_dict_get(ffp->is->ic->metadata, "server_ip", NULL, AV_DICT_IGNORE_SUFFIX);
                if (server_ip && server_ip->value) {
                    ffp->live_manifest_last_decoding_flv_index = ffp->kflv_player_statistic.kflv_stat.cur_decoding_flv_index;
                    av_freep(&ffp->is->server_ip);
                    ffp->is->server_ip = av_strdup(server_ip->value);
                    return ffp->is->server_ip;
                }
            } else if (ffp->is->server_ip != NULL) {
                return ffp->is->server_ip;
            }
            break;
        case FFP_PROP_STRING_STREAM_ID: {
            char* stream_id = NULL;
            if ((stream_id = KwaiQos_getStreamId(&ffp->kwai_qos)) != NULL) {
                return stream_id;
            }
            break;
        }
        case FFP_PROP_STRING_DOMAIN: {
            char* domain = NULL;
            if ((domain = KwaiQos_getDomain(&ffp->kwai_qos)) != NULL) {
                return domain;
            }
            break;
        }
        case FFP_PROP_STRING_HTTP_REDIRECT_INFO:
            if (ffp->http_redirect_info) {
                return ffp->http_redirect_info;
            } else {
                return NULL;
            }
            break;
        case FFP_PROP_STRING_PLAYING_URL:
            return ffp_get_playing_url(ffp);
            break;
        default:
            break;
    }

    return "N/A";
}

IjkMediaMeta* ffp_get_meta_l(FFPlayer* ffp) {
    if (!ffp)
        return NULL;

    return ffp->meta;
}

void ffp_get_qos_info(FFPlayer* ffp, KsyQosInfo* info) {
    KwaiQos_getQosInfo(ffp, info);
}

KFlvPlayerStatistic ffp_get_kflv_statisitc(FFPlayer* ffp) {
    return ffp->kflv_player_statistic;
}

void ffp_free_qos_info(FFPlayer* ffp, KsyQosInfo* info) {
    if (!ffp || !ffp->is || !info)
        return;
    if (info->comment) {
        freep((void**)&info->comment);
    }
    return;
}



void ffp_get_speed_change_info(FFPlayer* ffp, SpeedChangeStat* info) {
    if (!ffp || !info)
        return;

    info->down_duration = ffp->qos_speed_change.down_duration;
    info->up_1_duration = ffp->qos_speed_change.up_1_duration;
    info->up_2_duration = ffp->qos_speed_change.up_2_duration;
}


char* ffp_get_video_stat_json_str(FFPlayer* ffp) {
    if (!ffp) {
        return NULL;
    }

    KwaiQos_onError(&ffp->kwai_qos, ffp->kwai_error_code);
    KwaiQos_collectAwesomeCacheInfoOnce(&ffp->kwai_qos, ffp);
    KwaiQos_collectRealTimeStatInfoIfNeeded(ffp);
    KwaiQos_collectAudioTrackInfo(&ffp->kwai_qos, ffp);

    return KwaiQos_getVideoStatJson(&ffp->kwai_qos);
}

char* ffp_get_brief_video_stat_json_str(FFPlayer* ffp) {
    if (!ffp) {
        return NULL;
    }

    KwaiQos_onError(&ffp->kwai_qos, ffp->kwai_error_code);
    KwaiQos_collectAwesomeCacheInfoOnce(&ffp->kwai_qos, ffp);
    KwaiQos_collectRealTimeStatInfoIfNeeded(ffp);
    KwaiQos_collectAudioTrackInfo(&ffp->kwai_qos, ffp);

    return KwaiQos_getBriefVideoStatJson(&ffp->kwai_qos);
}

char* ffp_get_live_stat_json_str(FFPlayer* ffp) {
    cJSON* stat_json = cJSON_CreateObject();

    cJSON_AddStringToObject(stat_json, "ver", (&ffp->kwai_qos)->basic.sdk_version != NULL
                            ? (&ffp->kwai_qos)->basic.sdk_version : "N/A");

    {
        cJSON* live_delay = cJSON_CreateObject();
        {
            cJSON_AddItemToObject(live_delay, "a_render_delay",
                                  DelayStat_gen_live_delay_json(&ffp->qos_delay_audio_render));
            cJSON_AddItemToObject(live_delay, "v_recv_delay",
                                  DelayStat_gen_live_delay_json(&ffp->qos_delay_video_recv));
            cJSON_AddItemToObject(live_delay, "v_pre_dec_delay",
                                  DelayStat_gen_live_delay_json(&ffp->qos_delay_video_before_dec));
            cJSON_AddItemToObject(live_delay, "v_post_dec_delay",
                                  DelayStat_gen_live_delay_json(&ffp->qos_delay_video_after_dec));
            cJSON_AddItemToObject(live_delay, "v_render_delay",
                                  DelayStat_gen_live_delay_json(&ffp->qos_delay_video_render));
        }
        cJSON_AddItemToObject(stat_json, "av_delay", live_delay);
    }
    {

        cJSON* speed_change = cJSON_CreateObject();
        {
            // todo 改成和ffp_gen_live_delay_json一样的形式
            cJSON_AddNumberToObject(speed_change, "enable", ffp->audio_speed_change_enable);
            static char speed_down_name[JSON_NAME_MAX_LEN] = {0};
            static char speed_up_1_name[JSON_NAME_MAX_LEN] = {0};
            static char speed_up_2_name[JSON_NAME_MAX_LEN] = {0};
            static int speed_names_got = 0;
            if (0 == speed_names_got) {
                snprintf(speed_down_name, JSON_NAME_MAX_LEN, "%.2fx",
                         KS_AUDIO_PLAY_SPEED_DOWN / 100.f);
                snprintf(speed_up_1_name, JSON_NAME_MAX_LEN, "%.2fx",
                         KS_AUDIO_PLAY_SPEED_UP_1 / 100.f);
                snprintf(speed_up_2_name, JSON_NAME_MAX_LEN, "%.2fx",
                         KS_AUDIO_PLAY_SPEED_UP_2 / 100.f);
                speed_names_got = 1;
            }
            cJSON_AddNumberToObject(speed_change, speed_down_name,
                                    ffp->qos_speed_change.down_duration);
            cJSON_AddNumberToObject(speed_change, speed_up_1_name,
                                    ffp->qos_speed_change.up_1_duration);
            cJSON_AddNumberToObject(speed_change, speed_up_2_name,
                                    ffp->qos_speed_change.up_2_duration);
        }
        cJSON_AddItemToObject(stat_json, "speed_chg", speed_change);
    }

    {
        cJSON* av_diff = cJSON_CreateObject();
        {
            cJSON_AddNumberToObject(av_diff, "max_av_diff",
                                    (&ffp->kwai_qos)->runtime_stat.max_av_diff);
            cJSON_AddNumberToObject(av_diff, "min_av_diff",
                                    (&ffp->kwai_qos)->runtime_stat.min_av_diff);
        }
        cJSON_AddItemToObject(stat_json, "av_diff", av_diff);
    }

    if (ffp->is_live_manifest) {
        cJSON_AddNumberToObject(stat_json, "rep_switch_cnt", ffp->kflv_player_statistic.kflv_stat.rep_switch_cnt);
        cJSON_AddNumberToObject(stat_json, "switch_flag", KwaiQos_getLiveManifestSwitchFlag(&ffp->kwai_qos));
    }

    cJSON_AddStringToObject(stat_json, "codec_v",
                            (&ffp->kwai_qos)->media_metadata.video_codec_info != NULL
                            ? (&ffp->kwai_qos)->media_metadata.video_codec_info : "N/A");

    char* ret = cJSON_Print(stat_json);
    cJSON_Delete(stat_json);

    return ret;
}

void ffp_set_audio_data_callback(FFPlayer* ffp, void* arg) {
    if (ffp) {
        ffp->pipeline->extra = arg;
    }
}

void ffp_set_live_event_callback(FFPlayer* ffp, void* callback) {
    if (ffp) {
        ffp->live_event_callback = callback;
    }
}

int ffp_set_mute(FFPlayer* ffp, int mute) {
    if (ffp) {
        ffp->muted = mute;
        if (ffp->is) {
            ffp->is->muted = mute;
        }
        if (ffp->aout) {
            SDL_AoutMuteAudio(ffp->aout, mute);
        }
        return 0;

    }
    return -1;

}

void set_volume(FFPlayer* ffp, float leftVolume, float rightVolume) {
    if (ffp) {
        if (leftVolume < 0.0f || leftVolume > 1.0f) {
            leftVolume = 1.0f;
        }
        if (rightVolume < 0.0f || rightVolume > 1.0f) {
            rightVolume = 1.0f;
        }
        LOCK(ffp->volude_mutex);
        ffp->volumes[AUDIO_VOLUME_LEFT] = leftVolume;
        ffp->volumes[AUDIO_VOLUME_RIGHT] = rightVolume;
        UNLOCK(ffp->volude_mutex);

    }

}

void ffp_get_screen_shot(FFPlayer* ffp, int stride, void* dst_buffer) {
    if (!ffp || !ffp->is)
        return;

    VideoState* is = ffp->is;
    Frame* vp;

    vp = frame_queue_peek(&is->pictq);
    if (vp->bmp) {
        SDL_VoutLockYUVOverlay(vp->bmp);
        SDL_VoutGetScreenShot(vp->bmp, stride, dst_buffer);
        SDL_VoutUnlockYUVOverlay(vp->bmp);
    }
}

void ffp_set_config_json(FFPlayer* ffp, const char* config_json) {
    if (ffp && config_json) {
        ALOGI("[%u] speed_change:\n%s\n", ffp->session_id, config_json);
        cJSON* config = cJSON_Parse(config_json);
        if (config) {
            cJSON* speed_change = cJSON_GetObjectItem(config, "spd_chg_en");
            if (speed_change && cJSON_IsNumber(speed_change)) {
                ffp->audio_speed_change_enable = speed_change->valueint;
            }

            cJSON_Delete(config);
        }
    } else {
        ALOGW("[%u] %s called but ffp or config_json is null! ffp: %p, config_json: %s",
              ffp ? ffp->session_id : -1, __FUNCTION__, ffp, config_json);
    }
}

void ffp_set_live_low_delay_config_json(FFPlayer* ffp, const char* config_json) {
    if (ffp && config_json) {
        ALOGI("[%u] low_delay:\n%s\n", ffp->session_id, config_json);
        cJSON* config = cJSON_Parse(config_json);
        if (config) {
            cJSON* buffer_time_max = cJSON_GetObjectItem(config, "buffer_time_max");
            if (buffer_time_max && cJSON_IsNumber(buffer_time_max)) {
                ffp->live_low_delay_buffer_time_max = buffer_time_max->valueint;
            }

            cJSON_Delete(config);
        }
    } else {
        ALOGW("[%u] %s called but ffp or config_json is null! ffp: %p, config_json: %s",
              ffp ? ffp->session_id : -1, __FUNCTION__, ffp, config_json);
    }
}

void ffp_set_hevc_codec_name(FFPlayer* ffp, const char* hevc_codec_name) {
    if (!ffp || !hevc_codec_name) {
        return;
    }

    if (!strcmp(hevc_codec_name, "libqy265dec")
        || !strcmp(hevc_codec_name, "libks265dec")
        || !strcmp(hevc_codec_name, "libkvc265dec")) {
        if (ffp->preferred_hevc_codec_name) {
            av_freep(&ffp->preferred_hevc_codec_name);
        }
        ffp->preferred_hevc_codec_name = av_strdup(hevc_codec_name);
    }
}

void ffp_setup_cache_session_listener(FFPlayer* ffp, CCacheSessionListener* listener) {
    ffp->cache_session_listener = listener;
}

void ffp_setup_awesome_cache_callback(FFPlayer* ffp,
                                      AwesomeCacheCallback_Opaque callback) {
    if (ffp->cache_callback) {
        AwesomeCacheCallback_Opaque_delete(callback);
    }
    ffp->cache_callback = callback;
}
bool ffp_get_use_cache_l(FFPlayer* ffp) {
    if (!ffp)
        return false;
    return ffp->cache_actually_used;
}

bool ffp_is_live_manifest_l(FFPlayer* ffp) {
    if (!ffp)
        return false;
    return ffp->is_live_manifest == 1;
}

void ffp_get_kwai_live_voice_comment(FFPlayer* ffp, char* voice_comment, int64_t time) {
    if (!ffp || !ffp->is)
        return;

    VoiceComment vc;
    memset(&vc, 0, sizeof(VoiceComment));
    if (0 == live_voice_comment_queue_get(&ffp->is->vc_queue, &vc, time)) {
        ALOGI("[%u] ffp_get_kwai_live_voice_comment vc:%s time=%lld\n", ffp->session_id, vc.comment, vc.time);
        strncpy(voice_comment, vc.comment, vc.com_len);
    }
    return;
}

void ffp_get_kwai_sign(FFPlayer* ffp, char* sign) {
    if (ffp->cache_actually_used) {
        strncpy(sign, ffp->cache_stat.ac_runtime_info.download_task.kwaisign, CDN_KWAI_SIGN_MAX_LEN);
    } else if (ffp->islive) {
        strncpy(sign, ffp->live_kwai_sign, CDN_KWAI_SIGN_MAX_LEN);
    }
    return;
}

void ffp_get_x_ks_cache(FFPlayer* ffp, char* x_ks_cache) {
    if (ffp->islive) {
        if (ffp->cache_actually_used) {
            strncpy(x_ks_cache, ffp->cache_stat.ac_runtime_info.download_task.x_ks_cache, CDN_X_KS_CACHE_MAX_LEN);
        } else {
            strncpy(x_ks_cache, ffp->live_x_ks_cache, CDN_X_KS_CACHE_MAX_LEN);
        }
        return;
    }
}

void ffp_get_vod_adaptive_url(FFPlayer* ffp, char* current_url) {
    if (ffp->input_filename && current_url) {
        av_strlcpy(current_url, ffp->input_filename, DATA_SOURCE_URI_MAX_LEN);
        return;
    }
}

void ffp_get_vod_adaptive_cache_key(FFPlayer* ffp, char* cache_key) {
    if (ffp->cache_key && cache_key) {
        av_strlcpy(cache_key, ffp->cache_key, DATA_SOURCE_URI_MAX_LEN);
        return;
    }
}

void ffp_get_vod_adaptive_host_name(FFPlayer* ffp, char* host_name) {
    if (ffp->host && host_name) {
        av_strlcpy(host_name, ffp->host, DATA_SOURCE_IP_MAX_LEN);
        return;
    }
}

void ffp_set_last_try_flag(FFPlayer* ffp, int is_last_try) {
    if (!ffp) {
        return;
    }
    KwaiQos_setLastTryFlag(&ffp->kwai_qos, is_last_try);
}

void ffp_enable_pre_demux_l(FFPlayer* ffp, int pre_demux_ver, int64_t dur_ms) {
    if (!ffp) {
        return;
    }

    if (!ffp->pre_demux) {
        if (dur_ms > 0) {
            ffp->pre_demux = PreDemux_create(dur_ms);
            ffp->use_pre_demux_ver = pre_demux_ver;
        } else {
            ALOGE("[%u] set pre demux fail, PreDemux duration can not < 0 :%lld \n",
                  ffp->session_id, dur_ms);
        }
    }
}

void ffp_interrupt_pre_demux_l(FFPlayer* ffp) {
    if (!ffp) {
        return;
    }

    if (ffp->pre_demux) {
        PreDemux_abort(ffp->pre_demux);
    }
}


void ffp_enable_ab_loop_l(FFPlayer* ffp, int64_t a_pts_ms, int64_t b_pts_ms) {
    if (!ffp) {
        return;
    }

    AbLoop_set_ab(&ffp->ab_loop, a_pts_ms, b_pts_ms);
}

void ffp_enable_buffer_loop_l(FFPlayer* ffp, int buffer_start_percent,
                              int buffer_end_percent, int64_t loop_begin) {
    if (!ffp) {
        return;
    }
    BufferLoop_enable(&ffp->buffer_loop, buffer_start_percent, buffer_end_percent, loop_begin);
}

void  ffp_set_start_play_buffer_ms(FFplayer* ffp, int buffer_ms, int max_buffer_cost_ms) {
    if (!ffp) {
        return;
    }

    KwaiPacketQueueBufferChecker* ck = &ffp->kwai_packet_buffer_checker;
    KwaiPacketQueueBufferChecker_use_strategy(ck, kStrategyStartPlayBlockByTimeMs);
    KwaiPacketQueueBufferChecker_set_start_play_buffer_ms(ck, buffer_ms);
    KwaiPacketQueueBufferChecker_set_start_play_max_buffer_cost_ms(ck, max_buffer_cost_ms);

    ALOGD("[%u][%s], used strategy:%d, buffer_threshold_ms:%d, max_buffer_cost_ms:%d\n",
          ffp->session_id, __func__, ck->used_strategy, ck->buffer_threshold_ms, max_buffer_cost_ms);
}

bool ffp_check_can_start_play(FFplayer* ffp) {
    if (!ffp) {
        return false;
    }
    return ffp->kwai_packet_buffer_checker.func_check_can_start_play(&ffp->kwai_packet_buffer_checker, ffp);
}

int32_t ffp_get_downloaded_percent(FFplayer* ffp) {
    if (ffp && (ffp->cache_key || ffp->input_filename)) {
        int64_t total_bytes = ac_get_content_len_by_key(ffp->input_filename, ffp->cache_key);
        int64_t cached_bytes = ac_get_cached_bytes_by_key(ffp->input_filename, ffp->cache_key);
        if (total_bytes > 0) {
            return (int32_t)((cached_bytes * 100) / total_bytes);
        }
    }
    return 0;
}
