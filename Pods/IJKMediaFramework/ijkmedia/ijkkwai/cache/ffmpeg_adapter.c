//
//
//  Created by 帅龙成 on 26/10/2017.
//  Copyright © 2017 kuaishou. All rights reserved.
//

#include <unistd.h>
#include <assert.h>
#include <pthread.h>

#include <libavutil/avstring.h>
#include <libavformat/url.h>
#include <ijksdl/ijksdl_log.h>
#include <libavutil/time.h>
#include <awesome_cache/include/awesome_cache_c.h>
#include "ffmpeg_adapter.h"
#include "cache_statistic.h"

#define SCHEME_FILE_HEADER "file://"
#define SCHEME_HTTP_HEADER "http://"
#define SCHEME_HTTPS_HEADER "https://"

#define FILE_READ_BUFFER_MAX_LEN (32*1024)

static const bool VERBOSE = false;

typedef struct AvIoOpaqueWithDataSource {
    // context
    int context_id;
    // open config
    char* url;
    char* cache_key;
    int64_t position;
    AwesomeCacheInterruptCB interrupt_callback;

    // runtime stat
    ac_data_source_t data_source;
    int64_t total_size;
    bool opened;
    bool closed;
    FfmpegAdapterQos* adapter_qos;
} AvIoOpaqueWithDataSource;

bool AwesomeCache_util_is_fully_cached(const char* url, const char* cache_key) {
    return ac_is_fully_cached(url, cache_key);
}

DataSourceType kwai_get_cache_datasource_type(AVDictionary* format_opts) {
    AVDictionaryEntry* cache_mode = av_dict_get(format_opts, "cache-mode", NULL, 0);
    if (cache_mode) {
        int mode_int = (int) strtod(cache_mode->value, NULL);
        switch (mode_int) {
            case kDataSourceTypeAsyncDownload:
            case kDataSourceTypeDefault:
            case kDataSourceTypeLiveNormal:
            case kDataSourceTypeLiveAdaptive:
            case kDataSourceTypeSegment:
            case kDataSourceTypeAsyncV2:
                return (DataSourceType) mode_int;
            default:
                return kDataSourceTypeDefault;
        }
    } else {
        return kDataSourceTypeDefault;
    }
}

bool AwesomeCache_util_use_custom_protocol(AVDictionary* format_opts) {
    DataSourceType type = kwai_get_cache_datasource_type(format_opts);
    switch (type) {
        case kDataSourceTypeSegment:
            return true;
        case kDataSourceTypeAsyncDownload:
        case kDataSourceTypeDefault:
        case kDataSourceTypeLiveNormal:
        case kDataSourceTypeLiveAdaptive:
        default:
            return false;
    }
}

UpstreamDataSourceType kwai_get_cache_datasource_upstream_type(AVDictionary* format_opts) {
    AVDictionaryEntry* upstream_type_entry = av_dict_get(format_opts, "cache-upstream-type", NULL,
                                                         0);
    if (upstream_type_entry) {
        UpstreamDataSourceType upstream_type = (UpstreamDataSourceType) strtod(upstream_type_entry->value, NULL);
        switch (upstream_type) {
            case kDefaultHttpDataSource:
            case kMultiDownloadHttpDataSource:
            case kFFUrlHttpDataSource:
            case kP2spHttpDataSource:
            case kCronetHttpDataSource:
                return upstream_type;
            default:
                return kDefaultHttpDataSource;
        }
    } else {
        return kDefaultHttpDataSource;
    }
}

int kwai_get_int_val_opt(AVDictionary* opts, const char* key, int def) {
    AVDictionaryEntry* sizeKbEntry = av_dict_get(opts, key, NULL, 0);
    if (sizeKbEntry) {
        return (int) strtod(sizeKbEntry->value, NULL);
    } else {
        return def;
    }
}

int kwai_get_int_val_opt_safe(AVDictionary* opts, const char* key, int def, int min, int max) {
    int ret = kwai_get_int_val_opt(opts, key, def);
    if (ret < min || ret > max) {
        return def;
    } else {
        return ret;
    }
}

/**
 * 所有参数的有效性统一在这里检测
 */
