//
// Created by MarshallShuai on 2019/4/19.
//


#include <stdint.h>

#include "ff_ffplay_internal.h"

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

#include "../ijkavformat/ijkdatasource.h"
#include "../ijkavformat/ijk_index_content.h"

#include "ff_ffplay.h"
#include "ff_ffpipeline.h"
#include "ff_cmdutils.h"

#include "ijkkwai/kwai_error_code_manager.h"
#include "ijkkwai/kwai_qos.h"
#include "ijkkwai/cache/ffmpeg_adapter.h"

#if defined(CONFIG_KS_AUDIOPROCESS) || defined(__APPLE__)
#include "ijkkwai/c_audio_process.h"
#endif

#include "ijksdl/ijksdl_mutex.h"

#include "ff_ffplay_module_audio_decode.h"
#include "ff_ffplay_module_audio_render.h"
#include "ff_ffplay_module_video_decode.h"

// Audio device latency
#define AUDIO_DEVICE_LATENCY_THRESHOLD_1_S (0.6)
#define AUDIO_DEVICE_LATENCY_THRESHOLD_2_S (0.8)
#define AUDIO_DEVICE_LATENCY_MINUS_1_S (0.1)
#define AUDIO_DEVICE_LATENCY_MINUS_2_S (0.15)

// KW265 output pixel formats
#define KW265_OUTPUT_PIXFMT_YUV420P (0)
#define KW265_OUTPUT_PIXFMT_NV12    (1)
#define KW265_OUTPUT_PIXFMT_NV21    (2)
#define KW265_OUTPUT_PIXFMT_DEFAULT KW265_OUTPUT_PIXFMT_NV12

int64_t ffp_setup_open_AwesomeCache_AVIOContext(FFPlayer* ffp, const char* url,
                                                AVDictionary** options) {
    int64_t ret = 0;
    SDL_LockMutex(ffp->cache_avio_overrall_mutex);

    SDL_LockMutex(ffp->cache_avio_exist_mutex);
    ret = AwesomeCache_AVIOContext_create(&ffp->cache_avio_context,
                                          options,
                                          ffp->session_id,
                                          ffp->session_uuid,
                                          ffp->cache_session_listener,
                                          ffp->cache_callback,
                                          &ffp->cache_stat,
                                          ffp->player_statistic);
    SDL_UnlockMutex(ffp->cache_avio_exist_mutex);

    if (ret != 0) {
        ALOGE("[%u] AwesomeCache_AVIOContext_create FAIL, ret:%lld", ffp->session_id, ret);
        goto return_ret;
    } else {
        KwaiQos_setAwesomeCacheIsFullyCachedOnOpen(&ffp->kwai_qos,
                                                   ffp->islive ? false :
                                                   ac_is_fully_cached(url, ffp->cache_key));
    }

    ret = AwesomeCache_AVIOContext_open(ffp->cache_avio_context, url, ffp->cache_key);
    if (ret < 0) {
        ALOGE("[%u] AwesomeCache_AVIOContext_open FAIL, ret:%lld", ffp->session_id, ret);
        goto return_ret;
    }

return_ret:
    SDL_UnlockMutex(ffp->cache_avio_overrall_mutex);
    return ret;
}


void ffp_avformat_close_input(FFPlayer* ffp, AVFormatContext** s) {
    avformat_close_input(s);
}

/**
 * 这函数两个可能调用的线程：
 * 1.read_thread 结束的时候
 * 2.stream_close (调用stop_wait_l）的线程
 */
void ffp_close_release_AwesomeCache_AVIOContext(FFPlayer* ffp) {
    SDL_LockMutex(ffp->cache_avio_overrall_mutex);
    if (ffp->cache_avio_context) {
        // close first, duplicated is harmless
        AwesomeCache_AVIOContext_close(ffp->cache_avio_context);

        SDL_LockMutex(ffp->cache_avio_exist_mutex);
        AwesomeCache_AVIOContext_releasep(&ffp->cache_avio_context);
        SDL_UnlockMutex(ffp->cache_avio_exist_mutex);
    }
    SDL_UnlockMutex(ffp->cache_avio_overrall_mutex);
}

