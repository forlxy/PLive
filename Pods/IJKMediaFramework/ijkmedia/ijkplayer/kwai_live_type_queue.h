#pragma once

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <libavformat/avformat.h>
#include "ijksdl/ijksdl_mutex.h"

//用于存储直播时视频帧显示样式live_type以及对应的时间戳，主要用于pk场景
typedef struct LiveType {
    int64_t time;
    int value;
    struct LiveType* next;
} LiveType;

//由于live_type是在read_thread线程获取的，需要先存到队列，当对应的视频帧真正播放时，将live_type回调给客户端
typedef struct LiveTypeQueue {
    LiveType* first, *last;
    int nb_livetype;
    SDL_mutex* mutex;
} LiveTypeQueue;

int live_type_queue_init(LiveTypeQueue* q);
int live_type_queue_put(LiveTypeQueue* q, LiveType* lt);
int live_type_queue_get(LiveTypeQueue* q, int64_t ptsMs, int* live_type);
void live_type_queue_destroy(LiveTypeQueue* q);
