//
//  qos_live_realtime.h
//  IJKMediaPlayer
//
//  Created by 帅龙成 on 26/02/2018.
//  Copyright © 2018 kuaishou. All rights reserved.
//

#ifndef qos_live_realtime_h
#define qos_live_realtime_h

#include <stdbool.h>
#include <stdio.h>
#include "ijkmedia/ijksdl/ijksdl_mutex.h"
#include "ijkkwai/kwai_live_delay_stat.h"

typedef struct FFPlayer FFPlayer;

typedef struct SpeedChangeStat_t {
    int down_duration;  // 0.75倍速播放
    int up_1_duration;  // 1.25倍速播放
    int up_2_duration;  // 1.5倍速播放
} SpeedChangeStat;

#define LIVE_MANIFEST_AUTO  -1

enum LIVE_MANIFEST_SWITCH_FLAG {
    LIVE_MANIFEST_SWITCH_FLAG_AUTO = 1,
    LIVE_MANIFEST_SWITCH_FLAG_MANUAL,
    LIVE_MANIFEST_SWITCH_FLAG_AUTOMANUAL
};

typedef struct QosLiveAdaptive {
    int last_switch_mode;
    int cur_switch_mode;
    bool auto_mode_set;
    bool mannual_mode_set;
} QosLiveAdaptive;

typedef struct QosLiveRealtime {
    uint32_t dec_err_cnt;
    int reset_session_cnt;
    int v_hevc_paramete_set_change_cnt;
    int v_hevc_paramete_set_update_fail_cnt;
#if defined(__APPLE__)
    int v_tool_box_err_cnt;
    int pkt_cnt_on_err;
    int queue_is_full_cnt;
#elif defined(__ANDROID__)
    int v_mediacodec_input_err_cnt;
    int v_mediacodec_output_err_cnt;
    int v_mediacodec_output_try_again_err_cnt;
    int v_mediacodec_output_buffer_changed_err_cnt;
    int v_mediacodec_output_unknown_err_cnt;
#endif

    int64_t kbytes_read;
    uint32_t block_count;
    int64_t buffer_time;
    int64_t dropped_packet_duration;
    uint32_t read_video_frames;
    uint32_t decoded_video_frames;
    uint32_t rendered_video_frames;
    uint32_t rendered_audio_frames;
    uint32_t silence_audio_frames;
    uint32_t rep_switch_cnt;
    int64_t cur_rep_read_start_time;
    int64_t cur_rep_first_data_time;
    uint32_t adaptive_gop_info_collect_cnt;

    int64_t p2sp_used_bytes;
    int64_t cdn_used_bytes;

    int64_t p2sp_download_bytes;
    int64_t cdn_download_bytes;

    // audioonly case
    uint32_t audio_pts_jump_forward_cnt;
    int64_t audio_pts_jump_forward_duration;
    uint32_t audio_pts_jump_backward_index;

    uint32_t video_ts_rollback_cnt;
    int64_t video_ts_rollback_duration;

    int p2sp_switch_attempts;
    int cdn_switch_attempts;

    int p2sp_switch_success_attempts;
    int cdn_switch_success_attempts;

    int p2sp_switch_duration_ms;
    int cdn_switch_duration_ms;

    int got_first_screen;

    int index;

    DelayStat a_render_delay;
    DelayStat v_recv_delay;
    DelayStat v_pre_dec_delay;
    DelayStat v_post_dec_delay;
    DelayStat v_render_delay;

    SpeedChangeStat speed_change_stat;

    SDL_mutex* app_qos_json_mutex;
    cJSON* app_qos_json;
} QosLiveRealtime;

void QosLiveRealtime_init(QosLiveRealtime* qos, QosLiveAdaptive* qos_adaptive);
char* QosLiveRealtime_collect(FFPlayer* ffp, int first, int last,
                              int64_t start_time, int64_t duration, int64_t collectInterval);
void QosLiveRealtime_set_app_qos_info(FFPlayer* ffp, const char* app_qos_info);
#endif /* qos_live_realtime_h */
