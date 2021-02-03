//
//  ff_frame_queue.h
//  IJKMediaFramework
//
//  Created by 帅龙成 on 01/03/2018.
//  Copyright © 2018 kuaishou. All rights reserved.
//

#ifndef ff_frame_queue_h
#define ff_frame_queue_h

#include <stdio.h>
#include <libavformat/avformat.h>

//#include "ff_ffplay_def.h"
#include "ijksdl/ijksdl_mutex.h"
#include "ff_packet_queue.h"
#include "ijksdl/ijksdl_vout.h"

#define VIDEO_PICTURE_QUEUE_SIZE_MIN        (3)
#define VIDEO_PICTURE_QUEUE_SIZE_MAX        (16)
#define VIDEO_PICTURE_QUEUE_SIZE_DEFAULT    (VIDEO_PICTURE_QUEUE_SIZE_MIN)
#define SUBPICTURE_QUEUE_SIZE 16
#define SAMPLE_QUEUE_SIZE 9

#define FRAME_QUEUE_SIZE FFMAX(SAMPLE_QUEUE_SIZE, FFMAX(VIDEO_PICTURE_QUEUE_SIZE_MAX, SUBPICTURE_QUEUE_SIZE))

/* Common struct for handling all types of decoded data and allocated render buffers. */
typedef struct Frame {
    AVFrame* frame;
#ifdef FFP_MERGE
    AVSubtitle sub;
    AVSubtitleRect** subrects;  /* rescaled subtitle rectangles in yuva */
#endif
    int serial;
    double pts;           /* presentation timestamp for the frame */
    double duration;      /* estimated duration of the frame */
    int64_t pos;          /* byte position of the frame in the input file */
    SDL_VoutOverlay* bmp;
    int allocated;
    int reallocate;
    int width;
    int height;
    int rotation;
    AVRational sar;
    AVPacketTime pkttime;
} Frame;

typedef struct FrameQueue {
    Frame queue[FRAME_QUEUE_SIZE];
    int rindex;
    int windex;
    int size;
    int max_size;
    int keep_last;
    int rindex_shown;
    SDL_mutex* mutex;
    SDL_cond* cond;
    PacketQueue* pktq;
} FrameQueue;


void free_picture(Frame* vp);

void frame_queue_unref_item(Frame* vp);

int frame_queue_init(FrameQueue* f, PacketQueue* pktq, int max_size, int keep_last);

void frame_queue_flush(FrameQueue* f);

void frame_queue_destory(FrameQueue* f);

void frame_queue_signal(FrameQueue* f);

Frame* frame_queue_peek(FrameQueue* f);

Frame* frame_queue_peek_next(FrameQueue* f);

Frame* frame_queue_peek_last(FrameQueue* f);

Frame* frame_queue_peek_writable(FrameQueue* f);

Frame* frame_queue_peek_readable(FrameQueue* f);

void frame_queue_push(FrameQueue* f);

void frame_queue_next(FrameQueue* f);

/* return the number of undisplayed frames in the queue */
int frame_queue_nb_remaining(FrameQueue* f);
/* return last shown position */
#ifdef FFP_MERGE
int64_t frame_queue_last_pos(FrameQueue* f);
#endif /* ff_frame_queue_h */

#endif
