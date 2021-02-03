#pragma once
#include <string>
#include "constant.h"
#include "awesome_cache_interrupt_cb_c.h"
#include "awesome_cache_callback_c.h"
#include "player_statistic_c.h"
#include "awesome_cache_c.h"

namespace kuaishou {
namespace cache {

struct DownloadOpts {
    // context
    int context_id = -1;

    // 回调最小间隔
    int32_t progress_cb_interval_ms = 50;

    // for HttpDataSource
    UpstreamDataSourceType upstream_type = kDefaultHttpDataSource;
    CurlType curl_type = kCurlTypeDefault;
    int32_t http_connect_retry_cnt = kDefaultHttpConnectRetryCount;

    // for Download Task
    bool async_enable_reuse_manager;
    DownloadPriority priority = kPriorityDefault;
    int32_t connect_timeout_ms = kDefaultConnectTimeoutMs;
    int32_t read_timeout_ms = kDefaultReadTimeoutMs;
    int32_t curl_buffer_size_kb = kDefaultCurlBufferSizeKb;
    int32_t socket_buf_size_kb = -1;
    std::string headers = "";
    std::string user_agent = "";
    std::string product_context = "";
    std::string session_uuid = ""; // 一次播放生命周期对应一个session_id
    std::string md5_hash_code = "";
    AwesomeCacheInterruptCB interrupt_cb = {0};
    bool is_live = false;
    bool allow_content_length_unset = false;

    // for vod p2sp
    int vod_p2sp_task_max_size = kDefaultVodP2spTaskMaxSize; // max length for each p2sp task
    int vod_p2sp_cdn_request_max_size = kDefaultVodP2spCdnRequestMaxSize; // max length for each cdn request
    int vod_p2sp_cdn_request_initial_size = kDefaultVodP2spCdnRequestInitialSize;
    int vod_p2sp_on_threshold = kDefaultVodP2spOnThreshold;
    int vod_p2sp_off_threshold = kDefaultVodP2spOffThreshold;
    int vod_p2sp_task_timeout = kDefaultVodP2spTaskTimeout;

    std::string http_proxy_address = ""; // only for QA purpose
    std::string network_id = "";
    int tcp_keepalive_idle = 0; // set to 0 or negative to disable tcp keepalive
    int tcp_keepalive_interval = 0;
    int tcp_connection_reuse = 0;
    int tcp_connection_reuse_maxage = 0;


    // 这个字段是给cdn上报用的，更应该放在downloadOpts里，名字或许就叫cdnExtraMsg或者ExtraMsgFromApp比较好
    std::string datasource_extra_msg = "";
    int32_t enable_vod_adaptive = 0;    // 和dataSourceOpt是一个意思的，只不过AsyncV2需要在DownloadOpts里参考

    int live_p2sp_switch_on_buffer_threshold_ms = kDefaultLiveP2spSwitchOnBufferThresholdMs;
    int live_p2sp_switch_on_buffer_hold_threshold_ms = kDefaultLiveP2spSwitchOnBufferHoldThresholdMs; // buffer要超过threshold保持多长时间
    int live_p2sp_switch_off_buffer_threshold_ms = kDefaultLiveP2spSwitchOffBufferThresholdMs;
    int live_p2sp_switch_lag_threshold_ms = kDefaultLiveP2spSwitchLagThresholdMs;
    int live_p2sp_switch_max_count = kDefaultLiveP2spSwitchMaxCount;
    int live_p2sp_switch_cooldown_ms = kDefaultLiveP2spSwitchCooldownMs;

    ac_player_statistic_t player_statistic = nullptr;

    /**
     * 这个变量表示如果CacheV2的ScopeDataSource如果下载完整个CacheContent后，需要通知HodorDownloader的TrafficCoordinator
     */
    bool enable_traffic_coordinator = false;

    int max_speed_kbps = -1;

    bool is_sf2020_encrypt_source = false;
    std::string sf2020_aes_key;

};

struct DataSourceOpts {
    // for BufferDataSource
    BufferedDataSourceType buffered_datasource_type = kBufferedDataSource;
    int32_t buffered_datasource_size_kb = kDefaultCurlBufferSizeKb;
    int32_t seek_reopen_threshold_kb = kDefaultSeekReopenThresholdKb;

    // for CacheManager to create related DataSource
    DataSourceType type = kDataSourceTypeDefault;
    int32_t cache_flags = -1;

    // for SyncCacheDataSource
    int32_t progress_cb_interval_ms = 50;

    // for AsyncCacheDataSource
    int32_t enable_vod_adaptive = 0;
    int32_t byte_range_size = 1024 * 1024;
    int32_t first_byte_range_size = 3 * 1024 * 1024;

    DownloadOpts download_opts;
#ifdef TESTING
    bool pause_at_middle;
#endif
    // context
    int context_id = -1;

    // for new cache callback
    AwesomeCacheCallback_Opaque cache_callback = nullptr;
};


} // namespace cache
} // namespace kuaishou
