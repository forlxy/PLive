#include "default_http_data_source.h"
#include <assert.h>
#include <include/awesome_cache_runtime_info_c.h>
#include "utility.h"
#include "ac_log.h"

namespace kuaishou {
namespace cache {

#define RETURN_ERROR(error) \
    { \
        last_error = error; \
        stats_->OnError(error); \
        return error; \
    }

DefaultHttpDataSource::DefaultHttpDataSource(std::shared_ptr<DownloadManager> download_manager,
                                             std::shared_ptr<TransferListener<HttpDataSource>> listener,
                                             const DownloadOpts& opts,
                                             AwesomeCacheRuntimeInfo* ac_rt_info) :
    download_manager_(download_manager),
    listener_(listener),
    opts_(opts),
    interrupt_cb_(opts.interrupt_cb),
    ac_rt_info_(ac_rt_info) {
    http_max_retry_cnt_ = std::min(std::max(opts.http_connect_retry_cnt, kMinHttpConnectRetryCount),
                                   kMaxHttpConnectRetryCount);
    ac_rt_info_->http_ds.http_max_retry_cnt = http_max_retry_cnt_;
    SetContextId(opts.context_id);
}

DefaultHttpDataSource::~DefaultHttpDataSource() {
    download_task_.reset();
}

int64_t DefaultHttpDataSource::Open(const DataSpec& spec) {
    stats_.reset(new DefaultHttpDataSourceStats("DefHttpSrc"));
    DataSpec data_spec = spec;
    if (HasRequestProperty(kRequestPropertyTimeout)) {
        std::string timeout_str = GetRequestProperty(kRequestPropertyTimeout);
        auto maybe_timeout = kpbase::StringUtil::Str2Int(timeout_str);
        if (!maybe_timeout.IsNull()) {
            data_spec = data_spec.WithTimeout((int32_t) maybe_timeout.Value());
        }
    }
    uri_ = spec.uri;

    stats_->SaveDataSpec(spec);

    {
        std::lock_guard<std::mutex> lg(download_task_mutex_);

        int retried_cnt = -1;
        ac_rt_info_->http_ds.http_retried_cnt = 0;
        do {
            if (input_stream_) {
                input_stream_.reset();
            }
            if (download_task_) {
                download_task_.reset();
            }
            download_task_ = download_manager_->CreateDownloadTask(opts_, ac_rt_info_);
            if (download_listener_) {
                download_task_->AddListener(download_listener_);
            }
            connectInfo_ = download_task_->MakeConnection(spec);
            retried_cnt++;
            LOG_DEBUG("[%d] [DefaultHttpDataSource::Open] 第%d次Connect, connectInfo_.error_code:%d",
                      GetContextId(), retried_cnt + 1, connectInfo_.error_code);
            if (connectInfo_.error_code == 0) {
                break;
            }
        } while (retried_cnt < http_max_retry_cnt_ && !AwesomeCacheInterruptCB_is_interrupted(&interrupt_cb_));
        ac_rt_info_->http_ds.http_retried_cnt = retried_cnt;

        stats_->OnMakeConnectionFinish(connectInfo_);
        LOG_DEBUG(
            "[%d] [DefaultHttpDataSource::Open] AFTER MakeConnection, content length %lld, response_code %d, error_code %d, used time %d",
            GetContextId(), connectInfo_.content_length, connectInfo_.response_code, connectInfo_.error_code,
            connectInfo_.connection_used_time_ms);

        if (connectInfo_.error_code != 0) {
            RETURN_ERROR(connectInfo_.error_code);
        }

        input_stream_ = download_task_->GetInputStream();
    }

    if (listener_) {
        listener_->OnTransferStart(this, data_spec);
    }
    return connectInfo_.content_length;
}

int64_t DefaultHttpDataSource::Read(uint8_t* buf, int64_t offset, int64_t read_len) {
    assert(buf);

    if (!input_stream_) {
        return last_error;
    }

    int64_t bytes_read = 0;
    if (read_len == 0) {
        return 0;
    }
    if (!input_stream_->HasMoreData()) {
        if (input_stream_->error_code() != 0 && input_stream_->error_code() != kResultBlockingInputStreamEndOfStram) {
            LOG_ERROR_DETAIL("[%d] [DefaultHttpDataSource::Read] input stream error, code %d",
                             GetContextId(), input_stream_->error_code());
            // input_stream_->error_code()其实就是downloadTask的内部errorcode，这个还是需要记录下来的,直接反馈即可
            RETURN_ERROR(input_stream_->error_code());
        } else {
            RETURN_ERROR(kResultEndOfInput);
        }
    } else {
        bytes_read = input_stream_->Read(buf, offset, read_len);
        if (bytes_read > 0) {
            stats_->OnReadBytes(bytes_read);
            if (listener_) {
                listener_->OnBytesTransfered(this, bytes_read);
            }
        } else if (bytes_read == 0) {
            LOG_ERROR_DETAIL("[%d] [DefaultHttpDataSource::Read], input_stream_->Read return bytes_read = 0", GetContextId());
            RETURN_ERROR(kResultExceptionHttpDataSourceReadNoData);
        } else if (bytes_read == kResultBlockingInputStreamEndOfStram) {
            RETURN_ERROR(kResultEndOfInput);
        }
    }
    return bytes_read;
}

AcResultType DefaultHttpDataSource::Close() {
    bool opened;
    {
        std::lock_guard<std::mutex> lg(download_task_mutex_);
        opened = download_task_ != nullptr && input_stream_ != nullptr;
        if (download_task_) {
            download_task_->Close();
            connectInfo_ = download_task_->GetConnectionInfo();
        }
    }

    if (opened && listener_) {
        listener_->OnTransferEnd(this);
    }
    return kResultOK;
}

void DefaultHttpDataSource::LimitCurlSpeed() {
    if (download_task_) {
        download_task_->LimitCurlSpeed();
    }
}

Stats* DefaultHttpDataSource::GetStats() {
    return stats_.get();
}

const ConnectionInfo& DefaultHttpDataSource::GetConnectionInfo() {
    {
        std::lock_guard<std::mutex> lg(download_task_mutex_);
        if (download_task_) {
            HasConnectionInfo* hasConInfo = dynamic_cast<HasConnectionInfo*>(download_task_.get());
            return hasConInfo->GetConnectionInfo();
        }
    }

    LOG_DEBUG("[%d] [DefaultHttpDataSource::GetConnectionInfo]", GetContextId());
    return connectInfo_;
}

#undef RETURN_ERROR
} // cache
} // kuaishou
