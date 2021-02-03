//
// Created by MarshallShuai on 2019/4/22.
//

#include "ff_ffplay_decoder_internal.h"

#include <libavutil/avutil.h>
#include <libavutil/frame.h>

#include "ijksdl/ffmpeg/ijksdl_inc_ffmpeg.h"
#include "ijkkwai/kwai_qos.h"
#include "ff_ffplay_def.h"
#include "ff_ffplay.h"
#include "ff_ffplay_debug.h"
#include "ff_ffinc.h"
#include "ff_ffplay_internal.h"


void
decoder_init(Decoder* d, AVCodecContext* avctx, PacketQueue* queue, SDL_cond* empty_queue_cond) {
    memset(d, 0, sizeof(Decoder));
    d->avctx = avctx;
    d->queue = queue;
    d->empty_queue_cond = empty_queue_cond;
    d->start_pts = AV_NOPTS_VALUE;

    d->first_frame_decoded_time = SDL_GetTickHR();
    d->first_frame_decoded = 0;
    d->first_frame_pts = -1;
    SDL_ProfilerReset(&d->decode_profiler, -1);
}

int decoder_start(Decoder* d, int (*fn)(void*), void* arg, const char* name) {
    d->decoder_tid = SDL_CreateThreadEx(&d->_decoder_tid, fn, arg, name);
    if (!d->decoder_tid) {
        ALOGE("SDL_CreateThread(): %s\n", SDL_GetError());
        return AVERROR(ENOMEM);
    }
    return 0;
}

void decoder_destroy(Decoder* d) {
    if (!d)
        return;
    av_packet_unref(&d->pkt);
}


void decoder_abort(Decoder* d, FrameQueue* fq) {
    if (!d || !fq || !d->queue)
        return;

    packet_queue_abort(d->queue);
    frame_queue_signal(fq);
    if (d->decoder_tid) {
        SDL_WaitThread(d->decoder_tid, NULL);
        d->decoder_tid = NULL;
    }
    packet_queue_flush(d->queue);
}

static void ffp_on_decode_audio_ret(FFPlayer* ffp, int ret) {
    if (ret >= 0) {
        ffp->continue_audio_dec_err_cnt = 0;
        return;
    }
    ffp->continue_audio_dec_err_cnt++;
    if (ffp->continue_audio_dec_err_cnt == DECODE_ERROR_WARN_THRESHOLD) {
        ffp_notify_msg2(ffp, FFP_MSG_DECODE_ERROR, ret);
    }
}

