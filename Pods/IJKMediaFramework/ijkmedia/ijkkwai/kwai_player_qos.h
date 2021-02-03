/*
 * ksy_qos.h
 *
 * Copyright (c) 2014 Zhang Rui <bbcallen@gmail.com>
 *
 * This file is part of ijkPlayer.
 *
 * ijkPlayer is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * ijkPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with ijkPlayer; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef KSY_QOS_H
#define KSY_QOS_H

#include <stdint.h>
#include <stdlib.h>

#define KSY_QOS_STR_MAX_LEN 512

typedef struct KsyQosInfo {
    int audioBufferByteLength; // data size in audio queue
    int audioBufferTimeLength; // time length in audio queue... unit: ms.
    int64_t audioTotalDataSize; // size of audio data have arrived audio queue

    int videoBufferByteLength;
    int videoBufferTimeLength;
    int64_t videoTotalDataSize;

    int64_t totalDataBytes; // total audio and video data size since start playing

    // following units: milliseconds
    int audioDelay;
    int videoDelayRecv;
    int videoDelayBefDec;
    int videoDelayAftDec;
    int videoDelayRender;
    int fst_total; // first screen time
    int fst_dns_analyze;
    int fst_http_connect;
    int fst_http_first_data;
    int fst_input_open;
    int fst_stream_find;
    int fst_codec_open;
    int fst_all_prepared;
    int fst_wait_for_play;
    int fst_video_pkt_recv;
    int fst_video_pre_dec;
    int fst_video_dec;
    int fst_video_render;
    int fst_dropped_duration;
    int dropped_duration;

    char hostInfo[KSY_QOS_STR_MAX_LEN];
    char vencInit[KSY_QOS_STR_MAX_LEN];
    char aencInit[KSY_QOS_STR_MAX_LEN];
    char vencDynamic[KSY_QOS_STR_MAX_LEN];
    char* comment;

    // cache
    int64_t cached_bytes;
    int64_t total_bytes;
    int reopen_cnt_by_seek;
    char current_read_path[KSY_QOS_STR_MAX_LEN];

    // live manifest
    uint32_t rep_switch_cnt;

    // p2sp
    int live_native_p2sp_enabled;
    int64_t p2sp_download_bytes;
    int64_t cdn_download_bytes;
} KsyQosInfo;

#define KSY_AUDIO_BUFFER_BYTE "audio_buffer_byte"
#define KSY_AUDIO_BUFFER_TIME "audio_buffer_time"
#define KSY_AUIDO_TOTAL_DATA_SIZE "audio_total_data_size"

#define KSY_VIDEO_BUFFER_BYTE "video_buffer_byte"
#define KSY_VIDEO_BUFFER_TIME "video_buffer_time"
#define KSY_VIDEO_TOTAL_DATA_SIZE "video_total_data_size"

#define KSY_TOTAL_DATA_BYTES "total_data_bytes"

#define KSY_AUDIO_DELAY "audio_delay"
#define KSY_VIDEO_DELAY_RECV "video_delay_recv"
#define KSY_VIDEO_DELAY_BEF_DEC "video_delay_bef_dec"
#define KSY_VIDEO_DELAY_AFT_DEC "video_delay_aft_dec"
#define KSY_VIDEO_DELAY_RENDER "video_delay_render"

#define KSY_FST_TOTAL "fst_total"
#define KSY_FST_DNS_ANALYZE "fst_dns_analyze"
#define KSY_FST_HTTP_CONNECT "fst_http_connect"
#define KSY_FST_HTTP_FIRST_DATA "fst_http_first_data"
#define KSY_FST_INPUT_OPEN "fst_input_open"
#define KSY_FST_STREAM_FIND "fst_stream_find"
#define KSY_FST_CODEC_OPEN "fst_codec_open"
#define KSY_FST_ALL_PREPARED "fst_all_prepared"
#define KSY_FST_WAIT_FOR_PLAY "fst_wait_for_play"
#define KSY_FST_VIDEO_PKT_RECV "fst_video_pkt_recv"
#define KSY_FST_VIDEO_PRE_DEC "fst_video_pre_dec"
#define KSY_FST_VIDEO_DEC "fst_video_dec"
#define KSY_FST_VIDEO_RENDER "fst_video_render"
#define KSY_FST_DROPPED_DURATION "fst_dropped_duration"

#define KSY_DROPPED_DURATION "dropped_duration"

#define KSY_HOST_INFO "host_info"
#define KSY_VENC_INIT "venc_init"
#define KSY_AENC_INIT "aenc_init"
#define KSY_VENC_DYNAMIC "venc_dynamic"
#define KSY_COMMENT "comment"

#define KWAI_CURRENT_READ_URI "current_read_uri"
#define KWAI_CACHED_BYTES "cached_bytes"
#define KWAI_TOTAL_BYTES "total_bytes"
#define KWAI_REOPEN_CNT_BY_SEEK "reopen_cnt_by_seek"


#define KSY_DELAY_STAT_PERIOD_LAST_CALC_TIME  "delay_stat_period_last_calc_time"
#define KSY_DELAY_STAT_PERIOD_SUM             "delay_stat_period_sum"
#define KSY_DELAY_STAT_PERIOD_COUNT           "delay_stat_period_count"
#define KSY_DELAY_STAT_PERIOD_AVG             "delay_stat_period_avg"
#define KSY_DELAY_STAT_TOTAL_LAST_CALC_TIME   "delay_stat_total_last_calc_time"
#define KSY_DELAY_STAT_TOTAL_SUM              "delay_stat_total_sum"
#define KSY_DELAY_STAT_TOTAL_COUNT            "delay_stat_total_count"
#define KSY_DELAY_STAT_TOTAL_AVG              "delay_stat_total_avg"
#define KSY_DELAY_STAT_DISTRIBUTED_DURATION   "delay_stat_distributed_duration"

#define KSY_SPEED_CHANGE_STAT_DOWN_DURATION   "speed_change_stat_down_duration"
#define KSY_SPEED_CHANGE_STAT_UP_1_DURATION   "speed_change_stat_up_1_duration"
#define KSY_SPEED_CHANGE_STAT_UP_2_DURATION   "speed_change_stat_up_2_duration"

#define KSY_LIVE_ADAPTIVE_REP_SWITCH_CNT      "live_adaptive_rep_switch_cnt"

#define KSY_LIVE_NATIVE_P2SP_ENABLED "live_native_p2sp_enabled"
#define KSY_P2SP_DOWNLOAD_BYTES "p2sp_download_bytes"
#define KSY_CDN_DOWNLOAD_BYTES "cdn_download_bytes"

#endif//KSY_QOS_H
