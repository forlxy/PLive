#include "awesome_cache_c.h"
#include <memory>
#include <cstdlib>
#include <string>
#include <include/awesome_cache_c.h>
#include <include/awesome_cache_runtime_info_c.h>
#include "cache/cache_util.h"
#include "cache_manager.h"
#include "cache_opts.h"
#include "cache_session_listener_proxy.h"
#include "offline_cache_util.h"
#include "ac_datasource.h"
#include "cache_util.h"
#include "include/cache_opts.h"
#include "include/awesome_cache_c.h"
#include <assert.h>
#include "v2/cache/cache_content_v2.h"
#include "hodor_downloader/hodor_downloader.h"
#include "v2/cache/cache_def_v2.h"
#if ENABLE_CACHE_V2
#include "v2/cache/cache_v2_settings.h"
#include "v2/cache/cache_content_index_v2.h"
#include "v2/cache/cache_v2_file_manager.h"
#endif

using namespace kuaishou::cache;

void ac_util_generate_uuid(char* buf, int buf_size) {
    std::string uuid = CacheUtil::GenerateUUID();
    if (!uuid.empty()) {
        snprintf(buf, buf_size, "%s", uuid.c_str());
    }
}

bool ac_global_enabled() {
    return CacheManager::GetInstance()->IsEnabled();
}


bool ac_is_abort_by_callback_error_code(int error_code) {
    return is_cache_abort_by_callback_error_code(error_code);
}

void ac_free_strp(char** mem) {
    if (mem && *mem) {
        free(*mem);
        *mem = NULL;
    }
}

char* ac_dir_path_dup() {
    std::string path = CacheManager::GetInstance()->GetOptStr(kCacheDir);
    char* c_path = strdup(path.c_str());
    return c_path;
}

bool ac_is_fully_cached(const char* uri, const char* cache_key) {
    if (!cache_key && !uri) {
        return false;
    }
#if ENABLE_CACHE_V2
    if (CacheManager::GetInstance()->IsCacheV2Enabled()) {
        auto content = CacheV2FileManager::GetMediaDirManager()->Index()->GetCacheContent(uri,
                       cache_key ? cache_key : "");
        return content->IsFullyCached();
    }
#endif
    if (cache_key) {
        return CacheManager::GetInstance()->IsFullyCached(cache_key);
    } else {
        return CacheManager::GetInstance()->IsFullyCached(CacheUtil::GenerateKey(uri));
    }
}

int64_t ac_get_content_len_by_key(const char* uri, const char* cache_key) {
    if (!cache_key && !uri) {
        return 0;
    }
#if ENABLE_CACHE_V2
    if (CacheManager::GetInstance()->IsCacheV2Enabled()) {
        std::shared_ptr<CacheContentV2WithScope> cache_content = CacheV2FileManager::GetMediaDirManager()->Index()->GetCacheContent(uri, cache_key ? cache_key : "");
        return cache_content->GetContentLength();
    }
#endif
    if (cache_key) {
        return CacheManager::GetInstance()->GetTotalBytesForKey(cache_key);
    } else {
        return CacheManager::GetInstance()->GetTotalBytesForKey(CacheUtil::GenerateKey(uri));
    }
}

int64_t ac_get_cached_bytes_by_key(const char* uri, const char* cache_key) {
    if (!cache_key && !uri) {
        return 0;
    }
#if ENABLE_CACHE_V2
    if (CacheManager::GetInstance()->IsCacheV2Enabled()) {
        std::shared_ptr<CacheContentV2WithScope> cache_content = CacheV2FileManager::GetMediaDirManager()->Index()->GetCacheContent(uri, cache_key ? cache_key : "");
        return cache_content->GetCachedBytes();
    }
#endif
    if (cache_key) {
        return CacheManager::GetInstance()->GetCachedBytesForKey(cache_key);
    } else {
        return CacheManager::GetInstance()->GetCachedBytesForKey(CacheUtil::GenerateKey(uri));
    }
}

