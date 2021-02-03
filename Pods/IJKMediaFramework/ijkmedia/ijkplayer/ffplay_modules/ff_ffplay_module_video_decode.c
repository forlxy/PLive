//
// Created by MarshallShuai on 2019/4/19.
//

#include "ff_ffplay_module_video_decode.h"
#include <libavutil/frame.h>

#include "ff_cmdutils.h"
#include "ff_ffmsg.h"
#include "ff_ffplay.h"
#include "ff_ffplay_clock.h"
#include "ff_ffplay_internal.h"


/**
 * 视频解码线程的入口
 * @param arg
 * @return
 */
int video_decode_thread(void* arg) {
    FFPlayer* ffp = (FFPlayer*)arg;
    int       ret = 0;

    if (ffp->node_vdec) {
        ret = ffpipenode_run_sync(ffp->node_vdec);
    }
    return ret;
}
/**
 * 视频解码使用的解码器类型
 */
int get_video_decode_type(struct FFPlayer* ffp) {
    if (ffp && ffp->prepared) {
        return (int)ffp->stat.vdec_type;
    } else {
        return FFP_PROPV_DECODER_UNKNOWN;
    }
}


#if CONFIG_AVFILTER
static int configure_video_filters(FFPlayer* ffp, AVFilterGraph* graph, VideoState* is, const char* vfilters, AVFrame* frame) {
    // buffersink支持的pix_fmts列表，若解码器输出格式在此列表中则filter不会自己转换
    static const enum AVPixelFormat pix_fmts[] = { AV_PIX_FMT_YUV420P, AV_PIX_FMT_NV12, AV_PIX_FMT_NV21, AV_PIX_FMT_NONE };
    char sws_flags_str[512] = "";
    char buffersrc_args[256] = "";
    int ret;
    AVFilterContext* filt_src = NULL, *filt_out = NULL, *last_filter = NULL;
    AVCodecContext* codec = is->video_st->codec;
    AVRational fr = av_guess_frame_rate(is->ic, is->video_st, NULL);
    AVDictionaryEntry* e = NULL;

    while ((e = av_dict_get(ffp->sws_dict, "", e, AV_DICT_IGNORE_SUFFIX))) {
        if (!strcmp(e->key, "sws_flags")) {
            av_strlcatf(sws_flags_str, sizeof(sws_flags_str), "%s=%s:", "flags", e->value);
        } else
            av_strlcatf(sws_flags_str, sizeof(sws_flags_str), "%s=%s:", e->key, e->value);
    }
    if (strlen(sws_flags_str))
        sws_flags_str[strlen(sws_flags_str) - 1] = '\0';

    graph->scale_sws_opts = av_strdup(sws_flags_str);

    snprintf(buffersrc_args, sizeof(buffersrc_args),
             "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
             frame->width, frame->height, frame->format,
             is->video_st->time_base.num, is->video_st->time_base.den,
             codec->sample_aspect_ratio.num, FFMAX(codec->sample_aspect_ratio.den, 1));
    if (fr.num && fr.den)
        av_strlcatf(buffersrc_args, sizeof(buffersrc_args), ":frame_rate=%d/%d", fr.num, fr.den);

    if ((ret = avfilter_graph_create_filter(&filt_src,
                                            avfilter_get_by_name("buffer"),
                                            "ffplay_buffer", buffersrc_args, NULL,
                                            graph)) < 0)
        goto fail;

    ret = avfilter_graph_create_filter(&filt_out,
                                       avfilter_get_by_name("buffersink"),
                                       "ffplay_buffersink", NULL, NULL, graph);
    if (ret < 0)
        goto fail;

    if ((ret = av_opt_set_int_list(filt_out, "pix_fmts", pix_fmts, AV_PIX_FMT_NONE,
                                   AV_OPT_SEARCH_CHILDREN)) < 0)
        goto fail;

    last_filter = filt_out;

    /* Note: this macro adds a filter before the lastly added filter, so the
     * processing order of the filters is in reverse */
#define INSERT_FILT(name, arg) do {                                          \
        AVFilterContext *filt_ctx;                                               \
        \
        ret = avfilter_graph_create_filter(&filt_ctx,                            \
                                           avfilter_get_by_name(name),           \
                                           "ffplay_" name, arg, NULL, graph);    \
        if (ret < 0)                                                             \
            goto fail;                                                           \
        \
        ret = avfilter_link(filt_ctx, 0, last_filter, 0);                        \
        if (ret < 0)                                                             \
            goto fail;                                                           \
        \
        last_filter = filt_ctx;                                                  \
    } while (0)

    /* SDL YUV code is not handling odd width/height for some driver
     * combinations, therefore we crop the picture to an even width/height. */
    INSERT_FILT("crop", "floor(in_w/2)*2:floor(in_h/2)*2");

    if (ffp->autorotate) {
        double theta = get_rotation(is->video_st);

        if (fabs(theta - 90) < 1.0) {
            INSERT_FILT("transpose", "clock");
        } else if (fabs(theta - 180) < 1.0) {
            INSERT_FILT("hflip", NULL);
            INSERT_FILT("vflip", NULL);
        } else if (fabs(theta - 270) < 1.0) {
            INSERT_FILT("transpose", "cclock");
        } else if (fabs(theta) > 1.0) {
            char rotate_buf[64];
            snprintf(rotate_buf, sizeof(rotate_buf), "%f*PI/180", theta);
            INSERT_FILT("rotate", rotate_buf);
        }
    }