C_DataSourceOptions C_DataSourceOptions_from_options_dict(AVDictionary* options) {
    C_DataSourceOptions ds_opts = ac_default_data_source_options();

    // 写死使用BufferDataSource,避免使用已经停止维护的BufferDataSourctOpt
    ds_opts.buffered_datasource_type = kBufferedDataSource;
    ds_opts.buffered_datasource_size_kb = kwai_get_int_val_opt_safe(options,
                                                                    "buffered-datasource-size-kb",
                                                                    kDefaultBufferedDataSourceSizeKb,
                                                                    kMinBufferedDataSourceSizeKb,
                                                                    kMaxBufferedDataSourceSizeKb);
    ds_opts.seek_reopen_threshold_kb = kwai_get_int_val_opt_safe(options,
                                                                 "datasource-seek-reopen-threshold-kb",
                                                                 kDefaultSeekReopenThresholdKb,
                                                                 kMinSeekReopenThresoldSizeKb,
                                                                 kMaxSeekReopenThresoldSizeKb);

    // for CacheManager to create related DataSource
    ds_opts.type = kwai_get_cache_datasource_type(options);
    ds_opts.cache_flags = kwai_get_int_val_opt(options, "cache-flags", kFlagBlockOnCache);

    // for SyncCacheDataSource
    // 对于"progress_cb_interval_ms"，保持和变量名一样更方便维护，有没啥坏处，以后统一采用此方法
    ds_opts.progress_cb_interval_ms = kwai_get_int_val_opt_safe(options, "progress_cb_interval_ms",
                                                                kDefaultProgressIntervalMs,
                                                                kMinProgressIntervalMs,
                                                                kMaxProgressIntervalMs);

    // for AsyncCacheDataSource
    ds_opts.enable_vod_adaptive = kwai_get_int_val_opt(options,
                                                       "enable_vod_adaptive",
                                                       0);
    ds_opts.byte_range_size = kwai_get_int_val_opt(options,
                                                   "byte-range-size",
                                                   kDefaultByteRangeLength);
    ds_opts.first_byte_range_size = kwai_get_int_val_opt(options,
                                                         "first-byte-range-size",
                                                         kDefaultFirstByteRangeLength);

    // for HttpDataSource
    ds_opts.download_options.upstream_type = kwai_get_cache_datasource_upstream_type(options);
    if (ds_opts.type == kDataSourceTypeAsyncDownload) {
        ds_opts.download_options.curl_type = kCurlTypeAsyncDownload;
    } else {
        ds_opts.download_options.curl_type = kCurlTypeDefault;
    }
    ds_opts.download_options.http_connect_retry_cnt = kwai_get_int_val_opt(options,
                                                                           "cache-http-connect-retry-cnt",
                                                                           kDefaultHttpConnectRetryCount);

    // for Download Task
    ds_opts.download_options.async_enable_reuse_manager = kwai_get_int_val_opt(options,
                                                                               "async-enable-reuse-manager",
                                                                               0);
    ds_opts.download_options.priority = kPriorityHigh;

    ds_opts.download_options.connect_timeout_ms = kwai_get_int_val_opt_safe(options,
                                                                            "cache-connect-timeout-ms",
                                                                            kDefaultConnectTimeoutMs,
                                                                            kMinConnectTimeoutMs,
                                                                            kMaxConnectTimeoutMs);
    ds_opts.download_options.read_timeout_ms = kwai_get_int_val_opt_safe(options,
                                                                         "cache-read-timeout-ms",
                                                                         kDefaultReadTimeoutMs,
                                                                         kMinReadTimeoutMs,
                                                                         kMaxReadTimeoutMs);
    ds_opts.download_options.curl_buffer_size_kb = kwai_get_int_val_opt_safe(options,
                                                                             "curl-buffer-size-kb",
                                                                             kDefaultCurlBufferSizeKb,
                                                                             kMinCurlBufferSizeKb,
                                                                             kMaxCurlBufferSizeKb);
    ds_opts.download_options.socket_buf_size_kb = kwai_get_int_val_opt(options,
                                                                       "cache-socket-buf-size-kb", -1);
    ds_opts.download_options.max_speed_kbps = kwai_get_int_val_opt(options,
                                                                   "max-speed-kbps", -1);
    AVDictionaryEntry* headers = av_dict_get(options, "headers", NULL, 0);
    ds_opts.download_options.headers = headers ? av_strdup(headers->value) : NULL;
    AVDictionaryEntry* ua = av_dict_get(options, "user-agent", NULL, 0);
    ds_opts.download_options.user_agent = ua ? av_strdup(ua->value) : NULL;
    AVDictionaryEntry* product_context = av_dict_get(options, "product-context", NULL, 0);
    ds_opts.download_options.product_context = product_context ? av_strdup(product_context->value) : NULL;

    ds_opts.download_options.is_sf2020_encrypt_source = kwai_get_int_val_opt(options, "is-sf2020-encrypt-source", 0) == 1 ? true : false;
    AVDictionaryEntry* aes_key_entry = av_dict_get(options, "sf2020-aes-key", NULL, 0);
    ds_opts.download_options.sf2020_aes_key = aes_key_entry ? av_strdup(aes_key_entry->value) : NULL;
    // for vod p2sp
    ds_opts.download_options.vod_p2sp_task_max_size = kwai_get_int_val_opt(options,
                                                                           "vod-p2sp-task-max-size",
                                                                           kDefaultVodP2spTaskMaxSize);
    ds_opts.download_options.vod_p2sp_cdn_request_max_size = kwai_get_int_val_opt(options,
                                                                                  "vod-p2sp-cdn-request-max-size",
                                                                                  kDefaultVodP2spCdnRequestMaxSize);
    ds_opts.download_options.vod_p2sp_cdn_request_initial_size = kwai_get_int_val_opt(options,
                                                                 "vod-p2sp-cdn-request-initial-size",
                                                                 kDefaultVodP2spCdnRequestInitialSize);
    ds_opts.download_options.vod_p2sp_on_threshold = kwai_get_int_val_opt(options,
                                                                          "vod-p2sp-on-threshold",
                                                                          kDefaultVodP2spOnThreshold);
    ds_opts.download_options.vod_p2sp_off_threshold = kwai_get_int_val_opt(options,
                                                                           "vod-p2sp-off-threshold",
                                                                           kDefaultVodP2spOffThreshold);
    ds_opts.download_options.vod_p2sp_task_timeout = kwai_get_int_val_opt(options,
                                                                          "vod-p2sp-task-timeout",
                                                                          kDefaultVodP2spTaskTimeout);
    AVDictionaryEntry* network_id = av_dict_get(options, "network-id", NULL, 0);
    ds_opts.download_options.network_id = network_id ? av_strdup(network_id->value) : NULL;
    ds_opts.download_options.tcp_keepalive_idle = kwai_get_int_val_opt(options, "tcp-keepalive-idle", 0);
    ds_opts.download_options.tcp_keepalive_interval = kwai_get_int_val_opt(options, "tcp-keepalive-interval", 0);
    ds_opts.download_options.tcp_connection_reuse = kwai_get_int_val_opt(options, "tcp-connection-reuse", 0);
    ds_opts.download_options.tcp_connection_reuse_maxage = kwai_get_int_val_opt(options, "tcp-connection-reuse-maxage", 0);

    ds_opts.download_options.live_p2sp_switch_on_buffer_threshold_ms =
        kwai_get_int_val_opt(options, "live-p2sp-switch-on-buffer-threshold-ms", kDefaultLiveP2spSwitchOnBufferThresholdMs);
    ds_opts.download_options.live_p2sp_switch_on_buffer_hold_threshold_ms =
        kwai_get_int_val_opt(options, "live-p2sp-switch-on-buffer-hold-threshold-ms", kDefaultLiveP2spSwitchOnBufferHoldThresholdMs);
    ds_opts.download_options.live_p2sp_switch_off_buffer_threshold_ms =
        kwai_get_int_val_opt(options, "live-p2sp-switch-off-buffer-threshold-ms", kDefaultLiveP2spSwitchOffBufferThresholdMs);
    ds_opts.download_options.live_p2sp_switch_lag_threshold_ms =
        kwai_get_int_val_opt(options, "live-p2sp-switch-lag-threshold-ms", kDefaultLiveP2spSwitchLagThresholdMs);
    ds_opts.download_options.live_p2sp_switch_max_count =
        kwai_get_int_val_opt(options, "live-p2sp-switch-max-count", kDefaultLiveP2spSwitchMaxCount);
    ds_opts.download_options.live_p2sp_switch_cooldown_ms =
        kwai_get_int_val_opt(options, "live-p2sp-switch-cooldown-ms", kDefaultLiveP2spSwitchCooldownMs);

    memset(ds_opts.datasource_extra_msg, 0, DATASOURCE_OPTION_EXTRA_BUFFER_LEN);


    return ds_opts;
}

