//
// Created by MarshallShuai on 2019/4/19.
//
#pragma once
#include <stdbool.h>
#include <libavformat/avformat.h>

//#include "ff_packet_queue.h"

/**
 * video decoder线程入口
 * 调用关系：video_decode_thread -> ffplay_pipeline_node.func_run_sync -> ffp_video_thread -> ffplay_video_thread
 */
int video_decode_thread(void* arg);

int ffplay_video_thread(void* arg);

struct FFPlayer;
/**
 * 判断当前视频帧是否需要丢掉(early drop)，函数内部会负责做相关丢帧统计
 */
bool should_drop_decoded_video_frame(struct FFPlayer* ffp, AVFrame* frame, const char* debug_tag);

int get_video_decode_type(struct FFPlayer* ffp);
