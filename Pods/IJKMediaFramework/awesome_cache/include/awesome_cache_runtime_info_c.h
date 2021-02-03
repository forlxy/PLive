//
//  c_awesome_cache_qos.h
//  IJKMediaPlayer
//
//  Created by 帅龙成 on 2018/10/29.
//  Copyright © 2018 kuaishou. All rights reserved.
//
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "cache_defs.h"

#ifdef __cplusplus
extern "C" {
#endif


#define DataSourceQos_Initializer {0}

#define DATA_SOURCE_URI_MAX_LEN 512
#define DATA_SOURCE_IP_MAX_LEN 128
#define CDN_KWAI_SIGN_MAX_LEN   128
#define CDN_X_KS_CACHE_MAX_LEN   2048
#define HTTP_VERSION_MAX_LEN 31
#define CONNECT_INFO_COUNT 2    //记录top 2 connect信息
#define EXTRA_CONNECT_INFO_INDEX 1 //extra 记录第二次连接信息，对于hls即ts连接信息
#define INVALID_RESPONSE_HEADER 512

//

/**
 * 1.这个结构体的字段目前会输出给 debug_info和kwai_qos上报，所以新的dataSource务必认真对接和确认正确性、准确性
 * 2.目前只做成简单的，dataSource对自己不清楚的字段不管即可
 */
typedef struct AwesomeCacheRuntimeInfo {
    struct {
        int cache_flags;
        // 一些关键的设置信息放在这，方便后续上报和DebugInfo统一使用
        DataSourceType data_source_type;
        UpstreamDataSourceType upstream_type;
        BufferedDataSourceType buffered_type;
        CurlType curl_type;
    } cache_applied_config;


    struct {
        int reopen_cnt_by_seek;
        int buffered_datasource_size_kb;
        int buffered_datasource_seek_threshold_kb;
    } buffer_ds;

    // SyncCacheDataSource/ASyncCacheDataSource
    struct {
        char current_read_uri[DATA_SOURCE_URI_MAX_LEN + 1];

        int64_t total_bytes;
        int64_t cached_bytes; // 这个字段比较特殊，会在CacheDataSource初始化，但是会在DataSink更新; 但是对于async cache，会在async cache中更新。
        int64_t async_v2_cached_bytes;
        bool ignore_cache_on_error;
        int cache_read_source_cnt;
        int cache_write_source_cnt;
        int cache_upstream_source_cnt;
        bool is_reading_file_data_source;
        int first_open_error;

        int32_t progress_cb_interval_ms;

        // for AsyncCacheDataSource
        int byte_range_size;
        int first_byte_range_length;
        int download_exit_reason;
        int read_from_upstream;
        int64_t read_position;
        int64_t bytes_remaining;
        int pre_download_cnt;
    } cache_ds;

    struct {
        int bytes_not_commited; // 这个是还没正式提交到cacheContent的文件，但是内容可能在内存，也可能在文件里
        int fs_error_code;
    } sink;

    struct {
        int sink_write_cost_ms;
    } tee_ds;

    struct {
        int http_max_retry_cnt;
        int http_retried_cnt;
        int64_t task_downloaded_bytes; //已完成task下载数
        int64_t download_bytes; //实时更新
    } http_ds;

    struct {
        char* config_header;    // use AwesomeCacheRuntimeInfo_download_task_set_config_header to set new value
        char* config_user_agent;// use AwesomeCacheRuntimeInfo_download_task_set_config_user_agent to set new value

        char kwaisign[CDN_KWAI_SIGN_MAX_LEN];
        char x_ks_cache[CDN_X_KS_CACHE_MAX_LEN];
        int stop_reason;
        int error_code;
        long os_errno;

        // for asyncV2 for now，后续可能会移到cdn日志
        bool need_report_header;
        char invalid_header[INVALID_RESPONSE_HEADER]; // 当返回kCurlHttpResponseHeaderInvalid的时候，上报此内容
        int http_response_code;
        int curl_ret;
        int64_t downloaded_bytes;
        int64_t recv_valid_bytes;


        char resolved_ip[DATA_SOURCE_IP_MAX_LEN + 1];

        char http_version[HTTP_VERSION_MAX_LEN + 1]; // e.g. "HTTP/1.1", "gQUIC"
        int http_connect_ms;
        int http_first_data_ms;
        int http_dns_analyze_ms;

        int con_timeout_ms;
        int read_timeout_ms;
        int download_total_cost_ms;
        int feed_data_consume_ms_;
//        int download_total_drop_cnt;  // KwaiQos用到，但是不是关键数据，fixme
        int download_total_drop_bytes;
        int curl_buffer_size_kb;
        int curl_buffer_max_used_kb;
        int curl_byte_range_error;

        int speed_cal_avg_speed_kbps;   // 对于asyncV2 这个值表示 total_download/transfer_cost平均速度，也代表了 mark_speed_kbps
        int speed_cal_current_speed_index;
        int speed_cal_current_speed_kbps;
        int speed_cal_mark_speed_kbps;

        int sock_orig_size_kb;
        int sock_cfg_size_kb;
        int sock_act_size_kb;
        // inner
        int64_t ts_download_start_ms;
        int64_t ts_download_end_ms;
    } download_task;

    struct ConnectInfo {
        char resolved_ip[DATA_SOURCE_IP_MAX_LEN + 1];
        int http_connect_ms;
        int http_first_data_ms;
        int http_dns_analyze_ms;
        int64_t position;
        int64_t length;
        int64_t first_data_ts;
    } connect_infos[CONNECT_INFO_COUNT];

    volatile int datasource_index;

    struct {
        int enabled;

        int64_t p2sp_used_bytes;
        int64_t cdn_used_bytes;

        int64_t p2sp_download_bytes;
        int64_t cdn_download_bytes;

        int p2sp_switch_attempts;
        int cdn_switch_attempts;

        int p2sp_switch_success_attempts;
        int cdn_switch_success_attempts;

        int p2sp_switch_duration_ms;
        int cdn_switch_duration_ms;
    } p2sp_task;

    struct {
        uint64_t consumed_download_time_ms;
        uint64_t actual_video_size_byte;
        uint32_t average_download_rate_kbps;     // actual_video_size_byte*8000/(consumed_download_time_ms * 1024)
        uint32_t real_time_throughput_kbps;
    } vod_adaptive;

    struct {
        bool enabled;
        bool on;
        int64_t cdn_bytes;
        int cdn_open_count;
        int64_t p2sp_bytes_used;
        int64_t p2sp_bytes_repeated;
        int64_t p2sp_bytes_received;
        int64_t p2sp_bytes_requested;
        int64_t p2sp_start;
        int64_t p2sp_first_byte_duration;
        int64_t p2sp_first_byte_offset;
        int p2sp_error_code;
        int64_t p2sp_last_start;
        int64_t p2sp_last_end;
        int p2sp_open_count;
        int64_t player_buffer_ms;
        int64_t file_length;
        char* sdk_details;
        char p2sp_sdk_version[16];
    } vod_p2sp;

    struct {
        int scope_max_size_kb_of_settting;
        int scope_max_size_kb_of_cache_content;
        int skip_scope_cnt; // 有多少个scope发生过因服务器不支持range请求而skip的动作
        int skip_total_bytes; // 因为服务器不支持range请求而skip的总字节数
        int resume_file_fail_cnt;
        int flush_file_fail_cnt;
        // 播放器刚开播，第一次跟CacheDataSource要数据的时候，缓存里已经有多少byte数据了，初始化为-1
        // 注意这个字段只关注
        int64_t cached_bytes_on_play_start;
    } cache_v2_info;

} AwesomeCacheRuntimeInfo;

HODOR_EXPORT void AwesomeCacheRuntimeInfo_init(AwesomeCacheRuntimeInfo* info);
HODOR_EXPORT void AwesomeCacheRuntimeInfo_release(AwesomeCacheRuntimeInfo* info);

HODOR_EXPORT const char* AwesomeCacheRuntimeInfo_config_get_datas_source_type_str(AwesomeCacheRuntimeInfo* info);
HODOR_EXPORT const char* AwesomeCacheRuntimeInfo_config_get_upstream_type_to_str(AwesomeCacheRuntimeInfo* info);
HODOR_EXPORT const char* AwesomeCacheRuntimeInfo_config_get_buffered_datasource_type_str(
    AwesomeCacheRuntimeInfo* info);
HODOR_EXPORT const char* AwesomeCacheRuntimeInfo_config_get_download_task_type_to_str(AwesomeCacheRuntimeInfo* info);

HODOR_EXPORT void AwesomeCacheRuntimeInfo_cache_ds_init(AwesomeCacheRuntimeInfo* info);

HODOR_EXPORT void AwesomeCacheRuntimeInfo_download_task_init(AwesomeCacheRuntimeInfo* info);
HODOR_EXPORT int AwesomeCacheRuntimeInfo_download_task_get_transfer_cost_ms(AwesomeCacheRuntimeInfo* info);
HODOR_EXPORT void AwesomeCacheRuntimeInfo_download_task_start(AwesomeCacheRuntimeInfo* info);
HODOR_EXPORT void AwesomeCacheRuntimeInfo_download_task_end(AwesomeCacheRuntimeInfo* info);
HODOR_EXPORT void AwesomeCacheRuntimeInfo_download_task_set_config_user_agent(AwesomeCacheRuntimeInfo* info, const char* val);
HODOR_EXPORT void AwesomeCacheRuntimeInfo_download_task_set_config_header(AwesomeCacheRuntimeInfo* info, const char* val);

#ifdef __cplusplus
}
#endif