void C_DataSourceOptions_release(C_DataSourceOptions* option) {
    if (!option) {
        return;
    }
    if (option->download_options.headers) {
        av_freep(&option->download_options.headers);
    }
    if (option->download_options.user_agent) {
        av_freep(&option->download_options.user_agent);
    }
    if (option->download_options.network_id) {
        av_freep(&option->download_options.network_id);
    }
    if (option->download_options.product_context) {
        av_freep(&option->download_options.product_context);
    }
    if (option->download_options.sf2020_aes_key) {
        av_freep(&option->download_options.sf2020_aes_key);
    }
}

int AvIoOpaqueWithDataSource_interrupt_cb(void* opaque) {
    AvIoOpaqueWithDataSource* avIoOpaqueWithDataSource = opaque;
    return avIoOpaqueWithDataSource->interrupt_callback.interrupted;
}

static AvIoOpaqueWithDataSource* AvIoOpaqueWithDataSource_create(AVDictionary** options,
                                                                 unsigned ffplayer_id,
                                                                 const char* session_id,
                                                                 const CCacheSessionListener* listener,
                                                                 const AwesomeCacheCallback_Opaque cache_callback,
                                                                 AwesomeCacheRuntimeInfo* ac_rt_info,
                                                                 ac_player_statistic_t player_statistic) {
    // create AvIoOpaqueWithDataSource and ac_data_source_t
    AvIoOpaqueWithDataSource* avio_opaque = (AvIoOpaqueWithDataSource*)av_mallocz(
                                                sizeof(AvIoOpaqueWithDataSource));
    if (!avio_opaque) {
        return NULL;
    }
    avio_opaque->context_id = ffplayer_id;

    // init avio_opaque other params
    avio_opaque->position = 0;
    avio_opaque->closed = false;
    avio_opaque->interrupt_callback.interrupted = false;

    C_DataSourceOptions ds_opts = C_DataSourceOptions_from_options_dict(*options);

    snprintf(ds_opts.download_options.session_uuid, SESSION_UUID_BUFFER_LEN, "%s", session_id);

    ds_opts.download_options.interrupt_cb.opaque = avio_opaque;
    ds_opts.download_options.interrupt_cb.callback = AvIoOpaqueWithDataSource_interrupt_cb;

    ds_opts.context_id = ffplayer_id;
    ds_opts.download_options.context_id = ffplayer_id;
    ds_opts.download_options.player_statistic = player_statistic;

    ac_data_source_t data_source = ac_data_source_create(ds_opts, listener, cache_callback, ac_rt_info);
    C_DataSourceOptions_release(&ds_opts);
    if (!data_source) {
        ALOGW("[%d][avformat_open_input_using_cache] kResultCacheDataSourceCreateFail", avio_opaque->context_id);
        av_freep(&avio_opaque);
        return NULL;
    } else {
        avio_opaque->data_source = data_source;
    }

    return avio_opaque;
}

