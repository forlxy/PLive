//
//  kwai_ab_loop.h
//  IJKMediaFramework
//
//  Created by 帅龙成 on 08/03/2018.
//  Copyright © 2018 kuaishou. All rights reserved.
//

#ifndef kwai_ab_loop_h
#define kwai_ab_loop_h

#include <stdint.h>

typedef struct AbLoop {
    int enable;
    int64_t a_pts_ms;
    int64_t b_pts_ms;
} AbLoop;

void AbLoop_init(AbLoop* loop);
void AbLoop_set_ab(AbLoop* loop, int64_t a_pts_ms, int64_t b_pts_ms);

struct FFPlayer;
void AbLoop_on_play_start(AbLoop* loop, struct FFPlayer* ffp);
void AbLoop_on_frame_rendered(AbLoop* loop, struct FFPlayer* ffp);

/**
 * 卡顿loop,初始化为卡顿区间百分比，BufferLoop_update_pos 更新为对应的卡顿区间值
 * 第一次卡顿时候通过BufferLoop_loop_on_buffer 判断卡顿位置是否在buffer_start_pos～buffer_end_pos，
 * 记录卡顿位置，保存为loop_buffer_pos，
 * 后续BufferLoop_on_frame_rendered在render过程中判断当前渲染到的位置，达到loop_buffer_pos，seek到loop_begin_pos
 */
typedef struct BufferLoop {
    int enable;
    int buffer_start_percent;
    int buffer_end_percent;
    int64_t buffer_start_pos;
    int64_t buffer_end_pos;
    int64_t loop_begin_pos;
    int64_t loop_buffer_pos;
} BufferLoop;

void BufferLoop_init(BufferLoop* buffer_loop);
void BufferLoop_enable(BufferLoop* loop, int buffer_start_precent, int buffer_end_precent, int64_t loop_begin);
void BufferLoop_on_frame_rendered(BufferLoop* loop, struct FFPlayer* ffp);
void BufferLoop_update_pos(BufferLoop* loop, int64_t duration);
// return 1 : 满足loop条件，进行seek 0:不满足
int  BufferLoop_loop_on_buffer(BufferLoop* loop, struct FFPlayer* ffp);

#endif /* kwai_ab_loop_h */
