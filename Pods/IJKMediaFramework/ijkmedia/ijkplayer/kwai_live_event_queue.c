//
// Created by wangtao03 on 2019/1/31.
//

#include "kwai_live_event_queue.h"
#include "ijksdl/ijksdl_error.h"
#include "ijksdl/ijksdl_log.h"

int live_event_queue_init(LiveEventQueue* q) {
    memset(q, 0, sizeof(LiveEventQueue));
    q->mutex = SDL_CreateMutex();
    if (!q->mutex) {
        ALOGE("SDL_CreateMutex(): %s\n", SDL_GetError());
        return AVERROR(ENOMEM);
    }

    return 0;
}

static int live_event_queue_put_private(LiveEventQueue* q, LiveEvent* event) {
    LiveEvent* event1;

    event1 = av_malloc(sizeof(LiveEvent));
    memset(event1, 0, sizeof(LiveEvent));
    if (!event1)
        return -1;

    event1->time = event->time;
    event1->content_len = event->content_len;
    memcpy(event1->content, event->content, event->content_len);
    event1->peeked = false;
    event1->next = NULL;

    if (!q->last)
        q->first = event1;
    else
        q->last->next = event1;

    q->last = event1;
    q->nb_event++;

    return 0;
}

int live_event_queue_put(LiveEventQueue* q, LiveEvent* event) {
    int ret = 0;
    if (!event)
        return -1;
    SDL_LockMutex(q->mutex);
    ret = live_event_queue_put_private(q, event);
    SDL_UnlockMutex(q->mutex);
    return ret;
}

bool live_event_queue_peek(LiveEventQueue* q, int64_t audio_clk, int64_t* time) {
    bool ret = false;
    LiveEvent* event, *event1;

    SDL_LockMutex(q->mutex);
    for (event = q->first; event; event = event1) {
        event1 = event->next;
        if (event->peeked) {
            continue;  // Already peeked
        } else if (!event->peeked && event->time <= audio_clk) {
            ret = true;
            event->peeked = true;
            *time = event->time;
            break; // reached the point, break
        } else {
            break; // wait, break
        }
    }
    SDL_UnlockMutex(q->mutex);

    return ret;
}

int live_event_queue_get(LiveEventQueue* q, LiveEvent* event, int64_t time) {
    int ret = 0;
    LiveEvent* event1;
    SDL_LockMutex(q->mutex);

    event1 = q->first;
    if (event1 && event1->time == time && event1->peeked) {
        q->first = event1->next;
        if (!q->first)
            q->last = NULL;
        q->nb_event--;
        *event = *event1;
        av_freep(&event1);
    } else {
        ALOGE("live_event_queue_get, Fail, time=%lld\n", time);
        ret = -1;
    }

    SDL_UnlockMutex(q->mutex);
    return ret;
}

void live_event_queue_flush(LiveEventQueue* q) {
    LiveEvent* event, *event1;
    SDL_LockMutex(q->mutex);
    for (event = q->first; event; event = event1) {
        event1 = event->next;
        av_freep(&event);
    }

    q->first = NULL;
    q->last = NULL;
    q->nb_event = 0;
    SDL_UnlockMutex(q->mutex);
    return;
}

void live_event_queue_destroy(LiveEventQueue* q) {
    live_event_queue_flush(q);
    SDL_DestroyMutex(q->mutex);
    return;
}
