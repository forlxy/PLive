//
// Created by MarshallShuai on 2019/4/22.
//

#pragma once

#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>

#include "config.h"
#include "ijkmedia/ijkplayer/ff_packet_queue.h"
#include "ijkmedia/ijkplayer/ff_frame_queue.h"
#include "ijksdl_thread.h"
#include "ijksdl_timer.h"

typedef struct Decoder {
    AVPacket pkt;
    AVPacket pkt_temp;
    PacketQueue* queue;
    AVCodecContext* avctx;
    int pkt_serial;
    int finished;
    int packet_pending;
    int packet_pos;
    int bfsc_ret;
    uint8_t* bfsc_data;

    SDL_cond* empty_queue_cond;
    int64_t start_pts;
    AVRational start_pts_tb;
    int64_t next_pts;
    AVRational next_pts_tb;
    SDL_Thread* decoder_tid;
    SDL_Thread _decoder_tid;

    SDL_Profiler decode_profiler;
    Uint64 first_frame_decoded_time;
    int    first_frame_decoded;
    double first_frame_pts;
} Decoder;

void
decoder_init(Decoder* d, AVCodecContext* avctx, PacketQueue* queue, SDL_cond* empty_queue_cond);
int decoder_start(Decoder* d, int (*fn)(void*), void* arg, const char* name);
void decoder_destroy(Decoder* d);
void decoder_abort(Decoder* d, FrameQueue* fq);

struct FFPlayer;
int decoder_decode_frame(struct FFPlayer* ffp, Decoder* d, AVFrame* frame, AVSubtitle* sub,
                         AVPacketTime* p_pkttime);
/**
 *  264上seek使用丢弃非参考帧，目前有video解码线程的queue_picture和audio/video的解码线程的decoder_decode_frame用到了
 */
void setVideoDecodeDiscard(AVCodecContext* avctx);
void unsetVideoDecodeDiscard(AVCodecContext* avctx);

#if CONFIG_AVFILTER
int configure_filtergraph(AVFilterGraph* graph, const char* filtergraph,
                          AVFilterContext* source_ctx, AVFilterContext* sink_ctx);
#endif // CONFIG_AVFILTER

#if CONFIG_AVFILTER
static inline
int cmp_audio_fmts(enum AVSampleFormat fmt1, int64_t channel_count1,
                   enum AVSampleFormat fmt2, int64_t channel_count2) {
    /* If channel count == 1, planar and non-planar formats are the same */
    if (channel_count1 == 1 && channel_count2 == 1)
        return av_get_packed_sample_fmt(fmt1) != av_get_packed_sample_fmt(fmt2);
    else
        return channel_count1 != channel_count2 || fmt1 != fmt2;
}

static inline
int64_t get_valid_channel_layout(int64_t channel_layout, int channels) {
    if (channel_layout && av_get_channel_layout_nb_channels(channel_layout) == channels)
        return channel_layout;
    else
        return 0;
}
#endif // CONFIG_AVFILTER12
