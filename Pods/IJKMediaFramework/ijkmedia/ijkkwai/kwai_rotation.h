//
//  kwai_rotation.h
//  KSYPlayerCore
//
//  Created by 帅龙成 on 22/11/2017.
//  Copyright © 2017 kuaishou. All rights reserved.
//

#ifndef KWAI_ROTATION_H
#define KWAI_ROTATION_H

#include <pthread.h>
#include <stdint.h>
#include <libavutil/dict.h>
#include <libavformat/avformat.h>

// 一般来metadata比较极限的情况下也不会堆积超过这个值，而且一旦停止堆积，ring_buf的size会回归到1，只保留最近有效的值
#define kRotationBufLen 20

typedef struct {
    int64_t rotate_from_ts;
    int rotation;
} KwaiRotateInfo;

typedef struct {
    pthread_mutex_t buf_mutex;
    int head_index;
    int tail_index;
    int buf_size;
    KwaiRotateInfo ring_buf[kRotationBufLen];
} KwaiRotateControl;

void KwaiRotateControl_init(KwaiRotateControl* ctrl);
void KwaiRotateControl_update(KwaiRotateControl* ctrl, AVDictionary* metadata);
void KwaiRotateControl_set_degree_to_frame(KwaiRotateControl* ctrl, AVFrame* frame);


#endif /* KWAI_ROTATION_H */