int ffp_avformat_open_input(FFPlayer* ffp, AVFormatContext** ps,
                            const char* url, AVInputFormat* iformat, AVDictionary** fmt_options) {
    // 对ic成员的操作才能使用ic，对ic本身的操作必须使用ps， 尤其是在avformat_open_input的时候 因为avformat_open_input在失败的时候会把*ps置为null，
    AVFormatContext* ic = *ps;
    AVDictionaryEntry* t;
    int err = 0;

    ac_util_generate_uuid(ffp->session_uuid, SESSION_UUID_BUFFER_LEN);
    KwaiQos_setSessionUUID(&ffp->kwai_qos, ffp->session_uuid);
//    av_dict_set_int(&ffp->format_opts, "ffplayer-opaque", (int64_t)ffp, 0);
    ffp->cache_actually_used = false;
    if (ffp->is_live_manifest) {
        av_dict_set(&ic->metadata, "manifest_string", url, 0);

        // dump headers for Qos
        AVDictionaryEntry* headers = av_dict_get(ffp->format_opts, "headers", NULL, 0);
        if (headers) {
            av_dict_set(&ic->metadata, "headers", headers->value, 0);
        }

        if ((t = av_dict_get(ffp->format_opts, "liveAdaptConfig", NULL, 0))) {
            av_dict_set(&ic->metadata, "liveAdaptConfig", t->value, 0);
        }

        ALOGI("[%u] ffp->live_manifest_switch_mode = %d, ffp->timeout = %lld",
              ffp->session_id, ffp->live_manifest_switch_mode, ffp->timeout);
        av_dict_set_int(&ic->metadata, "kflv_switch_mode", (intptr_t)(&ffp->live_manifest_switch_mode), 0);
        av_dict_set_int(&ic->metadata, "audio_cached_duration_ms", (intptr_t)(&ffp->stat.audio_cache.duration), 0);
        av_dict_set_int(&ic->metadata, "video_cached_duration_ms", (intptr_t)(&ffp->stat.video_cache.duration), 0);

        if (ffp->timeout > 0) {
            av_dict_set_int(&ic->metadata, "timeout", ffp->timeout, 0);
        }

        if (0 != (err = avformat_open_input(ps, ".kflv", iformat, fmt_options))) {
            ALOGE("[%u] live manifest avformat_open_input fail, err = %d", ffp->session_id, err);
        }
    } else {
        bool cache_global_enabled = AwesomeCache_util_is_globally_enabled();
        bool url_is_cache_whitelist = AwesomeCache_util_is_url_white_list(url);
        bool use_custom_protocol = AwesomeCache_util_use_custom_protocol(ffp->format_opts);
        bool url_is_index_content = ffp->input_data_type == INPUT_DATA_TYPE_INDEX_CONTENT;
        bool url_is_hls_custome_manifest = ffp->input_data_type == INPUT_DATA_TYPE_HLS_CUSTOME_MANIFEST;
        int setup_err = 0;
        // 直播audioonly不使用cache:
        // 1.native p2sp不支持audioonly
        // 2.audioonly不能跟video共享AwesomeCache_AVIOContext
        bool is_cache_enable = ffp->expect_use_cache &&
                               cache_global_enabled &&
                               !ffp->is_audio_reloaded;

        bool use_custome_avio_context = is_cache_enable &&
                                        !use_custom_protocol &&
                                        url_is_cache_whitelist;

        bool use_custome_url_context  = is_cache_enable &&
                                        use_custom_protocol &&
                                        (url_is_cache_whitelist || url_is_index_content || url_is_hls_custome_manifest);

        av_dict_set_int(fmt_options, "prefer-bandwidth", ffp->prefer_bandwidth, 0);

        if (use_custome_avio_context) {
            ffp->cache_actually_used = true;

            int64_t ac_err = ffp_setup_open_AwesomeCache_AVIOContext(ffp, url, fmt_options);
            // 因为 err最后可能记录的是 avformat_open_input的err，
            // 所以单独用 setup_err记录ffp_setup_open_AwesomeCache_AVIOContext的错误码
            setup_err = (int) ac_err;
            if (ac_err >= 0) {
                KwaiQos_setAwesomeCacheIsFullyCachedOnOpen(
                    &ffp->kwai_qos,
                    ffp->islive ? false :
                    AwesomeCache_util_is_fully_cached(url, ffp->cache_key));

                ic->pb = ffp->cache_avio_context;
                ic->flags |= AVFMT_FLAG_CUSTOM_IO;
                err = avformat_open_input(ps, url, iformat, fmt_options);
            } else {
                // do nothing
                ALOGE("[%u] ffp_setup_open_AwesomeCache_AVIOContext FAIL, ac_err:%lld",
                      ffp->session_id, ac_err);
                err = (int) ac_err;
            }
        } else {
            if (ffp->islive && !ffp->is->http_headers) {
                AVDictionaryEntry* headers = av_dict_get(ffp->format_opts, "headers", NULL, 0);
                if (headers && headers->value) {
                    ffp->is->http_headers = av_strdup(headers->value);
                }
            }
            av_dict_set(fmt_options, "use_httpdns", "1", 0);

            char* hook_url = NULL;
            if (url_is_index_content) {
                av_dict_set(fmt_options, "url_protocol_index_content_name", ijk_index_content_get_hook_url_proto_name(), 0);
                hook_url = av_asprintf("%s:%s", ijk_index_content_get_hook_url_proto_name(), ffp->index_content.pre_path);
            } else if (url_is_hls_custome_manifest) {
                hook_url = av_strdup(ffp->index_content.pre_path);
                av_dict_set(fmt_options, "manifest_json", ffp->index_content.content, 0);
                av_dict_set_int(fmt_options, "use_custom_manifest", 1, 0);
                ic->flags |= AVFMT_FLAG_CUSTOM_IO;
                extern AVInputFormat ijkff_khls_demuxer;
                iformat = &ijkff_khls_demuxer;
            }
            if (use_custome_url_context) {
                ffp->cache_actually_used = true;
                av_dict_set(fmt_options, "use_user_protocol", "1", 0);
                av_dict_set(fmt_options, "url_protocol_ts_name", ijkds_get_hook_url_proto_name(), 0);
                if (!url_is_index_content) {
                    char* origin_url = hook_url;
                    hook_url = av_asprintf("%s:%s", ijkds_get_hook_url_proto_name(), origin_url ? origin_url : url);
                    if (origin_url) {
                        av_free(origin_url);
                    }
                }
                ffp->data_source_opt = C_DataSourceOptions_from_options_dict(*fmt_options);
            }
            err = avformat_open_input(ps, hook_url ? hook_url : url, iformat, fmt_options);
            if (hook_url) {
                av_freep(&hook_url);
            }
        }

        KwaiQos_onFFPlayerOpenInputOver(&ffp->kwai_qos, setup_err,
                                        cache_global_enabled,
                                        ffp->cache_actually_used,
                                        url_is_cache_whitelist);

        if (0 != err) {
            ALOGE("[%u] avformat_open_input fail, err = %d(%s), expect_use_cache:%d, cache_actually_used:%d",
                  ffp->session_id, err,
                  get_error_code_fourcc_string_macro(err),
                  ffp->expect_use_cache,
                  ffp->cache_actually_used);
        } else {
            ALOGD("[%u] avformat_open_input success, expect_use_cache:%d, cache_actually_used:%d",
                  ffp->session_id, ffp->expect_use_cache, ffp->cache_actually_used);
        }
    }
    return err;
}

void ffp_AwesomeCache_AVIOContext_abort(FFPlayer* ffp) {
    SDL_LockMutex(ffp->cache_avio_exist_mutex);
    if (ffp->cache_avio_context && ffp->cache_avio_context->opaque) {
        AvIoOpaqueWithDataSource_abort(ffp->cache_avio_context->opaque);
    }
    SDL_UnlockMutex(ffp->cache_avio_exist_mutex);
}


int packet_queue_get_or_buffering(FFPlayer* ffp, PacketQueue* q, AVPacket* pkt, int* serial, AVPacketTime* p_pkttime, int* finished) {
    assert(finished);

    if (!ffp->packet_buffering)
        return packet_queue_get(q, pkt, 1, serial, p_pkttime);

    while (1) {
        int new_packet = packet_queue_get(q, pkt, 0, serial, p_pkttime);
        if (new_packet < 0)
            return -1;
        else if (new_packet == 0) {
            if (q->is_buffer_indicator && !*finished) {
                if (ffp->kwai_packet_buffer_checker.func_check_pkt_q_need_buffering(&ffp->kwai_packet_buffer_checker, ffp, q)) {
                    if (!ffp->buffer_loop.enable) {
                        ffp_toggle_buffering(ffp, 1, 1);
                    } else if (!BufferLoop_loop_on_buffer(&ffp->buffer_loop, ffp)) {
                        ffp->error = EIJK_KWAI_BLOCK_ERR;
                    }
                } else {
                    av_usleep(10000);
                    continue;
                }
            }
            new_packet = packet_queue_get(q, pkt, 1, serial, p_pkttime);
            if (new_packet < 0) {
                return -1;
            }
        }

        if (*finished == *serial) {
            av_packet_unref(pkt);
            continue;
        } else {
            break;
        }
    }

    return 1;
}

void setVideoDecodeDiscard(AVCodecContext* avctx) {
//安卓平台在264上seek使用丢弃非参考帧时，seek耗时会更长一些，关闭该功能。
#ifdef __ANDROID__
    if (avctx->codec_id == AV_CODEC_ID_H264)
        return;
#endif
    avctx->skip_loop_filter = AVDISCARD_NONREF;
    avctx->skip_frame = AVDISCARD_NONREF;
    avctx->skip_idct = AVDISCARD_NONREF;
}

