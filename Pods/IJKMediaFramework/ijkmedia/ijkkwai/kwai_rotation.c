//
//  kwai_rotation.c
//  KSYPlayerCore
//
//  Created by 帅龙成 on 22/11/2017.
//  Copyright © 2017 kuaishou. All rights reserved.
//

#include <assert.h>
#include "kwai_rotation.h"
#include "ijksdl/ijksdl_log.h"

#define kRotateDegreePortrait 0
#define kRotateDegreeLandscape 90

// 下面两个值是和推流端协商好的
#define kDictKeyRotation "rotation"
#define kDictKeyRotationFrom "rotate_from"

//static int VERBOSE = 0;

//static void append_rotation_info(KwaiRotateControl* ctrl, int rotation, int64_t rotate_from_ts);
//static int get_rotation_for_ts(KwaiRotateControl* ctrl, int64_t rotate_from_ts);

void KwaiRotateControl_init(KwaiRotateControl* ctrl) {
//    assert(ctrl);
//
//    ctrl->head_index = 0;
//    ctrl->tail_index = 0;
//    ctrl->buf_size = 0;
//    pthread_mutex_init(&ctrl->buf_mutex, NULL);
}

void KwaiRotateControl_update(KwaiRotateControl* ctrl, AVDictionary* metadata) {
//    if (!metadata) {
//        return;
//    }
//    assert(ctrl);
//    AVDictionaryEntry *entry_rotation = av_dict_get(metadata, kDictKeyRotation, NULL, 0);
//    AVDictionaryEntry *entry_rotate_from_ms = av_dict_get(metadata, kDictKeyRotationFrom, NULL, 0);
//    if (!entry_rotation || !entry_rotate_from_ms) {
//        return;
//    }
//
//    int64_t rotate_from = strtol(entry_rotate_from_ms->value, NULL, 10);
//    int rotation = strtol(entry_rotation->value, NULL, 10);
//
//    append_rotation_info(ctrl, rotation, rotate_from);
}

void KwaiRotateControl_set_degree_to_frame(KwaiRotateControl* ctrl, AVFrame* frame) {
//    assert(frame);
//    assert(ctrl);
//
//    frame->angle = get_rotation_for_ts(ctrl, frame->pts);
}

#if 0
static void ring_buf_remove_head_l(KwaiRotateControl* ctrl);
static void ring_buf_push_tail_l(KwaiRotateControl* ctrl, int rotation, int64_t rotate_from_ts) ;
// return NULL if not found, and prune obsolete info by the way;
static KwaiRotateInfo* seek_to_floor_rotation_info_l(KwaiRotateControl* ctrl, int64_t rotate_from_ts);

void ring_buf_push_tail_l(KwaiRotateControl* ctrl, int rotation, int64_t rotate_from_ts) {
    if (VERBOSE)
        ALOGD("KwaiRotateControl ring_buf_append_l add rotation:%d, rotate_from_ts:%lld\n", rotation, rotate_from_ts);

    if (ctrl->buf_size == 0) {
        ctrl->head_index = 0;
        ctrl->tail_index = 0;
        ctrl->buf_size++;
        ctrl->ring_buf[ctrl->head_index].rotation = rotation;
        ctrl->ring_buf[ctrl->head_index].rotate_from_ts = rotate_from_ts;
    } else if (ctrl->buf_size < kRotationBufLen) {
        int next = (ctrl->tail_index + 1) % kRotationBufLen;
        ctrl->ring_buf[next].rotation = rotation;
        ctrl->ring_buf[next].rotate_from_ts = rotate_from_ts;
        ctrl->buf_size++;
        ctrl->tail_index = next;
    } else {
        assert((ctrl->tail_index + 1) % kRotationBufLen == ctrl->head_index);
        // abort the earlest info
        ctrl->ring_buf[ctrl->head_index].rotate_from_ts = rotate_from_ts;
        ctrl->ring_buf[ctrl->head_index].rotation = rotation;
        ctrl->tail_index = ctrl->head_index;
        ctrl->head_index = (ctrl->head_index + 1) % kRotationBufLen;
    }
}

