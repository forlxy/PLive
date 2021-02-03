#include <include/awesome_cache_runtime_info_c.h>
#include "async_cache_data_source.h"
#include "cache_util.h"
#include "constant.h"
#include "default_http_data_source.h"
#include "ac_log.h"
#include "abr/abr_engine.h"
#include "abr/abr_types.h"
#include "ac_utils.h"

#include "v2/cache/cache_content_index_v2.h"

namespace kuaishou {
namespace cache {

#define RETURN_ON_ERROR(error) if (error < kResultOK) {return error;}

namespace {
static const uint32_t kProgressSpanUpdateThreshold = 2 * 1024 * 1024; // 2M
}
namespace internal {


void AsyncCacheDataSourceStageStats::FillJson() {
    StageStats::FillJson();
    stats_["uri"] = uri;
    stats_["pos"] = pos;
    stats_["bytes_total"] = bytes_total;
    stats_["error"] = error;
    stats_["write_thread"] = write_data_source_stats_;
    stats_["read_thread"] = read_data_source_stats_;
}

void AsyncCacheDataSourceStageStats::AppendWriteThreadFileDataSourceStats(json stat) {
    write_data_source_stats_["sink"].push_back(stat);
}

void AsyncCacheDataSourceStageStats::AppendWriteThreadHttpDataSourceStats(json stat) {
    write_data_source_stats_["upstream"].push_back(stat);
}

void AsyncCacheDataSourceStageStats::AppendReadDataSourceStats(json stat) {
    read_data_source_stats_.push_back(stat);
}

} // internal

AsyncCacheDataSource::AsyncCacheDataSource(Cache* cache,
                                           std::shared_ptr<HttpDataSource> upstream,
                                           std::shared_ptr<DataSource> file_datasource,
                                           const DataSourceOpts& opts,
                                           AsyncCacheDataSource::EventListener* listener,
                                           std::shared_ptr<CacheSessionListener> session_listener,
                                           AwesomeCacheRuntimeInfo* ac_rt_info) :
    cache_(cache),
    cache_session_listener_(session_listener),
    cache_callback_(static_cast<AwesomeCacheCallback*>(opts.cache_callback)),
    product_context_(opts.download_opts.product_context),
    upstream_data_source_(upstream),
    cache_read_data_source_(file_datasource),
    cache_write_data_source_(nullptr),
    byte_range_length_(opts.byte_range_size),
    first_byte_range_length_(opts.first_byte_range_size),
    terminate_download_thread_(false),
    download_thread_exit_(false),
    directly_read_from_upstream_(false),
    last_cached_bytes_read_(0),
    write_position_(0),
    read_position_(0),
    download_thread_is_waiting_(false),
    first_init_source_(true),
    current_request_unbounded_(false),
    data_source_extra_(opts.download_opts.datasource_extra_msg),
    ignore_cache_on_error_((opts.cache_flags & kFlagIgnoreCacheOnError) != 0),
    seen_cache_error_(false),
    ignore_cache_for_unset_length_requests_((opts.cache_flags & kFlagIgnoreCacheForUnsetLengthRequest) != 0),
    event_listener_(listener),
    stats_(new SessionStats<internal::AsyncCacheDataSourceStageStats>("AsyncCacheDataSource")),
    // CacheDataSource part
    total_bytes_(0),
    error_code_(0),
    downloaded_bytes_(0),
    session_open_ts_ms_(0),
    session_close_ts_ms_(0),
    network_cost_ms_(0),
    session_opened_(false),
    session_close_reported_(false),
    should_report_progress_(false),
    download_stop_need_to_report_(false),
    is_span_error_(false),
    abort_(false),
    key_(""),
    enable_vod_adaptive_(opts.enable_vod_adaptive),
    ac_rt_info_(ac_rt_info),
    progress_cb_interval_ms_(opts.download_opts.progress_cb_interval_ms),
    runloop_(new kpbase::Runloop("AsyncCacheDataSourceRunLoop")) {
    qos_ = {0};
    bytes_remaining_ = kLengthUnset;
    is_reading_from_cache_ = false;
    callbackInfo_ = nullptr;
    AwesomeCacheRuntimeInfo_cache_ds_init(ac_rt_info_);
    SetContextId(opts.context_id);
    ac_rt_info_->cache_ds.byte_range_size = byte_range_length_;
    ac_rt_info_->cache_ds.first_byte_range_length = first_byte_range_length_;
    ac_rt_info_->cache_ds.download_exit_reason = DownloadOK;
    LOG_DEBUG("[%d][AsyncCacheDataSource] byte_range_length_:%d, first_byte_range_length_: %d\n",
              GetContextId(), byte_range_length_, first_byte_range_length_);
    LOG_DEBUG("[%d][AsyncCacheDataSource] progress_cb_interval_ms_:%dms\n",
              GetContextId(), progress_cb_interval_ms_);
    if (enable_vod_adaptive_) {
        ac_rt_info_->vod_adaptive.real_time_throughput_kbps = kuaishou::abr::AbrEngine::GetInstance()->get_real_time_throughput();
    }
}

AsyncCacheDataSource::~AsyncCacheDataSource() {
    runloop_->Stop();
}

int64_t AsyncCacheDataSource::Open(const DataSpec& spec) {
    int64_t read_position = 0;
    int64_t download_remaining = 0;
    bool is_need_wait = false;
    bool is_need_download = true;
    if (""  == spec.uri) {
        return kResultSpecExceptionUriUnset;
    }

    abort_ = false;
    terminate_download_thread_ = false;
    write_position_ = 0;
    download_thread_exit_ = false;
    download_thread_is_waiting_ = false;
    is_reading_from_cache_ = false;

    transfer_consume_ms_  = 0;
    error_code_ = 0;
    last_progress_time_ms_ = 0;
    last_progress_pos_ = 0;
    directly_read_from_upstream_ = false;
    is_span_error_ = false;
    total_bytes_ = 0;
    bytes_remaining_ = kLengthUnset;

    uri_ = spec.uri;
    flag_ = spec.flags;
    {
        std::lock_guard<std::mutex> lock(key_lock_);
        key_ = CacheUtil::GetKey(spec);
    }

    if (enable_vod_adaptive_) {
        LOG_DEBUG("[%d][AsyncCacheDataSource] uri:%s, key:%s\n",  GetContextId(), uri_.c_str(), key_.c_str());
    }

    read_position = read_position_ = spec.position;
    current_request_ignore_cache_ = (ignore_cache_on_error_ && seen_cache_error_)
                                    || (spec.length == kLengthUnset && ignore_cache_for_unset_length_requests_);
    ac_rt_info_->cache_ds.ignore_cache_on_error = current_request_ignore_cache_;
    LOG_INFO("[%d][AsyncCacheDataSource] spec.position: %lld, spec.length: %lld, current_request_ignore_cache_: %d\n",
             GetContextId(), spec.position, spec.length, current_request_ignore_cache_);

    AcResultType error = kResultOK;
    error = cache_->RemoveStaleSpans(key_);
    if (error < kResultOK) {
        LOG_INFO("[%d][AsyncCacheDataSource] cache_->RemoveStaleSpans failed. error: %d\n",
                 GetContextId(), error);
        ac_rt_info_->cache_ds.download_exit_reason = CacheRemoveStaleSpansFailed;
        return error;
    }

    if (spec.length != kLengthUnset || current_request_ignore_cache_) {
        total_bytes_ = bytes_remaining_ = spec.length;
    } else {
        total_bytes_ = bytes_remaining_ = cache_->GetContentLength(key_);
        if (bytes_remaining_ != kLengthUnset) {
            bytes_remaining_ -= spec.position;
            LOG_INFO("[%d][AsyncCacheDataSource] spec.position: %lld bytes_remaining_: %lld, total_bytes_: %lld\n",
                     GetContextId(), spec.position, bytes_remaining_, total_bytes_);
            if (bytes_remaining_ <= 0) {
                return kResultExceptionDataSourcePositionOutOfRange;
            }

            int64_t cached_bytes = cache_->GetContentCachedBytes(key_);
            if (cached_bytes < total_bytes_) {
                download_remaining = bytes_remaining_;
                read_position = FindDownloadReadPosition(read_position, &download_remaining);
                LOG_INFO("[%d][AsyncCacheDataSource] FindDownloadReadPosition read_position: %lld download_remaining: %lld\n",
                         GetContextId(), read_position, download_remaining);

                cached_bytes = cache_->GetContentCachedBytes(key_);
                if (cached_bytes < 0) {
                    // cache的目录可能已经被破坏了，需要重新下载。
                    LOG_INFO("[%d][AsyncCacheDataSource] cache dir failed, bytes_remaining_: %lld",
                             GetContextId(), bytes_remaining_);
                    download_remaining = total_bytes_ = bytes_remaining_ = kLengthUnset;
                    read_position = spec.position;
                    is_need_wait = true;
                    ac_rt_info_->cache_ds.download_exit_reason = DownloadCacheFileDirError;
                }
            } else {
                LOG_INFO("[%d][AsyncCacheDataSource] all cached cached_bytes: %lld, total_bytes_: %lld\n",
                         GetContextId(), cached_bytes, total_bytes_);
                is_need_download = false;
                download_thread_exit_ = true;
                terminate_download_thread_ = true;
                UpdateDataSourceQos();
            }


        } else {
            download_remaining = kLengthUnset;
            is_need_wait = true;
        }
    }

    ReportSessionOpened(spec.position, cache_->GetContentCachedBytes(key_));

    if (is_need_download) {
        download_thread_ = std::thread(&AsyncCacheDataSource::DownloadThread,
                                       this, read_position, download_remaining);

        if (is_need_wait) {
            tytes_remaining_event_.Wait();
        }
        if (bytes_remaining_ == kLengthUnset &&
            (is_span_error_
             || ac_rt_info_->cache_ds.download_exit_reason == DownloadByteRangeContentInvalid
             || ac_rt_info_->cache_ds.download_exit_reason == DownloadCacheError
             || ac_rt_info_->cache_ds.download_exit_reason == DownloadCacheStartRWFailed)) {
            if (is_span_error_) {
                LOG_INFO("[%d][AsyncCacheDataSource] get content len failed because of span locked\n",  GetContextId());
            }
            if (ac_rt_info_->cache_ds.download_exit_reason == DownloadByteRangeContentInvalid) {
                LOG_INFO("[%d][AsyncCacheDataSource] get content len failed because of DownloadByteRangeContentInvalid\n",  GetContextId());
            }
            if (ac_rt_info_->cache_ds.download_exit_reason == DownloadCacheStartRWFailed) {
                LOG_INFO("[%d][AsyncCacheDataSource] create cache dir failed\n",  GetContextId());
            }
            total_bytes_ = bytes_remaining_ = OpenHttpDataSource(bytes_remaining_);
            LOG_DEBUG("[%d][AsyncCacheDataSource] OpenHttpDataSource read_position_: %lld, bytes_remaining_: %lld\n",
                      GetContextId(), read_position_, bytes_remaining_);
        }

        if (bytes_remaining_ < 0) {
            return error_code_;
        }
    }

    return bytes_remaining_;
}

int64_t AsyncCacheDataSource::FindDownloadReadPosition(int64_t read_position, int64_t* download_remaining) {
    while (!terminate_download_thread_) {
        if (*download_remaining == 0) {
            LOG_INFO("[%d][AsyncCacheDataSource] bytes_remaining == 0\n",  GetContextId());
            break; // async cache finised
        }

        AcResultType error = kResultOK;
        std::shared_ptr<CacheSpan> span = nullptr;
        span = cache_->StartRead(key_, read_position, error);
        if (error != kResultOK) {
            LOG_ERROR_DETAIL("[%d][AsyncCacheDataSource] cache->StartReadWriteNonBlocking error %d\n",
                             GetContextId(), error);
            error_code_ = error;
            break;
        }

        if (span == nullptr || span->is_locked) {
            // write span was hold by another write thread, will take place
            // in case: the same url "double or more" streaming at the same time
            LOG_ERROR_DETAIL("[%d][AsyncCacheDataSource] span is nullptr, read_position: %lld\n",
                             GetContextId(), read_position);
            break;
        } else if (span->is_cached) {
            int64_t file_position = 0;
            int64_t length = 0;

            LOG_ERROR("[%d][AsyncCacheDataSource] span position: %lld, length: %lld\n",
                      GetContextId(), span->position, span->length);
            if (read_position >= span->position) {
                file_position = read_position - span->position;
                length = span->length - file_position;
            } else {
                ac_rt_info_->cache_ds.download_exit_reason = DownloadPosNotCache;
                LOG_ERROR("[%d][AsyncCacheDataSource] never cached, span position: %lld, read_position: %lld, download_remaining: %lld\n",
                          GetContextId(), span->position, read_position, *download_remaining);
                break;
            }

            read_position += length;
            *download_remaining -= length;
            continue;
        } else {
            LOG_ERROR_DETAIL("[%d][AsyncCacheDataSource] don't cache. read_position: %lld, download_remaining: %lld\n",
                             GetContextId(), read_position, *download_remaining);
            break;
        }
    }

    return read_position;
}

int64_t AsyncCacheDataSource::OpenHttpDataSource(int64_t len) {
    int64_t result = 0;
    if (!directly_read_from_upstream_) {
        //read from upstream and don't cache
        LOG_DEBUG("[%d][AsyncCacheDataSource] directly read data  from upstrem read_position_: %lld, len: %lld\n",
                  GetContextId(), read_position_, len);
        DataSpec data_spec;
        data_spec.WithUri(uri_).WithPosition(read_position_).WithKey(key_).WithFlags(flag_).WithLength(kLengthUnset);
        snprintf(ac_rt_info_->cache_ds.current_read_uri, DATA_SOURCE_URI_MAX_LEN, "%s", uri_.c_str());
        ac_rt_info_->cache_ds.read_from_upstream += 1;
        ac_rt_info_->cache_ds.read_position = read_position_;
        ac_rt_info_->cache_ds.bytes_remaining = len;

        result = upstream_data_source_->Open(data_spec);
        current_upstream_data_spec_ = data_spec;
        OnDataSourceOpened(result, read_position_);
        if (result < 0) {
            LOG_DEBUG("[%d][AsyncCacheDataSource] open failed result:%lld, read_position_: %lld, bytes_remaining_: %lld\n",
                      GetContextId(), result, read_position_, len);
            error_code_ = (int32_t)result;
            return result;
        }
        directly_read_from_upstream_ = true;
    }
    return result;
}

int64_t AsyncCacheDataSource::GetContentLength(int64_t read_position) {
    int64_t result = 0;
    DataSpec data_spec;

    data_spec.WithUri(uri_).WithPosition(read_position)
    .WithLength(kLengthUnset).WithKey(key_).WithFlags(flag_);

    result = upstream_data_source_->Open(data_spec);
    current_upstream_data_spec_ = data_spec;
    if (result < 0) {
        LOG_ERROR_DETAIL("[%d][AsyncCacheDataSource] Open fails with error %d",  GetContextId(), result);
        upstream_data_source_->Close();
        total_bytes_ = bytes_remaining_ = kLengthUnset;
        error_code_ = (int32_t)result;
        return result;
    }

    total_bytes_ = bytes_remaining_ = result;
    cache_->SetContentLength(key_, read_position + bytes_remaining_);
    upstream_data_source_->Close();

    return result;
}

int AsyncCacheDataSource::setContentLength(int64_t read_position) {
    HasConnectionInfo* hasConnectionInfo = dynamic_cast<HasConnectionInfo*>(upstream_data_source_.get());
    if (!hasConnectionInfo) {
        LOG_ERROR_DETAIL("[%d][AsyncCacheDataSource] setContentLength fail to get file length",  GetContextId());
        return kLengthUnset;
    }

    const ConnectionInfo info(hasConnectionInfo->GetConnectionInfo());
    if (info.file_length == kLengthUnset) {
        LOG_ERROR_DETAIL("[%d][AsyncCacheDataSource] can't get file len",  GetContextId());
        return kLengthUnset;
    }
    total_bytes_ = bytes_remaining_ = info.file_length;

    if (read_position > 0) {
        // 文件并不是从最开头开始下载的
        bytes_remaining_ -= read_position;
        ac_rt_info_->cache_ds.download_exit_reason = DownloadPosNotZero;
    }
    // 虽然文件不一定是从最开始下载的，但是文件长度还是应该应该存储真实的文件长度
    int32_t result = cache_->SetContentLength(key_, info.file_length);
    LOG_DEBUG("[%d][AsyncCacheDataSource] setContentLength read_position: %lld bytes_remaining_: %lld, result: %d",
              GetContextId(), read_position, bytes_remaining_, result);
    return result;
}

void AsyncCacheDataSource::HandleDownloadError(DownloadStopReason download_error) {
    LOG_ERROR_DETAIL("[%d][AsyncCacheDataSource]: Download Error %d\n",  GetContextId(), download_error);
    upstream_data_source_->Close();
    OnDataSourceCancel();
}

void AsyncCacheDataSource::DownloadThread(int64_t read_position, int64_t download_remaining) {
    std::shared_ptr<CacheSpan> span = nullptr;
    AcResultType error = kResultOK;
    uint8_t* buf = nullptr;
    bool first_byte_range = true;
    int32_t result = 0;
    bool is_tytes_remaining_event_post = false;
    bool is_cached = false;

    int64_t read_position_diff = read_position - read_position_;
    if (read_position_diff > byte_range_length_) {
        // cache中已经缓存了数据，在这种情况下，不要马上下载数据。
        // 等到需要的时候，再下载。
        LOG_ERROR_DETAIL("[%d][AsyncCacheDataSource] diff: %lld, dw_r_position: %lld, read_position_: %lld\n",
                         GetContextId(), read_position_diff, read_position, read_position_);
        first_byte_range = false;
        UpdateDataSourceQos();
    }

    while (!terminate_download_thread_) {
        AcUtils::SetThreadName("AsyncCacheDownLoad");

        write_position_ = read_position;
        if (download_remaining == 0 ||
            (download_remaining != kLengthUnset && download_remaining < 0)) {
            if (download_remaining != kLengthUnset && download_remaining < 0) {
                LOG_ERROR_DETAIL("[%d][AsyncCacheDataSource] bytes_remaining < 0\n", GetContextId());
            }
            break; // async cache finised
        }

        int32_t real_byte_range_len = byte_range_length_;
        if (!first_byte_range) {
            LOG_DEBUG("[%d][AsyncCacheDataSource] download_pause_event_ read_position: %lld, download_remaining: %lld\n",
                      GetContextId(), read_position, download_remaining);
            real_byte_range_len = byte_range_length_;
            if (!is_cached) {
                download_thread_is_waiting_ = true;
                download_pause_event_.Wait();
                download_thread_is_waiting_ = false;
                if (terminate_download_thread_) {
                    break;
                }
            }
            is_cached = false;
        } else {
            first_byte_range = false;
            real_byte_range_len = first_byte_range_length_;    // default 3M
        }

        span = cache_->StartReadWriteNonBlocking(key_, read_position, error);
        if (error != kResultOK) {
            LOG_ERROR_DETAIL("[%d][AsyncCacheDataSource] cache->StartReadWriteNonBlocking error %d\n",
                             GetContextId(), error);
            error_code_ = error;
            ac_rt_info_->cache_ds.download_exit_reason = DownloadCacheStartRWFailed;
            break;
        }

        if (span == nullptr || span->is_locked) {
            // write span was hold by another write thread, will take place
            // in case: the same url "double or more" streaming at the same time
            is_span_error_ = true;
            if (span == nullptr) {
                LOG_ERROR_DETAIL("[%d][AsyncCacheDataSource] span is nullptr, break Write Thread(%d)\n",
                                 GetContextId(), std::this_thread::get_id());
                ac_rt_info_->cache_ds.download_exit_reason = DownloadSpanNull;
            }
            if (span->is_locked) {
                LOG_ERROR_DETAIL("[%d][AsyncCacheDataSource] span is locked, break Write Thread(%d)\n",
                                 GetContextId(), std::this_thread::get_id());
                ac_rt_info_->cache_ds.download_exit_reason = DownloadSpanLocked;
                // 该span被另外一个播放器占用，不需要调用ReleaseHoleSpan
                span = nullptr;
            }
            break;
        } else if (span->is_cached) {
            int64_t file_position = read_position - span->position;
            int64_t length = span->length - file_position;

            read_position += length;
            download_remaining -= length;
            is_cached = true;
            continue;
        } else {
            // Data is not cached, read from upstream with async cache backing.
            int64_t byte_range_len = 0;
            int32_t max_file_size = 0;
            //while (!terminate_download_thread_ && bytes_remaining > 0) {
            {
                max_file_size = real_byte_range_len + 1;

                if (download_remaining == kLengthUnset || download_remaining > real_byte_range_len) {
                    byte_range_len = real_byte_range_len;
                } else {
                    LOG_INFO("[%d][AsyncCacheDataSource] download_remaining(%lld)\n",
                             GetContextId(), download_remaining);
                    byte_range_len = download_remaining;
                }
                if (span->length != -1 && span->length < byte_range_len) {
                    LOG_ERROR_DETAIL("[%d][AsyncCacheDataSource] reset byte_range_len from (%d) to span len: %d\n",
                                     GetContextId(), byte_range_len, span->length);
                    byte_range_len = span->length;
                }

                if (byte_range_len >= max_file_size)
                    byte_range_len = max_file_size - 1;

                DataSpec data_spec;
                uint64_t start_ms = kpbase::SystemUtil::GetCPUTime();
                int32_t got_transfer_consume_ms = 0;
                int32_t content_len = 0;
                data_spec.WithUri(uri_).WithPosition(read_position).WithLength(byte_range_len).WithKey(key_).WithFlags(flag_);

                result = (int32_t)upstream_data_source_->Open(data_spec);
                current_upstream_data_spec_ = data_spec;
                OnDataSourceOpened(result, read_position);
                if (result < 0) {
                    LOG_ERROR_DETAIL("[%d][AsyncCacheDataSource] HttpDataSource Open fails with error %d, exit download, thread(%d)\n",
                                     GetContextId(), result, std::this_thread::get_id());
                    error_code_ = result;
                    ac_rt_info_->cache_ds.download_exit_reason = DownloadUpstreamOpenFailed;
                    break;
                }
                content_len = result;
                ac_rt_info_->cache_ds.is_reading_file_data_source = false;
                ac_rt_info_->cache_ds.cache_upstream_source_cnt++;

                if (content_len >= max_file_size) {
                    // CDN返回的长度与请求的长度不一致，这种情况使用非Rang的方式直接从网络下载，不缓存.
                    LOG_INFO("[%d][AsyncCacheDataSource] content_len >= max_file_size, content_len: %d, max_file_size: %d\n",
                             GetContextId(), content_len, max_file_size);
                    ac_rt_info_->cache_ds.download_exit_reason = DownloadByteRangeContentInvalid;
                    HandleCdnError(kDownloadStopReasonCancelled, kResultExceptionHttpDataSourceByteRangeLenInvalid);
                    break;
                }

                if (download_remaining == kLengthUnset) {
                    // get content length from content range
                    result = setContentLength(read_position);
                    if (CacheUtil::IsFileSystemError(result)) {
                        LOG_ERROR("[%d] [AsyncCacheDataSource] setContentLength fail:%d", GetContextId(), result);
                        ac_rt_info_->cache_ds.download_exit_reason = DownloadCacheError;
                        HandleCdnError(kDownloadStopReasonCancelled, result);
                        bytes_remaining_ = kLengthUnset;
                        break;
                    }

                    if (result == kLengthUnset) {
                        LOG_INFO("[%d][AsyncCacheDataSource] file len is -1\n", GetContextId());
                        ac_rt_info_->cache_ds.download_exit_reason = DownloadByteRangeContentInvalid;
                        HandleCdnError(kDownloadStopReasonCancelled, kResultExceptionHttpDataSourceByteRangeLenInvalid);
                        break;
                    } else {
                        download_remaining = bytes_remaining_;
                    }
                    if (download_remaining < byte_range_len) {
                        LOG_ERROR_DETAIL("[%d][AsyncCacheDataSource] change byte_range_len from %d to %lld\n",
                                         GetContextId(), byte_range_len, download_remaining);
                        byte_range_len = download_remaining;
                    }
                    tytes_remaining_event_.Signal();
                    is_tytes_remaining_event_post = true;
                }
                //LOG_ERROR_DETAIL("[AsyncCacheDataSource]  byte_range_len:%d, max_file_size:%d",
                //                 byte_range_len, max_file_size);


                snprintf(ac_rt_info_->cache_ds.current_read_uri, DATA_SOURCE_URI_MAX_LEN, "%s", uri_.c_str());
                kpbase::File file_cache;
                AcResultType ret;
                file_cache = cache_->StartFile(data_spec.key,
                                               data_spec.absolute_stream_position,
                                               max_file_size,
                                               ret);
                if (ret != kResultOK) {
                    terminate_download_thread_ = true;
                    LOG_ERROR_DETAIL("[%d][AsyncCacheDataSource] cache_->StartFile failed, ret: %d\n", GetContextId(), ret);
                    HandleDownloadError(kDownloadStopReasonCancelled);
                    error_code_ = ret;
                    ac_rt_info_->cache_ds.download_exit_reason = DownloadCacheStartFileFailed;
                    break;
                }

                cache_write_data_source_ = std::make_shared<FileWriterMemoryDataSource>(
                                               file_cache,
                                               data_spec.absolute_stream_position,
                                               max_file_size);
                cache_write_data_source_->Open(data_spec);
                buf = cache_write_data_source_->Buf();
                cache_write_data_source_event_.Signal();

                int32_t offset = 0;
                int32_t bytes_read = 0;
                int64_t download_bytes = read_position;
                int64_t ret_of_cache_write = kpbase::kIoStreamOK;
                while (!terminate_download_thread_) { // writing each  file
                    int left_len = content_len - bytes_read;
                    if (left_len <= 0) {
                        OnDataSourceReadResult(0, got_transfer_consume_ms);
                        break;
                    }
                    result = (int32_t)upstream_data_source_->Read(buf, offset, left_len);
                    if (result > 0) {
                        download_bytes += result;
                        OnDataSourceReadResult(result, got_transfer_consume_ms);
                        {
                            std::lock_guard<std::mutex> lg(cache_write_data_source_mutex_);
                            ret_of_cache_write = cache_write_data_source_->Write(buf, offset, result);
                        }
                        if (kpbase::kIoStreamOK == ret_of_cache_write) {
                            offset += result;
                            bytes_read += result;
                            write_position_ += result;
                            read_pause_event_.Signal();
                        } else {
                            LOG_ERROR_DETAIL("[%d][AsyncCacheDataSource] cache_write_data_source_->Write failed, drop: %d\n",
                                             GetContextId(), result);
                            terminate_download_thread_ = true;
                            ac_rt_info_->cache_ds.download_exit_reason = DownloadCacheWriteBufFailed;
                            break;
                        }
                        if (bytes_read >= max_file_size) {
                            LOG_ERROR_DETAIL("[%d][AsyncCacheDataSource] max_file_size(%d) reached\n", GetContextId(), max_file_size);
                            ac_rt_info_->cache_ds.download_exit_reason = DownloadCacheWriteExceedMax;
                            break;
                        }
                    } else if (result == kResultEndOfInput || result == 0) {
                        OnDataSourceReadResult(0, got_transfer_consume_ms);
                        break;
                    } else {
                        LOG_ERROR_DETAIL("[%d][AsyncCacheDataSource] upstream read exception: %d\n", GetContextId(), result);
                        terminate_download_thread_ = true;
                        OnDataSourceReadResult(result, got_transfer_consume_ms);
                        error_code_ = result;
                        break;
                    }
                } // while(!terminate_download_thread_) writing each  file end

                if (enable_vod_adaptive_ && download_bytes > read_position) {
                    AbrUpdateDownloadInfo(start_ms, download_bytes - read_position);
                }

                // Commit File
                result = cache_write_data_source_->Close();
                if (kResultOK == result) {
                    if (bytes_read == 0) {
                        LOG_ERROR_DETAIL("[%d][AsyncCacheDataSource] file_.Remove() because of 0\n", GetContextId());
                        file_cache.Remove();
                    } else {
                        if (bytes_read < byte_range_len) {
                            LOG_ERROR_DETAIL("[%d][AsyncCacheDataSource] bytes_read(%d) < byte_range_len(%d)\n",
                                             GetContextId(), bytes_read, byte_range_len);
                        }
                        cache_->CommitFile(file_cache);
                        UpdateDataSourceQos();
                    }
                } else {
                    // 文件没有正确写入cache，此时cache_write_data_source_马上也要被释放。
                    // 在这种情况下，Read函数有可能无法获取当前range最后一部分的内容，造成视频卡住无法播放的问题。
                    // 此时应该退出当前下载线程,尝试直接从网路下载。
                    file_cache.Remove();
                    LOG_ERROR_DETAIL("[%d][AsyncCacheDataSource] file_.Remove() because of cache_write_data_source_ close failed\n", GetContextId());
                    error_code_ = result;
                    ac_rt_info_->cache_ds.download_exit_reason = DownloadFileWriteError;
                    break;
                }
                buf = nullptr;
                read_position += bytes_read;
                download_remaining -= bytes_read;
                upstream_data_source_->Close();
                if (!got_transfer_consume_ms) {
                    OnDataSourceReadResult(0, got_transfer_consume_ms);
                }

                if (cache_write_data_source_) {
                    std::lock_guard<std::mutex> lg(cache_write_data_source_mutex_);
                    cache_write_data_source_ = nullptr;
                }
            } //(whole_bytes_written < bytes_to_write)

            if (span != nullptr) {
                cache_->ReleaseHoleSpan(span);
                span = nullptr;
            }
        }

    } // while(!terminate_download_thread_)

    // For case HTTP Open fails
    if (span != nullptr) {
        cache_->ReleaseHoleSpan(span);
        span = nullptr;
    }

    if (buf) {
        buf = nullptr;
    }

    // FOR Writing Thread Exit normally or with exception, read
    // thread cannot use FileWriterMemoryDataSource anymore
    terminate_download_thread_ = true;

    // Set cache_write_data_source_ null when Thread Exit, if not do, last segment
    // reading will use cache_write_data_source_ with priority, although already cached
    if (cache_write_data_source_) {
        std::lock_guard<std::mutex> lg(cache_write_data_source_mutex_);
        cache_write_data_source_ = nullptr;
    }

    if (!is_tytes_remaining_event_post) {
        tytes_remaining_event_.Signal();
    }

    download_thread_exit_ = true;
    read_pause_event_.Signal();
    cache_write_data_source_event_.Signal();

    LOG_ERROR_DETAIL("[%d][AsyncCacheDataSource]: Write Thread(%d) Exit\n",  GetContextId(), std::this_thread::get_id());
}


/*
* block read. If no more data wait.
* buf: save data.
* offset: the offset of buf, copy data to buf+offset.
* read_len: need read how many data.
*/
int64_t AsyncCacheDataSource::Read(uint8_t* buf, int64_t offset, int64_t read_len) {
    if (read_len == 0 || abort_) {
        return 0;
    }

    if (bytes_remaining_ == 0) {
        return kResultEndOfInput;
    }

    AcResultType error = kResultOK;
    int64_t bytes_read = 0;
    int32_t got_transfer_consume_ms = 0;
    std::string last_file_uri = "";

    while (!abort_) {

        if (!directly_read_from_upstream_) {

            if (is_reading_from_cache_) {
                bytes_read = cache_read_data_source_->Read(buf, offset, read_len);
                if (bytes_read > 0) {
                    if (write_position_ > read_position_
                        && (write_position_ - read_position_) < byte_range_length_
                        && download_thread_is_waiting_) {
                        download_pause_event_.Signal();
                        ac_rt_info_->cache_ds.pre_download_cnt++;
                        LOG_DEBUG("[%d][AsyncCacheDataSource] pre download next byterange when read from cache, read_position:%lld, diff:%lld\n",
                                  GetContextId(), read_position_, (write_position_ - read_position_));
                    }
                    read_position_ += bytes_read;
                    bytes_remaining_ -= bytes_read;
                    if (bytes_remaining_ <= 0) {
                        LOG_ERROR_DETAIL("[%d][AsyncCacheDataSource] close last range result:%d, read_position_: %lld, bytes_remaining_: %lld\n",
                                         GetContextId(), bytes_read, read_position_, bytes_remaining_);
                        is_reading_from_cache_ = false;
                        cache_read_data_source_->Close();
                    }
                    break;
                } else {
                    LOG_ERROR_DETAIL("[%d][AsyncCacheDataSource] Read result:%d, read_position_: %lld, bytes_remaining_: %lld\n",
                                     GetContextId(), bytes_read, read_position_, bytes_remaining_);
                    is_reading_from_cache_ = false;
                    cache_read_data_source_->Close();
                    if (CacheUtil::IsFileReadError(bytes_read)) {
                        ac_rt_info_->cache_ds.download_exit_reason = DownloadCacheError;
                        int32_t result = 0;
                        result = HandleFileError();
                        if (result < 0) {
                            return result;
                        }
                        continue;
                    }

                    if (bytes_remaining_ > 0 && kResultEndOfInput == bytes_read) {
                        LOG_ERROR_DETAIL("[%d][AsyncCacheDataSource] try next range\n", GetContextId());
                        continue;
                    }
                    return bytes_read;
                }
            }

            std::shared_ptr<CacheSpan> span = nullptr;
            span = cache_->StartRead(key_, read_position_, error);

            if (error != kResultOK) {
                // 打开文件失败，尝试从网络直接下载。
                ac_rt_info_->cache_ds.download_exit_reason = DownloadCacheError;
                LOG_ERROR_DETAIL(" [%d][AsyncCacheDataSource] StartRead Error %d", GetContextId(), error);

                int32_t result = 0;
                result = HandleFileError();
                if (result < 0) {
                    return result;
                }
                continue;
            }

            if (span && span->is_cached) {
                LOG_ERROR_DETAIL(" [%d][AsyncCacheDataSource] offset: %lld, read_position_: %lld, bytes_remaining_: %lld",
                                 GetContextId(), offset, read_position_, bytes_remaining_);

                // Data is cached, read from cache
                // span->position: span start postion corresponding to the whole span
                // span->length: span length, file size of bytes
                DataSpec data_spec;
                std::string file_uri = span->file->path();
                int64_t file_position = read_position_ - span->position;
                int64_t length = span->length - file_position;

                data_spec.WithUri(file_uri).WithPositions(read_position_, file_position)
                .WithLength(length).WithKey(key_).WithFlags(flag_);

                int32_t result = (int32_t)cache_read_data_source_->Open(data_spec);
                if (result < 0) {
                    if (kResultFileDataSourceIOError_0 == result && last_file_uri != file_uri) {
                        // downloadthread有可能在cache_->StartRead之后打开同一个span,造成span对应的文件名被修改，
                        // 遇到这种情况，需要再重试一次。
                        last_file_uri = file_uri;
                        continue;
                    }
                    LOG_ERROR_DETAIL("[%d][AsyncCacheDataSource] cache_read_data_source_ Error %d, file_uri: %s \n",
                                     GetContextId(), result, file_uri.c_str());
                    return result;
                }
                if (download_thread_exit_) {
                    UpdateDataSourceQos();
                    snprintf(ac_rt_info_->cache_ds.current_read_uri, DATA_SOURCE_URI_MAX_LEN, "%s", file_uri.c_str());
                    ac_rt_info_->cache_ds.is_reading_file_data_source = true;
                }
                is_reading_from_cache_ = true;
                continue;
            } else if (!download_thread_exit_) {
                if (!terminate_download_thread_) {
                    // need download from network,  get data from cache_write_data_source_
                    bool need_wait = false;
                    {
                        std::lock_guard<std::mutex> lg(cache_write_data_source_mutex_);
                        if (nullptr == cache_write_data_source_) {
                            LOG_ERROR_DETAIL("[%d][AsyncCacheDataSource] cache_write_data_source_ should not null, wait and try again\n", GetContextId());
                            cache_write_data_source_event_.Wait();
                            continue;
                        }
                        if (write_position_ > read_position_ && cache_write_data_source_->InFileWriterMemory(read_position_)) {
                            ac_rt_info_->cache_ds.cache_write_source_cnt++;
                            bytes_read = cache_write_data_source_->Read(buf, offset, read_len);
                            if (bytes_read > 0) {
                                read_position_ += bytes_read;
                                bytes_remaining_ -= bytes_read;
                                break;
                            } else {
                                LOG_ERROR_DETAIL("[%d][AsyncCacheDataSource] error on cache_write_data_source_->read read_position_: %lld, bytes_remaining_: %lld\n",
                                                 GetContextId(), read_position_, bytes_remaining_);
                                return kResultExceptionSourceNotOpened_0;
                            }
                        } else {
                            need_wait = true;
                        }
                    }

                    if (need_wait) {
                        download_pause_event_.Signal();
                        read_pause_event_.Wait();
                    }
                } else {
                    LOG_ERROR_DETAIL("[%d][AsyncCacheDataSource] terminate_download_thread_ is true, try again\n", GetContextId());
                    continue;
                }
            } else {
                int64_t result = 0;
                if (error_code_ == kLibcurlErrorBase + (-CURLE_ABORTED_BY_CALLBACK) ||
                    error_code_ == kLibcurlErrorBase + (-CURLE_OPERATION_TIMEDOUT) ||
                    error_code_ == kLibcurlErrorBase + (-CURLE_COULDNT_CONNECT) ||
                    error_code_ == kLibcurlErrorBase + (-CURLE_COULDNT_RESOLVE_HOST) ||
                    error_code_ == kResultExceptionNetDataSourceReadTimeout) {
                    LOG_ERROR("[%d][AsyncCacheDataSource] do not read from upstream, error_code: %d\n",
                              GetContextId(), error_code_);
                    return error_code_;
                } else {
                    result = OpenHttpDataSource(bytes_remaining_);
                    if (result < 0) {
                        return result;
                    }
                    LOG_ERROR("[%d][AsyncCacheDataSource] open upstream in read, error_code_: %d\n",
                              GetContextId(), error_code_);
                }
            }
        } else {
            bytes_read = upstream_data_source_->Read(buf, offset, read_len);
            if (bytes_read > 0) {
                read_position_ += bytes_read;
                bytes_remaining_ -= bytes_read;
                OnDataSourceReadResult(bytes_read, got_transfer_consume_ms);
                //LOG_DEBUG("[AsyncCacheDataSource] read from upsteam, bytes_read: %d, read_position_: %lld, bytes_remaining_: %lld\n",
                //          bytes_read, read_position_, bytes_remaining_);
                break;
            } else {
                LOG_DEBUG("[%d][AsyncCacheDataSource] close upsteam, bytes_read: %d, read_position_: %lld, bytes_remaining_: %lld\n",
                          GetContextId(), bytes_read, read_position_, bytes_remaining_);
                upstream_data_source_->Close();
                directly_read_from_upstream_ = false;
                OnDataSourceReadResult(bytes_read, got_transfer_consume_ms);
                return bytes_read;
            }
        }
    }

    if (abort_ && directly_read_from_upstream_) {
        LOG_DEBUG("[%d][AsyncCacheDataSource] 2 close upsteam result: %d, read_position_: %lld, bytes_remaining_: %lld\n",
                  GetContextId(), read_position_, bytes_remaining_);
        directly_read_from_upstream_ = false;
        upstream_data_source_->Close();
        OnDataSourceReadResult(bytes_read, got_transfer_consume_ms);
    }

    if (abort_ && is_reading_from_cache_) {
        cache_read_data_source_->Close();
        is_reading_from_cache_ = false;
    }

    return bytes_read;
}

int32_t AsyncCacheDataSource::HandleFileError() {
    if (!download_thread_exit_) {
        terminate_download_thread_ = true;
        download_pause_event_.Signal();
        if (download_thread_.joinable()) {
            download_thread_.join();
            LOG_DEBUG("[%d][AsyncCacheDataSource]: Write Thread(%d) exit in read", GetContextId(),
                      download_thread_.get_id());
        }
    }
    return (int32_t)OpenHttpDataSource(bytes_remaining_);
}

void AsyncCacheDataSource::HandleErrorBeforeReturn(AcResultType ret) {
    if (IsCacheError(ret)) {
        seen_cache_error_ = true;
    }
}

bool AsyncCacheDataSource::IsCacheError(AcResultType ret) {
    return ret >= kResultCacheExceptionStart && ret <= kResultCacheExceptionEnd;
}

AcResultType AsyncCacheDataSource::Close() {
    LOG_DEBUG("[%d][AsyncCacheDataSource]: close",  GetContextId());
    AcResultType ret = kResultOK;
    abort_ = true;
    terminate_download_thread_ = true;
    download_pause_event_.Signal();
    read_pause_event_.Signal();
    cache_write_data_source_event_.Signal();

    if (download_thread_.joinable()) {
        download_thread_.join();
        LOG_DEBUG("[%d][AsyncCacheDataSource]: Write Thread(%d) returned", GetContextId(), download_thread_.get_id());
    }

    uri_.clear();
    ReportSessionClosed();


    return ret;
}

Stats* AsyncCacheDataSource::GetStats() {
    return stats_.get();
}

void AsyncCacheDataSource::UpdateDataSourceQos() {
    std::string copied_key;
    {
        std::lock_guard<std::mutex> lock(key_lock_);
        copied_key = key_;
    }

    ac_rt_info_->cache_ds.total_bytes = cache_->GetContentLength(copied_key);
    ac_rt_info_->cache_ds.cached_bytes = cache_->GetContentCachedBytes(copied_key);
}

#pragma CacheDataSource part
bool AsyncCacheDataSource::AlmostFullyCached(int64_t cached_bytes, uint64_t total_bytes) {
    return total_bytes == 0 ? false : ((float)cached_bytes / (float)total_bytes > 0.98);
}

void AsyncCacheDataSource::DoReportProgressOnCallbackThread(uint64_t position) {
    auto iter = span_list_.begin();
    bool updated = false;
    Span updated_span;

    for (; iter != span_list_.end(); iter++) {
        if (position >= iter->start && position <= iter->end) {
            // if this span is not the last span in this set, report its end as position.
            // if it is the last, don't report position.
            auto next_span_iter = std::next(iter);
            if (next_span_iter != span_list_.end()) {
                span_list_.erase(next_span_iter, span_list_.end());
                cache_session_listener_->OnDownloadProgress(iter->end, total_bytes_);
            }
            return;
        }
        if (position > iter->end && position <= iter->end + kProgressSpanUpdateThreshold) {
            // update progress span.
            updated_span = Span{iter->start, position};
            updated = true;
            break;
        } else if (position < iter->start) {
            break;
        }
    }
    // erase the spans that is bigger than current position.
    span_list_.erase(iter, span_list_.end());

    if (updated) {
        span_list_.insert(updated_span);
    } else {
        span_list_.insert(Span{position, position});
    }

    cache_session_listener_->OnDownloadProgress(position, total_bytes_);
}

void AsyncCacheDataSource::ReportSessionOpened(uint64_t pos, int64_t cached_bytes) {
    if (!session_opened_) {
        session_open_ts_ms_ = kpbase::SystemUtil::GetCPUTime();
        should_report_progress_ = not AlmostFullyCached(cached_bytes, total_bytes_);
        //因为AsyncCacheDataSource是可以重复打开的，所以这里要reset session_close_reported_的状态
        session_close_reported_ = false;
        LOG_DEBUG("[%d][AsyncCacheDataSource ReportSessionOpened] pos: %lld, cached_bytes: %lld, total_bytes: %lld",
                  GetContextId(), pos, cached_bytes, total_bytes_);
        if (cache_session_listener_) {
            runloop_->Post([ = ] {
                cache_session_listener_->OnSessionStarted(key_, pos, cached_bytes, total_bytes_);
            });
        }
        session_opened_ = true;
    }
}

void AsyncCacheDataSource::ReportProgress(uint64_t position) {
    if (cache_session_listener_ && total_bytes_ && should_report_progress_) {
        if (cache_callback_) {
            callbackInfo_->SetCachedBytes(cache_->GetContentCachedBytes(key_));
            callbackInfo_->SetTotalBytes(total_bytes_);
            callbackInfo_->SetProgressPosition(position);
            runloop_->Post([ = ] {
                cache_callback_->onSessionProgress(callbackInfo_);
            });
        }
        runloop_->Post([ = ] {
            DoReportProgressOnCallbackThread(position);
        });
    }
}

void AsyncCacheDataSource::ReportDownloadStarted(uint64_t position, const ConnectionInfo& info) {
    if (cache_callback_) {
        callbackInfo_ = std::shared_ptr<AcCallbackInfo>(AcCallbackInfoFactory::CreateCallbackInfo());
        callbackInfo_->SetCacheKey(key_);
        callbackInfo_->SetCurrentUri(info.uri);
        callbackInfo_->SetHost(info.host);
        callbackInfo_->SetIp(info.ip);
        callbackInfo_->SetHttpResponseCode(info.response_code);
        callbackInfo_->SetHttpRedirectCount(info.redirect_count);
        callbackInfo_->SetEffectiveUrl(info.effective_url);
        callbackInfo_->SetKwaiSign(info.sign);
        callbackInfo_->SetXKsCache(info.x_ks_cache);
        callbackInfo_->SetSessionUUID(info.session_uuid);
        callbackInfo_->SetDownloadUUID(info.download_uuid);
        callbackInfo_->SetProductContext(product_context_);
        callbackInfo_->SetContentLength(info.content_length_from_curl_);
        download_stop_need_to_report_ = true;
    }
    if (cache_session_listener_) {
        runloop_->Post([ = ] {
            cache_session_listener_->OnDownloadStarted(position, info.uri, info.host, info.ip, info.response_code,
                                                       info.connection_used_time_ms);
        });

        download_stop_need_to_report_ = true;
    }
}

void AsyncCacheDataSource::CheckAndCloseCurrentDataSource(int error_code, DownloadStopReason stop_reason) {
    CacheSessionListener::NeedRetryOnStopReason(stop_reason);
}

void AsyncCacheDataSource::ReportDownloadStopped(const char* tag, const ConnectionInfo& info) {

    LOG_DEBUG("[%d][AsyncCacheDataSource] OnDataSourceStop tag[%s], downloaded_bytes:%lld, info.error_code:%d, stop_reason:%d, transfer_consume_ms: %d, extra:%s\n",
              GetContextId(),
              tag,
              info.downloaded_bytes_,
              info.error_code,
              info.stop_reason_,
              info.transfer_consume_ms_,
              data_source_extra_.c_str());
    if (download_stop_need_to_report_) {
        download_stop_need_to_report_ = false;
        if (cache_session_listener_) {
            runloop_->Post([ = ] {
                cache_session_listener_->OnDownloadStopped(info.stop_reason_, info.downloaded_bytes_,
                                                           info.transfer_consume_ms_, info.sign,
                                                           info.error_code, info.x_ks_cache,
                                                           info.session_uuid, info.download_uuid, data_source_extra_);
            });
        }
        if (cache_callback_) {
            callbackInfo_->SetDataSourceType(kDataSourceTypeAsyncDownload);
            callbackInfo_->SetUpstreamType(ac_rt_info_->cache_applied_config.upstream_type);
            callbackInfo_->SetHttpVersion(ac_rt_info_->download_task.http_version);
            callbackInfo_->SetStopReason(info.stop_reason_);
            callbackInfo_->SetErrorCode(info.error_code);
            callbackInfo_->SetTransferConsumeMs(info.transfer_consume_ms_);
            callbackInfo_->SetDownloadBytes(info.GetDownloadedBytes());
            callbackInfo_->SetRangeRequestStart(info.range_request_start);
            callbackInfo_->SetRangeRequestEnd(info.range_request_end);
            callbackInfo_->SetRangeResponseStart(info.range_response_start);
            callbackInfo_->SetRangeResponseEnd(info.range_response_end);
            callbackInfo_->SetDnsCost(info.http_dns_analyze_ms);
            callbackInfo_->SetConnectCost(info.connection_used_time_ms);
            callbackInfo_->SetFirstDataCost(info.http_first_data_ms);
            callbackInfo_->SetTotalBytes(info.file_length);
#if defined(__ANDROID__)
            callbackInfo_->SetTcpClimbingInfo(info.tcp_climbing_info);
#endif
            callbackInfo_->SetOsErrno(info.os_errno);

            runloop_->Post([ = ] {
                cache_callback_->onDownloadFinish(callbackInfo_);
            });
        }
    }
}

void AsyncCacheDataSource::ReportSessionClosed() {
    // disable the possibility that mulitple thread is reporting session closed,
    // that may cause the PostAndWait event call waiting forever.
    std::lock_guard<std::mutex> lg(close_session_lock_);
    if (cache_session_listener_) {
        if (!session_close_reported_) {
            LOG_DEBUG("[%d][listener ReportSessionClosed]", GetContextId());
            session_close_reported_ = true;
            should_report_progress_ = false;
            auto stat = GetStats();
            session_close_ts_ms_ = kpbase::SystemUtil::GetCPUTime();
            runloop_->PostAndWait([ = ] {
                cache_session_listener_->OnSessionClosed(
                    error_code_,
                    static_cast<uint64_t>(network_cost_ms_),
                    static_cast<uint64_t>(session_close_ts_ms_ - session_open_ts_ms_),
                    static_cast<uint64_t>(downloaded_bytes_),
                    stat ? stat->ToString() : "",
                    session_opened_);
            });
            session_opened_ = false;
        }
    }
}

#pragma mark new API for download callback
void AsyncCacheDataSource::OnDataSourceOpened(int64_t open_ret, int64_t position) {
    std::lock_guard<std::recursive_mutex> lg(current_data_source_lock_);
    if (upstream_data_source_) {
        HasConnectionInfo* hasConnectionInfo = dynamic_cast<HasConnectionInfo*>(upstream_data_source_.get());
        if (hasConnectionInfo) {
            // 因为这里后面可能会关闭 DataSource，所以用引用是不安全的；
            const ConnectionInfo info(hasConnectionInfo->GetConnectionInfo());
            ReportDownloadStarted(position, info);
            LOG_DEBUG("[%d][AsyncCacheDataSource] OnDataSourceOpened position: %lld, downloaded_bytes_: %lld\n",
                      GetContextId(), position, info.downloaded_bytes_);
            if (open_ret < 0) {
                ReportDownloadStopped("OnDataSourceOpened fail", info);
            }
        }
    }
}

void AsyncCacheDataSource::OnDataSourceReadResult(int64_t read_len, int32_t& got_transfer_consume_ms) {
    std::lock_guard<std::recursive_mutex> lg(current_data_source_lock_);
    if (upstream_data_source_) {
        HasConnectionInfo* hasConnectionInfo = dynamic_cast<HasConnectionInfo*>(upstream_data_source_.get());
        if (hasConnectionInfo) {
            const ConnectionInfo& info = hasConnectionInfo->GetConnectionInfo();
            int64_t download_position = current_upstream_data_spec_.position + info.GetDownloadedBytes();
            if (read_len >= 0) {

                uint64_t now = kpbase::SystemUtil::GetCPUTime();
                if ((now - last_progress_time_ms_ > progress_cb_interval_ms_ &&
                     download_position - last_progress_pos_ > kProgressBytesThreshold) ||
                    read_len == 0 || info.connection_closed || info.IsDownloadComplete()) {
                    bool need_report = false;
                    need_report = ((last_progress_pos_ == download_position) ? false : true);
                    last_progress_pos_ = download_position;
                    last_progress_time_ms_ = now;
                    if (need_report) {
                        LOG_DEBUG("[%d][AsyncCacheDataSource] OnDataSourceProgress position: %lld, total_bytes_: %lld\n",
                                  GetContextId(), download_position, total_bytes_);
                        ReportProgress(download_position);
                    }
                }

                if (info.connection_closed || info.IsDownloadComplete()) {
                    if (!got_transfer_consume_ms) {
                        got_transfer_consume_ms = 1;
                        ReportDownloadStopped("OnDataSourceReadResult finish", info);
                    }
                }
            } else {
                if (!got_transfer_consume_ms) {
                    got_transfer_consume_ms = 1;
                    ReportDownloadStopped("OnDataSourceReadResult fail", info);
                }
            }
        }
    }
}

void AsyncCacheDataSource::OnDataSourceCancel() {
    std::lock_guard<std::recursive_mutex> lg(current_data_source_lock_);
    if (upstream_data_source_) {
        HasConnectionInfo* hasConnectionInfo = dynamic_cast<HasConnectionInfo*>(upstream_data_source_.get());
        if (hasConnectionInfo) {
            const ConnectionInfo info(hasConnectionInfo->GetConnectionInfo());
            ReportDownloadStopped("OnDataSourceCancel cancel", info);

        }
    }
}

void AsyncCacheDataSource::AbrUpdateDownloadInfo(uint64_t start_time_ms, uint64_t bytes_transferred) {
    kuaishou::abr::DownloadSampleInfo last_sample_info;
    uint64_t now_ms = kpbase::SystemUtil::GetCPUTime();

    last_sample_info.begin_timestamp = start_time_ms;
    last_sample_info.end_timestamp = now_ms;
    last_sample_info.total_bytes = bytes_transferred;
    kuaishou::abr::AbrEngine::GetInstance()->UpdateDownloadInfo(last_sample_info);

    ac_rt_info_->vod_adaptive.real_time_throughput_kbps = kuaishou::abr::AbrEngine::GetInstance()->get_real_time_throughput();
    ac_rt_info_->vod_adaptive.consumed_download_time_ms += now_ms - start_time_ms;
    ac_rt_info_->vod_adaptive.actual_video_size_byte += bytes_transferred;
}

void AsyncCacheDataSource::HandleCdnError(DownloadStopReason stop_reason, int error_code) {
    LOG_ERROR_DETAIL("[%d][AsyncCacheDataSource]: CDN Error stop_reason:%d, error_code:%d\n",  GetContextId(), stop_reason, error_code);
    upstream_data_source_->Close();
    std::lock_guard<std::recursive_mutex> lg(current_data_source_lock_);
    if (upstream_data_source_) {
        HasConnectionInfo* hasConnectionInfo = dynamic_cast<HasConnectionInfo*>(upstream_data_source_.get());
        if (hasConnectionInfo) {
            const ConnectionInfo info(hasConnectionInfo->GetConnectionInfo());
            LOG_DEBUG("[%d][AsyncCacheDataSource] OnDataSourceStop tag[%s], downloaded_bytes:%lld, info.error_code:%d, stop_reason:%d, transfer_consume_ms: %d, extra:%s\n",
                      GetContextId(),
                      "cdn error",
                      info.downloaded_bytes_,
                      error_code,
                      stop_reason,
                      info.transfer_consume_ms_,
                      data_source_extra_.c_str());
            if (download_stop_need_to_report_) {
                download_stop_need_to_report_ = false;
                if (cache_session_listener_) {
                    runloop_->Post([ = ] {
                        cache_session_listener_->OnDownloadStopped(stop_reason, info.downloaded_bytes_,
                                                                   info.transfer_consume_ms_, info.sign,
                                                                   error_code, info.x_ks_cache,
                                                                   info.session_uuid, info.download_uuid, data_source_extra_);
                    });
                }
                if (cache_callback_) {
                    callbackInfo_->SetStopReason(stop_reason);
                    callbackInfo_->SetErrorCode(error_code);
                    runloop_->Post([ = ] {
                        cache_callback_->onDownloadFinish(callbackInfo_);
                    });
                }
            }

        }
    }
}

#undef RETURN_ON_ERROR

} // namespace cache
} // namespace kuaishou
