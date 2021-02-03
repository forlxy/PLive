//
//  qos_live_adaptive_realtime.h
//  IJKMediaFramework
//
//  Created by wangtao03 on 2018/3/6.
//  Copyright © 2018年 kuaishou. All rights reserved.
//

#ifndef qos_live_adaptive_realtime_h
#define qos_live_adaptive_realtime_h

#include "ff_ffplay_def.h"

#define LiveAdaptiveRealtimeInfo_videoBufferTime "videoBufferTime"
#define LiveAdaptiveRealtimeInfo_audioBufferTime "audioBufferTime"
#define LiveAdaptiveRealtimeInfo_bandwidthCurrent "bandwidthCurrent"
#define LiveAdaptiveRealtimeInfo_bandwidthFragment "bandwidthFragment"
#define LiveAdaptiveRealtimeInfo_bitrateDownloading "bitrateDownloading"
#define LiveAdaptiveRealtimeInfo_bitratePlaying "bitratePlaying"
#define LiveAdaptiveRealtimeInfo_currentBufferMs "currentBufferMs"
#define LiveAdaptiveRealtimeInfo_estimateBufferMs "estimateBufferMs"
#define LiveAdaptiveRealtimeInfo_predictedBufferMs "predictedBufferMs"
#define LiveAdaptiveRealtimeInfo_curRepReadStartTime "curRepReadStartTime"
#define LiveAdaptiveRealtimeInfo_curRepFirstDataTime "curRepFirstDataTime"
#define LiveAdaptiveRealtimeInfo_curRepStartTime "curRepStartTime"
#define LiveAdaptiveRealtimeInfo_repSwitchGapTime "repSwitchGapTime"
#define LiveAdaptiveRealtimeInfo_repSwitchCnt "repSwitchCnt"
#define LiveAdaptiveRealtimeInfo_repSwitchPointVideoBufferTime "repSwitchPointVideoBufferTime"
#define LiveAdaptiveRealtimeInfo_cachedTagDurationMs "cachedTagDurationMs"
#define LiveAdaptiveRealtimeInfo_cachedTotalDurationMs "cachedTotalDurationMs"

typedef struct QosLiveAdaptiveRealtime {
    int64_t video_buffer_time;
    int64_t audio_buffer_time;
    char url[MAX_URL_SIZE];
    uint32_t bandwidth_current;
    uint32_t bandwidth_fragment;
    uint32_t bitrate_downloading;
    uint32_t bitrate_playing;
    uint32_t current_buffer_ms;
    uint32_t estimate_buffer_ms;
    uint32_t predicted_buffer_ms;
    int64_t cur_rep_read_start_time;
    int64_t cur_rep_first_data_time;
    int64_t cur_rep_start_time;
    int64_t rep_switch_gap_time;  //new_rep_start_pts - request_pts
    uint32_t rep_switch_cnt;
    uint32_t rep_switch_point_v_buffer_ms;
    int64_t cached_tag_dur_ms;
    int64_t cached_total_dur_ms; // 缓存tag+上层播放器packet_queue长度
} QosLiveAdaptiveRealtime;

void QosLiveAdaptiveRealtime_init(QosLiveAdaptiveRealtime* qos);
void QosLiveAdaptiveRealtime_collect(QosLiveAdaptiveRealtime* qos, FFPlayer* ffp);

#endif /* qos_live_adaptive_realtime_h */
