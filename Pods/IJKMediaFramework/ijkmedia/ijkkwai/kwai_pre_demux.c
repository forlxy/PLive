//
//  kwai_pre_demux.c
//  IJKMediaFramework
//
//  Created by 帅龙成 on 01/03/2018.
//  Copyright © 2018 kuaishou. All rights reserved.
//
#include <stdio.h>
#include "kwai_pre_demux.h"
#include "ff_ffplay_def.h"
#include "ijksdl/ijksdl_misc.h"
#include "ijksdl/ijksdl_log.h"
#include "ff_ffplay.h"
#include "ff_ffmsg.h"
#include "kwai_error_code_manager.h"

#define MAX_PRE_DEMUX_DURATION_MS (50*1000)

PreDemux* PreDemux_create(int64_t duration_ms) {
    PreDemux* pd = mallocz(sizeof(PreDemux));

    if (!pd) {
        return NULL;
    }
    pd->abort = 0;
    pd->pre_read_duration_ms = duration_ms;
    pd->mutex = SDL_CreateMutex();
    pd->cond = SDL_CreateCond();
    if (!pd->mutex || !pd->cond) {
        goto fail;
    }
    return pd;

fail:
    PreDemux_destroy_p(&pd);
    return NULL;
}

void PreDemux_destroy_p(PreDemux** ppd) {
    if (!ppd || !*ppd) {
        return;
    }

    PreDemux_abort(*ppd);
    SDL_DestroyMutex((*ppd)->mutex);
    SDL_DestroyCond((*ppd)->cond);
    freep((void**) ppd);
}

void PreDemux_abort(PreDemux* pd) {
    if (!pd) {
        return;
    }

    SDL_LockMutex(pd->mutex);
    if (pd->pre_download_avio_opaque) {
        AvIoOpaqueWithDataSource_abort(pd->pre_download_avio_opaque);
    }
    pd->abort = 1;
    SDL_CondSignal(pd->cond);
    SDL_UnlockMutex(pd->mutex);
}

inline static int64_t get_duration_ms(int64_t dur, int stream_index, AVFormatContext* ic) {
    if (stream_index < 0 || !ic || !ic->streams || !ic->streams[stream_index]) {
        return -1;
    }

    return av_rescale_q(dur, ic->streams[stream_index]->time_base, (AVRational) {1, 1000});
}


inline static int64_t get_current_time_ms() {
    return av_gettime_relative() / 1000;
}