void hodor_clear_cache_by_key(const char* uri, const char* cache_key) {
    if (!cache_key && !uri) {
        return;
    }
    if (cache_key) {
        HodorDownloader::GetInstance()->DeleteCacheByKey(cache_key, TaskType::Media);
    } else {
        HodorDownloader::GetInstance()->DeleteCacheByKey(CacheUtil::GenerateKey(uri), TaskType::Media);
    }
}

char* hodor_generate_cache_key(const char* uri) {
    string key = CacheContentV2::GenerateKey(uri);
    return strdup(key.c_str());
}

C_DataSourceOptions ac_default_data_source_options() {
    C_DataSourceOptions options;
    options.type = kDataSourceTypeDefault;
    options.cache_flags = kFlagBlockOnCache;
    // 暂时弱化PritorityDownloadManager带来的影响，所有任务在不特殊指定为Default或者Low的情况下都是High优先级
    options.download_options.priority = kPriorityHigh;
    options.download_options.user_agent = NULL;
    options.download_options.headers = NULL;
    options.download_options.connect_timeout_ms = kDefaultConnectTimeoutMs;
    return options;
}


DataSourceOpts DataSouceOpts_from_c_options(const C_DataSourceOptions options) {
    // Translate c struct to c++ struct.
    DataSourceOpts opts;
    // player context
    opts.context_id = options.context_id;

    // for HttpDataSource
    opts.download_opts.upstream_type = options.download_options.upstream_type;
    opts.download_opts.curl_type = options.download_options.curl_type;
    opts.download_opts.http_connect_retry_cnt = options.download_options.http_connect_retry_cnt;

    // for Download Task
    opts.download_opts.async_enable_reuse_manager = options.download_options.async_enable_reuse_manager == 1 ? true : false;
    opts.download_opts.priority = options.download_options.priority;
    opts.download_opts.connect_timeout_ms = options.download_options.connect_timeout_ms;
    opts.download_opts.read_timeout_ms = options.download_options.read_timeout_ms;
    opts.download_opts.curl_buffer_size_kb = options.download_options.curl_buffer_size_kb;
    if (options.download_options.headers) {
        opts.download_opts.headers = options.download_options.headers;
    }
    if (options.download_options.user_agent) {
        opts.download_opts.user_agent = options.download_options.user_agent;
    }
    if (options.download_options.product_context) {
        opts.download_opts.product_context = options.download_options.product_context;
    }
    opts.download_opts.session_uuid = options.download_options.session_uuid;
    opts.download_opts.interrupt_cb = options.download_options.interrupt_cb;

    opts.download_opts.socket_buf_size_kb = options.download_options.socket_buf_size_kb;

    opts.download_opts.max_speed_kbps = options.download_options.max_speed_kbps;

    // player context
    opts.download_opts.context_id = options.download_options.context_id;

    if (options.download_options.network_id) {
        opts.download_opts.network_id = options.download_options.network_id;
    }
    opts.download_opts.tcp_keepalive_idle = options.download_options.tcp_keepalive_idle;
    opts.download_opts.tcp_keepalive_interval = options.download_options.tcp_keepalive_interval;
    opts.download_opts.tcp_connection_reuse = options.download_options.tcp_connection_reuse;
    opts.download_opts.tcp_connection_reuse_maxage = options.download_options.tcp_connection_reuse_maxage;

    opts.download_opts.datasource_extra_msg = options.datasource_extra_msg;

    // for vod p2sp
    opts.download_opts.vod_p2sp_task_max_size = options.download_options.vod_p2sp_task_max_size;
    opts.download_opts.vod_p2sp_cdn_request_max_size = options.download_options.vod_p2sp_cdn_request_max_size;
    opts.download_opts.vod_p2sp_cdn_request_initial_size = options.download_options.vod_p2sp_cdn_request_initial_size;
    opts.download_opts.vod_p2sp_on_threshold = options.download_options.vod_p2sp_on_threshold;
    opts.download_opts.vod_p2sp_off_threshold = options.download_options.vod_p2sp_off_threshold;
    opts.download_opts.vod_p2sp_task_timeout = options.download_options.vod_p2sp_task_timeout;

    // for live p2sp
    opts.download_opts.live_p2sp_switch_on_buffer_threshold_ms = options.download_options.live_p2sp_switch_on_buffer_threshold_ms;
    opts.download_opts.live_p2sp_switch_on_buffer_hold_threshold_ms = options.download_options.live_p2sp_switch_on_buffer_hold_threshold_ms;
    opts.download_opts.live_p2sp_switch_off_buffer_threshold_ms = options.download_options.live_p2sp_switch_off_buffer_threshold_ms;
    opts.download_opts.live_p2sp_switch_lag_threshold_ms = options.download_options.live_p2sp_switch_lag_threshold_ms;
    opts.download_opts.live_p2sp_switch_max_count = options.download_options.live_p2sp_switch_max_count;
    opts.download_opts.live_p2sp_switch_cooldown_ms = options.download_options.live_p2sp_switch_cooldown_ms;

    opts.download_opts.player_statistic = options.download_options.player_statistic;

    // 短视频的播放都是和hodor download的联动的
    opts.download_opts.enable_traffic_coordinator = true;
    opts.download_opts.is_sf2020_encrypt_source = options.download_options.is_sf2020_encrypt_source;
    opts.download_opts.sf2020_aes_key = options.download_options.sf2020_aes_key ? options.download_options.sf2020_aes_key : "";

    // for BufferDataSource
    opts.buffered_datasource_type = options.buffered_datasource_type;
    opts.buffered_datasource_size_kb = options.buffered_datasource_size_kb;
    opts.seek_reopen_threshold_kb = options.seek_reopen_threshold_kb;

    // for CacheManager to create related DataSource
    opts.type = options.type;
    opts.cache_flags = options.cache_flags;

    // for SyncCacheDataSource
    opts.download_opts.progress_cb_interval_ms = options.progress_cb_interval_ms;

    // for AsyncCacheDataSource
    opts.download_opts.enable_vod_adaptive = opts.enable_vod_adaptive = options.enable_vod_adaptive;
    opts.byte_range_size = options.byte_range_size;
    opts.first_byte_range_size = options.first_byte_range_size;

    return opts;
}


