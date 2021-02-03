//
// Created by wangtao03 on 2019/1/31.
//

#ifndef IJKPLAYER_KWAI_LIVE_EVENT_QUEUE_H
#define IJKPLAYER_KWAI_LIVE_EVENT_QUEUE_H

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include "libavformat/avformat.h"
#include "ijksdl/ijksdl_mutex.h"

enum {
    KWAI_LIVE_EVENT_TYPE_PASSTHROUGH_LEGACY = 0,
    KWAI_LIVE_EVENT_TYPE_PASSTHROUGH = 1,
    KWAI_LIVE_EVENT_TYPE_VIDEO_MIX_TYPE = 2,
    KWAI_LIVE_EVENT_TYPE_ABS_TIME = 3,
    KWAI_LIVE_EVENT_TYPE_VOICE_COMMENT = 4,
    KWAI_LIVE_EVENT_TYPE_TBD
};

#define KWAI_LIVE_EVENT_LEN 256
#define KWAI_LIVE_EVENT_AUDIOCLK_TIME_DIFF_ABS 50    // dequeue condition
#define KWAI_LIVE_EVENT_AUDIOCLK_TIME_DIFF_RANGE 15000    // invalid vc_time condition
#define KWAI_LIVE_EVENT_AVPKT_TIME_DIFF 500      // enqueue condition

typedef struct LiveEvent {
    int64_t time;
    char content[KWAI_LIVE_EVENT_LEN];
    int content_len;
    bool peeked;
    struct LiveEvent* next;
} LiveEvent;

typedef struct LiveEventQueue {
    LiveEvent* first, *last;
    int nb_event;
    SDL_mutex* mutex;
} LiveEventQueue;

int live_event_queue_init(LiveEventQueue* q);

int live_event_queue_put(LiveEventQueue* q, LiveEvent* event);

bool live_event_queue_peek(LiveEventQueue* q, int64_t audio_clk, int64_t* time);

int live_event_queue_get(LiveEventQueue* q, LiveEvent* event, int64_t time);

void live_event_queue_flush(LiveEventQueue* q);

void live_event_queue_destroy(LiveEventQueue* q);

#endif //IJKPLAYER_KWAI_LIVE_EVENT_QUEUE_H
