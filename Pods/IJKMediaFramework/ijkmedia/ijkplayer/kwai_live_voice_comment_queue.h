//
//  kwai_live_voice_comment_queue.h
//  IJKMediaFramework
//
//  Created by wangtao03 on 2018/8/15.
//  Copyright © 2018年 kuaishou. All rights reserved.
//

#ifndef kwai_live_voice_comment_queue_h
#define kwai_live_voice_comment_queue_h

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include "libavformat/avformat.h"
#include "ijksdl/ijksdl_mutex.h"

#define KWAI_LIVE_VOICE_COMMENT_LEN 64
#define KWAI_LIVE_VOICE_COMMENT_AUDIOCLK_TIME_DIFF_ABS 50    // dequeue condition
#define KWAI_LIVE_VOICE_COMMENT_AUDIOCLK_TIME_DIFF_RANGE 15000    // invalid vc_time condition
#define KWAI_LIVE_VOICE_COMMENT_AVPKT_TIME_DIFF 500      // enqueue condition

typedef struct VoiceComment {
    int64_t time;
    char comment[KWAI_LIVE_VOICE_COMMENT_LEN];
    int com_len;
    bool peeked;
    struct VoiceComment* next;
} VoiceComment;

typedef struct VoiceCommentQueue {
    VoiceComment* first, *last;
    int nb_comment;
    SDL_mutex* mutex;
} VoiceCommentQueue;

int live_voice_comment_queue_init(VoiceCommentQueue* q);

int live_voice_comment_queue_put(VoiceCommentQueue* q, VoiceComment* vc);

bool live_voice_comment_queue_peek(VoiceCommentQueue* q, int64_t audio_clk, int64_t* vc_time);

int live_voice_comment_queue_get(VoiceCommentQueue* q, VoiceComment* vc, int64_t vc_time);

void live_voice_comment_queue_flush(VoiceCommentQueue* q);

void live_voice_comment_queue_destroy(VoiceCommentQueue* q);

#endif /* kwai_live_voice_comment_queue_h */
