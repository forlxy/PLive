#include "multi_download_http_data_source.h"
#include <assert.h>
#include <thread>
#include "utility.h"
#include "ac_log.h"

namespace kuaishou {
namespace cache {
namespace {
static const int kDownloadTaskCount = 2;
static const int kSeekThreshold = 32 * 1024; // 32K
static const int kDownloadTaskTimeoutMs = 5 * 1000; // 5s
}

#define RETURN_ERROR(error) \
    { \
        stats_->error_code = error; \
        return error; \
    }

MultiDownloadHttpDataSource::MultiDownloadHttpDataSource(
    std::shared_ptr<DownloadManager> download_manager,
    shared_ptr<TransferListener<HttpDataSource>> listener, const DownloadOpts& opts,
    AwesomeCacheRuntimeInfo* ac_rt_info) :
    download_manager_(download_manager),
    listener_(listener),
    opts_(opts),
    buf_(new uint8_t[kSeekThreshold]),
    ac_rt_info_(ac_rt_info) {
}

MultiDownloadHttpDataSource::~MultiDownloadHttpDataSource() {
    delete [] buf_;
    ClearDownloadTasks();
}

int64_t MultiDownloadHttpDataSource::Open(const DataSpec& spec) {
    stats_.reset(new HttpDataSourceStats());
    DataSpec data_spec = spec;
    if (HasRequestProperty(kRequestPropertyTimeout)) {
        std::string timeout_str = GetRequestProperty(kRequestPropertyTimeout);
        auto maybe_timeout = kpbase::StringUtil::Str2Int(timeout_str);
        if (!maybe_timeout.IsNull()) {
            data_spec = data_spec.WithTimeout((int32_t)maybe_timeout.Value());
        }
    }
    stats_->uri = spec.uri;
    spec_ = spec;
    // select download tasks.
    current_download_task_ = SelectDownloadTask(spec.position);
    if (!current_download_task_) {
        // make sure there are less than kDownloadTaskCount tasks at the same time.
        if (download_task_set_.size() == kDownloadTaskCount) {
            auto task = *download_task_set_.begin();
            task->download_task->Close();
            task->download_task.reset();
            download_task_set_.erase(task);
        }
        current_download_task_ = make_shared<DownloadTaskWrapper>();
        current_download_task_->download_task = download_manager_->CreateDownloadTask(opts_, ac_rt_info_);
        if (download_listener_) {
            current_download_task_->download_task->AddListener(download_listener_);
        }
        ConnectionInfo conn_info = current_download_task_->download_task->MakeConnection(spec);
        LOG_DEBUG("[MultiDownloadHttpDataSource] content length %lld, response_code %d, error_code %d, used time %lf",
                  conn_info.content_length, conn_info.response_code, conn_info.error_code, conn_info.connection_used_time_ms);
        stats_->connect_time_ms = conn_info.connection_used_time_ms;
        stats_->response_code = conn_info.response_code;

        if (conn_info.error_code != 0) {
            RETURN_ERROR(conn_info.error_code);
        }

        current_download_task_->ts = kpbase::SystemUtil::GetCPUTime();
        current_download_task_->position = spec.position;
        current_download_task_->bytes_remaining = conn_info.content_length;
        conn_info_ = conn_info;
    } else {
        stats_->connect_time_ms = 0;
        stats_->response_code = 200;
    }

    stats_->bytes_total = current_download_task_->bytes_remaining;
    stats_->pos = spec.position;
    input_stream_ = current_download_task_->download_task->GetInputStream();

    // skip some bytes.
    if (current_download_task_->position < spec.position) {
        int64_t bytes_to_skip = spec.position - current_download_task_->position;
        while (bytes_to_skip) {
            if (!input_stream_->HasMoreData()) {
                return kResultExceptionDataSourceSkipDataFail;
            }
            auto read = input_stream_->Read(buf_, 0, bytes_to_skip);
            if (read == 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            bytes_to_skip -= read;
            current_download_task_->position += read;
            current_download_task_->bytes_remaining -= read;
        }
    }

    if (listener_) {
        listener_->OnTransferStart(this, data_spec);
    }

    return (int32_t)current_download_task_->bytes_remaining;
}

int64_t MultiDownloadHttpDataSource::Read(uint8_t* buf, int64_t offset, int64_t read_len) {
    assert(buf);

    if (!input_stream_) {
        RETURN_ERROR(stats_->error_code);
    }

    int64_t bytes_read = 0;
    if (read_len == 0) {
        return 0;
    }
    if (!input_stream_->HasMoreData()) {
        if (input_stream_->error_code() != 0) {
            LOG_ERROR_DETAIL("[MultiDownloadHttpDataSource] input stream error, code %d", input_stream_->error_code());
            RETURN_ERROR(kResultExceptionNetIO);
        } else {
            return kResultEndOfInput;
        }
    } else {
        bytes_read = input_stream_->Read(buf, offset, read_len);

        stats_->bytes_transfered += bytes_read;
        current_download_task_->position += bytes_read;
        current_download_task_->bytes_remaining -= bytes_read;
        if (listener_) {
            listener_->OnBytesTransfered(this, bytes_read);
        }
    }
    // if there are enqueued download task that is timeout, close it.
    CheckEnqueuedDownloadTaskTimeout();
    return bytes_read;
}

AcResultType MultiDownloadHttpDataSource::Close() {
    bool opened = current_download_task_ != nullptr && input_stream_ != nullptr;
    if (input_stream_) {
        input_stream_.reset();
    }

//    MultiDownloadHttpDataSource 这块实现有问题，Close()必须关闭所有数据源，
// 而MaybeEnqueueDownloadTaskOrClose和这个原则是冲突的。MultiDownloadHttpDataSource解决了频繁seek的问题，
// 但是接口原则上没遵守导致引入新的回调生命周期难解决的bug，先切回DefaultHttpDataSource
    // close download tasks or enqueue set.
    if (current_download_task_) {
        MaybeEnqueueDownloadTaskOrClose(current_download_task_);
    }

    if (opened && listener_) {
        listener_->OnTransferEnd(this);
    }
    return kResultOK;
}

Stats* MultiDownloadHttpDataSource::GetStats() {
    return stats_.get();
}

std::shared_ptr<DownloadTaskWrapper> MultiDownloadHttpDataSource::SelectDownloadTask(int64_t position) {
    std::shared_ptr<DownloadTaskWrapper> selected = nullptr;
    int64_t selected_position = -1;
    for (auto& it : download_task_set_) {
        if (position >= it->position && (position - it->position < kSeekThreshold) &&
            (selected_position == -1 || it->position > selected_position)) {
            selected = it;
            selected_position = it->position;
        }
    }
    if (selected) {
        LOG_DEBUG("[MultiDownloadHttpDataSource] select existing data source.");
        download_task_set_.erase(selected);
    }
    return selected;
}

void MultiDownloadHttpDataSource::MaybeEnqueueDownloadTaskOrClose(std::shared_ptr<DownloadTaskWrapper> download_task) {
    auto input_stream = download_task->download_task->GetInputStream();
    if (input_stream->HasMoreData()) {
        download_task->ts = kpbase::SystemUtil::GetCPUTime();
        download_task_set_.insert(download_task);
    } else {
        download_task->download_task->Close();
        download_task->download_task.reset();
    }
}

void MultiDownloadHttpDataSource::CheckEnqueuedDownloadTaskTimeout() {
    uint64_t now = kpbase::SystemUtil::GetCPUTime();
    std::set<std::shared_ptr<DownloadTaskWrapper>> to_be_removed;
    for (auto& it : download_task_set_) {
        if (now - it->ts > kDownloadTaskTimeoutMs) {
            //remove it.
            to_be_removed.insert(it);
        }
    }
    for (auto& it : to_be_removed) {
        it->download_task->Close();
        it->download_task.reset();
        download_task_set_.erase(it);
    }
}

void MultiDownloadHttpDataSource::ClearDownloadTasks() {
    for (auto& task : download_task_set_) {
        task->download_task->Close();
        task->download_task.reset();
    }
    download_task_set_.clear();
}

#undef RETURN_ERROR

}
}