void ring_buf_remove_head_l(KwaiRotateControl* ctrl) {
    if (ctrl->buf_size <= 0) {
        return;
    }
    ctrl->head_index = (ctrl->head_index + 1) % kRotationBufLen;
    ctrl->buf_size--;
    if (VERBOSE)
        ALOGD("KwaiRotateControl ring_buf_remove_head_l ,CUR SIZE:%d\n", ctrl->buf_size);
}

void append_rotation_info(KwaiRotateControl* ctrl, int rotation, int64_t rotate_from_ts) {
    pthread_mutex_lock(&ctrl->buf_mutex);

    if (ctrl->buf_size > 0) {
        if (rotate_from_ts < ctrl->ring_buf[ctrl->tail_index].rotate_from_ts) {
            // earlier info ,warn
            if (VERBOSE)
                ALOGE("KwaiRotateContro append_rotation_info, rotate_from(%lld) < ctrl->tail_rotate_from(%lld) \n", rotate_from_ts, ctrl->ring_buf[ctrl->tail_index]);
        } else if (rotation == ctrl->ring_buf[ctrl->tail_index].rotation) {
            // duplicate ,ignore
            // ALOGD("KwaiRotateContro append_rotation_info duplicate ,ignore \n");
        } else {
            // valid info ,append it to ring buf
            ring_buf_push_tail_l(ctrl, rotation, rotate_from_ts);
        }
    } else {
        ring_buf_push_tail_l(ctrl, rotation, rotate_from_ts);
    }

    pthread_mutex_unlock(&ctrl->buf_mutex);
    return;
}

// return NULL if not found, and prune obsolete info by the way;
KwaiRotateInfo* seek_to_floor_rotation_info_l(KwaiRotateControl* ctrl, int64_t rotate_from_ts) {
    if (ctrl->buf_size == 0) {
        return NULL;
    }

    int prev_index = -1;

    int i = 0, break_index = 0;

    for (i = 0; i < ctrl->buf_size; i++) {
        break_index = (ctrl->head_index + i) % kRotationBufLen;
        if (ctrl->ring_buf[break_index].rotate_from_ts > rotate_from_ts) {
            break;
        }
        prev_index = break_index;
    }

    if (i == 0) {
        return NULL;
    } else {
        KwaiRotateInfo* result = NULL;
        if (i == kRotationBufLen && ctrl->ring_buf[break_index].rotate_from_ts <= rotate_from_ts) {
            // floor not found, return tail, the biggest one
            result = &ctrl->ring_buf[break_index];
        } else {
            result = &ctrl->ring_buf[prev_index];
        }

        // prune the obsolete infos
        for (int j = 0; j < i - 1 ; j++) {
            ring_buf_remove_head_l(ctrl);
        }

        return result;
    }
}

int get_rotation_for_ts(KwaiRotateControl* ctrl, int64_t frame_ts) {
    pthread_mutex_lock(&ctrl->buf_mutex);
    int rotation = kRotateDegreePortrait;
    if (ctrl->buf_size <= 0) {
        // do nothing , use default value
    } else {
        // 遍历ringbuf，floor(rotate_from_ts)的KwaiRotateInfo里。
        KwaiRotateInfo* period_info = seek_to_floor_rotation_info_l(ctrl, frame_ts);
        if (period_info == NULL) {
            if (ctrl->buf_size <= 0) {
                // 没收到任何rotation信息，采用默认的；
                rotation = kRotateDegreePortrait;
            } else {
                // 所有RotionInfo 的rotate_from_ts 都比 当前frame->pts大
                int oldest = ctrl->ring_buf[ctrl->head_index].rotation;
                rotation = oldest == kRotateDegreePortrait ? kRotateDegreeLandscape : kRotateDegreePortrait;
            }
        } else {
            rotation = period_info->rotation;
        }
    }
    // ALOGD("KwaiRotateControl, result: rotation(%d) for ts:%lld \n", rotate_from_ts, rotation);

    pthread_mutex_unlock(&ctrl->buf_mutex);
    return rotation;
}
#endif
