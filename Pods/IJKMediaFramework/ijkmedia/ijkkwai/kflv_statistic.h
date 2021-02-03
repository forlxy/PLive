//
//  kflv_statistic.h
//  KSYPlayerCore
//
//  Created by 帅龙成 on 07/02/2018.
//  Copyright © 2018 kuaishou. All rights reserved.
//

#ifndef kflv_statistic_h
#define kflv_statistic_h
#include <libavformat/avformat.h>

// ======================================= all info the player need START  =======================================
#define KFLV_STAT_DICT_KEY "kflv-statistic"
#define MAX_STREAM_NUM 10
#define MAX_URL_SIZE 4096

#define TIME_SPEED_UP_THRESHOLD_DEFAULT_MS (7*1000)

typedef struct FlvInfo {
    int total_bandwidth_kbps;
    char url[MAX_URL_SIZE];
} FlvInfo;

typedef struct KFlvStatistic {
    // info from manifest.json
    FlvInfo flvs[MAX_STREAM_NUM];
    int flv_nb;

    // algo specific
    uint32_t bandwidth_current;
    uint32_t bandwidth_fragment;
    uint32_t bitrate_downloading;
    uint32_t current_buffer_ms;
    uint32_t estimate_buffer_ms;
    uint32_t predicted_buffer_ms;

    // speed-up threshold
    uint32_t speed_up_threshold;

    // status
    int cur_decoding_flv_index;
    uint32_t switch_point_a_buffer_ms;
    uint32_t switch_point_v_buffer_ms;

    char cur_playing_url[MAX_URL_SIZE];   // playurl with kabr_spts

    // rep switch speed
    int64_t cur_rep_read_start_time;
    int64_t cur_rep_http_open_time;
    int64_t cur_rep_read_header_time;
    int64_t cur_rep_first_data_time;  // first tag time
    int64_t cur_rep_start_time;
    int64_t rep_switch_gap_time;  //new_rep_start_pts - request_pts
    uint32_t rep_switch_cnt;

    // error
    int cur_rep_http_reading_error;  // errors during gop reading

    int64_t cached_tag_dur_ms;
    int64_t cached_a_dur_ms; // 上层播放器a_packet_queue长度
    int64_t cached_v_dur_ms; // 上层播放器v_packet_queue长度
    int64_t total_bytes_read;
} KFlvStatistic;
typedef struct KFlvPlayerStatistic {
    KFlvStatistic kflv_stat;

    int init_index;
    char init_url[MAX_URL_SIZE];
    uint32_t init_bitrate;
} KFlvPlayerStatistic;
// ======================================= all info the player need END =======================================


void KFlvPlayerStatistic_collect_playing_info(KFlvPlayerStatistic* dst, AVFormatContext* s);
void KFlvPlayerStatistic_collect_initial_info(KFlvPlayerStatistic* stat, char* filename);

uint32_t KFlvPlayerStatistic_get_playing_bitrate(KFlvPlayerStatistic* stat);
uint32_t KFlvPlayerStatistic_get_downloading_bitrate(KFlvPlayerStatistic* stat);
char* KFlvPlayerStatistic_get_playing_url(KFlvPlayerStatistic* stat);
int KFlvPlayerStatistic_get_http_reading_error(KFlvPlayerStatistic* stat);


#endif /* kflv_statistic_h */
