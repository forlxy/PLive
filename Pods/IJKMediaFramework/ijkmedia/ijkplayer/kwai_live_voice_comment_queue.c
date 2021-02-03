//
//  kwai_live_voice_comment_queue.c
//  IJKMediaFramework
//
//  Created by wangtao03 on 2018/8/15.
//  Copyright © 2018年 kuaishou. All rights reserved.
//

//#include <memory.h>
#include "kwai_live_voice_comment_queue.h"
#include "ijksdl/ijksdl_error.h"
#include "ijksdl/ijksdl_log.h"

int live_voice_comment_queue_init(VoiceCommentQueue* q) {
    memset(q, 0, sizeof(VoiceCommentQueue));
    q->mutex = SDL_CreateMutex();
    if (!q->mutex) {
        ALOGE("SDL_CreateMutex(): %s\n", SDL_GetError());
        return AVERROR(ENOMEM);
    }

    return 0;
}

static int live_voice_comment_queue_put_private(VoiceCommentQueue* q, VoiceComment* vc) {
    VoiceComment* vc1;

    vc1 = av_malloc(sizeof(VoiceComment));
    memset(vc1, 0, sizeof(VoiceComment));
    if (!vc1)
        return -1;

    vc1->time = vc->time;
    vc1->com_len = vc->com_len;
    strncpy(vc1->comment, vc->comment, vc->com_len);
    vc1->peeked = false;
    vc1->next = NULL;

    if (!q->last)
        q->first = vc1;
    else
        q->last->next = vc1;

    q->last = vc1;
    q->nb_comment++;

    return 0;
}

int live_voice_comment_queue_put(VoiceCommentQueue* q, VoiceComment* vc) {
    int ret = 0;
    if (!vc)
        return -1;
    SDL_LockMutex(q->mutex);
    ret = live_voice_comment_queue_put_private(q, vc);
    SDL_UnlockMutex(q->mutex);
    return ret;
}

bool live_voice_comment_queue_peek(VoiceCommentQueue* q, int64_t audio_clk, int64_t* vc_time) {
    bool ret = false;
    VoiceComment* vc, *vc1;

    SDL_LockMutex(q->mutex);
    for (vc = q->first; (vc && !vc->peeked); vc = vc1) {
        vc1 = vc->next;
        if (((vc->time < audio_clk) && (llabs(audio_clk - vc->time) > KWAI_LIVE_VOICE_COMMENT_AUDIOCLK_TIME_DIFF_ABS)) ||
            ((vc->time > audio_clk) && (llabs(vc->time - audio_clk) > KWAI_LIVE_VOICE_COMMENT_AUDIOCLK_TIME_DIFF_RANGE))) {
            ALOGI("live_voice_comment_queue_peek, Invalid and dequeued, vc_time=%lld, audio_clk=%lld\n", vc->time, audio_clk);
            q->first = vc1;
            if (!q->first)
                q->last = NULL;
            q->nb_comment--;
            av_freep(&vc);
            continue;
        } else {
            break;
        }
    }

    for (vc = q->first; vc; vc = vc1) {
        vc1 = vc->next;
        if (vc->peeked) {
            continue;  // Already peeked
        } else if (!vc->peeked && llabs(vc->time - audio_clk) < KWAI_LIVE_VOICE_COMMENT_AUDIOCLK_TIME_DIFF_ABS) {
            ret = true;
            vc->peeked = true;
            *vc_time = vc->time;
            break; // reached the point, break
        } else {
            break; // wait, break
        }
    }
    SDL_UnlockMutex(q->mutex);

    return ret;
}

int live_voice_comment_queue_get(VoiceCommentQueue* q, VoiceComment* vc, int64_t vc_time) {
    int ret = 0;
    VoiceComment* vc1;
    SDL_LockMutex(q->mutex);

    vc1 = q->first;
    if (vc1 && vc1->time == vc_time && vc1->peeked) {
        q->first = vc1->next;
        if (!q->first)
            q->last = NULL;
        q->nb_comment--;
        *vc = *vc1;
        av_freep(&vc1);
    } else {
        ALOGE("live_voice_comment_queue_get, Fail, vc_time=%lld\n", vc_time);
        ret = -1;
    }

    SDL_UnlockMutex(q->mutex);
    return ret;
}

void live_voice_comment_queue_flush(VoiceCommentQueue* q) {
    VoiceComment* vc, *vc1;
    SDL_LockMutex(q->mutex);
    for (vc = q->first; vc; vc = vc1) {
        vc1 = vc->next;
        av_freep(&vc);
    }

    q->first = NULL;
    q->last = NULL;
    q->nb_comment = 0;
    SDL_UnlockMutex(q->mutex);
    return;
}

void live_voice_comment_queue_destroy(VoiceCommentQueue* q) {
    live_voice_comment_queue_flush(q);
    SDL_DestroyMutex(q->mutex);
    return;
}
