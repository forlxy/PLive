#include <include/awesome_cache_runtime_info_c.h>
#include "file_data_source.h"
#include "sync_cache_data_source.h"
#include "cache_data_sink.h"
#include "cache_util.h"
#include "tee_data_source.h"
#include "constant.h"
#include "multi_download_http_data_source.h"
#include "ac_log.h"
#include "cache_manager.h"
#include "abr/abr_engine.h"
#include "abr/abr_types.h"

namespace kuaishou {
namespace cache {

#define MIN_CUR_BUFFER_SIZE_BYTES  800 * 1024

namespace {
static const uint32_t kProgressSpanUpdateThreshold = 2 * 1024 * 1024; // 2M
}

#define RETURN_ON_ERROR(error) \
    if (error < kResultOK) { \
        stats_->CurrentOnError((int32_t)error); \
        return error; \
    }

#define RETURN_ANY_WAY(ret) \
    HandleErrorBeforeReturn(ret); \
    return ret;

SyncCacheDataSource::SyncCacheDataSource(Cache* cache,
                                         std::shared_ptr<HttpDataSource> upstream,
                                         std::shared_ptr<DataSource> cache_read_data_source,
                                         std::shared_ptr<DataSink> cache_write_data_sink,
                                         const DataSourceOpts& opts,
                                         SyncCacheDataSource::EventListener* listener,
                                         std::shared_ptr<CacheSessionListener> session_listener,
                                         AwesomeCacheRuntimeInfo* ac_rt_info) :
    cache_(cache),
    cache_session_listener_(session_listener),
    cache_callback_(static_cast<AwesomeCacheCallback*>(opts.cache_callback)),
    cache_read_data_source_(cache_read_data_source),
    current_request_ignore_cache_(false),
    upstream_data_source_(upstream),
    data_source_extra_(opts.download_opts.datasource_extra_msg),
    block_on_cache_((opts.cache_flags & kFlagBlockOnCache) != 0),
    progress_cb_interval_ms_(opts.download_opts.progress_cb_interval_ms),
    cache_write_data_source_(cache_write_data_sink != nullptr
                             ? std::make_shared<TeeDataSource>(upstream, cache_write_data_sink, ac_rt_info)
                             : nullptr),
    event_listener_(listener),
    stats_(new internal::SyncCacheDataSourceSessionStats("SyncSrc")),
    // CacheDataSource part
    total_bytes_(0),
    error_code_(0),
    stop_reason_for_qos_(kDownloadStopReasonUnset),
    downloaded_bytes_(0),
    session_open_ts_ms_(0),
    session_close_ts_ms_(0),
    network_cost_ms_(0),
    session_opened_(false),
    session_close_reported_(false),
    should_report_progress_(false),
    download_stop_need_to_report_(false),
    key_(""),
    product_context_(opts.download_opts.product_context),
    enable_vod_adaptive_(opts.enable_vod_adaptive),
    runloop_(new kpbase::Runloop("SyncCacheDataSourceRunLoop")),
    ac_rt_info_(ac_rt_info),
    bytes_remaining_(0) {
    qos_ = {0};
    //CacheDataSource part
    span_list_.insert(Span{0, 0});

    callbackInfo_ = std::shared_ptr<AcCallbackInfo>(AcCallbackInfoFactory::CreateCallbackInfo());
    AwesomeCacheRuntimeInfo_cache_ds_init(ac_rt_info_);
    ac_rt_info->cache_ds.progress_cb_interval_ms = progress_cb_interval_ms_;

    SetContextId(opts.context_id);
    if (enable_vod_adaptive_) {
        ac_rt_info_->vod_adaptive.real_time_throughput_kbps = kuaishou::abr::AbrEngine::GetInstance()->get_real_time_throughput();
        curl_buffer_size_kb_ = opts.download_opts.curl_buffer_size_kb;
        if (curl_buffer_size_kb_ < MIN_CUR_BUFFER_SIZE_BYTES) {
            curl_buffer_size_kb_ = MIN_CUR_BUFFER_SIZE_BYTES;
        }
    }
}

SyncCacheDataSource::~SyncCacheDataSource() {
    runloop_->Stop();
}

int64_t SyncCacheDataSource::Open(const DataSpec& spec) {
    if (""  == spec.uri) {
        return kResultSpecExceptionUriUnset;
    }


    stats_->NewStage();
    spec_ = spec;
    stats_->CurrentSaveDataSpec(spec);
    uri_ = spec.uri;
    flags_ = spec.flags;
    {
        std::lock_guard<std::mutex> lock(key_lock_);
        key_ = CacheUtil::GetKey(spec);
    }

    if (cache_callback_) {
        callbackInfo_->SetCacheKey(key_);
    }

    LOG_INFO("[%d] SyncCacheDataSource::Open, spec.position:%lld, spec.length:%lld, key:%s",
             GetContextId(), spec.position, spec.length, spec.key.c_str());

    read_position_ = spec.position;
    last_progress_time_ms_ = 0;
    last_progress_pos_ = 0;

    int64_t total_bytes = 0;
    if (spec.length != kLengthUnset || current_request_ignore_cache_) {
        bytes_remaining_ = spec.length;
    } else {
        total_bytes = bytes_remaining_ = cache_->GetContentLength(key_);
        if (bytes_remaining_ != kLengthUnset) {
            bytes_remaining_ -= spec.position;
            if (bytes_remaining_ <= 0) {
                RETURN_ON_ERROR(kResultExceptionDataSourcePositionOutOfRange);
            }
        }
    }
    int64_t content_cached_bytes = cache_->GetContentCachedBytes(key_);

    ReportSessionOpened(spec.position, content_cached_bytes, total_bytes);

    LOG_INFO(
        "[%d] SyncCacheDataSource::Open, ReportSessionOpened spec.position + bytes_remaining_=cal_toal, %lld:%lld=%lld",
        GetContextId(), spec.position, bytes_remaining_, spec.position + bytes_remaining_);
    LOG_INFO("[%d] SyncCacheDataSource::Open, ReportSessionOpened content_cached_bytes/total_bytes_, %lld:/%lld",
             GetContextId(), content_cached_bytes, total_bytes_);

    ac_rt_info_->cache_ds.total_bytes = bytes_remaining_ + spec.position;
    ac_rt_info_->cache_ds.cached_bytes = content_cached_bytes;

    // 第一次先从cache读
    int64_t result = OpenNextSourceWithCache(true, true);
    LOG_INFO("[%d] [SyncCacheDataSource::Open] OpenNextSource error, result:%d", GetContextId(), (int)result);
    if (result < 0) {
        ac_rt_info_->cache_ds.first_open_error = (int)result;
        if (CacheUtil::IsFileSystemError(result)) {
            // 尝试从网络流直接读取
            LOG_ERROR("[%d] [SyncCacheDataSource::Open] first open fail:%d, try upstream_data_source", GetContextId(), (int)result);
            result = OpenNextSourceWithCache(false, true);
            LOG_INFO("[%d] [SyncCacheDataSource::Open] upstream_data_source Open result:%d", GetContextId(), (int)result);
        }  else {
            // do nothing
        }

        if (result < 0) {
            HandleErrorBeforeReturn(result);
        }
        RETURN_ON_ERROR(result);
    }

    if (cache_callback_) {
        callbackInfo_->SetCachedBytes(cached_bytes_);
        callbackInfo_->SetTotalBytes(spec_.position + bytes_remaining_);
        runloop_->Post([ = ] {
            cache_callback_->onSessionProgress(callbackInfo_);
        });
    }

    stats_->CurrentSetRemainBytes(bytes_remaining_);
    return bytes_remaining_;
}

int64_t SyncCacheDataSource::Read(uint8_t* buf, int64_t offset, int64_t read_len) {
    if (read_len == 0) {
        return 0;
    }

    if (bytes_remaining_ == 0) {
        RETURN_ON_ERROR(kResultEndOfInput);
    }
    if (current_data_source_ == nullptr) {
        if (error_code_ != 0) {
            return error_code_;
            RETURN_ON_ERROR(error_code_);
        } else {
            LOG_ERROR_DETAIL("[%d] SyncCacheDataSource::Read, current_data_source_ == nullptr", GetContextId());
            RETURN_ON_ERROR(kResultSyncCacheDataSourceInnerError);
        }
    }
    int64_t bytes_read = current_data_source_->Read(buf, offset, read_len);
    if (bytes_read == 0) {
        LOG_ERROR_DETAIL("[%d] SyncCacheDataSource::Read, current_data_source_->Read return 0", GetContextId());
    } else if (bytes_read > 0) {
        if (current_data_source_ == cache_read_data_source_) {
            total_cached_bytes_read_ += bytes_read;
        }
        read_position_ += bytes_read;
        if (bytes_remaining_ != kLengthUnset) {
            bytes_remaining_ -= bytes_read;
        }
        // successfully read, clear error codes if there was any.
        error_code_ = 0;

        stats_->CurrentOnReadBytes(bytes_read);
        OnDataSourceReadResult(bytes_read);
    } else if (bytes_read == kResultEndOfInput || bytes_read == kResultContentAlreadyCached) {
        if (current_request_unbonded_ && bytes_read == kResultEndOfInput) {
            // We only do unbounded requests to upstream and only when we don't know the actual stream
            // length. So we reached the end of stream.
            LOG_DEBUG("[%d] SyncCacheDataSource::Read, to SetContentLength, read_position_ = %lld",
                      GetContextId(), read_position_);
            AcResultType ret = SetContentLength(read_position_);
            bytes_remaining_ = 0;
            RETURN_ON_ERROR(ret);
        }
        LOG_INFO("[%d] SyncCacheDataSource::Read, bytes_read(is EOF or sth):%lld, to CloseCurrentSource()",
                 GetContextId(), bytes_read);
        int64_t ret = CloseCurrentSource();
        RETURN_ON_ERROR(ret);
        if (bytes_remaining_ > 0 || bytes_remaining_ == kLengthUnset) {
            ret = OpenNextSource(false);
            RETURN_ON_ERROR(ret);
            ret = Read(buf, offset, read_len);
            RETURN_ANY_WAY(ret);
        }
    } else {
        // Exception(error) occurs;
        // if it is write error, we should remember it and will not write to file this time.
        if (CacheUtil::IsFileReadError(bytes_read)) {
            ac_rt_info_->sink.fs_error_code = (int32_t)bytes_read;
            int64_t ret = CloseCurrentSource();
            RETURN_ON_ERROR(ret);

            if (bytes_remaining_ > 0 || bytes_remaining_ == kLengthUnset) {
                ret = OpenNextSourceWithCache(false, true);
                RETURN_ON_ERROR(ret);
                bytes_read = Read(buf, offset, read_len);
            }
        }
        OnDataSourceReadResult(bytes_read);
        RETURN_ANY_WAY(bytes_read);
    }
    if (bytes_remaining_ == 0) {
        LOG_INFO("[%d] SyncCacheDataSource::Read, bytes_remaining_ == 0, to CloseCurrentSource()",
                 GetContextId(), bytes_read);
        AcResultType ret = CloseCurrentSource();
        RETURN_ON_ERROR(ret);
    }
    return bytes_read;
}

AcResultType SyncCacheDataSource::Close() {
    AcResultType ret = CloseCurrentSource();
    uri_.clear();
    NotifyBytesRead();
    OnClose();

    ReportSessionClosed();
    RETURN_ANY_WAY(ret);
}

void SyncCacheDataSource::LimitCurlSpeed() {
    if (current_data_source_ && (current_data_source_ == cache_write_data_source_ || current_data_source_ == upstream_data_source_)) {
        current_data_source_->LimitCurlSpeed();
    }
}

int64_t SyncCacheDataSource::OpenNextSource(bool initial) {
    DataSpec data_spec;
    std::shared_ptr<CacheSpan> span;
    AcResultType error = kResultOK;

    if (current_request_ignore_cache_) {
        // do nothing
    } else {
        if (initial) {
            error = cache_->RemoveStaleSpans(key_);
            RETURN_ON_ERROR(error);
        }

        span = cache_->StartRead(key_, read_position_, error);
    }

    // todo check error
    ac_rt_info_->cache_ds.ignore_cache_on_error = current_request_ignore_cache_;

    LOG_INFO("[%d] [SyncCacheDataSource::OpenNextSource], initial:%d, current_request_ignore_cache_:%d, cache_->StartRead -> error:%d",
             GetContextId(), initial, current_request_ignore_cache_, error);

    {
        std::lock_guard<std::recursive_mutex> lg(current_data_source_lock_);
        if (span == nullptr) {
            // The data is locked in the cache, or we're ignoring the cache. Bypass the cache and read
            // from upstream.
            current_data_source_ = upstream_data_source_;
            LOG_INFO("[%d] [SyncCacheDataSource::OpenNextSource], use upstream_data_source_",
                     GetContextId());

            ac_rt_info_->cache_ds.cache_upstream_source_cnt++;
            ac_rt_info_->cache_ds.is_reading_file_data_source = false;

            data_spec.WithUri(uri_).WithPositions(read_position_, read_position_)
            .WithLength(bytes_remaining_).WithKey(key_).WithFlags(flags_);
        } else if (span->is_cached) {
            // Data is cached, read from cache
            std::string file_uri = span->file->path();
            int64_t file_position = read_position_ - span->position;
            int64_t length = span->length - file_position; // useful length left in this span
            if (bytes_remaining_ == kLengthUnset) {
                // if bytes_remaining is unset, there is a chance here to get the actual content length,
                // because another data source in another thread has got it.
                bytes_remaining_ = cache_->GetContentLength(key_);
            }
            if (bytes_remaining_ != kLengthUnset) {
                length = std::min(length, bytes_remaining_);
            }

            data_spec.WithUri(file_uri)
            .WithPositions(read_position_, file_position)
            .WithLength(length)
            .WithKey(key_)
            .WithFlags(flags_);
            LOG_INFO("[%d] [SyncCacheDataSource::OpenNextSource], use cache_read_data_source_",
                     GetContextId());
            current_data_source_ = cache_read_data_source_;
            ac_rt_info_->cache_ds.cache_read_source_cnt++;
            ac_rt_info_->cache_ds.is_reading_file_data_source = true;
        } else {
            // Data is not cached, and data is not locked, read from upstream with cache backing.
            int64_t length;
            if (span->IsOpenEnded()) {
                length = bytes_remaining_;
            } else {
                length = span->length;
                if (bytes_remaining_ != kLengthUnset) {
                    length = std::min(length, bytes_remaining_);
                }
            }
            data_spec.WithUri(uri_).WithPositions(read_position_, read_position_)
            .WithLength(length).WithKey(key_).WithFlags(flags_);
            if (cache_write_data_source_ != nullptr) {
                LOG_INFO("[%d] [SyncCacheDataSource::OpenNextSource], use cache_write_data_source_",
                         GetContextId());
                current_data_source_ = cache_write_data_source_;
                ac_rt_info_->cache_ds.cache_write_source_cnt++;
            } else {
                LOG_INFO("[%d] [SyncCacheDataSource::OpenNextSource], cache_write_data_source_ = null, use upstream_data_source_ ",
                         GetContextId());
                current_data_source_ = upstream_data_source_;
                ac_rt_info_->cache_ds.cache_upstream_source_cnt++;
            }
            ac_rt_info_->cache_ds.is_reading_file_data_source = false;
        }
    }

    current_request_unbonded_ = data_spec.length == kLengthUnset;
    int64_t result = current_data_source_->Open(data_spec);
    current_spec_ = data_spec;
    snprintf(ac_rt_info_->cache_ds.current_read_uri, DATA_SOURCE_URI_MAX_LEN, "%s", data_spec.uri.c_str());

    if (current_data_source_ == cache_write_data_source_ && result == kResultContentAlreadyCached) {
        int64_t ret = CloseCurrentSource();
        RETURN_ON_ERROR(ret);

        // the span got from cache.StartRead() is a hole span, but in the operation cache.StartReadWrite
        // this position is already cached, so request openNextSource again.
        LOG_INFO("[%d] SyncCacheDataSource::OpenNextSource, result = kResultContentAlreadyCached, to OpenNextSource again",
                 GetContextId());
        result = OpenNextSource(initial);
        RETURN_ANY_WAY(result);
    }

    if (total_bytes_ == kLengthUnset) {
        total_bytes_ = result;
    }
    cached_bytes_ = cache_->GetContentCachedBytes(key_);

    OnDataSourceOpened(result);

    if (result < 0) {
        // if this isn't the initial open call (we had read some bytes) and an unbounded range request
        // failed because of POSITION_OUT_OF_RANGE then mute the exception. We are trying to find the
        // end of the stream.
        if (!initial && current_request_unbonded_) {
            if (result != kResultExceptionDataSourcePositionOutOfRange) {
                // any error other than kResultExceptionDataSourcePositionOutOfRange should be returned to imply an error
                RETURN_ANY_WAY(result);
            } else {
                // ignore kResultExceptionDataSourcePositionOutOfRange; we will deal with this error right behind
            }
        }
    } else if (result > 0) {
        // If we did an unbounded request (which means it's to upstream and
        // bytesRemaining == C.LENGTH_UNSET) and got a resolved length from open() request
        if (current_request_unbonded_ && result != kLengthUnset) {
            bytes_remaining_ = result;
            LOG_DEBUG("[%d] SyncCacheDataSource::Read, to SetContentLength, data_spec.position:%lld,"
                      " bytes_remaining_ = %lld",
                      GetContextId(), data_spec.position, bytes_remaining_);
            result = SetContentLength(data_spec.position + bytes_remaining_);
        } else {
            if (initial) {
                LOG_WARN("[%d] SyncCacheDataSource::Read warning, initial OpenNextDataSource, "
                         "not to set contentLength, data_spec.length:%lld, result:%lld",
                         GetContextId(), data_spec.length, result)
            }
        }
    }

    RETURN_ANY_WAY(result);
}

int64_t SyncCacheDataSource::OpenNextSourceWithCache(bool use_cache, bool initial) {
    current_request_ignore_cache_ = !use_cache;
    int64_t ret = OpenNextSource(initial);
    return ret;
}

AcResultType SyncCacheDataSource::CloseCurrentSource() {
    AcResultType ret = kResultOK;
    {
        std::lock_guard<std::recursive_mutex> lg(current_data_source_lock_);
        if (current_data_source_ != nullptr) {
            ret = current_data_source_->Close();
            stats_->CurrentAppendDataSourceStats(
                ((JsonStats*) current_data_source_->GetStats())->ToJson());
            //close 补齐onDownStopped 状态
            if (current_data_source_ == cache_write_data_source_ || current_data_source_ == upstream_data_source_) {
                HasConnectionInfo* hasConnectionInfo = dynamic_cast<HasConnectionInfo*>(current_data_source_.get());
                if (hasConnectionInfo) {
                    ReportProgress(hasConnectionInfo->GetConnectionInfo());
                    ReportDownloadStopped("CloseCurrentSource", hasConnectionInfo->GetConnectionInfo());
                }
            }
            current_data_source_ = nullptr;
            current_request_unbonded_ = false;
        }
    }
    return ret;
}

AcResultType SyncCacheDataSource::SetContentLength(int64_t length) {
    // if writing into cache
    AcResultType ret = kResultOK;
    if (current_data_source_ == cache_write_data_source_) {
        if (length > cache_->GetContentLength(key_)) {
            LOG_INFO("[%d] [SyncCacheDataSource::SetContentLength] length %lld, success \n", GetContextId(), length);
            // For unbounded request, only set content length, when length is bigger than before.
            ret = cache_->SetContentLength(key_, length);
            ac_rt_info_->cache_ds.total_bytes = length;
        }
    }
    return ret;
}

void SyncCacheDataSource::HandleErrorBeforeReturn(int64_t ret) {
    if (ret >= kResultOK) {
        return;
    }
    if (current_data_source_ == cache_read_data_source_ || IsCacheError(ret)) {
        CacheManager::GetInstance()->onCacheErrorOccur();
    }
    error_code_ = (int32_t)ret;
    stats_->CurrentOnError(error_code_);
}

bool SyncCacheDataSource::IsCacheError(int64_t ret) {
    return ret <= kResultCacheExceptionStart && ret >= kResultCacheExceptionEnd;
}

void SyncCacheDataSource::NotifyBytesRead() {
    if (event_listener_ != nullptr && total_cached_bytes_read_ > 0) {
        event_listener_->OnCachedBytesRead(cache_->GetCacheSpace(), total_cached_bytes_read_);
        total_cached_bytes_read_ = 0;
    }
}

void SyncCacheDataSource::OnClose() {
    MultiDownloadHttpDataSource* source =
        dynamic_cast<MultiDownloadHttpDataSource*>(upstream_data_source_.get());
    if (source) {
        source->ClearDownloadTasks();
    }
}

Stats* SyncCacheDataSource::GetStats() {
    return stats_.get();
}

#pragma CacheDataSource part
bool SyncCacheDataSource::AlmostFullyCached(int64_t cached_bytes, uint64_t total_bytes) {
    return total_bytes == 0 ? false : ((float)cached_bytes / (float)total_bytes > 0.98);
}

void SyncCacheDataSource::DoReportProgressOnCallbackThread(uint64_t position, int64_t total_bytes) {
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
                cache_session_listener_->OnDownloadProgress(iter->end, total_bytes);
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

    if (cache_callback_) {
        cache_callback_->onSessionProgress(callbackInfo_);
    }

    cache_session_listener_->OnDownloadProgress(position, total_bytes);

}

void SyncCacheDataSource::ReportSessionOpened(uint64_t pos, int64_t cached_bytes, int64_t total_bytes) {
    if (!session_opened_) {
        session_open_ts_ms_ = kpbase::SystemUtil::GetCPUTime();
        total_bytes_ = total_bytes;
        should_report_progress_ = not AlmostFullyCached(cached_bytes, total_bytes);
        // 因为SyncCacheDataSource是可以重复打开的，所以这里要reset session_close_reported_的状态
        session_close_reported_ = false;
        if (cache_session_listener_) {
            runloop_->Post([ = ] {
                cache_session_listener_->OnSessionStarted(key_, pos, cached_bytes, total_bytes_);

            });
        }
        session_opened_ = true;
    }
}
void SyncCacheDataSource::ReportProgress(const ConnectionInfo& info) {
    if (cache_session_listener_ && should_report_progress_) {
        int64_t total_bytes = std::max(total_bytes_, (int64_t)info.GetContentLength());
        uint64_t position = current_spec_.position + info.GetDownloadedBytes();
        uint64_t now = kpbase::SystemUtil::GetCPUTime();
        if ((now - last_progress_time_ms_ > progress_cb_interval_ms_ &&
             position - last_progress_pos_ > kProgressBytesThreshold) ||
            position == total_bytes) {
            last_progress_time_ms_ = now;
            last_progress_pos_ = position;
            runloop_->Post([ = ] {
                if (cache_callback_) {
                    callbackInfo_->SetCachedBytes(cached_bytes_);
                    callbackInfo_->SetProgressPosition(position);
                }
                DoReportProgressOnCallbackThread(position, total_bytes);
            });
        }
    }
}

void SyncCacheDataSource::ReportDownloadStarted(uint64_t position, const ConnectionInfo& info) {
    if (cache_callback_) {
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

void SyncCacheDataSource::CheckAndCloseCurrentDataSource(int error_code, DownloadStopReason stop_reason) {
    bool needCloseDataSource = CacheSessionListener::NeedRetryOnStopReason(stop_reason);
    if (error_code != 0 && needCloseDataSource) {
        LOG_ERROR("[%d] SyncCacheDataSource::CheckAndCloseCurrentDataSource, error_code:%d, to CloseCurrentSource()",
                  GetContextId(), error_code);
        CloseCurrentSource();
        error_code_ = error_code;
    }
}

void SyncCacheDataSource::ReportDownloadStopped(const char* tag, const ConnectionInfo& info) {
    if (download_stop_need_to_report_) {
        if (info.GetDownloadedBytes() > 0) {
            downloaded_bytes_ += info.GetDownloadedBytes();
        }
        LOG_DEBUG("[%d] [SyncCacheDataSource::ReportDownloadStopped][from:%s],"
                  " needCloseDataSource:%d , info.error_code:%d, stop_reason:%s data_source_extra_:%s \n",
                  GetContextId(), tag, CacheSessionListener::NeedRetryOnStopReason(info.stop_reason_),
                  info.error_code, CacheSessionListener::DownloadStopReasonToString(info.stop_reason_), data_source_extra_.c_str());

        stop_reason_for_qos_ = info.stop_reason_;
        download_stop_need_to_report_ = false;
        if (cache_session_listener_) {
            runloop_->Post([ = ] {
                cache_session_listener_->OnDownloadStopped(info.stop_reason_,
                                                           info.GetDownloadedBytes(),
                                                           info.transfer_consume_ms_, info.sign,
                                                           info.error_code, info.x_ks_cache,
                                                           info.session_uuid, info.download_uuid, data_source_extra_);
            });

        }
        if (cache_callback_) {
            callbackInfo_->SetDataSourceType(kDataSourceTypeDefault);
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

void SyncCacheDataSource::ReportSessionClosed() {
    // disable the possibility that mulitple thread is reporting session closed,
    // that may cause the PostAndWait event call waiting forever.
    std::lock_guard<std::mutex> lg(close_session_lock_);
    if (cache_session_listener_) {
        if (!session_close_reported_) {
            session_close_reported_ = true;
            should_report_progress_ = false;
            session_close_ts_ms_ = kpbase::SystemUtil::GetCPUTime();
            runloop_->PostAndWait([ = ] {
                cache_session_listener_->OnSessionClosed(
                    error_code_,
                    static_cast<uint64_t>(network_cost_ms_),
                    static_cast<uint64_t>(session_close_ts_ms_ - session_open_ts_ms_),
                    static_cast<uint64_t>(downloaded_bytes_),
                    "",
                    session_opened_);
            });
            session_opened_ = false;
        }
    }
}

#pragma mark new API for download callback

void SyncCacheDataSource::OnDataSourceOpened(int64_t open_ret) {
    std::lock_guard<std::recursive_mutex> lg(current_data_source_lock_);
    if (current_data_source_ == cache_write_data_source_ || current_data_source_ == upstream_data_source_) {
        HasConnectionInfo* hasConnectionInfo = dynamic_cast<HasConnectionInfo*>(current_data_source_.get());
        if (hasConnectionInfo) {
            // 因为这里后面可能会关闭 DataSource，所以用引用是不安全的；
            const ConnectionInfo& info = hasConnectionInfo->GetConnectionInfo();
            OnUpstreamDataSourceOpened(read_position_, info);
            if (open_ret < 0) {
                CheckAndCloseCurrentDataSource(info.error_code, info.stop_reason_);
            }
        }
    }
}

void SyncCacheDataSource::OnDataSourceReadResult(int64_t read_ret) {
    std::lock_guard<std::recursive_mutex> lg(current_data_source_lock_);
    if (current_data_source_ == cache_write_data_source_ || current_data_source_ == upstream_data_source_) {
        HasConnectionInfo* hasConnectionInfo = dynamic_cast<HasConnectionInfo*>(current_data_source_.get());
        if (hasConnectionInfo) {
            const ConnectionInfo& connectionInfo = hasConnectionInfo->GetConnectionInfo();
            OnUpstreamDataSourceRead(read_ret, connectionInfo);
        }
    }
}

void SyncCacheDataSource::OnUpstreamDataSourceOpened(uint64_t position, const ConnectionInfo& info) {
    error_code_ = info.error_code;
    last_download_start_time_ = kpbase::SystemUtil::GetCPUTime();
    is_update_abr_ = false;
    ReportDownloadStarted(position, info);
}

void SyncCacheDataSource::OnUpstreamDataSourceRead(int64_t read_ret, const ConnectionInfo& info) {
    if (read_ret < 0) {
        if (info.error_code != 0) {
            if (enable_vod_adaptive_ && info.GetDownloadedBytes() > 0 && !is_update_abr_) {
                AbrUpdateDownloadInfo(last_download_start_time_, info.GetDownloadedBytes());
                is_update_abr_ = true;
            }
            CheckAndCloseCurrentDataSource(info.error_code, info.stop_reason_);
        } else {
            LOG_ERROR_DETAIL("[%d] SyncCacheDataSource::OnUpstreamDataSourceRead, read_ret:%lld, "
                             "info.stop_reason:%d, not call ReportDownloadStopped",
                             GetContextId(), read_ret, info.stop_reason_);
        }
    } else if (read_ret > 0) {
        ReportProgress(info);
        if (info.connection_closed || info.IsDownloadComplete()) {
            if (enable_vod_adaptive_ && info.GetDownloadedBytes() > 0 && !is_update_abr_) {
                AbrUpdateDownloadInfo(last_download_start_time_, info.GetDownloadedBytes());
                is_update_abr_ = true;
            }
            ReportDownloadStopped("OnUpstreamDataSourceRead finish", info);
        } else {
            if (enable_vod_adaptive_ && info.GetDownloadedBytes() > curl_buffer_size_kb_ && !is_update_abr_) {
                AbrUpdateDownloadInfo(last_download_start_time_, info.GetDownloadedBytes());
                is_update_abr_ = true;
            }
            CheckAndCloseCurrentDataSource(info.error_code, info.stop_reason_);
        }
    } else {
        LOG_ERROR_DETAIL("[%d] [SyncCacheDataSource::OnUpstreamDataSourceRead], other error, read_ret:%lld \n",
                         GetContextId(), read_ret);
        assert(0);
    }
}

void SyncCacheDataSource::AbrUpdateDownloadInfo(uint64_t start_time_ms, uint64_t bytes_transferred) {
    kuaishou::abr::DownloadSampleInfo last_sample_info;
    uint64_t now_ms = kpbase::SystemUtil::GetCPUTime();

    last_sample_info.begin_timestamp = start_time_ms;
    last_sample_info.end_timestamp = now_ms;
    last_sample_info.total_bytes = bytes_transferred;
    kuaishou::abr::AbrEngine::GetInstance()->UpdateDownloadInfo(last_sample_info);

    LOG_DEBUG("[%d] [AbrUpdateDownloadInfo], start_time_ms:%lld, now:%lld, diff:%lld, bytes_transferred:%lld \n",
              GetContextId(), start_time_ms, now_ms,
              now_ms - start_time_ms, bytes_transferred);
    ac_rt_info_->vod_adaptive.real_time_throughput_kbps = kuaishou::abr::AbrEngine::GetInstance()->get_real_time_throughput();
    ac_rt_info_->vod_adaptive.consumed_download_time_ms += now_ms - start_time_ms;
    ac_rt_info_->vod_adaptive.actual_video_size_byte += bytes_transferred;
}

#undef RETURN_ON_ERROR
#undef RETURN_ANY_WAY

}
}