int PreDemux_pre_demux_ver1(FFPlayer* ffp,
                            AVFormatContext* ic, AVPacket* pkt,
                            int audio_stream, int video_stream) {

    assert(ffp);

    VideoState* is = ffp->is;
    PreDemux* pd = ffp->pre_demux;

    if (!pd || !ic || !pkt || !is) {
        ALOGE("[%s] invalid state, some thing is null, pre_demux:%p, is:%p, ic:%p, pkt:%p is:%p\n",
              __func__, pd, ic, pkt, is);
        return 0;
    }

    pd->ts_start_ms = get_current_time_ms();

    PacketQueue* audio_q = &is->audioq;
    PacketQueue* video_q = &is->videoq;
    KwaiQos* qos = &ffp->kwai_qos;


    // 1.packet_queue_start() are originally called by decoder_start()
    // 2.if not set discard = AVDISCARD_DEFAULT ,av_read_frame encounter "End of file" error;
    if (audio_stream >= 0) {
        ic->streams[audio_stream]->discard = AVDISCARD_DEFAULT;
        packet_queue_start(audio_q);
    }
    if (video_stream >= 0) {
        ic->streams[video_stream]->discard = AVDISCARD_DEFAULT;
        packet_queue_start(video_q);
    }


    int64_t pre_demux_threshold_ms = FFMIN(MAX_PRE_DEMUX_DURATION_MS, pd->pre_read_duration_ms);
    // 500 ms 是为了有足够冗余，不至于read to eof 还不满足predemux阈值
    int64_t total_dur = fftime_to_milliseconds(ic->duration) - 500;
    if (total_dur > 0) {
        pre_demux_threshold_ms = FFMIN(pre_demux_threshold_ms, total_dur);
    }
    int64_t audio_q_dur = 0;
    int64_t video_q_dur = 0;

    while (1) {
        int ret = av_read_frame(ic, pkt);

        if (ret < 0) {
            if (ret == AVERROR_EOF) {
                ALOGE("[%s] av_read_frame get EOF :%s \n", __func__, av_err2str(ret));
                break;
            }
            ALOGE("[%s] av_read_frame fail:%s \n", __func__, av_err2str(ret));

            int pb_error = 0;
            if (ic->pb && ic->pb->error) {
                pb_error = ic->pb->error;
            }
            if (ret == AVERROR_EXIT) {
                pb_error = AVERROR_EXIT;
            }
            //当pb_error小于0时，才认定为播放出错。此处与起播后上报last_error逻辑保持一致
            if (pb_error < 0) {
                return pb_error;
            }
            //接着读取下一个packet
            continue;
        }

        if (pkt->pts < 0) {
            if (pkt->stream_index == audio_stream && ffp->audio_pts_invalid)
                ffp->audio_invalid_duration += av_rescale_q(pkt->duration, ic->streams[audio_stream]->time_base,
                                                            AV_TIME_BASE_Q);
            else if (pkt->stream_index == video_stream && ffp->video_pts_invalid)
                ffp->video_invalid_duration += av_rescale_q(pkt->duration, ic->streams[video_stream]->time_base,
                                                            AV_TIME_BASE_Q);
        } else {
            if (pkt->stream_index == audio_stream)
                ffp->audio_pts_invalid = false;
            else if (pkt->stream_index == video_stream)
                ffp->video_pts_invalid = false;
        }

        ffp_kwai_collect_dts_info(ffp, pkt, audio_stream, video_stream, ic->streams[pkt->stream_index]);

        if (pkt->stream_index == audio_stream && audio_q) {
            packet_queue_put(audio_q, pkt, NULL);
            audio_q_dur = get_duration_ms(audio_q->duration, audio_stream, ic);
        } else if (pkt->stream_index == video_stream && video_q) {
            if (qos) {
                KwaiQos_onVideoPacketReceived(qos);
            }
            packet_queue_put(video_q, pkt, NULL);
            video_q_dur = get_duration_ms(video_q->duration, video_stream, ic);
#ifdef FFP_MERGE
            // 暂时不用考虑字幕
//        } else if (pkt->stream_index == is->subtitle_stream && pkt_in_play_range) {
//             packet_queue_put(&is->subtitleq, pkt);
#endif
        } else {
            av_packet_unref(pkt);
        }

        if ((audio_q_dur >= pre_demux_threshold_ms) || (video_q_dur >= pre_demux_threshold_ms)) {
            pd->pre_loaded_ms_when_abort = (int) IJKMAX(audio_q_dur, video_q_dur);
            break;
        } else if (pd->abort) {
            ALOGD("[%s], aborted in while, not to reopen AwesomeCache later\n", __func__);
            pd->pre_loaded_ms_when_abort = (int) IJKMAX(audio_q_dur, video_q_dur);
            return 0;
        } else {
            continue;
        }
    }

    pd->complete = 1;
    // dont delete it.
    ALOGD("[%s], complete, audio_q->duration:%lld, video_q->duration:%lld, pre_read_duration_ms:%lldms \n",
          __func__, audio_q_dur, video_q_dur, pd->pre_read_duration_ms);

    SDL_LockMutex(pd->mutex);
    if (!pd->abort) {
        // 海外需要preLoad的耗时
        pd->ts_end_ms = get_current_time_ms();
        ALOGD("[%s], start_ts_ms:%lld, end_ts_ms:%lld， cost_ms:%lld",
              __func__, pd->ts_start_ms, pd->ts_end_ms, pd->pre_load_cost_ms);
        pd->pre_load_cost_ms = (int)(pd->ts_end_ms - pd->ts_start_ms);
        ffp_notify_msg2(ffp, FFP_MSG_PRE_LOAD_FINISH, pd->pre_load_cost_ms);

        SDL_CondWait(pd->cond, pd->mutex);
    }
    SDL_UnlockMutex(pd->mutex);

    return 0;
}

