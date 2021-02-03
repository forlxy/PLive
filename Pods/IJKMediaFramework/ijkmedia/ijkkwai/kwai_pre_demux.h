//
//  kwai_pre_demux.h
//  IJKMediaFramework
//
//  Created by 帅龙成 on 01/03/2018.
//  Copyright © 2018 kuaishou. All rights reserved.
//

#ifndef kwai_pre_demux_h
#define kwai_pre_demux_h

#include <libavformat/avformat.h>
#include "ijksdl/ijksdl_mutex.h"
#include "ijkkwai/kwai_qos.h"
#include "ff_packet_queue.h"

typedef struct AvIoOpaqueWithDataSource AvIoOpaqueWithDataSource;
typedef struct PreDemux {
    int64_t pre_read_duration_ms;

    SDL_cond* cond;
    SDL_mutex* mutex;

    int complete;
    int abort;
    int pre_loaded_ms_when_abort;

    int64_t ts_start_ms;
    int64_t ts_end_ms;
    int pre_load_cost_ms;   // pre_load花费的时间

    AvIoOpaqueWithDataSource* pre_download_avio_opaque;
} PreDemux;

PreDemux* PreDemux_create(int64_t duration_ms);

void PreDemux_destroy_p(PreDemux** pd);

void PreDemux_abort(PreDemux* pd);

/**
 * 只能短视频场景用，不能直播用
 * return: 0 表示成功，否则应该结束read_thread, 直接跳转到fail,并记录到ffp->error_code
 */
//
int PreDemux_pre_demux_ver1(FFPlayer* ffp,
                            AVFormatContext* ic, AVPacket* pkt,
                            int audio_stream, int video_stream);

int PreDemux_pre_demux_ver2(FFPlayer* ffp,
                            AVFormatContext* ic, AVPacket* pkt,
                            int audio_stream, int video_stream);

#endif /* kwai_pre_demux_h */