#ifdef FFP_AVFILTER_PLAYBACK_RATE
    if (fabsf(ffp->pf_playback_rate) > 0.00001 &&
        fabsf(ffp->pf_playback_rate - 1.0f) > 0.00001) {
        char setpts_buf[256];
        float rate = 1.0f / ffp->pf_playback_rate;
        rate = av_clipf_c(rate, 0.5f, 2.0f);
        ALOGI("[%u] vf_rate=%f(1/%f)\n", ffp->session_id, ffp->pf_playback_rate, rate);
        snprintf(setpts_buf, sizeof(setpts_buf), "%f*PTS", rate);
        INSERT_FILT("setpts", setpts_buf);
    }
#endif

    if ((ret = configure_filtergraph(graph, vfilters, filt_src, last_filter)) < 0)
        goto fail;

    is->in_video_filter = filt_src;
    is->out_video_filter = filt_out;

fail:
    return ret;
}
#endif // CONFIG_AVFILTER

bool should_drop_decoded_video_frame(struct FFPlayer* ffp, AVFrame* frame, const char* debug_tag) {
    VideoState* is = ffp->is;

    if (frame->pts != AV_NOPTS_VALUE) {
        double dpts = av_q2d(is->video_st->time_base) * frame->pts;
        if (ffp->framedrop > 0
            || (ffp->framedrop && get_master_sync_type(is) != AV_SYNC_VIDEO_MASTER)) {

            double master_clk =  get_master_clock(is);
            double diff = dpts - master_clk;

            if (!isnan(diff) && fabs(diff) < AV_NOSYNC_THRESHOLD
                && diff - is->frame_last_filter_delay < 0
                && is->viddec.pkt_serial == is->vidclk.serial
                && is->videoq.nb_packets
                && (!ffp->islive || 0 != is->show_frame_count)  // 直播里必须先显示一帧，短视频不需要这个逻辑（2017年3月逻辑, @see git show eba4c70);
                && (is->video_st && is->audio_st) // 主动丢帧的逻辑主要是为了av同步，在videoonly的情况下，可以不走这个逻辑(A1优化)
               ) {

                if (is->continuous_frame_drops_early >= ffp->framedrop) {
                    // force render this frame
                    is->continuous_frame_drops_early = 0;
                } else {
                    is->continuous_frame_drops_early++;

                    if (is_playback_rate_normal(ffp->pf_playback_rate)) {
                        is->frame_drops_early++;
                        KwaiQos_onDecodedDroppedFrame(&ffp->kwai_qos, is->frame_drops_early);
                    }

                    ALOGW("[%d][%s] early_drop, drop/equeue/total:%d/%d/%d, continues_drop:%d/%d,"
                          "diff:%3.3f, dpts:%3.3f, master_clk:%3.3f,"
                          "frame_last_filter_delay:%f \n",
                          ffp->session_id, debug_tag,
                          is->frame_drops_early, is->v_frame_enqueue_count, ffp->kwai_qos.runtime_stat.v_decode_frame_count,
                          is->continuous_frame_drops_early, ffp->framedrop,
                          diff, dpts, master_clk,
                          is->frame_last_filter_delay);

                    return true;
                }
            } else {
                is->continuous_frame_drops_early = 0;
            }
        }
    } // if (frame->pts != AV_NOPTS_VALUE)

    is->v_frame_enqueue_count++;
    return false;
}

static int get_video_frame(FFPlayer* ffp, AVFrame* frame, AVPacketTime* p_pkttime) {
    VideoState* is = ffp->is;
    int got_picture;

    ffp_video_statistic_l(ffp);

    if ((got_picture = decoder_decode_frame(ffp, &is->viddec, frame, NULL, p_pkttime)) < 0)
        return -1;

    if (got_picture) {
        frame->sample_aspect_ratio = av_guess_sample_aspect_ratio(is->ic, is->video_st, frame);

#ifdef FFP_MERGE
        is->viddec_width  = frame->width;
        is->viddec_height = frame->height;
#endif

        if (should_drop_decoded_video_frame(ffp, frame, "ffplay:get_video_frame")) {
            av_frame_unref(frame);
            got_picture = 0;
        }
    }

    return got_picture;
}