ac_data_source_t ac_data_source_create(const C_DataSourceOptions options,
                                       const CCacheSessionListener* listener,
                                       const AwesomeCacheCallback_Opaque callback,
                                       AwesomeCacheRuntimeInfo* ac_rt_info) {
    assert(CacheManager::GetInstance()->IsEnabled());
    ac_data_source_t result = new ACDataSource();

    DataSourceOpts opts = DataSouceOpts_from_c_options(options);
    opts.download_opts.http_proxy_address = CacheManager::GetInstance()->GetOptStr(kHttpProxyAddress);
    try {
        opts.cache_callback = callback;
        auto cache_session_listener = make_shared<CacheSessionListenerProxy>(listener, result);
        result->data_source = CacheManager::GetInstance()->CreateDataSource(
                                  opts, cache_session_listener, ac_rt_info, &result->data_source_seekable);
        if (!cache_session_listener || !result->data_source) {
            delete result;
            return nullptr;
        }
    } catch (std::bad_alloc) {
        LOG_ERROR_DETAIL("[%d] ac_data_source_create, catch std::bad_alloc", options.context_id);
        delete result;
        return nullptr;
    }

    return result;
}

void ac_data_source_close(ac_data_source_t opaque, bool need_report) {
    if (!opaque) {
        LOG_ERROR(" [ac_data_source_close], opaque is null");
        return;
    }
    if (opaque->data_source) {
        opaque->need_report = need_report;
        opaque->data_source->Close();
    }
}

void ac_data_source_releasep(ac_data_source_t* p_opaque) {
    if (!p_opaque || !*p_opaque) {
        LOG_ERROR("[ac_data_source_release], opaque is null");
        return;
    }
    ac_data_source_t opaque = *p_opaque;
    opaque->data_source.reset();
    delete opaque;
    *p_opaque = NULL;
}