void AvIoOpaqueWithDataSource_releasep(AvIoOpaqueWithDataSource** pp) {
    if (!pp || !*pp) {
        return;
    }

    ac_data_source_releasep(&(*pp)->data_source);

    av_freep(&(*pp)->url);
    av_freep(&(*pp)->cache_key);
    av_freep(pp);
}

void AvIoOpaqueWithDataSource_abort(AvIoOpaqueWithDataSource* opaque) {
    opaque->interrupt_callback.interrupted = true;
}

int64_t AvIoOpaqueWithDataSource_open(AvIoOpaqueWithDataSource* opaque, const char* uri,
                                      const char* cache_key) {
    FfmpegAdapterQos_init(opaque->adapter_qos);

    int64_t ret = ac_data_source_open(opaque->data_source, uri, cache_key, 0, kLengthUnset, true);
    ALOGI("[%d] [%s] position:%lld, ret:%lld, cache_key:%s \n",
          opaque->context_id, __func__, opaque->position, ret, cache_key ? "null" : cache_key);
    if (ret > 0) {
        opaque->total_size = ret;
        opaque->opened = true;
        opaque->url = av_strdup(uri);
        opaque->cache_key = av_strdup(cache_key);
        opaque->closed = false;
        opaque->adapter_qos->adapter_error = 0;
    } else if (ret == 0) {
        opaque->adapter_qos->adapter_error = kResultAdapterOpenNoData;
    } else {
        opaque->adapter_qos->adapter_error = (int32_t) ret;
    }
    return ret;
}

