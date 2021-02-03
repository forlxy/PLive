//
// Created by liuyuxin on 2018/5/31.
//

#include "offline_cache_preload_task.h"
#include <thread>
#include "cache_defs.h"
#include "io_stream.h"
#include "ac_log.h"

namespace kuaishou {
namespace cache {

OfflineCachePreloadTask::OfflineCachePreloadTask(std::unique_ptr<DataSourceSeekable> data_source,
                                                 const DataSpec& spec,
                                                 DataSourceType type,
                                                 TaskListener* listener,
                                                 int64_t pos,
                                                 int64_t len)
    : data_source_(std::move(data_source))
    , spec_(spec)
    , buf_(new uint8_t[kDefaultBufferSizeBytes])
    , type_(type)
    , listener_(listener)
    , pos_(pos)
    , length_(len) {
}

OfflineCachePreloadTask::~OfflineCachePreloadTask() {
    delete [] buf_;
    buf_ = nullptr;
    LOG_DEBUG("~OfflineCachePreloadTask");
}

void OfflineCachePreloadTask::RunInternal() {
    LOG_DEBUG("[OfflineCachePreloadTask]: RunInternal, thread_id:%d Started\n", std::this_thread::get_id());
    int64_t bytes_remaining = 0;

    spec_.WithPosition(pos_);
    int64_t ret = data_source_->Open(spec_);
    if (ret < 0) {
        LOG_ERROR_DETAIL("[OfflineCachePreloadTask::RunInternal], thread_id:%d, data_source open err(%d), Thread Exit\n",
                         std::this_thread::get_id(), ret);
        data_source_->Close();
        if (listener_) {
            listener_->OnTaskComplete(kTaskFailReasonOpenDataSource);
        }
        return;
    }

    if (length_ != kLengthUnset) {
        bytes_remaining = ret < length_ ? ret : length_;
    } else {
        bytes_remaining = ret;
    }


    // only exec for cache data source
    if (kDataSourceTypeDefault == type_ || kDataSourceTypeAsyncDownload == type_) {
        int64_t bytes_read = 0;

        while (!stop_signal_ &&  bytes_remaining > 0) {
            bytes_read = data_source_->Read(buf_, 0, std::min(bytes_remaining, (int64_t)(kDefaultBufferedDataSourceSizeKb * 1024)));
            if (bytes_read >= 0) {
                bytes_remaining -= bytes_read;
            } else {
                break;
            }
        }
    }

    data_source_->Close();

    if (listener_) {
        if (stop_signal_ && (bytes_remaining > 0)) {
            listener_->OnTaskComplete(kTaskFailReasonCancel);
        } else if (bytes_remaining <= 0) {
            listener_->OnTaskComplete(kTaskSuccess);
        } else {
            listener_->OnTaskComplete(kTaskFailReasonReadFail);
        }
    }

    LOG_DEBUG("[OfflineCachePreloadTask]: RunInternal, thread_id:%d Exit\n", std::this_thread::get_id());
}

void OfflineCachePreloadTask::LimitCurlSpeed() {
    if (data_source_) {
        data_source_->LimitCurlSpeed();
    }
}

OfflineCachePreloadLister::OfflineCachePreloadLister(TaskListener* listener) {
    listener_ = listener;
}

void OfflineCachePreloadLister::OnDownloadStarted(uint64_t position, const std::string& url,
                                                  const std::string& host,
                                                  const std::string& ip,
                                                  int response_code,
                                                  uint64_t connect_time_ms) {
}

void OfflineCachePreloadLister::OnDownloadProgress(uint64_t download_position, uint64_t total_bytes) {
    if (listener_) {
        listener_->onTaskProgress(download_position, total_bytes);
    }
}

void OfflineCachePreloadLister::OnDownloadStopped(DownloadStopReason reason, uint64_t downloaded_bytes,
                                                  uint64_t transfer_consume_ms, const std::string& sign,
                                                  int error_code, const std::string& x_ks_cache,
                                                  std::string session_uuid, std::string download_uuid, std::string datasource_extra_msg) {

    if (listener_) {
        listener_->onTaskStopped(downloaded_bytes, transfer_consume_ms, sign.c_str());
    }
}

void OfflineCachePreloadLister::OnSessionStarted(const std::string& key, uint64_t start_pos,
                                                 int64_t cached_bytes,
                                                 uint64_t total_bytes) {

    if (listener_) {
        listener_->onTaskStarted(start_pos, cached_bytes, total_bytes);
    }
}

void OfflineCachePreloadLister::OnSessionClosed(int32_t error_code, uint64_t network_cost,
                                                uint64_t total_cost,
                                                uint64_t downloaded_bytes,
                                                const std::string& detail_stat,
                                                bool has_opened) {
}

OfflineCachePreloadLister::~OfflineCachePreloadLister() {
    LOG_DEBUG("~OfflineCachePreloadLister()");
}

}
} // namespace kuaishou::cache
