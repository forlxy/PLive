#ifndef C_AWESOME_CACHE_H
#define C_AWESOME_CACHE_H

#include "hodor_config.h"
#include "cache_defs.h"
#include "cache_errors.h"
#include "cache_session_listener_c.h"
#include "awesome_cache_runtime_info_c.h"
#include "awesome_cache_interrupt_cb_c.h"
#include "player_statistic_c.h"
#include "awesome_cache_callback_c.h"

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// 有一个字节给 \0
#define SESSION_UUID_BUFFER_LEN (32+1)
#define DATASOURCE_OPTION_EXTRA_BUFFER_LEN (256+1)

typedef struct ACDataSource* ac_data_source_t;

typedef struct {
    // for HttpDataSource
    UpstreamDataSourceType upstream_type;
    CurlType curl_type;
    int http_connect_retry_cnt;

    // for Download Task
    int32_t async_enable_reuse_manager;
    DownloadPriority priority;
    int32_t connect_timeout_ms;
    int32_t read_timeout_ms;
    int32_t curl_buffer_size_kb;
    int socket_buf_size_kb;
    char* headers;
    char* user_agent;
    char* product_context;
    char session_uuid[SESSION_UUID_BUFFER_LEN]; // 一次播放生命周期对应一个session_id
    AwesomeCacheInterruptCB interrupt_cb;

    // for vod p2sp
    int vod_p2sp_task_max_size;
    int vod_p2sp_cdn_request_max_size;
    int vod_p2sp_cdn_request_initial_size;
    int vod_p2sp_on_threshold;
    int vod_p2sp_off_threshold;
    int vod_p2sp_task_timeout;
    char* network_id;
    int tcp_keepalive_idle; // set to 0 to negative to disable tcp keepalive
    int tcp_keepalive_interval;
    int tcp_connection_reuse;
    int tcp_connection_reuse_maxage;

    // context
    int context_id;

    int live_p2sp_switch_on_buffer_threshold_ms;
    int live_p2sp_switch_on_buffer_hold_threshold_ms;
    int live_p2sp_switch_off_buffer_threshold_ms;
    int live_p2sp_switch_lag_threshold_ms;
    int live_p2sp_switch_max_count;
    int live_p2sp_switch_cooldown_ms;

    // Always updated statistics from player
    // Datasource may also use player_statistic->add_listener to register updates in realtime
    ac_player_statistic_t player_statistic;

    int max_speed_kbps;

    bool is_sf2020_encrypt_source;
    char* sf2020_aes_key;
} C_DownloadOptions;

typedef struct {
    // for BufferDataSource
    BufferedDataSourceType buffered_datasource_type;
    int buffered_datasource_size_kb;
    int seek_reopen_threshold_kb;

    // for CacheManager to create related DataSource
    DataSourceType type;
    int32_t cache_flags;

    // for SyncCacheDataSource
    int progress_cb_interval_ms;

    // for AsyncCacheDataSource
    int byte_range_size;
    int first_byte_range_size;

    // for vod adaptive
    int enable_vod_adaptive;

    C_DownloadOptions download_options;
    char datasource_extra_msg[DATASOURCE_OPTION_EXTRA_BUFFER_LEN];
#ifdef TESTING
    bool pause_at_middle;
#endif

    // context
    int context_id;
} C_DataSourceOptions;

HODOR_EXPORT void ac_util_generate_uuid(char* buf, int buf_size);

HODOR_EXPORT bool ac_global_enabled();

HODOR_EXPORT bool ac_is_abort_by_callback_error_code(int error_code);

HODOR_EXPORT C_DataSourceOptions ac_default_data_source_options();

HODOR_EXPORT void ac_free_strp(char** pp);
/**
 *
 * @return cache dir path, need to call free the memory with ac_free_strp()
 */
HODOR_EXPORT char* ac_dir_path_dup();

/**
 * 这个接口默认删除的都是Media文件夹下的cache
 */
HODOR_EXPORT void hodor_clear_cache_by_key(const char* uri, const char* cache_key);

/**
 * @return cache_key, need to call free the memory with ac_free_strp()
 *
 */
HODOR_EXPORT char* hodor_generate_cache_key(const char* uri);

/**
 * Make sure you are not reading this uri when you are clearing it.
 * @param uri the url to clear.
 */
HODOR_EXPORT bool ac_is_fully_cached(const char* uri, const char* cache_key);

HODOR_EXPORT int64_t ac_get_content_len_by_key(const char* uri, const char* cache_key);

HODOR_EXPORT int64_t ac_get_cached_bytes_by_key(const char* uri, const char* cache_key);

HODOR_EXPORT ac_data_source_t ac_data_source_create(const C_DataSourceOptions options,
                                                    const CCacheSessionListener* listener,
                                                    const AwesomeCacheCallback_Opaque callback,
                                                    AwesomeCacheRuntimeInfo* ac_rt_info);

HODOR_EXPORT int64_t ac_data_source_open(ac_data_source_t opaque, const char* url, const char* cache_key, int64_t position, int64_t length, bool need_report);

HODOR_EXPORT int64_t ac_data_source_read(ac_data_source_t opaque, uint8_t* buf, int64_t offset, int64_t buf_size);

HODOR_EXPORT int64_t ac_data_source_seek(ac_data_source_t opaque, int64_t pos);

/**
 * 非耗时操作
 */
HODOR_EXPORT int ac_data_source_seekable(ac_data_source_t opaque);

HODOR_EXPORT char* ac_data_source_get_stats_json_string(ac_data_source_t opaque);

HODOR_EXPORT void ac_data_source_close(ac_data_source_t opaque, bool is_reconnect);
HODOR_EXPORT void ac_data_source_releasep(ac_data_source_t* p_opaque);

/**
 * ac_offline_cache_start: start offline cache
 * ac_offline_cache_stop:  provided for canceling the started offline cache
 * ac_offline_cache_read:  just used for reading async offline-cache video data,
 *                         may comment out later
 */
HODOR_EXPORT ac_data_source_t ac_offline_cache_start(const char* url,
                                                     const char* cache_key,
                                                     const C_DataSourceOptions options,
                                                     void* listener);
HODOR_EXPORT void ac_offline_cache_stop(ac_data_source_t opaque);
#ifdef TESTING
int32_t ac_offline_cache_read(ac_data_source_t opaque, uint8_t* buf, int32_t offset, int32_t read_len);
#endif

#ifdef __cplusplus
}
#endif
#endif // C_AWESOME_CACHE_H