void AvIoOpaqueWithDataSource_close(AvIoOpaqueWithDataSource* opaque, bool need_report) {
    if (VERBOSE) {
        ALOGI("[%s] ", __func__);
    }
    if (!opaque->closed) {
        ac_data_source_close(opaque->data_source, need_report);
        opaque->closed = true;
    }
}

int64_t AvIoOpaqueWithDataSource_reopen(AvIoOpaqueWithDataSource* opaque, bool need_report) {
    if (VERBOSE) {
        ALOGE("[%s] ", __func__);
    }
    if (!opaque->opened) {
        ALOGE("[%s] error:kResultAdapterDataSourceNeverOpened \n", __func__);
        return kResultAdapterDataSourceNeverOpened;
    }

    int64_t ret = ac_data_source_open(opaque->data_source, opaque->url,
                                      opaque->cache_key, opaque->position, kLengthUnset, need_report);
    ALOGI("[%s] from position:%lld ret:%d, position+ret:%lld, totalsize:%lld\n",
          __func__, opaque->position, ret, opaque->position + ret, opaque->total_size);
    if (ret > 0) {
        opaque->closed = false;
    } else if (ret == 0) {
        opaque->adapter_qos->adapter_error = kResultAdapterReOpenNoData;
    } else {
        opaque->adapter_qos->adapter_error = (int)ret;
    }
    return ret;
}

bool AvIoOpaqueWithDataSource_is_read_compelete(AvIoOpaqueWithDataSource* opaque) {
    return opaque->position >= opaque->total_size;
}

int AvIoOpaqueWithDataSource_read(void* opaque, uint8_t* buf, int buf_size) {
    AvIoOpaqueWithDataSource* handle = (AvIoOpaqueWithDataSource*) opaque;

    // used when there is error when reading.
    int64_t start = av_gettime_relative();
    int64_t ret_or_len = ac_data_source_read(handle->data_source, buf, 0, buf_size);

    // 此日志保留
    if (VERBOSE) {
        ALOGD("[%s], current position:%lld,  to read buf_size:%d, ret_or_len:%d \n",
              __func__, handle->position, buf_size, ret_or_len);
    }

    int64_t end = av_gettime_relative();
    handle->adapter_qos->read_cost_ms += (end - start) / 1000;
    if (ret_or_len > 0) {
        handle->adapter_qos->total_read_bytes += ret_or_len;
    }
    if (ret_or_len == 0) {
        handle->adapter_qos->adapter_error = kResultAdapterReadNoData;
        ALOGE("[%s], ret_or_len == 0, return kResultAdapterReadNoData", __func__);
        return AVERROR_EXIT;
    } else if (ret_or_len < 0) {
        if (ret_or_len == kResultEndOfInput) {
            if (handle->position == handle->total_size) {
                handle->adapter_qos->adapter_error = kResultEndOfInputAlreadyReadAllData;
                ALOGE("[%s], ret_or_len = kResultEndOfInputAlreadyReadAllData, return AVERROR_EOF,"
                      ", handle->position:%lld, handle->total_size:%lld "
                      "\n", __func__, handle->position, handle->total_size);
            } else {
                handle->adapter_qos->adapter_error = (int32_t) ret_or_len;
                ALOGE("[%s], ret_or_len = kResultEndOfInput, return AVERROR_EOF \n", __func__);
            }
            return AVERROR_EOF;
        } else if (ret_or_len == kResultLiveNoData) {
            ALOGE("[%s], ret_or_len = kResultLiveNoData, return 0", __func__, ret_or_len);
            return 0;
        } else {
            handle->adapter_qos->adapter_error = (int32_t) ret_or_len;
            ALOGE("[%s], ret_or_len = %d, return AVERROR_EXIT", __func__, ret_or_len);
            return AVERROR_EXIT;
        }
    } else {
        handle->position += ret_or_len;
        return (int) ret_or_len;
    }

    ALOGE("[%s], logic error, not supposed to be here, return AVERROR_EXIT", __func__);
    return kResultFFmpegAdapterInnerError;
}

