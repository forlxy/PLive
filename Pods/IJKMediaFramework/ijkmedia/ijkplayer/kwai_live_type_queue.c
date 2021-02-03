#include "kwai_live_type_queue.h"
#include "ijksdl/ijksdl_error.h"
#include "ijksdl/ijksdl_log.h"

int live_type_queue_init(LiveTypeQueue* q) {
    memset(q, 0, sizeof(LiveTypeQueue));
    q->mutex = SDL_CreateMutex();
    if (!q->mutex) {
        ALOGE("SDL_CreateMutex(): %s\n", SDL_GetError());
        return AVERROR(ENOMEM);
    }

    return 0;
}

static int live_type_queue_put_private(LiveTypeQueue* q, LiveType* lt) {
    LiveType* lt1;

    lt1 = av_malloc(sizeof(LiveType));
    if (!lt1)
        return -1;

    memset(lt1, 0, sizeof(LiveType));

    lt1->time = lt->time;
    lt1->value = lt->value;
    lt1->next = NULL;

    if (!q->last)
        q->first = lt1;
    else
        q->last->next = lt1;

    q->last = lt1;
    q->nb_livetype++;

    return 0;
}

int live_type_queue_put(LiveTypeQueue* q, LiveType* lt) {
    int ret = 0;
    if (!lt)
        return -1;
    SDL_LockMutex(q->mutex);
    ret = live_type_queue_put_private(q, lt);
    SDL_UnlockMutex(q->mutex);
    return ret;
}

int live_type_queue_get(LiveTypeQueue* q, int64_t ptsMs, int* live_type) {
    int ret = 0;
    LiveType* lt;
    SDL_LockMutex(q->mutex);

    lt = q->first;
    if (!lt) {
        ALOGE("live_type_queue_get queue empty\n");
        return -1;
    }

    if (lt->time <= ptsMs) {
        *live_type = lt->value;
        q->first = lt->next;
        if (!q->first)
            q->last = NULL;
        q->nb_livetype--;
        av_freep(&lt);
    } else {
        ret = -1;
    }

    SDL_UnlockMutex(q->mutex);
    return ret;
}

void live_type_queue_flush(LiveTypeQueue* q) {
    LiveType* lt, *lt1;
    SDL_LockMutex(q->mutex);
    for (lt = q->first; lt; lt = lt1) {
        lt1 = lt->next;
        av_freep(&lt);
    }

    q->first = NULL;
    q->last = NULL;
    q->nb_livetype = 0;
    SDL_UnlockMutex(q->mutex);
    return;
}

void live_type_queue_destroy(LiveTypeQueue* q) {
    live_type_queue_flush(q);
    SDL_DestroyMutex(q->mutex);
    return;
}