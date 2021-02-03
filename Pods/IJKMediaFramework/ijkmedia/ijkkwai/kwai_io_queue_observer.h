//
// Created by MarshallShuai on 2019-08-15.
//

#pragma once

#include <stdint.h>
#include <stdbool.h>

/**
 * 本类主要是负责记录播放器在前几秒的音视频数据和实际需要的字节数的关系
 *
 */
typedef struct KwaiIoQueueObserver {
    int64_t read_bytes_on_open_input;
    int64_t read_bytes_on_find_stream_info;
    int64_t read_bytes_on_fst_audio_pkt;
    int64_t read_bytes_on_fst_video_pkt;
    int64_t read_bytes_on_1s;
    int64_t read_bytes_on_2s;
    int64_t read_bytes_on_3s;

    int64_t byterate; // bitrate / 8
    bool finish_collect;
} KwaiIoQueueObserver;

typedef struct FFPlayer FFPlayer;

void KwaiIoQueueObserver_init(KwaiIoQueueObserver* self);

/**
 * 只会调用一次，主要收集 open_input 后读了多少字节数
 */
void KwaiIoQueueObserver_on_open_input(KwaiIoQueueObserver* self, FFPlayer* ffp);

/**
 * 只会调用一次，主要收集 find_stream_info 后读了多少字节数
 */
void KwaiIoQueueObserver_on_find_stream_info(KwaiIoQueueObserver* self, FFPlayer* ffp);

/**
 * // 会调用多次
 */
void KwaiIoQueueObserver_on_read_audio_frame(KwaiIoQueueObserver* self, FFPlayer* ffp);

void KwaiIoQueueObserver_on_read_video_frame(KwaiIoQueueObserver* self, FFPlayer* ffp);
