//
//  ff_packet_queue.h
//  IJKMediaFramework
//
//  Created by 帅龙成 on 01/03/2018.
//  Copyright © 2018 kuaishou. All rights reserved.
//

#ifndef ff_packet_queue_h
#define ff_packet_queue_h

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

#include "libavformat/avformat.h"

#include "ijksdl/ijksdl_mutex.h"

extern AVPacket flush_pkt;

typedef struct AVPacketTime {
    int64_t wrap_base;
    int64_t abs_pts;
    int64_t recv_time;
    int64_t render_time;
} AVPacketTime;

typedef struct MyAVPacketList {
    AVPacket pkt;
    struct MyAVPacketList* next;
    int serial;
    AVPacketTime pkttime;
} MyAVPacketList;

typedef struct PacketQueue {
    MyAVPacketList* first_pkt, *last_pkt;
    int nb_packets;
    int size;
    int64_t duration;
    int abort_request;
    int serial;
    SDL_mutex* mutex;
    SDL_cond* cond;
    MyAVPacketList* recycle_pkt;
    int recycle_count;
    int alloc_count;

    int is_buffer_indicator;
    int64_t max_pts;

    const char* name;   // for debug purpose

    // for start_play_block_purpoose, never reset to 0 even when packet_queue_flush called
    int64_t history_total_size; // only record av packet size
    int64_t history_total_duration;
} PacketQueue;


int packet_queue_put_private(PacketQueue* q, AVPacket* pkt, AVPacketTime* p_pkttime);

int packet_queue_put(PacketQueue* q, AVPacket* pkt, AVPacketTime* p_pkttime);

int packet_queue_put_nullpacket(PacketQueue* q, int stream_index);

/* packet queue handling */
int packet_queue_init(PacketQueue* q);
int packet_queue_init_with_name(PacketQueue* q, const char* name);

void packet_queue_flush(PacketQueue* q);
void packet_queue_destroy(PacketQueue* q);

void packet_queue_abort(PacketQueue* q);

void packet_queue_start(PacketQueue* q);

/* return < 0 if aborted, 0 if no packet and > 0 if packet.  */
int packet_queue_get(PacketQueue* q, AVPacket* pkt, int block, int* serial, AVPacketTime* p_pkttime);

/* return delete num*/
void packet_queue_delete_elements_until_by_pts(PacketQueue* q, double base, double seek_point_pts_time);

/* if is_video_stream is 1,  it is video queue and need to seek to position according to key frame.
 *  if is_video_stream is 0, it is audio queue and seek to position according to pts.
 * return value:
 * if seek succeed, return pts.
 * if failed, return 0
 */
int64_t packet_queue_seek(PacketQueue* q, int64_t seek_pos, int is_video_stream);

int packet_queue_is_started(PacketQueue* q);

int64_t packet_queue_get_first_packet_pts(PacketQueue* q);

#endif /* ff_packet_queue_h */
