//
// Created by MarshallShuai on 2019-07-01.
//

#include <io_stream.h>
#include <memory>
#include <utility>
#include <include/awesome_cache_runtime_info_c.h>
#include "ac_log.h"
#include "file.h"
#include "async_scope_data_source.h"
#include "v2/cache/cache_v2_settings.h"
#include "v2/cache/cache_content_index_v2.h"
#include "v2/cache/cache_def_v2.h"
#include "v2/data_sink/cache_scope_data_sink.h"
#include "v2/cache/cache_v2_file_manager.h"
#include "hodor_downloader/hodor_downloader.h"
#include "runloop.h"

namespace kuaishou {
namespace cache {

static const bool kVerbose = false;
static const bool kInfo = false;

AsyncScopeDataSource::AsyncScopeDataSource(
    const DownloadOpts& opts,
    std::shared_ptr<CacheSessionListener> listener,
    AwesomeCacheCallback_Opaque ac_callback,
    AwesomeCacheRuntimeInfo* ac_rt_info):
    download_opts_(opts),
    // buffer manage
    scope_buf_len_(0),
    scope_buf_(nullptr),
    // callback
    last_notify_progress_ts_ms_(0),
    ac_callback_(static_cast<AwesomeCacheCallback*>(ac_callback)),
    cache_session_listener_(std::move(listener)),  // old callback
    // runtime info
    ac_rt_info_(ac_rt_info) {

    // 暂时不用Opt里的
    progress_cb_interval_ms_ = kMinAcCallbackProgressIntervalMs;
    if (ac_rt_info_) {
        ac_rt_info_->cache_ds.progress_cb_interval_ms = progress_cb_interval_ms_;
    }

    callback_runloop_.reset(new kpbase::Runloop("AsyncScopeDataSource"));
    ResetBufferStatus();
    if (download_opts_.is_sf2020_encrypt_source) {
        if (download_opts_.sf2020_aes_key.size() != AES_128_BIT_KEY_BYTES) {
            LOG_ERROR("[AsyncScopeDataSource::AsyncScopeDataSource] download_opts_.sf2020_aes_key.size() not valid:%d",
                      download_opts_.sf2020_aes_key.size());
            download_opts_.is_sf2020_encrypt_source = false;
        } else {
            aes_dec_ = std::make_shared<AesDecrypt>((uint8_t*)download_opts_.sf2020_aes_key.c_str());
        }
    }
}

AsyncScopeDataSource::~AsyncScopeDataSource() {
    // Clang-Tidy: 'if' statement is unnecessary; deleting null pointer has no effect
    delete [] scope_buf_;

    if (callback_runloop_) {
        callback_runloop_->Stop();
    }
}

void AsyncScopeDataSource::ResetBufferStatus() {
    init_cache_buf_len_ = 0;
    valid_cache_buf_len_ = 0;
    current_read_offset_ = 0;
    total_cached_bytes_when_open_ = 0;
}

AcResultType AsyncScopeDataSource::WaitForDownloadFinish() {
    if (download_task_) {
        download_task_->WaitForTaskFinish();
    }
    return last_error_;
}

void AsyncScopeDataSource::Abort() {
    std::lock_guard<std::mutex> lg(download_task_mutex_);
    if (download_task_) {
        download_task_->Abort();
    }
}

int64_t AsyncScopeDataSource::Open(const std::shared_ptr<CacheScope> scope, std::string& url, int64_t expect_consume_length) {
    // buffer manage
    ResetBufferStatus();
    last_error_ = kResultOK;

    cache_scope_ = scope;

    // reset scope_buf_ and scope_buf_len_
    int64_t new_scope_buf_len = 0;
    if (scope->GetContentLength() > 0) {
        new_scope_buf_len = scope->GetActualLength();
    } else {
        new_scope_buf_len = scope->GetScopeMaxSize();
    }
    assert(new_scope_buf_len > 0);
    if (new_scope_buf_len != scope_buf_len_) {
        if (scope_buf_) {
            delete[] scope_buf_;
        }
        scope_buf_ = new (std::nothrow) uint8_t[new_scope_buf_len];
        scope_buf_len_ = new_scope_buf_len;
    }
    last_error_ = scope_buf_ != nullptr ? kResultOK : kAsyncCacheV2NoMemory;

    if (ac_rt_info_) {
        ac_rt_info_->cache_v2_info.scope_max_size_kb_of_settting =
            static_cast<int>(CacheV2Settings::GetScopeMaxSize() / KB);
        ac_rt_info_->cache_v2_info.scope_max_size_kb_of_cache_content =
            static_cast<int>(cache_scope_->GetScopeMaxSize() / KB);
    }
    if (ac_callback_) {
        ac_cb_info_ = std::shared_ptr<AcCallbackInfo>(AcCallbackInfoFactory::CreateCallbackInfo());
        ac_cb_info_->SetDataSourceType(kDataSourceTypeAsyncV2);
        ac_cb_info_->SetCacheKey(cache_scope_->GetKey());
    }

    url_ = url;

    if (last_error_ < 0) {
        LOG_ERROR("[%d,AsyncScopeDataSource::Open，return kAsyncCacheV2NoMemory", context_id_);
        return last_error_;
    }

    if (cache_scope_->GetContentLength() > 0) {
        content_length_unknown_on_open_ = false;
        if (ac_cb_info_) {
            ac_cb_info_->SetTotalBytes(cache_scope_->GetContentLength());
        }
        // 已知content-length，acutual_length就能算出来了
        auto b_ret = TryResumeFromScopeCacheFile();
        if (!b_ret && ac_rt_info_) {
            ac_rt_info_->cache_v2_info.resume_file_fail_cnt++;
        }
        if (kInfo) {
            LOG_INFO("[%d][AsyncScopeDataSource::Open]，已缓存文件数据/scope：%dKB / %dKB",
                     context_id_, init_cache_buf_len_ / 1024, cache_scope_->GetActualLength() / 1024);
        }
        expect_consume_length_ = expect_consume_length > 0 ?
                                 std::min(cache_scope_->GetActualLength(), expect_consume_length)
                                 : cache_scope_->GetActualLength();

        if (valid_cache_buf_len_ >= expect_consume_length_) {
            if (kVerbose) {
                LOG_DEBUG("[%d][AsyncScopeDataSource::Open] all data is from cache data ,no need to download :)",
                          context_id_);
            }

            if (ac_rt_info_) {
                ac_rt_info_->cache_ds.is_reading_file_data_source = true;
            }

            goto OPEN_SUCCESS;
        } else {
            int64_t ret = OpenDownloadTask(cache_scope_->GetStartPosition() + valid_cache_buf_len_,
                                           expect_consume_length_ - valid_cache_buf_len_);
            if (ret < 0) {
                last_error_ = static_cast<int32_t>(ret);
                LOG_ERROR("[%d][AsyncScopeDataSource::Open] OpenDownloadTask()(content-length known on open) error:%lld", context_id_, ret);
                return ret;
            } else {
                goto OPEN_SUCCESS;
            }
        }
    } else {
        if (valid_cache_buf_len_ > 0) {
            LOG_WARN("[%d][AsyncScopeDataSource::Open] 没有ContentLength，竟然有缓存的文件分片，应该是CacheContentIndex被破坏了", context_id_);
            if (valid_cache_buf_len_ > expect_consume_length) {
                expect_consume_length_ = expect_consume_length;
                goto OPEN_SUCCESS;
            }
        }
        if (kInfo) {
            LOG_INFO("[%d][AsyncScopeDataSource::Open]，无任何缓存文件数据，scope: %.1fKB, scope max len:%.1fKB",
                     context_id_, init_cache_buf_len_  * 1.f / 1024, cache_scope_->GetScopeMaxSize() * 1.f / 1024);
        }

        content_length_unknown_on_open_ = true;
        expect_consume_length_ = expect_consume_length > 0
                                 ? std::min(expect_consume_length, cache_scope_->GetScopeMaxSize())
                                 : cache_scope_->GetScopeMaxSize();
        int64_t ret = OpenDownloadTask(cache_scope_->GetStartPosition() + valid_cache_buf_len_, expect_consume_length_);
        if (ret < 0) {
            last_error_ = static_cast<int32_t>(ret);
            LOG_ERROR("[%d][AsyncScopeDataSource::Open] OpenDownloadTask()(content-length unknown on open) error:%lld", context_id_, ret);
            return ret;
        } else {
            goto OPEN_SUCCESS;
        }
    }

OPEN_SUCCESS:

    total_cached_bytes_when_open_ = cache_scope_->GetBelongingCacheContent()->GetCachedBytes();
    // 新的callback
    if (ac_callback_) {
        ac_cb_info_->SetCachedBytes(total_cached_bytes_when_open_);
        //todo，预加载时ac_rt_info_为空，callback暂停不上报upstreamType、httpVersion，后续考虑修复
        if (ac_rt_info_) {
            ac_cb_info_->SetUpstreamType(ac_rt_info_->cache_applied_config.upstream_type);
            ac_cb_info_->SetHttpVersion(ac_rt_info_->download_task.http_version);
        }
    }
    ThrottleNotifyProgressToCallbacks(total_cached_bytes_when_open_);

    if (ac_rt_info_) {
        ac_rt_info_->cache_ds.async_v2_cached_bytes = total_cached_bytes_when_open_;
        ac_rt_info_->cache_ds.cached_bytes = total_cached_bytes_when_open_;
        if (ac_rt_info_->cache_v2_info.cached_bytes_on_play_start < 0) {
            LOG_DEBUG("total_cached_bytes_when_open_:%lld", total_cached_bytes_when_open_);
            ac_rt_info_->cache_v2_info.cached_bytes_on_play_start = total_cached_bytes_when_open_;
        }
    }

    if (download_opts_.is_sf2020_encrypt_source && cache_scope_->GetContentLength() % AES_BLOCK_LEN != 0) {
        LOG_ERROR("[AsyncScopeDataSource::Open]enctrypt file is not valid:%lld, should be multiple of %d",
                  cache_scope_->GetContentLength(), AES_BLOCK_LEN);
        return kAsyncScopeAesEcryptSourceLengthInvalid;
    }

    return cache_scope_->GetContentLength();
}

int64_t AsyncScopeDataSource::Read(uint8_t* buf, int64_t offset, int64_t read_len) {
    return download_opts_.is_sf2020_encrypt_source ? ReadCrypt(buf, offset, read_len) : ReadPlain(buf, offset, read_len);

}

int64_t AsyncScopeDataSource::Seek(int64_t pos) {
    return download_opts_.is_sf2020_encrypt_source ? SeekCrypt(pos) : SeekPlain(pos);
}

void AsyncScopeDataSource::Close() {
    if (download_task_) {
        download_task_->Close();
    }
    if (kInfo) {
        LOG_INFO("[%d][AsyncScopeDataSource::Close]，init cached len: %.1fKB | download len:%.1fKB | total len %.1fKB",
                 context_id_,  init_cache_buf_len_ * 1.f / 1024,
                 (valid_cache_buf_len_ - init_cache_buf_len_) * 1.f / 1024,  valid_cache_buf_len_ * 1.f / 1024);
    }
}

int64_t AsyncScopeDataSource::GetStartPosition() {
    return cache_scope_->GetStartPosition();
}

int64_t AsyncScopeDataSource::GetEndPosition() {
    return cache_scope_->GetEndPosition();
}

bool AsyncScopeDataSource::ContainsPosition(int64_t pos) {
    return pos >= cache_scope_->GetStartPosition() && pos <= cache_scope_->GetEndPosition();
}


void AsyncScopeDataSource::PostEventInCallbackThread(function<void()> func, bool shouldWait) {
    if (shouldWait) {
        callback_runloop_->PostAndWait([ = ] {
            func();
        });
    } else {
        callback_runloop_->Post([ = ] {
            func();
        });
    }
}

bool AsyncScopeDataSource::TryResumeFromScopeCacheFile() {
    kpbase::File file;

    auto file_path = cache_scope_->GetCacheFilePath();
    if (file_path.empty()) {
        LOG_ERROR("[AsyncScopeDataSource::TryResumeFromScopeCacheFile] file_name should not be null, key:%s",
                  cache_scope_->GetKey().c_str());
        // 读文件失败，直接返回
        return false;
    }

    file = kpbase::File(cache_scope_->GetCacheFilePath());
    if (!file.Exists()) {
        // 文件不存在是正常现象，不是异常，应该返回true
        return true;
    }

    auto file_length = file.file_size();
    if (file_length < 0) {
        LOG_ERROR_DETAIL("[AsyncScopeDataSource::TryResumeFromScopeCacheFile]file_length invalid:%lld, path:%s",
                         file_length, file.path().c_str());
        file.Remove();
        return false;
    } else if (file_length > cache_scope_->GetActualLength()) {
        LOG_ERROR_DETAIL("[AsyncScopeDataSource::TryResumeFromScopeCacheFile]file_length(%lld) > ScopeActualLength(%lld), path:%s",
                         file_length, cache_scope_->GetActualLength(), file.path().c_str());
        file.Remove();
        return false;
    }

    auto input_stream = kpbase::InputStream(file);
    auto acutal_read_len = input_stream.Read(scope_buf_, 0, cache_scope_->GetActualLength());

    if (acutal_read_len < 0) {
        LOG_ERROR_DETAIL("[AsyncScopeDataSource::TryResumeFromScopeCacheFile] input_stream.Read, error:%lld, path:%s",
                         acutal_read_len, file.path().c_str());
        file.Remove();
        return false;
    }
    if (acutal_read_len != file_length) {
        // 做一次多余校验吧，放错
        LOG_ERROR_DETAIL("[AsyncScopeDataSource::TryReadCacheScopeFile] acutal_read_len(%lld) != file_length(%lld), path:%s",
                         acutal_read_len, file_length, file.path().c_str());
        file.Remove();
        return false;
    }

    valid_cache_buf_len_ = init_cache_buf_len_ = acutal_read_len;
    return true;
}

// todo 这逻辑以后可以抽象到CacheScope内部，来和ClearFile成对实现。这样可以统一管理给CacheV2FileManager的通知
bool AsyncScopeDataSource::FlushToScopeCacheFileIfNeeded() {
    if (init_cache_buf_len_ >= valid_cache_buf_len_) {
        return true;
    }

    // 防止文件夹被删的恢复机制
    if (!CacheV2Settings::MakeSureMediaCacheDirExists()) {
        LOG_ERROR("[%d][AsyncScopeDataSource::FlushToScopeCacheFileIfNeeded] MakeSureMediaCacheDirExists fail",
                  context_id_);
        return false;
    }

    CacheScopeDataSink sink(cache_scope_);
    if (sink.Open() != kResultOK) {
        LOG_ERROR("[%d][AsyncScopeDataSource::FlushToScopeCacheFileIfNeeded] file_name is null, can not flush, key:%s",
                  context_id_, cache_scope_->GetKey().c_str());
        return false;
    }

    auto ret = sink.Write(scope_buf_, 0, valid_cache_buf_len_);
    if (ret < 0) {
        LOG_ERROR("[%d][AsyncScopeDataSource::FlushToScopeCacheFileIfNeeded] write file fail, error:%d",
                  context_id_, ret);
        return false;
    } else {
        ret = CacheV2FileManager::GetMediaDirManager()->CommitScopeFile(*cache_scope_);
        return ret == kResultOK;
    }
}

void AsyncScopeDataSource::SetContextId(int id) {
    context_id_ = id;
}

int64_t AsyncScopeDataSource::OpenDownloadTask(int64_t range_start, int64_t expect_length) {
    if (kVerbose) {
        LOG_INFO("[%d][ AsyncScopeDataSource::OpenDownloadTask] %lld~%lld, expect_length:%lld ", context_id_,
                 range_start, range_start + expect_length - 1, expect_length);
    }

    if (ac_rt_info_) {
        ac_rt_info_->cache_ds.is_reading_file_data_source = false;
    }

    if (!download_task_ || !download_task_->CanReopen()) {
        // Abort接口会在不同线程访问download_task_，所以需要加锁
        std::lock_guard<std::mutex> lg(download_task_mutex_);
        download_task_ = ScopeTask::CreateTask(download_opts_, this, ac_rt_info_);
    }
    // reset this event for every download task, otherwise, the old signals may still be pending
    download_update_event_.reset(new kpbase::Event());

    if (!download_task_) {
        LOG_ERROR("[%d][AsyncCacheDataSourceV2::Open] alloc ScopeCurlHttpTask fail",
                  download_opts_.context_id);
        last_error_ = kAsyncCacheV2NoMemory;
        return kAsyncCacheV2NoMemory;
    }

    DataSpec spec = DataSpec().WithUri(url_)
                    .WithPosition(range_start).WithLength(expect_length);

    if (ac_callback_) {
        ac_cb_info_->SetCurrentUri(url_);
    }

    int64_t ret = download_task_->Open(spec);
    if (ret < 0) {
        last_error_ = static_cast<int32_t>(ret);
        LOG_ERROR("[%d][ AsyncScopeDataSource::OpenDownloadTask] download_task_->Open fail:%d ", context_id_, last_error_);
        return last_error_;
    }

    if (cache_scope_->GetContentLength() <= 0) {
        // contentLength都没有，说明dataSource从未打开过，需要等网络连接回来的
        download_update_event_->Wait();
    }
    if (last_error_ != kResultOK) {
        LOG_ERROR("[%d][AsyncScopeDataSource::OpenDownloadTask] after download_task_->Open  download_task_->Open back, error:%d ",
                  context_id_, last_error_);
        return last_error_;
    } else if (cache_scope_->GetContentLength() > 0) {
        return cache_scope_->GetContentLength();
    } else {
        LOG_ERROR("[%d][AsyncScopeDataSource::OpenDownloadTask](kAsyncCacheInnerError_10) "
                  "after download_task_->Open  last_error_:%d,  cache_scope_->GetContentLength():%lld",
                  context_id_, last_error_, cache_scope_->GetContentLength());
        return kAsyncCacheInnerError_10;
    }
}

void AsyncScopeDataSource::OnConnectionInfoParsed(const ConnectionInfoV2& info) {
    if (info.content_length > 0) {
        if (ac_callback_) {
            ac_cb_info_->SetContentLength(info.content_length);
        }
        cache_scope_->UpdateContentLength(info.GetFileLength());

        // 有可能 cache_scope_->GetActualLength() < expect_consume_length_，这个时候需要更新下expect_consume_length_， 不过一般来说，逻辑上保证了不太可能发生
        if (content_length_unknown_on_open_) {
            expect_consume_length_ = std::min(cache_scope_->GetActualLength(), expect_consume_length_);
            CacheV2FileManager::GetMediaDirManager()->Index()->PutCacheContent(cache_scope_->GetBelongingCacheContent());

            if (ac_cb_info_) {
                ac_cb_info_->SetTotalBytes(cache_scope_->GetContentLength());
            }
        }
    } else {
        // no need to set last_error_, we will catch this error in Open and return kAsyncCacheInnerError_6
        LOG_ERROR("[%d][ AsyncScopeDataSource::OnConnectionInfoParsed], connection_info.content_length(%lld) invalid!",
                  context_id_, info.content_length);
    }

    // 兼容老的callback
    if (cache_session_listener_) {
        callback_runloop_->Post([this, &info] {
            cache_session_listener_->OnDownloadStarted(static_cast<uint64_t>(total_cached_bytes_when_open_),
                                                       info.uri, info.http_dns_host, info.ip,
                                                       info.response_code, static_cast<uint64_t>(info.connection_used_time_ms));
        });
    }

    // 这里需要通知一下OpenDownloadTask的Wait信号
    download_update_event_->Signal();

    if (kVerbose) {
        LOG_DEBUG("[%d][AsyncScopeDataSource::OnConnectionInfoParsed] GetContentLength:%lld", context_id_, info.content_length);
    }
}

void AsyncScopeDataSource::OnReceiveData(uint8_t* data_buf, int64_t data_len) {
    size_t to_copy = static_cast<size_t>(std::min(data_len, scope_buf_len_ - valid_cache_buf_len_));
    if (data_len > to_copy) {
        // 这块逻辑上不会走到，目前打个日志观察下
        LOG_ERROR("[%d][AsyncScopeDataSource::OnReceiveData]wanring receive data_len(%lld) 大于 剩余scope buffer长度(%lld)",
                  context_id_, data_len, to_copy);
    }
    memcpy(scope_buf_ + valid_cache_buf_len_, data_buf, to_copy);
    valid_cache_buf_len_ += to_copy;
    debug_.total_recv_len_ += data_len;
    download_update_event_->Signal();

    // notify progress
    auto total_progress_bytes = total_cached_bytes_when_open_ + (valid_cache_buf_len_ - init_cache_buf_len_);

    ThrottleNotifyProgressToCallbacks(total_progress_bytes);

    if (ac_rt_info_) {
        ac_rt_info_->cache_ds.async_v2_cached_bytes = total_progress_bytes;
        ac_rt_info_->cache_ds.cached_bytes = total_progress_bytes;
    }
}

void AsyncScopeDataSource::OnDownloadComplete(int32_t error, int32_t stop_reason) {
    // todo 这里可以实现retry逻辑
    if (error != 0) {
        last_error_ = error;
    } else if (stop_reason != kDownloadStopReasonCancelled && expect_consume_length_ != valid_cache_buf_len_) {
        // 不应该走到这的，没出错，又没下载完？以后如果能出现，看下是什么导致的
        // 如果content_length是0，是有可能走到这的，因为不会走write_callback流程。同时curl_ret也是返回0的。
        LOG_ERROR("[%d][AsyncScopeDataSource::OnDownloadComplete] should not got here, stop_reason:%lld, expect_consume_length_:%lld, valid_cache_buf_len_:%lld",
                  context_id_, stop_reason, expect_consume_length_, valid_cache_buf_len_)
        last_error_ = kAsyncCacheInnerError_9;
    }

    if (kVerbose) {
        LOG_DEBUG("[%d][AsyncScopeDataSource::OnDownloadComplete] error:%d, stop_reason:%d",
                  context_id_, error, stop_reason);
    }

    auto b_ret = FlushToScopeCacheFileIfNeeded();
    if (!download_opts_.md5_hash_code.empty() && cache_scope_->GetBelongingCacheContent()->IsFullyCached()) {
        AcResultType ret = cache_scope_->GetBelongingCacheContent()->VerifyMd5(download_opts_.md5_hash_code);
        if (ret < 0) {
            stop_reason = kDownloadStopReasonFailed;
            last_error_ = error = ret;
            cache_scope_->GetBelongingCacheContent()->DeleteScopeFiles();
        }
    }
    // update RuntimeInfo
    if (ac_rt_info_) {
        ac_rt_info_->cache_ds.is_reading_file_data_source = true;
        if (!b_ret) {
            ac_rt_info_->cache_v2_info.flush_file_fail_cnt++;
        }
    }

    download_update_event_->Signal();

    if (kVerbose) {
        LOG_VERBOSE("[AsyncScopeDataSource::OnDownloadComplete]  valid_cache_buf_len_:%lld, debug_.total_recv_len_:%lld", valid_cache_buf_len_, debug_.total_recv_len_);
    }

    ResumeHodorDownloaderIfNeeded();

    // AwesomeCacheCallback:notify finish
    if (ac_callback_) {
        ac_cb_info_->SetStopReason(static_cast<DownloadStopReason>(stop_reason));
        ac_cb_info_->SetErrorCode(error);
        ac_cb_info_->SetProductContext(download_opts_.product_context);
        auto total_progress_bytes = total_cached_bytes_when_open_ + (valid_cache_buf_len_ - init_cache_buf_len_);
        ac_cb_info_->SetProgressPosition(total_progress_bytes);

        AcCallbackInfo::CopyConnectionInfoIntoAcCallbackInfo(
            download_task_->GetConnectionInfo(), *ac_cb_info_);

        callback_runloop_->PostAndWait([ = ] {
            ac_callback_->onDownloadFinish(ac_cb_info_);
        });
    }
    // 兼容老的CacheSessionListener:notify finish
    if (cache_session_listener_) {
        // LOG_DEBUG("[AsyncScopeDataSource::OnDownloadComplete] stop_reason:%d", stop_reason);
        callback_runloop_->PostAndWait([ = ] {
            ConnectionInfoV2 const& info = download_task_->GetConnectionInfo();
            cache_session_listener_->OnDownloadStopped(static_cast<DownloadStopReason>(stop_reason),
                                                       static_cast<uint64_t>(valid_cache_buf_len_ - init_cache_buf_len_),
                                                       static_cast<uint64_t>(info.transfer_consume_ms),
                                                       info.sign, error,
                                                       info.x_ks_cache, info.session_uuid, info.download_uuid,
                                                       download_opts_.datasource_extra_msg);
        });
    }

}

void AsyncScopeDataSource::ThrottleNotifyProgressToCallbacks(int64_t total_progress_bytes) {
    // throttle
    auto cur_ts = kpbase::SystemUtil::GetCPUTime();
    if (cur_ts - last_notify_progress_ts_ms_ > progress_cb_interval_ms_) {
        if (kVerbose) {
            LOG_DEBUG("[OnReceiveData] cur_ts(%llu) - last_notify_progress_ts_ms_(%lld):%lld, progress_cb_interval_ms_:%lld", cur_ts, last_notify_progress_ts_ms_, (int64_t)(
                          cur_ts - last_notify_progress_ts_ms_), progress_cb_interval_ms_);
        }

        last_notify_progress_ts_ms_ = cur_ts;

        // 新的callback
        if (ac_callback_) {
            ac_cb_info_->SetProgressPosition(total_progress_bytes);
            NotifyProgressToAcCallback();
        }


//        LOG_DEBUG("to call [NotifyProgressToCacheSessionListener][after OnReceiveData]");
        // 兼容老的callback
        NotifyProgressToCacheSessionListener(total_progress_bytes);
    }
}

void AsyncScopeDataSource::NotifyProgressToAcCallback() {
    if (ac_callback_) {
        callback_runloop_->Post([&] {
            ac_callback_->onSessionProgress(ac_cb_info_);
        });
    }
}

void AsyncScopeDataSource::NotifyProgressToCacheSessionListener(int64_t total_progress_bytes) {
    if (cache_session_listener_) {
        callback_runloop_->Post([total_progress_bytes, this] {
            cache_session_listener_->OnDownloadProgress(static_cast<uint64_t>(total_progress_bytes),
                                                        static_cast<uint64_t>(cache_scope_->GetContentLength()));
        });
    }
}


void AsyncScopeDataSource::ResumeHodorDownloaderIfNeeded() {
    if (usage_ == PlayerDataSource) {
        auto fully_cached = cache_scope_->GetBelongingCacheContent()->IsFullyCached(false);
//        LOG_DEBUG("AsyncScopeDataSource::ResumeHodorDownloaderIfNeeded] fully_cached:%d, key:%s",
//                  fully_cached, cache_scope_->GetKey().c_str());
        if (fully_cached) {
            HodorDownloader::GetInstance()->GetTrafficCoordinator()->OnPlayerDownloadFinish(cache_scope_->GetKey(),
                                                                                            last_error_ != kResultOK && !is_cache_abort_by_callback_error_code(last_error_));
        }
    }
}

int64_t AsyncScopeDataSource::ReadPlain(uint8_t* buf, int64_t offset, int64_t read_len) {
    if (current_read_offset_ > cache_scope_->GetEndPosition()) {
        last_error_ = kAsyncScopeEOF;
        return last_error_;
    }

    while (valid_cache_buf_len_ <= current_read_offset_ && last_error_ == kResultOK) {
        download_update_event_->Wait();
    }
    if (last_error_ != kResultOK) {
        LOG_ERROR("[AsyncScopeDataSource::Read] error happed already, last_error:%d", last_error_);
        return last_error_;
    }

    int64_t to_read = std::min(read_len, (valid_cache_buf_len_ - current_read_offset_));

    memcpy(buf + offset, scope_buf_ + current_read_offset_, to_read);
    debug_.total_output_len_ += to_read;
    current_read_offset_ += to_read;

    // LOG_DEBUG("[AsyncScopeDataSource::Read] after read , read_len :%lld, current_read_offset_:%lld", to_read, current_read_offset_);

    return to_read;
}

int64_t AsyncScopeDataSource::SeekPlain(int64_t pos) {
    if (pos >= expect_consume_length_) {
        last_error_ = kAsyncScopeSeekPosExceedInputExpectLen;
        return last_error_;
    }

    if (pos >= valid_cache_buf_len_) {
        if (pos > cache_scope_->GetEndPosition()) {
            last_error_ = kAsyncScopeSeekPosOverflow;
            return kAsyncScopeSeekPosOverflow;
        }

        while (valid_cache_buf_len_ <= pos
               && last_error_ == kResultOK) {
            if (kVerbose) {
                LOG_DEBUG("[%d][AsyncScopeDataSource::Seek] continue wait, valid_cache_buf_len_:%lld, pos:%lld",
                          context_id_, valid_cache_buf_len_, pos);
            }
            download_update_event_->Wait();
        }

        if (kVerbose) {
            LOG_INFO("[%d][AsyncScopeDataSource::Seek] success, valid_cache_buf_len_:%lld, return pos:%lld",
                     context_id_, valid_cache_buf_len_, pos);
        }
    }
    if (last_error_ != kResultOK) {
        return last_error_;
    } else {
        current_read_offset_ = pos;
        return pos;
    }
}


void print_hex_with_len(uint8_t* str, int len) {
    for (int i = 0; i < len; ++i)
        printf("%.2x", str[i]);
    printf("\n");
}

int64_t AsyncScopeDataSource::ReadCrypt(uint8_t* buf, int64_t offset, int64_t read_len) {
#if (PROFILE_AES_DEC)
    auto t0 = kpbase::SystemUtil::GetCPUTime();
#endif
    if (current_read_offset_ > cache_scope_->GetEndPosition()) {
        last_error_ = kAsyncScopeEOF;
        return last_error_;
    }

    while (last_error_ == kResultOK
           && (valid_cache_buf_len_ / AES_BLOCK_LEN * AES_BLOCK_LEN <= current_read_offset_)) {
        download_update_event_->Wait();
    }

    if (last_error_ != kResultOK) {
        LOG_ERROR("[%d][AsyncScopeDataSource::Read] error happed already, last_error:%d", context_id_, last_error_);
        return last_error_;
    }

    int64_t to_read_max_len = std::min(read_len, (valid_cache_buf_len_ / AES_BLOCK_LEN * AES_BLOCK_LEN - current_read_offset_));

    int64_t total_read_len = 0;

    // 前序不完整的单个分片
    if (current_read_offset_ % AES_BLOCK_LEN != 0) {
        int64_t pre_block_abandon_len = 0;
        // 读指针位置不是 block整数倍，需要把对齐到证书倍，把这非整数倍的前面几个字节先解码出来
        uint8_t block[AES_BLOCK_LEN];
        memcpy(block, scope_buf_ + current_read_offset_ / AES_BLOCK_LEN * AES_BLOCK_LEN, AES_BLOCK_LEN);

        auto ret = aes_dec_->Decrypt(block, AES_BLOCK_LEN);

        if (ret <= 0) {
            LOG_ERROR("[%d][AsyncScopeDataSource::ReadCrypt] Decrypt block fail, ret:%lld, will return kAsyncScopeDecryptFail(%lld)",
                      context_id_, ret, kAsyncScopeDecryptFail);
            return kAsyncScopeDecryptFail;
        }
        pre_block_abandon_len = static_cast<int>(current_read_offset_ % AES_BLOCK_LEN);
        memcpy(buf + offset, block + pre_block_abandon_len, static_cast<size_t>(AES_BLOCK_LEN -
                                                                                pre_block_abandon_len));
        total_read_len += AES_BLOCK_LEN - pre_block_abandon_len;
    }

    // 中间的若干个完整分片
    if (to_read_max_len - total_read_len > AES_BLOCK_LEN) {
        auto next_read_len = (to_read_max_len - total_read_len) / AES_BLOCK_LEN * AES_BLOCK_LEN;
        memcpy(buf + offset + total_read_len, scope_buf_ + current_read_offset_ + total_read_len, next_read_len);
        auto ret = aes_dec_->Decrypt(buf + offset + total_read_len, to_read_max_len - total_read_len);
        if (ret <= 0) {
            LOG_ERROR("[%d][AsyncScopeDataSource::ReadCrypt] Decrypt fail, ret:%lld, will return kAsyncScopeDecryptFail(%lld)",
                      context_id_, ret, kAsyncScopeDecryptFail);
            return kAsyncScopeDecryptFail;
        }
        total_read_len += next_read_len;
    }

    // 完整分片后续的非完整的分片
    if (to_read_max_len > total_read_len) {
        uint8_t block[AES_BLOCK_LEN];
        memcpy(block, scope_buf_ + current_read_offset_ + total_read_len, AES_BLOCK_LEN);
        auto ret = aes_dec_->Decrypt(block, AES_BLOCK_LEN);
        if (ret <= 0) {
            LOG_ERROR("[%d][AsyncScopeDataSource::ReadCrypt] Decrypt block fail, ret:%lld, will return kAsyncScopeDecryptFail(%lld)",
                      context_id_, ret, kAsyncScopeDecryptFail);
            return kAsyncScopeDecryptFail;
        }
        memcpy(buf + offset + total_read_len, block, static_cast<size_t>(to_read_max_len -
                                                                         total_read_len));
        total_read_len = to_read_max_len;
    }

    debug_.total_output_len_ += total_read_len;
    current_read_offset_ += total_read_len;

#if (PROFILE_AES_DEC)
    auto t1 = kpbase::SystemUtil::GetCPUTime();
    LOG_DEBUG("[aes_cost][AsyncScopeDataSource::ReadCrypt] total_read_len:%lld, cost:%lldms", total_read_len, t1 - t0);
#endif
    return to_read_max_len;
}

int64_t AsyncScopeDataSource::SeekCrypt(int64_t pos) {
    if (pos >= expect_consume_length_) {
        last_error_ = kAsyncScopeSeekPosExceedInputExpectLen;
        return last_error_;
    }

    if (pos > cache_scope_->GetEndPosition()) {
        last_error_ = kAsyncScopeSeekPosOverflow;
        return kAsyncScopeSeekPosOverflow;
    }

    while (last_error_ == kResultOK
           && (valid_cache_buf_len_ / AES_BLOCK_LEN * AES_BLOCK_LEN <= pos)) {
        if (kVerbose) {
            LOG_DEBUG("[%d][AsyncScopeDataSource::SeekCrypt] continue wait, valid_cache_buf_len_:%lld, pos:%lld",
                      context_id_, valid_cache_buf_len_, pos);
        }

        download_update_event_->Wait();
    }

    if (kVerbose) {
        LOG_INFO("[%d][AsyncScopeDataSource::SeekCrypt] success, valid_cache_buf_len_:%lld, return pos:%lld",
                 context_id_, valid_cache_buf_len_, pos);
    }

    if (last_error_ != kResultOK) {
        return last_error_;
    } else {
        current_read_offset_ = pos;
        return pos;
    }
}


} // namespace cache
} // namespace kuaishou
