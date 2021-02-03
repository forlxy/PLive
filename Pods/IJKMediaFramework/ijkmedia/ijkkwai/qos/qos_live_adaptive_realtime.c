//
//  qos_live_adaptive_realtime.c
//  IJKMediaFramework
//
//  Created by wangtao03 on 2018/3/6.
//  Copyright © 2018年 kuaishou. All rights reserved.
//

#include <stdio.h>
#include "qos_live_adaptive_realtime.h"
#include "ijkkwai/kwai_qos.h"


void QosLiveAdaptiveRealtime_init(QosLiveAdaptiveRealtime* qos) {
    assert(qos);
    memset(qos, 0, sizeof(QosLiveAdaptiveRealtime));
}

void QosLiveAdaptiveRealtime_collect(QosLiveAdaptiveRealtime* qos, FFPlayer* ffp) {
    assert(qos);
    assert(ffp);

    QosLiveAdaptiveRealtime_init(qos);

    qos->video_buffer_time = ffp->stat.video_cache.duration;
    qos->audio_buffer_time = ffp->stat.audio_cache.duration;
    qos->bandwidth_current = ffp->kflv_player_statistic.kflv_stat.bandwidth_current;
    qos->bandwidth_fragment = ffp->kflv_player_statistic.kflv_stat.bandwidth_fragment;
    qos->bitrate_downloading = KFlvPlayerStatistic_get_downloading_bitrate(&ffp->kflv_player_statistic);
    qos->bitrate_playing = KFlvPlayerStatistic_get_playing_bitrate(&ffp->kflv_player_statistic);
    qos->current_buffer_ms = ffp->kflv_player_statistic.kflv_stat.current_buffer_ms;
    qos->estimate_buffer_ms = ffp->kflv_player_statistic.kflv_stat.estimate_buffer_ms;
    qos->predicted_buffer_ms = ffp->kflv_player_statistic.kflv_stat.predicted_buffer_ms;
    qos->cur_rep_read_start_time = ffp->kflv_player_statistic.kflv_stat.cur_rep_read_start_time;
    qos->cur_rep_first_data_time = ffp->kflv_player_statistic.kflv_stat.cur_rep_first_data_time;
    qos->cur_rep_start_time = ffp->kflv_player_statistic.kflv_stat.cur_rep_start_time;
    qos->rep_switch_gap_time = ffp->kflv_player_statistic.kflv_stat.rep_switch_gap_time;
    qos->rep_switch_cnt = ffp->kflv_player_statistic.kflv_stat.rep_switch_cnt;
    qos->rep_switch_point_v_buffer_ms = ffp->kflv_player_statistic.kflv_stat.switch_point_v_buffer_ms;
    qos->cached_tag_dur_ms = ffp->kflv_player_statistic.kflv_stat.cached_tag_dur_ms;
    qos->cached_total_dur_ms = ffp->kflv_player_statistic.kflv_stat.cached_v_dur_ms
                               + ffp->kflv_player_statistic.kflv_stat.cached_tag_dur_ms;
    strncpy(qos->url, KFlvPlayerStatistic_get_playing_url(&ffp->kflv_player_statistic), MAX_URL_SIZE - 1);
}