/**
 * @param whence is oring flag of AVSEEK_xxx(like AVSEEK_SIZE);
 * @return new position or AVERROR.
 */
int64_t AvIoOpaqueWithDataSource_seek(void* opaque, int64_t offset, int32_t whence) {
    // ALOGD("[AvIoOpaqueWithDataSource_seek], offset:%lld", offset);
    AvIoOpaqueWithDataSource* handle = (AvIoOpaqueWithDataSource*) opaque;
    int64_t pos = 0;
    if (whence == AVSEEK_SIZE) {
        int64_t ret = handle->total_size;
        handle->adapter_qos->seek_size_cnt++;
        if (ret < 0) {
            ALOGE("%s, whence:AVSEEK_SIZE,return -1, coz totalsize is:%d\n", __func__, ret);
            return -1;
        } else {
            ALOGW("AvIoOpaqueWithDataSource_seek, offset:%lld, return ret(handle->total_size):%lld", offset, ret);
            return ret;
        }
    } else if (whence == SEEK_SET) {
        handle->adapter_qos->seek_set_cnt++;
        pos = offset;
    } else if (whence == SEEK_CUR) {
        handle->adapter_qos->seek_cur_cnt++;
        pos = handle->position + offset;
        if (pos > handle->total_size) {
            pos = handle->total_size;
        }
    } else if (whence == SEEK_END) {
        handle->adapter_qos->seek_end_cnt++;
        pos = handle->total_size + offset;
    }
    if (handle->total_size > 0) {
        if (pos > handle->total_size) {
            handle->adapter_qos->adapter_error = kResultAdapterSeekOutOfRange;
            ALOGE("[%s], pos（%lld) > handle->total_size(%lld), return AVERROR_EOF \n",
                  __func__, pos, handle->total_size);
            return AVERROR_EOF;
        } else if (pos == handle->total_size) {
            // 有的视频会seek到最后一个字节，后续也不会做什么，这个直接返回当前位置即可兼容
            // ALOGD("AvIoOpaqueWithDataSource_seek, offset:%lld, return handle->position:%lld", offset, handle->position);
            return handle->position;
        } else {
            // continue to do seek
        }
    }

    int64_t ret = ac_data_source_seek(handle->data_source, pos);
    // 此日志保留
    if (VERBOSE) {   // 调试 -20001 错误，暂时打开
        ALOGI("[%d] [%s], current handle->position:%lld , target pos:%lld, ret:%lld, total_size:%lld whence:%d <<< \n",
              handle->context_id, __func__, handle->position, pos, ret, handle->total_size, whence);
    }

    if (ret < 0) {
        ALOGE("[%d] %s, ac_data_source_seek %lld\n", handle->context_id, __func__, ret);
        handle->adapter_qos->adapter_error = (int) ret;
        return AVERROR_EXIT;
    } else {
        handle->position = pos;
    }

    if (VERBOSE) {
        ALOGW("AvIoOpaqueWithDataSource_seek, offset:%lld, return pos:%lld", offset, pos);
    }
    return pos;
}

bool AwesomeCache_util_is_globally_enabled() {
    return ac_global_enabled();
}