int PreDemux_pre_demux_ver2(FFPlayer* ffp,
                            AVFormatContext* ic, AVPacket* pkt,
                            int audio_stream, int video_stream) {
    assert(ffp);
    AVIOContext* avio = ffp->cache_avio_context;
    VideoState* is = ffp->is;
    PreDemux* pd = ffp->pre_demux;

    if (!pd || !ic || !pkt || !is) {
        ALOGE("[%s] invalid state, some thing is null, pre_demux:%p, is:%p, ic:%p, pkt:%p is:%p\n",
              __func__, pd, ic, pkt, is);
        return 0;
    }

    pd->ts_start_ms = get_current_time_ms();

    PacketQueue* audio_q = &is->audioq;
    PacketQueue* video_q = &is->videoq;
    KwaiQos* qos = &ffp->kwai_qos;

    // 1.packet_queue_start() are originally called by decoder_start()
    // 2.if not set discard = AVDISCARD_DEFAULT ,av_read_frame encounter "End of file" error;
    if (audio_stream >= 0) {
        ic->streams[audio_stream]->discard = AVDISCARD_DEFAULT;
        packet_queue_start(audio_q);
    }
    if (video_stream >= 0) {
        ic->streams[video_stream]->discard = AVDISCARD_DEFAULT;
        packet_queue_start(video_q);
    }

    int64_t pre_demux_threshold_ms = FFMIN(MAX_PRE_DEMUX_DURATION_MS, pd->pre_read_duration_ms);
    // 500 ms 是为了有足够冗余，不至于read to eof 还不满足predemux阈值
    int64_t total_dur = fftime_to_milliseconds(ic->duration) - 500;
    if (total_dur > 0) {
        pre_demux_threshold_ms = FFMIN(pre_demux_threshold_ms, total_dur);
    }
    int64_t audio_q_dur = 0;
    int64_t video_q_dur = 0;

    while (1) {
        int ret = av_read_frame(ic, pkt);

        if (ret < 0) {
            if (ret == AVERROR_EOF) {
                ALOGE("[%s] av_read_frame get EOF :%s \n", __func__, av_err2str(ret));
                break;
            }
            ALOGE("[%s] av_read_frame fail:%s \n", __func__, av_err2str(ret));

            int pb_error = 0;
            if (ic->pb && ic->pb->error) {
                pb_error = ic->pb->error;
            }
            if (ret == AVERROR_EXIT) {
                pb_error = AVERROR_EXIT;
            }
            //当pb_error小于0时，才认定为播放出错。此处与起播后上报last_error逻辑保持一致
            if (pb_error < 0) {
                return pb_error;
            }
            //接着读取下一个packet
            continue;
        }

        if (pkt->pts < 0) {
            if (pkt->stream_index == audio_stream && ffp->audio_pts_invalid)
                ffp->audio_invalid_duration += av_rescale_q(pkt->duration, ic->streams[audio_stream]->time_base,
                                                            AV_TIME_BASE_Q);
            else if (pkt->stream_index == video_stream && ffp->video_pts_invalid)
                ffp->video_invalid_duration += av_rescale_q(pkt->duration, ic->streams[video_stream]->time_base,
                                                            AV_TIME_BASE_Q);
        } else {
            if (pkt->stream_index == audio_stream)
                ffp->audio_pts_invalid = false;
            else if (pkt->stream_index == video_stream)
                ffp->video_pts_invalid = false;
        }

        ffp_kwai_collect_dts_info(ffp, pkt, audio_stream, video_stream, ic->streams[pkt->stream_index]);

        if (pkt->stream_index == audio_stream && audio_q) {
            packet_queue_put(audio_q, pkt, NULL);
            audio_q_dur = get_duration_ms(audio_q->duration, audio_stream, ic);
        } else if (pkt->stream_index == video_stream && video_q) {
            if (qos) {
                KwaiQos_onVideoPacketReceived(qos);
            }
            packet_queue_put(video_q, pkt, NULL);
            video_q_dur = get_duration_ms(video_q->duration, video_stream, ic);
#ifdef FFP_MERGE
            // 暂时不用考虑字幕
//        } else if (pkt->stream_index == is->subtitle_stream && pkt_in_play_range) {
//             packet_queue_put(&is->subtitleq, pkt);
#endif
        } else {
            av_packet_unref(pkt);
        }

        if ((audio_q_dur >= pre_demux_threshold_ms) || (video_q_dur >= pre_demux_threshold_ms)) {
            break;
        } else if (pd->abort) {
            ALOGD("[%s], aborted in while, not to reopen AwesomeCache later\n", __func__);
            return 0;
        } else {
            continue;
        }
    }

    pd->complete = 1;
    // dont delete it.
    ALOGD("[%s], complete, audio_q->duration:%lld, video_q->duration:%lld, pre_read_duration_ms:%lldms \n",
          __func__, audio_q_dur, video_q_dur, pd->pre_read_duration_ms);

    SDL_LockMutex(pd->mutex);
    if (!pd->abort) {
        // 海外需要preLoad的耗时
        pd->ts_end_ms = get_current_time_ms();
        pd->pre_load_cost_ms = (int)(pd->ts_start_ms - pd->ts_end_ms);
        ffp_notify_msg2(ffp, FFP_MSG_PRE_LOAD_FINISH, pd->pre_load_cost_ms);

        if (avio && avio->opaque && !AvIoOpaqueWithDataSource_is_read_compelete(avio->opaque)) {
            AvIoOpaqueWithDataSource_close(avio->opaque, false);
            SDL_CondWait(pd->cond, pd->mutex);
            AvIoOpaqueWithDataSource_reopen(avio->opaque, false);
        } else {
            SDL_CondWait(pd->cond, pd->mutex);
        }
    }
    SDL_UnlockMutex(pd->mutex);

    return 0;
}