int decoder_decode_frame(FFPlayer* ffp, Decoder* d, AVFrame* frame, AVSubtitle* sub,
                         AVPacketTime* p_pkttime) {
    int got_frame = 0;

    do {
        int ret = -1;

        if (d->queue->abort_request)
            return -1;

        if (!d->packet_pending || d->queue->serial != d->pkt_serial) {
            AVPacket pkt;
            do {
                if (d->queue->nb_packets == 0)
                    SDL_CondSignal(d->empty_queue_cond);
                if (packet_queue_get_or_buffering(ffp, d->queue, &pkt, &d->pkt_serial, p_pkttime,
                                                  &d->finished) < 0)
                    return -1;
                if (pkt.data == flush_pkt.data) {
                    avcodec_flush_buffers(d->avctx);
                    d->finished = 0;
                    d->next_pts = d->start_pts;
                    d->next_pts_tb = d->start_pts_tb;
                }
            } while (pkt.data == flush_pkt.data || d->queue->serial != d->pkt_serial);
            av_packet_unref(&d->pkt);
            d->pkt_temp = d->pkt = pkt;
            d->packet_pending = 1;
        }

        switch (d->avctx->codec_type) {
            case AVMEDIA_TYPE_VIDEO: {
                // QosInfo
                if (d->pkt_temp.pts != AV_NOPTS_VALUE && ffp->qos_pts_offset_got &&
                    ffp->wall_clock_updated) {
                    int64_t ptsMs = av_rescale_q(d->pkt_temp.pts, ffp->is->video_st->time_base,
                    (AVRational) {1, 1000});
                    DelayStat_calc_pts_delay(&ffp->qos_delay_video_before_dec, ffp->wall_clock_offset,
                                             ffp->qos_pts_offset, ptsMs);
                }

                ffp->stat.vrps = SDL_SpeedSamplerAdd(&ffp->vrps_sampler, FFP_SHOW_VRPS_AVCODEC,
                                                     "vrps[avcodec]");
                KwaiQos_onVideoFrameBeforeDecode(&ffp->kwai_qos);

                if (ffp->enable_accurate_seek && ffp->is->video_accurate_seek_req
                    && !ffp->is->seek_req) {
                    if (d->pkt_temp.pts != AV_NOPTS_VALUE) {
                        double pts = d->pkt_temp.pts * av_q2d(ffp->is->video_st->time_base);
                        if (pts * 1000 * 1000 >= ffp->is->seek_pos)
                            unsetVideoDecodeDiscard(ffp->is->viddec.avctx);
                    }
                }

                ret = avcodec_decode_video2(d->avctx, frame, &got_frame, &d->pkt_temp);

                if (ret < 0) {
                    ALOGE("[%u] avcodec_decode_video2 ret:%d, :%s \n", ffp->session_id,
                          ret, get_error_code_fourcc_string_macro(ret));
                }

                if (got_frame) {
                    // QosInfo
                    if (frame->pkt_pts != AV_NOPTS_VALUE && ffp->qos_pts_offset_got &&
                        ffp->wall_clock_updated) {
                        int64_t ptsMs = av_rescale_q(frame->pkt_pts, ffp->is->video_st->time_base,
                        (AVRational) {1, 1000});
                        DelayStat_calc_pts_delay(&ffp->qos_delay_video_after_dec, ffp->wall_clock_offset,
                                                 ffp->qos_pts_offset, ptsMs);
                    }

                    ffp->i_video_decoded_size += ret;
                    ffp->stat.vdps = SDL_SpeedSamplerAdd(&ffp->vdps_sampler, FFP_SHOW_VDPS_AVCODEC,
                                                         "vdps[avcodec]");
                    KwaiQos_onVideoFrameDecoded(&ffp->kwai_qos);
                    if (ffp->decoder_reorder_pts == -1) {
                        frame->pts = av_frame_get_best_effort_timestamp(frame);
                    } else if (ffp->decoder_reorder_pts) {
                        frame->pts = frame->pkt_pts;
                    } else {
                        frame->pts = frame->pkt_dts;
                    }
                }
                if (ret < 0) {
                    ffp->error_count++;
                    if (got_frame) {
                        ffp->v_dec_err = ret;
                    }
                }
                KwaiQos_onSoftDecodeErr(&ffp->kwai_qos, d->avctx->decoder_errors);
            }
            break;
            case AVMEDIA_TYPE_AUDIO: {
                int64_t pkt_dur = av_rescale_q(d->pkt_temp.duration,
                                               av_codec_get_pkt_timebase(d->avctx),
                (AVRational) {1, 1000});
                KwaiQos_onAudioFrameBeforeDecode(&ffp->kwai_qos, pkt_dur);

                ret = avcodec_decode_audio4(d->avctx, frame, &got_frame, &d->pkt_temp);
                if (ret < 0) {
                    ALOGE("[%u] avcodec_decode_audio4 ret:%d, :%s \n", ffp->session_id,
                          ret, get_error_code_fourcc_string_macro(ret));
                }
                ffp_on_decode_audio_ret(ffp, ret);

                if (got_frame) {
                    ffp->i_audio_decoded_size += ret;
                    AVRational tb = (AVRational) {1, frame->sample_rate};
                    if (frame->pts != AV_NOPTS_VALUE)
                        frame->pts = av_rescale_q(frame->pts, d->avctx->time_base, tb);
                    else if (frame->pkt_pts != AV_NOPTS_VALUE)
                        frame->pts = av_rescale_q(frame->pkt_pts,
                                                  av_codec_get_pkt_timebase(d->avctx), tb);
                    else if (d->next_pts != AV_NOPTS_VALUE)
                        frame->pts = av_rescale_q(d->next_pts, d->next_pts_tb, tb);
                    if (frame->pts != AV_NOPTS_VALUE) {
                        d->next_pts = frame->pts + frame->nb_samples;
                        d->next_pts_tb = tb;
                    }
                    int64_t frame_dur = (int64_t) av_q2d(
                    (AVRational) {frame->nb_samples * 1000, frame->sample_rate});
                    KwaiQos_onAudioFrameDecoded(&ffp->kwai_qos, frame_dur);
                    // check audio pts
                    if (ffp->is->prev_audio_pts > frame->pts) {
                        ffp->is->illegal_audio_pts = 1;
                    }
                    ffp->is->prev_audio_pts = frame->pts;
                }

                if (ret < 0) {
                    KwaiQos_onAudioDecodeErr(&ffp->kwai_qos, pkt_dur);
                }
            }
            break;
            // FFP_MERGE: case AVMEDIA_TYPE_SUBTITLE:
            default:
                break;
        }

        if (ret < 0) {
            d->packet_pending = 0;
        } else {
            d->pkt_temp.dts =
                d->pkt_temp.pts = AV_NOPTS_VALUE;
            if (d->pkt_temp.data) {
                if (d->avctx->codec_type != AVMEDIA_TYPE_AUDIO)
                    ret = d->pkt_temp.size;
                d->pkt_temp.data += ret;
                d->pkt_temp.size -= ret;
                if (d->pkt_temp.size <= 0)
                    d->packet_pending = 0;
            } else {
                if (!got_frame) {
                    d->packet_pending = 0;
                    d->finished = d->pkt_serial;
                }
            }
        }
    } while (!got_frame && !d->finished);

    return got_frame;
}


#if CONFIG_AVFILTER
int configure_filtergraph(AVFilterGraph* graph, const char* filtergraph,
                          AVFilterContext* source_ctx, AVFilterContext* sink_ctx) {
    int ret, i;
    int nb_filters = graph->nb_filters;
    AVFilterInOut* outputs = NULL, *inputs = NULL;

    if (filtergraph) {
        outputs = avfilter_inout_alloc();
        inputs = avfilter_inout_alloc();
        if (!outputs || !inputs) {
            ret = AVERROR(ENOMEM);
            goto fail;
        }

        outputs->name = av_strdup("in");
        outputs->filter_ctx = source_ctx;
        outputs->pad_idx = 0;
        outputs->next = NULL;

        inputs->name = av_strdup("out");
        inputs->filter_ctx = sink_ctx;
        inputs->pad_idx = 0;
        inputs->next = NULL;

        if ((ret = avfilter_graph_parse_ptr(graph, filtergraph, &inputs, &outputs, NULL)) < 0)
            goto fail;
    } else {
        if ((ret = avfilter_link(source_ctx, 0, sink_ctx, 0)) < 0)
            goto fail;
    }

    /* Reorder the filters to ensure that inputs of the custom filters are merged first */
    for (i = 0; i < graph->nb_filters - nb_filters; i++)
        FFSWAP(AVFilterContext*, graph->filters[i], graph->filters[i + nb_filters]);

    ret = avfilter_graph_config(graph, NULL);
fail:
    avfilter_inout_free(&outputs);
    avfilter_inout_free(&inputs);
    return ret;
}

#endif // CONFIG_AVFILTER