int64_t ac_data_source_open(ac_data_source_t opaque, const char* url, const char* cache_key, int64_t position, int64_t length,
                            bool need_report) {
    if (!opaque || !opaque->data_source) {
        return 0;
    }
    opaque->spec = DataSpec()
                   .WithUri(url)
                   .WithKey(cache_key ? cache_key : std::string())
                   .WithPosition(position)
                   .WithLength(length);
    opaque->need_report = need_report;
    int64_t ret;
    try {
        ret = opaque->data_source->Open(opaque->spec);
    } catch (std::bad_alloc) {
        LOG_ERROR_DETAIL("ac_data_source_open, catch std::bad_alloc")
        ret = kResultCacheNoMemory;
    }

    return ret;
}

int64_t ac_data_source_read(ac_data_source_t opaque, uint8_t* buf, int64_t offset, int64_t read_size) {
    if (!opaque) {
        return kResultAdapterReadOpaqueNull;
    }

    if (!opaque->data_source) {
        return kResultAdapterReadDataSourceNull;
    }

    int64_t ret = opaque->data_source->Read(buf, offset, read_size);
    return ret;
}

int ac_data_source_seekable(ac_data_source_t opaque) {
    return opaque->data_source_seekable ? 1 : 0;
}

int64_t ac_data_source_seek(ac_data_source_t opaque, int64_t pos) {
//  LOG_DEBUG("ac datasource seek, position: %lld\n", pos);
    if (!opaque || !opaque->data_source || !opaque->data_source_seekable) {
        LOG_WARN("ac_data_source_seek opaque:%p data_source_seekable:%d pos:%lld \n", opaque, opaque != NULL ? opaque->data_source_seekable : false, pos);
        return pos;
    }
    int64_t ret = dynamic_cast<DataSourceSeekable*>(opaque->data_source.get())->Seek(pos);
    return ret;
}

char* ac_data_source_get_stats_json_string(ac_data_source_t opaque) {
    if (!opaque || !opaque->data_source) {
        return NULL;
    }

    Stats* stats = opaque->data_source->GetStats();
    if (!stats) {
        return NULL;
    }

    std::string stats_str = stats->ToString();
    size_t str_len = stats_str.size();
    if (str_len == 0) {
        return NULL;
    }

    char* ret = (char*)malloc(str_len + 1);
    if (!ret) {
        return ret;
    }
    memset(ret, 0, str_len + 1);
    memcpy(ret, stats_str.c_str(), str_len);

    LOG_DEBUG("[%s]: --- --- stats_str len:%ld  --- ", __func__, str_len);
    LOG_DEBUG("[%s]:  %s", __func__, ret);
    LOG_DEBUG("[%s]: --- --- --- --- --- --- --- --- ", __func__);
    return ret;
}


ac_data_source_t ac_offline_cache_start(const char* url,
                                        const char* cache_key,
                                        const C_DataSourceOptions options,
                                        void* listener) {
    if (!CacheManager::GetInstance()->Initialized()) {
        CacheManager::GetInstance()->Init();
        if (!CacheManager::GetInstance()->Initialized())
            return nullptr;
    }
    ac_data_source_t opaque = new ACDataSource;
    // Translate c struct to c++ struct.
    DataSourceOpts opts = DataSouceOpts_from_c_options(options);

#ifdef TESTING
    opts.pause_at_middle = options.pause_at_middle;
#endif
    opaque->offline_cache_util = std::make_shared<OfflineCacheUtil>(std::string(url), std::string(cache_key),
                                                                    opts, (TaskListener*)listener);
    return opaque;
}

void ac_offline_cache_stop(ac_data_source_t opaque) {
    if (opaque && opaque->offline_cache_util) {
        if (opaque->offline_cache_util->OnTaskThread()) {
            std::thread t([ = ] {
                opaque->offline_cache_util->StopOfflineCache();
            });
            t.join();
        } else {
            opaque->offline_cache_util->StopOfflineCache();
        }
        opaque->offline_cache_util.reset();
    }

    delete opaque;
    opaque = nullptr;
}

#ifdef TESTING
int32_t ac_offline_cache_read(ac_data_source_t opaque, uint8_t* buf, int32_t offset, int32_t read_len) {
    if (!opaque || !opaque->offline_cache_util) {
        return kResultNothingRead;
    }
    return opaque->offline_cache_util->AsyncRead(buf, offset, read_len);
}
#endif