/**
 * video软解的线程执行体
 */
int ffplay_video_thread(void* arg) {
    FFPlayer* ffp = arg;
    VideoState* is = ffp->is;
    AVFrame* frame = av_frame_alloc();
    double pts;
    double duration;
    int ret;
    AVRational tb = is->video_st->time_base;
    AVRational frame_rate = av_guess_frame_rate(is->ic, is->video_st, NULL);
    AVPacketTime pkttime;

#if CONFIG_AVFILTER
    AVFilterGraph* graph = avfilter_graph_alloc();
    AVFilterContext* filt_out = NULL, *filt_in = NULL;
    int last_w = 0;
    int last_h = 0;
    enum AVPixelFormat last_format = -2;
    int last_serial = -1;
    int last_vfilter_idx = 0;
    if (!graph) {
        av_frame_free(&frame);
        return AVERROR(ENOMEM);
    }
#else
    ffp_notify_msg2(ffp, FFP_MSG_VIDEO_ROTATION_CHANGED, ffp_get_video_rotate_degrees(ffp));
#endif

    if (!frame) {
#if CONFIG_AVFILTER
        avfilter_graph_free(&graph);
#endif
        return AVERROR(ENOMEM);
    }

    for (;;) {
        memset(&pkttime, 0, sizeof(AVPacketTime));
        ret = get_video_frame(ffp, frame, &pkttime);

        if (ret < 0) {
            goto the_end;
        }
        if (!ret) {
            continue;
        }

#if CONFIG_AVFILTER
        if (last_w != frame->width
            || last_h != frame->height
            || last_format != frame->format
            || last_serial != is->viddec.pkt_serial
            || last_vfilter_idx != is->vfilter_idx
            || ffp->vf_changed) {
            SDL_LockMutex(ffp->vf_mutex);
            ffp->vf_changed = 0;
            ALOGD("[%u] Video frame changed from size:%dx%d format:%s serial:%d to size:%dx%d format:%s serial:%d\n",
                  ffp->session_id, last_w, last_h,
                  (const char*)av_x_if_null(av_get_pix_fmt_name(last_format), "none"), last_serial,
                  frame->width, frame->height,
                  (const char*)av_x_if_null(av_get_pix_fmt_name(frame->format), "none"), is->viddec.pkt_serial);
            avfilter_graph_free(&graph);
            graph = avfilter_graph_alloc();
            if ((ret = configure_video_filters(ffp, graph, is, ffp->vfilters_list
                                               ? ffp->vfilters_list[is->vfilter_idx]
                                               : NULL, frame)) < 0) {
                SDL_UnlockMutex(ffp->vf_mutex);
                goto the_end;
            }
            filt_in = is->in_video_filter;
            filt_out = is->out_video_filter;
            last_w = frame->width;
            last_h = frame->height;
            last_format = frame->format;
            last_serial = is->viddec.pkt_serial;
            last_vfilter_idx = is->vfilter_idx;
            frame_rate = filt_out->inputs[0]->frame_rate;
            SDL_UnlockMutex(ffp->vf_mutex);
        }

        ret = av_buffersrc_add_frame(filt_in, frame);
        if (ret < 0) {
            goto the_end;
        }

        while (ret >= 0 && filt_out) {
            is->frame_last_returned_time = av_gettime_relative() / 1000000.0;

            ret = av_buffersink_get_frame_flags(filt_out, frame, 0);
            if (ret < 0) {
                if (ret == AVERROR_EOF)
                    is->viddec.finished = is->viddec.pkt_serial;
                ret = 0;
                break;
            }

            is->frame_last_filter_delay =
                av_gettime_relative() / 1000000.0 - is->frame_last_returned_time;
            if (fabs(is->frame_last_filter_delay) > AV_NOSYNC_THRESHOLD / 10.0)
                is->frame_last_filter_delay = 0;
            tb = filt_out->inputs[0]->time_base;
#endif
            duration = (frame_rate.num && frame_rate.den ? av_q2d((AVRational) {frame_rate.den, frame_rate.num}) : 0);
            pts = (frame->pts == AV_NOPTS_VALUE) ? NAN : frame->pts * av_q2d(tb);
            ret = queue_picture(ffp, frame, pts, duration, av_frame_get_pkt_pos(frame),
                                is->viddec.pkt_serial, &pkttime);
            av_frame_unref(frame);
#if CONFIG_AVFILTER
        }
#endif

        if (ret < 0) {
            goto the_end;
        }
    }
the_end:
#if CONFIG_AVFILTER
    avfilter_graph_free(&graph);
#endif
    av_frame_free(&frame);
    return 0;
}