void unsetVideoDecodeDiscard(AVCodecContext* avctx) {
//安卓平台在264上seek使用丢弃非参考帧时，seek耗时会更长一些，关闭该功能。
#ifdef __ANDROID__
    if (avctx->codec_id == AV_CODEC_ID_H264)
        return;
#endif
    avctx->skip_loop_filter = AVDISCARD_DEFAULT;
    avctx->skip_frame = AVDISCARD_DEFAULT;
    avctx->skip_idct = AVDISCARD_DEFAULT;
}


void ffp_show_av_sync_status(FFPlayer* ffp, VideoState* is) {
    if (ffp->show_status) {
        static int64_t last_time;
        int64_t cur_time;
        int aqsize, vqsize, sqsize __unused;
        double av_diff;
#define SHOW_AV_SYNC_STATUS_THRESHOLD_US 30000
        cur_time = av_gettime_relative();
        if (true || !last_time || (cur_time - last_time) >= SHOW_AV_SYNC_STATUS_THRESHOLD_US) {
            aqsize = 0;
            vqsize = 0;
            sqsize = 0;
            if (is->audio_st)
                aqsize = is->audioq.size;
            if (is->video_st)
                vqsize = is->videoq.size;
#ifdef FFP_MERGE
            if (is->subtitle_st)
                sqsize = is->subtitleq.size;
#else
            sqsize = 0;
#endif
            av_diff = 0;
            if (is->audio_st && is->video_st)
                av_diff = get_clock(&is->audclk) - get_clock(&is->vidclk);
            else if (is->video_st)
                av_diff = get_master_clock(is) - get_clock(&is->vidclk);
            else if (is->audio_st)
                av_diff = get_master_clock(is) - get_clock(&is->audclk);
            ALOGI("[%u][av_sync_status]%7.2f %s:%7.3f fd=%4d aq=%5dKB vq=%5dKB sq=%5dB f=%"PRId64"/%"PRId64"   \r",
                  ffp->session_id,
                  get_master_clock(is),
                  (is->audio_st && is->video_st) ? "A-V" : (is->video_st ? "M-V" : (is->audio_st ? "M-A" : "   ")),
                  av_diff,
                  is->frame_drops_early + is->frame_drops_late,
                  aqsize / 1024,
                  vqsize / 1024,
                  sqsize,
                  is->video_st ? is->video_st->codec->pts_correction_num_faulty_dts : 0,
                  is->video_st ? is->video_st->codec->pts_correction_num_faulty_pts : 0);
            fflush(stdout);
            last_time = cur_time;
        }
    }
}


/**
 * allocate a picture (needs to do that in main thread to avoid
 * potential locking problems
 * called only by queue_picture
 */