bool AwesomeCache_util_is_url_white_list(const char* url) {
    if (0 != av_strstart(url, SCHEME_FILE_HEADER, NULL)) {
        // 已经是本地文件，直接走原生接口
        return false;
    } else if (0 != av_strstart(url, "/", NULL)) {
        // 已经是本地文件，直接走原生接口
        return false;
    } else if ((0 != av_strstart(url, SCHEME_HTTP_HEADER, NULL)) ||
               (0 != av_strstart(url, SCHEME_HTTPS_HEADER, NULL))) {
        return true;
    } else {
        // kwai_avformat_open_input 无法支持的格式
        return false;
    }
}


int64_t AwesomeCache_AVIOContext_open(AVIOContext* avio, const char* uri, const char* cache_key) {
    assert(avio->opaque);
    return AvIoOpaqueWithDataSource_open(avio->opaque, uri, cache_key);
}

int64_t AwesomeCache_AVIOContext_reopen(AVIOContext* avio) {
    assert(avio->opaque);
    return AvIoOpaqueWithDataSource_reopen(avio->opaque, true);
}

void AwesomeCache_AVIOContext_close(AVIOContext* avio) {
    if (avio) {
        if (avio->opaque) {
            AvIoOpaqueWithDataSource_close(avio->opaque, true);
        } else {
            ALOGE("[%s], avio->opaque is NULL! \n", __func__);
        }
    }
}

void AwesomeCache_AVIOContext_abort(AVIOContext* avio) {
    if (avio) {
        if (avio->opaque) {
            AvIoOpaqueWithDataSource_abort(avio->opaque);
        }
    }
}


int AwesomeCache_AVIOContext_create(AVIOContext** result,
                                    AVDictionary** options,
                                    unsigned ffplayer_id,
                                    const char* session_uuid,
                                    const CCacheSessionListener* listener,
                                    const AwesomeCacheCallback_Opaque cache_callback,
                                    CacheStatistic* cache_stat,
                                    ac_player_statistic_t player_statistic) {
    assert(cache_stat);
    // create AvIoOpaqueWithDataSource and ac_data_source_t
    AvIoOpaqueWithDataSource* avio_opaque_with_datasource =
        AvIoOpaqueWithDataSource_create(options,
                                        ffplayer_id,
                                        session_uuid,
                                        listener,
                                        cache_callback,
                                        &cache_stat->ac_runtime_info,
                                        player_statistic);
    if (!avio_opaque_with_datasource) {
        return kResultCacheNoMemory;
    }
    avio_opaque_with_datasource->adapter_qos = &cache_stat->ffmpeg_adapter_qos;

    unsigned char* aviobuffer = (unsigned char*) av_malloc(FILE_READ_BUFFER_MAX_LEN);
    AVIOContext* avio = avio_alloc_context(aviobuffer, FILE_READ_BUFFER_MAX_LEN, 0,
                                           avio_opaque_with_datasource,
                                           AvIoOpaqueWithDataSource_read,
                                           NULL,
                                           ac_data_source_seekable(avio_opaque_with_datasource->data_source) ?
                                           AvIoOpaqueWithDataSource_seek :
                                           NULL);

    if (!avio) {
        AvIoOpaqueWithDataSource_releasep(&avio_opaque_with_datasource);
        return kResultCacheNoMemory;
    }
    *result = avio;

    return 0;
}

void AwesomeCache_AVIOContext_releasep(AVIOContext** pp_avio) {
    if (!pp_avio || !*pp_avio) {
        return;
    }
    AVIOContext* avio = *pp_avio;

    // do release work
    AvIoOpaqueWithDataSource_releasep((AvIoOpaqueWithDataSource**) &avio->opaque);
    // this will release the buffer[FILE_READ_BUFFER_MAX_LEN] allocated in AwesomeCache_AVIOContext_create
    avio_closep(pp_avio);
}


char* AwesomeCache_AVIOContext_get_DataSource_StatsJsonString(AVIOContext* avio) {
    char* ret = NULL;

    AvIoOpaqueWithDataSource* av_io_datasource_opaque = avio->opaque;
    if (av_io_datasource_opaque) {
        ret = ac_data_source_get_stats_json_string(av_io_datasource_opaque->data_source);
    }

    return ret;
}