static void alloc_picture(FFPlayer* ffp, int frame_format) {
    VideoState* is = ffp->is;
    Frame* vp;
#ifdef FFP_MERGE
    int64_t bufferdiff;
#endif

    vp = &is->pictq.queue[is->pictq.windex];

    free_picture(vp);

#ifdef FFP_MERGE
    video_open(ffp, 0, vp);
#endif

    SDL_VoutSetOverlayFormat(ffp->vout, ffp->overlay_format);
    vp->bmp = SDL_Vout_CreateOverlay(vp->width, vp->height,
                                     frame_format,
                                     ffp->vout);
#ifdef FFP_MERGE
    bufferdiff = vp->bmp ? FFMAX(vp->bmp->pixels[0], vp->bmp->pixels[1]) - FFMIN(vp->bmp->pixels[0], vp->bmp->pixels[1]) : 0;
    if (!vp->bmp || vp->bmp->pitches[0] < vp->width || bufferdiff < (int64_t)vp->height * vp->bmp->pitches[0]) {
#else
    /* RV16, RV32 contains only one plane */
    if (!vp->bmp || (!vp->bmp->is_private && vp->bmp->pitches[0] < vp->width)) {
#endif
        /* SDL allocates a buffer smaller than requested if the video
         * overlay hardware is unable to support the requested size. */
        av_log(NULL, AV_LOG_FATAL,
               "Error: the video system does not support an image\n"
               "size of %dx%d pixels. Try using -lowres or -vf \"scale=w:h\"\n"
               "to reduce the image size.\n", vp->width, vp->height);
        free_picture(vp);
    }

    SDL_LockMutex(is->pictq.mutex);
    vp->allocated = 1;
    SDL_CondSignal(is->pictq.cond);
    SDL_UnlockMutex(is->pictq.mutex);
}

int queue_picture(FFPlayer* ffp, AVFrame* src_frame, double pts, double duration,
                  int64_t pos, int serial, AVPacketTime* p_pkttime) {
    VideoState* is = ffp->is;
    Frame* vp;
    int video_accurate_seek_fail = 0;
    int64_t video_seek_pos = 0;
    int64_t now = 0;
    int64_t deviation = 0;
    bool video_eos = false;
    if (ffp->islive) {
        KwaiRotateControl_set_degree_to_frame(&ffp->kwai_rotate_control, src_frame);
    }

    if (ffp->enable_accurate_seek && is->video_accurate_seek_req && !is->seek_req) {
        if (!isnan(pts)) {
            video_seek_pos = is->seek_pos;
            is->accurate_seek_vframe_pts = pts * 1000 * 1000;
            deviation = llabs((int64_t)(pts * 1000 * 1000) - is->seek_pos);
            video_eos = fftime_to_milliseconds((pts + 2 * duration) * 1000 * 1000) >
                        ffp_get_duration_l(ffp) ||
                        (is->video_duration != AV_NOPTS_VALUE &&
                         fftime_to_milliseconds((pts + 2 * duration) * 1000 * 1000) >
                         fftime_to_milliseconds(is->video_duration));
        }
        if ((!video_eos && (pts * 1000 * 1000 < is->seek_pos)) || isnan(pts)) {
            now = av_gettime_relative() / 1000;
            if (is->drop_vframe_count == 0) {
                SDL_LockMutex(is->accurate_seek_mutex);
                if (is->accurate_seek_start_time <= 0 &&
                    (is->audio_stream < 0 || is->audio_accurate_seek_req)) {
                    is->accurate_seek_start_time = now;
                }
                SDL_UnlockMutex(is->accurate_seek_mutex);

                //精准seek过程中丢弃非参考帧，不解码，只对软解有效
                setVideoDecodeDiscard(is->viddec.avctx);

                ALOGI("[%u]video accurate_seek start, is->seek_pos=%lld, pts=%lf, is->accurate_seek_time = %lld\n",
                      ffp->session_id, is->seek_pos, pts, is->accurate_seek_start_time);
            }
            is->drop_vframe_count++;
            if ((now - is->accurate_seek_start_time) <= ffp->accurate_seek_timeout) {
                return 1;  // drop some old frame when do accurate seek
            } else {
                ALOGW("[%u]video accurate_seek is error, is->drop_vframe_count=%d, now = %lld, pts = %lf\n",
                      ffp->session_id, is->drop_vframe_count, now, pts);
                video_accurate_seek_fail = 1;  // if KEY_FRAME interval too big, disable accurate seek
            }
        } else {
            ALOGI("[%u]video accurate_seek is ok, is->drop_vframe_count =%d, is->seek_pos=%lld, pts=%lf\n",
                  ffp->session_id, is->drop_vframe_count, is->seek_pos, pts);
            if (video_seek_pos == is->seek_pos) {
                is->drop_vframe_count = 0;
                SDL_LockMutex(is->accurate_seek_mutex);
                is->video_accurate_seek_req = 0;
                SDL_CondSignal(is->audio_accurate_seek_cond);
                if (video_seek_pos == is->seek_pos && is->audio_accurate_seek_req &&
                    !is->abort_request && !is->pause_req) {
                    //don't wait in pause status.
                    SDL_CondWaitTimeout(is->video_accurate_seek_cond,
                                        is->accurate_seek_mutex,
                                        ffp->accurate_seek_timeout);
                } else {
                    if (!is->accurate_seek_notify) {
                        ffp_notify_msg2(ffp, FFP_MSG_ACCURATE_SEEK_COMPLETE,
                                        (int)(pts * 1000));
                        is->accurate_seek_notify = 1;
                    }
                }
                if (video_seek_pos != is->seek_pos && !is->abort_request) {
                    is->video_accurate_seek_req = 1;
                    SDL_UnlockMutex(is->accurate_seek_mutex);
                    return 1;
                }

                SDL_UnlockMutex(is->accurate_seek_mutex);
            }
        }

        if (video_accurate_seek_fail) {
            is->drop_vframe_count = 0;
            SDL_LockMutex(is->accurate_seek_mutex);
            is->video_accurate_seek_req = 0;
            SDL_CondSignal(is->audio_accurate_seek_cond);
            if (is->audio_accurate_seek_req && !is->abort_request && !is->pause_req) {
                SDL_CondWaitTimeout(is->video_accurate_seek_cond, is->accurate_seek_mutex,
                                    ffp->accurate_seek_timeout);
            } else {
                if (!is->accurate_seek_notify) {
                    if (!isnan(pts)) {
                        ffp_notify_msg2(ffp, FFP_MSG_ACCURATE_SEEK_COMPLETE, (int)(pts * 1000));
                    } else {
                        ffp_notify_msg2(ffp, FFP_MSG_ACCURATE_SEEK_COMPLETE, 0);
                    }
                    is->accurate_seek_notify = 1;
                }
            }
            SDL_UnlockMutex(is->accurate_seek_mutex);
        }
        unsetVideoDecodeDiscard(is->viddec.avctx);
        is->accurate_seek_start_time = 0;
        video_accurate_seek_fail = 0;
        is->accurate_seek_vframe_pts = 0;
    }

#if defined(DEBUG_SYNC) && 0
    printf("frame_type=%c pts=%0.3f\n",
           av_get_picture_type_char(src_frame->pict_type), pts);
#endif

    if (!(vp = frame_queue_peek_writable(&is->pictq))) {
        return -1;
    }

    /* alloc or resize hardware picture buffer */
    if (!vp->bmp || vp->reallocate || !vp->allocated ||
        vp->width != src_frame->width ||
        vp->height != src_frame->height ||
        vp->rotation != src_frame->angle) {

        vp->allocated = 0;
        vp->reallocate = 0;
        vp->width = src_frame->width;
        vp->height = src_frame->height;
        vp->rotation = (int) src_frame->angle;

        /* the allocation must be done in the main thread to avoid
           locking problems. */
        alloc_picture(ffp, src_frame->format);

        if (is->videoq.abort_request) {
            return -1;
        }
    }

    /* if the frame is not skipped, then display it */
    if (vp->bmp) {
        /* get a pointer on the bitmap */
        SDL_VoutLockYUVOverlay(vp->bmp);

#ifdef FFP_MERGE
#if CONFIG_AVFILTER
        // FIXME use direct rendering
        av_picture_copy(&pict, (AVPicture*)src_frame,
                        src_frame->format, vp->width, vp->height);
#else
        // sws_getCachedContext(...);
#endif
#endif
        // FIXME: set swscale options
        if (SDL_VoutFillFrameYUVOverlay(vp->bmp, src_frame) < 0) {
            av_log(NULL, AV_LOG_FATAL, "Cannot initialize the conversion context\n");
            exit(1);
        }
        /* update the bitmap content */
        SDL_VoutUnlockYUVOverlay(vp->bmp);

        vp->pts = pts;
        vp->duration = duration;
        vp->pos = pos;
        vp->serial = serial;
        vp->sar = src_frame->sample_aspect_ratio;

        vp->bmp->sar_num = vp->sar.num;
        vp->bmp->sar_den = vp->sar.den;

        memset(&(vp->pkttime), 0, sizeof(AVPacketTime));
        if (p_pkttime != NULL)
            memcpy(&(vp->pkttime), p_pkttime, sizeof(AVPacketTime));

        /* now we can update the picture count */
        frame_queue_push(&is->pictq);
        if (is->viddec.first_frame_pts == -1 && !isnan(pts)) {
            is->viddec.first_frame_pts = pts;
        }
        if (!is->viddec.first_frame_decoded) {
            ALOGD("[%u] Video: first frame decoded\n", ffp->session_id);
            is->viddec.first_frame_decoded_time = SDL_GetTickHR();
            is->viddec.first_frame_decoded = 1;

            KwaiQos_onVideoRenderFirstFrameFilled(&ffp->kwai_qos);
            KwaiQos_setVideoFramePixelInfo(&ffp->kwai_qos, src_frame->colorspace, src_frame->format);
            KwaiQos_setOverlayOutputFormat(&ffp->kwai_qos, vp->bmp->format);
        }
    }
    return 0;
}


/**
 * 打开音频输出设备
 * only called by stream_component_open
 */
#define MAX_AOUT_OPEN_LIMIT 3
static int audio_open(FFPlayer* opaque, int64_t wanted_channel_layout, int wanted_nb_channels,
                      int wanted_sample_rate, struct AudioParams* audio_hw_params) {
    FFPlayer* ffp = opaque;
    VideoState* is = ffp->is;
    SDL_AudioSpec wanted_spec, spec;
    const char* env;
    static const int next_nb_channels[] = {0, 0, 1, 6, 2, 6, 4, 6};
#ifdef FFP_MERGE
    static const int next_sample_rates[] = {0, 44100, 48000, 96000, 192000};
#endif
    static const int next_sample_rates[] = {0, 44100, 48000};
    int next_sample_rate_idx = FF_ARRAY_ELEMS(next_sample_rates) - 1;

    env = SDL_getenv("SDL_AUDIO_CHANNELS");
    if (env) {
        wanted_nb_channels = atoi(env);
        wanted_channel_layout = av_get_default_channel_layout(wanted_nb_channels);
    }
    if (!wanted_channel_layout ||
        wanted_nb_channels != av_get_channel_layout_nb_channels(wanted_channel_layout)) {
        wanted_channel_layout = av_get_default_channel_layout(wanted_nb_channels);
        wanted_channel_layout &= ~AV_CH_LAYOUT_STEREO_DOWNMIX;
    }
    wanted_nb_channels = av_get_channel_layout_nb_channels(wanted_channel_layout);
    wanted_spec.channels = wanted_nb_channels;
    wanted_spec.freq = wanted_sample_rate;
    if (wanted_spec.freq <= 0 || wanted_spec.channels <= 0) {
        ALOGE("[%u] Invalid sample rate or channel count!\n", ffp->session_id);
        return -1;
    }
    while (next_sample_rate_idx && next_sample_rates[next_sample_rate_idx] >= wanted_spec.freq)
        next_sample_rate_idx--;
    wanted_spec.format = AUDIO_S16SYS;
    wanted_spec.silence = 0;
    wanted_spec.samples = FFMAX(SDL_AUDIO_MIN_BUFFER_SIZE,
                                2 << av_log2(wanted_spec.freq / SDL_AUDIO_MAX_CALLBACKS_PER_SEC));
    wanted_spec.callback = sdl_audio_callback;
    wanted_spec.userdata = opaque;

    int open_aout_time = 0;
    while (SDL_AoutOpenAudio(ffp->aout, &wanted_spec, &spec) < 0) {
        /* avoid infinity loop on exit. --by bbcallen */
        if (is->abort_request)
            return -1;
        ALOGW("[%u] SDL_OpenAudio (%d channels, %d Hz): %s\n",
              ffp->session_id, wanted_spec.channels, wanted_spec.freq, SDL_GetError());
        wanted_spec.channels = next_nb_channels[FFMIN(7, wanted_spec.channels)];
        if (!wanted_spec.channels) {
            wanted_spec.freq = next_sample_rates[next_sample_rate_idx--];
            wanted_spec.channels = wanted_nb_channels;
            if (!wanted_spec.freq) {
                ALOGE("[%u] No more combinations to try, audio open failed\n", ffp->session_id);
                return -1;
            }
        }
        wanted_channel_layout = av_get_default_channel_layout(wanted_spec.channels);

        open_aout_time++;
        ALOGD("[%u] SDL_AoutOpenAudio retry:%d", ffp->session_id, open_aout_time);
        // avoid infinity trying open
        if (open_aout_time > MAX_AOUT_OPEN_LIMIT) {
            ALOGE("[%u] SDL_AoutOpenAudio fail, return -1", ffp->session_id);
            return -1;
        }
    }
    if (ffp->muted)
        SDL_AoutMuteAudio(ffp->aout, ffp->muted);

    if (spec.format != AUDIO_S16SYS) {
        ALOGE("[%u] SDL advised audio format %d is not supported!\n", ffp->session_id, spec.format);
        return -1;
    }
    if (spec.channels != wanted_spec.channels) {
        wanted_channel_layout = av_get_default_channel_layout(spec.channels);
        if (!wanted_channel_layout) {
            ALOGE("[%u] SDL advised channel count %d is not supported!\n", ffp->session_id, spec.channels);
            return -1;
        }
    }

    audio_hw_params->fmt = AV_SAMPLE_FMT_S16;
    audio_hw_params->freq = spec.freq;
    audio_hw_params->channel_layout = wanted_channel_layout;
    audio_hw_params->channels = spec.channels;
    audio_hw_params->frame_size = av_samples_get_buffer_size(NULL, audio_hw_params->channels, 1,
                                                             audio_hw_params->fmt, 1);
    audio_hw_params->bytes_per_sec = av_samples_get_buffer_size(NULL, audio_hw_params->channels,
                                                                audio_hw_params->freq,
                                                                audio_hw_params->fmt, 1);
    if (audio_hw_params->bytes_per_sec <= 0 || audio_hw_params->frame_size <= 0) {
        ALOGE("[%u] av_samples_get_buffer_size failed\n", ffp->session_id);
        return -1;
    }

    double latency = ((double) spec.size) / audio_hw_params->bytes_per_sec;

    KwaiQos_setAudioDeviceLatencyMs(&ffp->kwai_qos, (int)(latency * 1000));

    latency *= 2;  // original logic

    if (latency > AUDIO_DEVICE_LATENCY_THRESHOLD_2_S) {
        latency -= AUDIO_DEVICE_LATENCY_MINUS_2_S;
    } else if (latency > AUDIO_DEVICE_LATENCY_THRESHOLD_1_S) {
        latency -= AUDIO_DEVICE_LATENCY_MINUS_1_S;
    }

    KwaiQos_setAudioDeviceAppliedLatencyMs(&ffp->kwai_qos, (int)(latency * 1000));

    SDL_AoutSetDefaultLatencySeconds(ffp->aout, latency);
    return spec.size;
}

#if defined(__ANDROID__)
#define AV_CODEC_ID_KVC ((enum AVCodecID)0x30000)
void check_mediacodec_availability(FFPlayer* ffp, int stream_index) {
    if (!ffp || !(ffp->mediacodec_all_videos || ffp->mediacodec_hevc || ffp->mediacodec_avc)) {
        // not using mediacodec
        return;
    }

    VideoState* is = ffp->is;
    AVCodecContext* avctx = NULL;

    if (is && is->ic && stream_index >= 0 && stream_index < is->ic->nb_streams) {
        avctx = is->ic->streams[stream_index]->codec;
    } else {
        return;
    }

    if (avctx->codec_id == AV_CODEC_ID_HEVC
        && (ffp->mediacodec_all_videos || ffp->mediacodec_hevc)
        && (avctx->height > 0)
        && (avctx->width > 0)
        && (ffp->mediacodec_hevc_height_limit > 0)
        && (ffp->mediacodec_hevc_width_limit > 0)) {
        if ((avctx->height > ffp->mediacodec_hevc_height_limit)
            || (avctx->width > ffp->mediacodec_hevc_width_limit)) {
            ffp->mediacodec_all_videos = ffp->mediacodec_hevc = 0;
        }
    } else if (avctx->codec_id == AV_CODEC_ID_H264
               && (ffp->mediacodec_all_videos || ffp->mediacodec_avc)
               && (avctx->height > 0)
               && (avctx->width > 0)
               && (ffp->mediacodec_avc_height_limit > 0)
               && (ffp->mediacodec_avc_width_limit > 0)) {
        if ((avctx->height > ffp->mediacodec_avc_height_limit)
            || (avctx->width > ffp->mediacodec_avc_width_limit)) {
            ffp->mediacodec_all_videos = ffp->mediacodec_avc = 0;
        }
    } else if (avctx->codec_id == AV_CODEC_ID_KVC) {
        ALOGW("[%u] AV_CODEC_ID_KVC not use mediacodec\n", ffp->session_id);
        ffp->mediacodec_all_videos = ffp->mediacodec_avc = ffp->mediacodec_hevc = 0;
    }
}
#endif

bool use_video_hardware_decoder(FFPlayer* ffp, int stream_index) {
    VideoState* is = ffp->is;
    AVFormatContext* ic = is->ic;
    AVCodecContext* avctx = NULL;

    if (stream_index < 0 || stream_index >= ic->nb_streams)
        return false;

    avctx = ic->streams[stream_index]->codec;

    if (avctx->codec_id == AV_CODEC_ID_HEVC
#if defined(__ANDROID__)
        && (ffp->mediacodec_all_videos || ffp->mediacodec_hevc)
#elif defined(__APPLE__)
        && ffp->vtb_h265
#endif
       ) {
        return true;
    } else if (avctx->codec_id == AV_CODEC_ID_H264
#if defined(__ANDROID__)
               && (ffp->mediacodec_all_videos || ffp->mediacodec_avc)
#elif defined(__APPLE__)
               && ffp->vtb_h264
#endif
              ) {
        return true;
    } else {
        return false;
    }
}

/* open a given stream. Return 0 if OK */
int stream_component_open(FFPlayer* ffp, int stream_index) {
    VideoState* is = ffp->is;
    AVFormatContext* ic = is->ic;
    AVCodecContext* avctx;
    AVCodec* codec;
    const char* forced_codec_name = NULL;
    AVDictionary* opts;
    AVDictionaryEntry* t = NULL;
    int sample_rate, nb_channels;
    int64_t channel_layout;
    int ret = 0;
    int stream_lowres = ffp->lowres;

    if (stream_index < 0 || stream_index >= ic->nb_streams)
        return -1;
    avctx = ic->streams[stream_index]->codec;

    if (avctx->codec_id == AV_CODEC_ID_HEVC) {
        codec = avcodec_find_decoder_by_name(
                    ffp->preferred_hevc_codec_name ? ffp->preferred_hevc_codec_name : "libqy265dec");
        if (!codec) {
            codec = avcodec_find_decoder(avctx->codec_id);
            ALOGW("[%u] Preferred HEVC codec not found '%s', falling back to '%s'\n", ffp->session_id,
                  ffp->preferred_hevc_codec_name ? ffp->preferred_hevc_codec_name : "null", codec ? codec->name : "null");
        }
    } else if (ffp->aac_libfdk == 1 && avctx->codec_id == AV_CODEC_ID_AAC) {
        codec = avcodec_find_decoder_by_name("libfdk_aac");
        if (!codec) {
            codec = avcodec_find_decoder(avctx->codec_id);
        }
    } else {
        codec = avcodec_find_decoder(avctx->codec_id);
    }

    switch (avctx->codec_type) {
        case AVMEDIA_TYPE_AUDIO   :
            is->last_audio_stream = stream_index;
            forced_codec_name = ffp->audio_codec_name;
            break;
        // FFP_MERGE: case AVMEDIA_TYPE_SUBTITLE:
        case AVMEDIA_TYPE_VIDEO   :
            is->last_video_stream = stream_index;
            forced_codec_name = ffp->video_codec_name;
            break;
        default:
            break;
    }
    if (forced_codec_name)
        codec = avcodec_find_decoder_by_name(forced_codec_name);
    if (!codec) {
        if (forced_codec_name)
            ALOGW("[%u] No codec could be found with name '%s'\n", ffp->session_id, forced_codec_name);
        else
            ALOGW("[%u] No codec could be found with id %d\n", ffp->session_id, avctx->codec_id);
        return -1;
    }

    if (codec->name && (strcmp(codec->name, "libqy265dec") == 0)) {
        // if codec is qy265, fill in QY265's auth info.
        // Note that qy265_auth_opaque is NULL on iOS, and will be set in ijkplay_jni.c in android
        avctx->opaque = ffp->qy265_auth_opaque;
    }

    avctx->codec_id = codec->id;
    if (stream_lowres > av_codec_get_max_lowres(codec)) {
        ALOGW("[%u] The maximum value for lowres supported by the decoder is %d\n",
              ffp->session_id, av_codec_get_max_lowres(codec));
        stream_lowres = av_codec_get_max_lowres(codec);
    }
    av_codec_set_lowres(avctx, stream_lowres);

#if FF_API_EMU_EDGE
    if (stream_lowres)
        avctx->flags |= CODEC_FLAG_EMU_EDGE;
#endif
    if (ffp->fast)
        avctx->flags2 |= AV_CODEC_FLAG2_FAST;
#if FF_API_EMU_EDGE
    if (codec->capabilities & AV_CODEC_CAP_DR1)
        avctx->flags |= CODEC_FLAG_EMU_EDGE;
#endif

    opts = filter_codec_opts(ffp->codec_opts, avctx->codec_id, ic, ic->streams[stream_index],
                             codec);
    if (!av_dict_get(opts, "threads", NULL, 0))
        av_dict_set(&opts, "threads", "auto", 0);
    if (stream_lowres)
        av_dict_set_int(&opts, "lowres", stream_lowres, 0);
    if (avctx->codec_type == AVMEDIA_TYPE_VIDEO || avctx->codec_type == AVMEDIA_TYPE_AUDIO)
        av_dict_set(&opts, "refcounted_frames", "1", 0);

    if ((t = av_dict_get(opts, "", NULL, AV_DICT_IGNORE_SUFFIX))) {
        ALOGE("[%u] Option %s not found.\n", ffp->session_id, t->key);
#ifdef FFP_MERGE
        ret =  AVERROR_OPTION_NOT_FOUND;
        goto fail;
#endif
    }

    // 给kw265/kvc设置输出格式: 0 - yuv420p, 1 - nv12, 2 - nv21
    // 如果overlay_format为SDL_FCC_NV21，则直接把输出格式设置为nv21
    // 如果overlay_format为SDL_FCC_YV12或SDL_FCC_I420，则直接把输出格式设置为yuv420p
    // 否则把输出格式设置为默认格式nv12，之后由播放器渲染部分自行转换
    if (codec->name &&
        (strcmp(codec->name, "libks265dec") == 0 ||
         strcmp(codec->name, "libkvc265dec") == 0 ||
         strcmp(codec->name, "libkvcdec") == 0)) {
        if (ffp->overlay_format == SDL_FCC_NV21) {
            av_dict_set_int(&opts, "output_pixfmt", KW265_OUTPUT_PIXFMT_NV21, 0);
        } else if (ffp->overlay_format == SDL_FCC_YV12 || ffp->overlay_format == SDL_FCC_I420) {
            av_dict_set_int(&opts, "output_pixfmt", KW265_OUTPUT_PIXFMT_YUV420P, 0);
        } else {
            av_dict_set_int(&opts, "output_pixfmt", KW265_OUTPUT_PIXFMT_DEFAULT, 0);
        }
    }

    is->eof = 0;
    ic->streams[stream_index]->discard = AVDISCARD_DEFAULT;

    switch (avctx->codec_type) {
        case AVMEDIA_TYPE_AUDIO:
#if CONFIG_AVFILTER
            {
                AVFilterLink* link;
                if ((ret = avcodec_open2(avctx, codec, &opts)) < 0) {
                    goto fail;
                }
                is->audio_filter_src.freq = avctx->sample_rate;
                is->audio_filter_src.channels = avctx->channels;
                is->audio_filter_src.channel_layout = get_valid_channel_layout(avctx->channel_layout,
                                                                               avctx->channels);
                is->audio_filter_src.fmt = avctx->sample_fmt;
                SDL_LockMutex(ffp->af_mutex);
                if ((ret = configure_audio_filters(ffp, ffp->afilters, 0)) < 0) {
                    SDL_UnlockMutex(ffp->af_mutex);
                    goto fail;
                }
                ffp->af_changed = 0;
                SDL_UnlockMutex(ffp->af_mutex);
                link = is->out_audio_filter->inputs[0];
                sample_rate = link->sample_rate;
                nb_channels = link->channels;
                channel_layout = link->channel_layout;
            }
#else
            sample_rate    = avctx->sample_rate;
            nb_channels    = avctx->channels;
            channel_layout = avctx->channel_layout;
#endif
            /* prepare audio output */
            if ((ret = audio_open(ffp, channel_layout, nb_channels, sample_rate, &is->audio_tgt)) <
                0)
                goto fail;
            ffp_set_audio_codec_info(ffp, avcodec_get_name(avctx->codec_id),
                                     avctx->codec->name);
            ALOGI("[%u] selected codec: %s for codec id %d\n", ffp->session_id, codec->name, codec->id);

            is->audio_hw_buf_size = ret;
            is->audio_src = is->audio_tgt;
            is->audio_buf_size = 0;
            is->audio_buf_index = 0;

            /* init averaging filter */
            is->audio_diff_avg_coef = exp(log(0.01) / AUDIO_DIFF_AVG_NB);
            is->audio_diff_avg_count = 0;
            /* since we do not have a precise anough audio fifo fullness,
               we correct audio sync only if larger than this threshold */
            is->audio_diff_threshold = 2.0 * is->audio_hw_buf_size / is->audio_tgt.bytes_per_sec;

            is->audio_stream = stream_index;
            is->audio_st = ic->streams[stream_index];

            decoder_init(&is->auddec, avctx, &is->audioq, is->continue_read_thread);
            if ((is->ic->iformat->flags &
                 (AVFMT_NOBINSEARCH | AVFMT_NOGENSEARCH | AVFMT_NO_BYTE_SEEK)) &&
                !is->ic->iformat->read_seek) {
                is->auddec.start_pts = is->audio_st->start_time;
                is->auddec.start_pts_tb = is->audio_st->time_base;
            }
            if ((ret = decoder_start(&is->auddec, audio_decode_thread, ffp, "ff_audio_dec")) < 0)
                goto fail;
            SDL_AoutPauseAudio(ffp->aout, 0);
            break;
        case AVMEDIA_TYPE_VIDEO:
            is->video_stream = stream_index;
            is->video_st = ic->streams[stream_index];

            if (avctx->colorspace == AVCOL_SPC_BT2020_NCL && avctx->color_trc == AVCOL_TRC_SMPTEST2084) {
                ALOGD("[%u] colorspace: %s, color_primaries: %s, color_trc: %s\n",
                      ffp->session_id, av_color_space_name(avctx->colorspace),
                      av_color_primaries_name(avctx->color_primaries),
                      av_color_transfer_name(avctx->color_trc));
                ffp->is_hdr = true;
            }

            decoder_init(&is->viddec, avctx, &is->videoq, is->continue_read_thread);
            ffp->node_vdec = ffpipeline_open_video_decoder(ffp->pipeline, ffp);
            if (!ffp->node_vdec) {
                ALOGE("[%u] ffpipeline_open_video_decoder failed\n", ffp->session_id);
                goto fail;
            } else if (ffp->stat.vdec_type == FFP_PROPV_DECODER_AVCODEC) {
                if ((ret = avcodec_open2(avctx, codec, &opts)) < 0) {
                    ALOGE("[%u] avcodec_open2 failed\n", ffp->session_id);
                    goto fail;
                } else {
                    ffp_set_video_codec_info(ffp, avcodec_get_name(avctx->codec_id),
                                             avctx->codec->name);
                }
                ALOGI("[%u] selected codec: %s for codec id %d\n", ffp->session_id, codec->name, codec->id);
            }
            KwaiQos_copyVideoStreamMetadata(&ffp->kwai_qos, is->video_st);

#ifdef FFP_MERGE
            is->viddec_width  = avctx->width;
            is->viddec_height = avctx->height;
#endif
            KwaiQos_setResolution(&ffp->kwai_qos, avctx->width, avctx->height);

            if ((ret = decoder_start(&is->viddec, video_decode_thread, ffp, "ff_video_dec")) < 0)
                goto fail;
            is->queue_attachments_req = 1;

            if (is->video_st->avg_frame_rate.den && is->video_st->avg_frame_rate.num) {
                double fps = av_q2d(is->video_st->avg_frame_rate);
                SDL_ProfilerReset(&is->viddec.decode_profiler, fps + 0.5);
                if (fps > ffp->max_fps && fps < 130.0) {
                    is->is_video_high_fps = 1;
                    ALOGW("[%u][%s] fps: %lf (too high)\n", ffp->session_id, __func__, fps);
                } else {
                    ALOGW("[%u][%s] fps: %lf (normal)\n", ffp->session_id, __func__, fps);
                }
            }
            if (is->video_st->r_frame_rate.den && is->video_st->r_frame_rate.num) {
                double tbr = av_q2d(is->video_st->r_frame_rate);
                if (tbr > ffp->max_fps && tbr < 130.0) {
                    is->is_video_high_fps = 1;
                    ALOGW("[%u][%s] tbr: %lf (too high)\n", ffp->session_id, __func__, tbr);
                } else {
                    ALOGW("[%u][%s] tbr: %lf (normal)\n", ffp->session_id, __func__, tbr);
                }
            }

            //workaround disable is_video_high_fps as frame_rate is not correct
//        if (is->is_video_high_fps) {
//            avctx->skip_frame       = FFMAX(avctx->skip_frame, AVDISCARD_NONREF);
//            avctx->skip_loop_filter = FFMAX(avctx->skip_loop_filter, AVDISCARD_NONREF);
//            avctx->skip_idct        = FFMAX(avctx->skip_loop_filter, AVDISCARD_NONREF);
//        }

            break;
        // FFP_MERGE: case AVMEDIA_TYPE_SUBTITLE:
        default:
            break;
    }
fail:
    av_dict_free(&opts);

    return ret;
}


int sync_chasing_threshold(FFPlayer* ffp) {
    VideoState* is = ffp->is;
    is->chasing_enabled = 1;
    is->chasing_status = 0;
    is->i_buffer_time_max = ffp->i_buffer_time_max;
    if (ffp->i_buffer_time_max <= 0) {
        is->chasing_enabled = 0;
    }
    if (is->realtime && ffp->i_buffer_time_max >= 1700 &&
        ffp->i_buffer_time_max - 300 < ffp->dcc.last_high_water_mark_in_ms
       ) {
        ffp->dcc.last_high_water_mark_in_ms = ffp->i_buffer_time_max - 300;
    }
    return 0;
}

/* pause or resume the video */
void stream_toggle_pause_l(FFPlayer* ffp, int pause_on) {
    VideoState* is = ffp->is;
    if (is->paused && !pause_on) {
        is->frame_timer += av_gettime_relative() / 1000000.0 - is->vidclk.last_updated;

#ifdef FFP_MERGE
        if (is->read_pause_return != AVERROR(ENOSYS)) {
            is->vidclk.paused = 0;
        }
#endif
        set_clock(&is->vidclk, get_clock(&is->vidclk), is->vidclk.serial);
        set_clock(&is->audclk, get_clock(&is->audclk), is->audclk.serial);
        ffp_on_clock_changed(ffp, &is->vidclk);
        ffp_on_clock_changed(ffp, &is->audclk);
    } else {
    }
    // 这一行理论不可能崩溃的，发个灰度日志 https://bugly.qq.com/v2/crash-reporting/crashes/900014602/121495289/report?pid=1
    ALOGI("[%u] stream_toggle_pause_l->set_clock, is:%p, &is->extclk:%p, is->extclk.queue_serial:%p \n",
          ffp->session_id, is, &is->extclk, is->extclk.queue_serial);
    set_clock(&is->extclk, get_clock(&is->extclk), is->extclk.serial);
    ffp_on_clock_changed(ffp, &is->extclk);
    if (is->step && (is->pause_req || is->buffering_on)) {
        is->paused = is->vidclk.paused = is->extclk.paused = pause_on;
    } else {
        if (ffp->first_audio_frame_rendered > 0 || ffp->first_video_frame_rendered > 0) {
            if (!pause_on) {
                KwaiQos_onStartPlayer(&ffp->kwai_qos);
            } else {
                KwaiQos_onPausePlayer(&ffp->kwai_qos);
            }
        }
        is->paused = is->audclk.paused = is->vidclk.paused = is->extclk.paused = pause_on;
        SDL_AoutPauseAudio(ffp->aout, pause_on);
    }
}

void stream_update_pause_l(FFPlayer* ffp) {
    VideoState* is = ffp->is;
    if (!is->step && (is->pause_req || is->buffering_on)) {
        stream_toggle_pause_l(ffp, 1);
    } else {
        stream_toggle_pause_l(ffp, 0);
    }
}


/* seek in the stream */
void stream_seek(VideoState* is, int64_t pos, int64_t rel, int seek_by_bytes) {
    if (!is->seek_req) {
        // add for testing photo-algo
        // kwai_clear_cache_dir();
        is->seek_pos = pos;
        is->seek_rel = rel;
        is->seek_flags &= ~AVSEEK_FLAG_BYTE;
        if (seek_by_bytes)
            is->seek_flags |= AVSEEK_FLAG_BYTE;
        is->seek_req = 1;
        SDL_CondSignal(is->continue_read_thread);
    } else {
        SDL_LockMutex(is->cached_seek_mutex);
        is->seek_cached_pos = pos;
        SDL_UnlockMutex(is->cached_seek_mutex);
        is->seek_cached_rel = rel;
        is->seek_cached_flags &= ~AVSEEK_FLAG_BYTE;
        if (seek_by_bytes)
            is->seek_cached_flags |= AVSEEK_FLAG_BYTE;
    }
}

void toggle_pause_l(FFPlayer* ffp, int pause_on) {
    VideoState* is = ffp->is;
    if (is->pause_req && !pause_on) {
        set_clock(&is->vidclk, get_clock(&is->vidclk), is->vidclk.serial);
        set_clock(&is->audclk, get_clock(&is->audclk), is->audclk.serial);
        ffp_on_clock_changed(ffp, &is->vidclk);
        ffp_on_clock_changed(ffp, &is->audclk);
    }
    is->pause_req = pause_on;
    ffp->auto_resume = !pause_on;

    stream_update_pause_l(ffp);
    is->step = 0;
}

void toggle_pause(FFPlayer* ffp, int pause_on) {
    SDL_LockMutex(ffp->is->play_mutex);
    toggle_pause_l(ffp, pause_on);
    SDL_UnlockMutex(ffp->is->play_mutex);
}

// FFP_MERGE: toggle_mute
// FFP_MERGE: update_volume

void step_to_next_frame_l(FFPlayer* ffp) {
    VideoState* is = ffp->is;
    /* if the stream is paused unpause it, then step */
    is->step = 1;
    if (is->paused)
        stream_toggle_pause_l(ffp, 0);
}

void step_to_next_frame(FFPlayer* ffp) {
    SDL_LockMutex(ffp->is->play_mutex);
    step_to_next_frame_l(ffp);
    SDL_UnlockMutex(ffp->is->play_mutex);
}

bool is_hls(AVFormatContext* ic) {
    return !strcmp(ic->iformat->name, "hls,applehttp") ||
           !strcmp(ic->iformat->name, "hls,kwai") ||
           !strcmp(ic->iformat->name, "khls,kwai");
}

int ffp_get_total_history_cached_duration_ms(FFplayer* ffp) {
    VideoState* is = ffp->is;

    int audio_time_base_valid = 0;
    int video_time_base_valid = 0;

    int cached_duration_in_ms = -1;
    int64_t audio_cached_duration = -1;
    int64_t video_cached_duration = -1;

    if (is->audio_st)
        audio_time_base_valid = is->audio_st->time_base.den > 0 && is->audio_st->time_base.num > 0;
    if (is->video_st)
        video_time_base_valid = is->video_st->time_base.den > 0 && is->video_st->time_base.num > 0;

    if (is->audio_st && audio_time_base_valid) {
        audio_cached_duration = (int64_t)(ffp->is->audioq.history_total_duration *
                                          av_q2d(is->audio_st->time_base) * 1000);
    }

    if (is->video_st && video_time_base_valid) {
        video_cached_duration = (int64_t)(ffp->is->videoq.history_total_duration *
                                          av_q2d(is->video_st->time_base) * 1000);
    }

    if (video_cached_duration > 0 && audio_cached_duration > 0) {
        cached_duration_in_ms = (int) IJKMIN(video_cached_duration, audio_cached_duration);
    } else if (video_cached_duration > 0) {
        cached_duration_in_ms = (int) video_cached_duration;
    } else if (audio_cached_duration > 0) {
        cached_duration_in_ms = (int) audio_cached_duration;
    }

    return cached_duration_in_ms;
}

bool ffp_is_pre_demux_enabled(FFPlayer* ffp) {
    return ffp
           && !ffp->islive
           && ffp->pre_demux
           && (ffp_is_pre_demux_ver1_enabled(ffp) || ffp_is_pre_demux_ver2_enabled(ffp));
}

bool ffp_is_pre_demux_ver1_enabled(FFPlayer* ffp) {
    return ffp->use_pre_demux_ver == 1;
}

bool ffp_is_pre_demux_ver2_enabled(FFPlayer* ffp) {
    return ffp->use_pre_demux_ver == 2;
}
