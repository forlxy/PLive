//
// Created by liuyuxin on 2018/5/31.
//

#include "offline_cached_file_task.h"
#include <thread>
#include "cache_defs.h"
#include "io_stream.h"
#include "ac_log.h"

namespace kuaishou {
namespace cache {

OfflineCachedFileTask::OfflineCachedFileTask(std::unique_ptr<DataSourceSeekable> data_source,
                                             const DataSpec& spec,
                                             DataSourceType type,
                                             TaskListener* listener)
    : data_source_(std::move(data_source))
    , spec_(spec)
    , buf_(new uint8_t[kDefaultBufferSizeBytes])
    , type_(type)
    , listener_(listener) {
}

OfflineCachedFileTask::~OfflineCachedFileTask() {
    delete [] buf_;
    buf_ = nullptr;
    LOG_DEBUG("~OfflineCachedFileTask");
}

void OfflineCachedFileTask::RunInternal() {
    LOG_DEBUG("[OfflineCachedFileTask]: RunInternal, thread_id:%d Started\n", std::this_thread::get_id());

    int64_t ret = data_source_->Open(spec_);
    if (ret < 0) {
        LOG_ERROR_DETAIL("[OfflineCachedFileTask::RunInternal], thread_id:%d, data_source open err(%d), Thread Exit\n",
                         std::this_thread::get_id(), ret);
        data_source_->Close();
        if (listener_) {
            listener_->OnTaskFailed(kTaskFailReasonOpenDataSource);
        }
        return;
    }

    int64_t length = ret;
    int64_t total_read = 0;

    // only exec for cache data source
    if (kDataSourceTypeDefault == type_
        || kDataSourceTypeAsyncV2 == type_) {
        int64_t bytes_read = 0;

        while (!stop_signal_ && total_read != length) {
            bytes_read = data_source_->Read(buf_, 0, std::min(length - total_read, (int64_t)kDefaultBufferSizeBytes));
            if (kResultEndOfInput == bytes_read) {
                break;
            } else if (bytes_read >= 0) {
                total_read += bytes_read;
            } else {
                if (listener_) {
                    listener_->OnTaskFailed(kTaskFailReasonReadFail);
                }
                break;
            }
        }
    }

    data_source_->Close();

    if (listener_) {
        if (stop_signal_ && (length > 0 ? total_read < length : true)) {
            listener_->OnTaskCancelled();
        } else if (total_read == length) {
            listener_->OnTaskSuccessful();
        } else {
            listener_->OnTaskFailed(kTaskFailReasonReadFail);
        }
    }

    LOG_DEBUG("[OfflineCachedFileTask]: RunInternal, thread_id:%d Exit\n", std::this_thread::get_id());
}

OfflineCachedLister::OfflineCachedLister(TaskListener* listener) {
    listener_ = listener;
}

void OfflineCachedLister::OnDownloadStarted(uint64_t position, const std::string& url,
                                            const std::string& host,
                                            const std::string& ip,
                                            int response_code,
                                            uint64_t connect_time_ms) {
}

void OfflineCachedLister::OnDownloadProgress(uint64_t download_position, uint64_t total_bytes) {
    if (listener_) {
        listener_->onTaskProgress(download_position, total_bytes);
    }
}

void OfflineCachedLister::OnDownloadStopped(DownloadStopReason reason, uint64_t downloaded_bytes,
                                            uint64_t transfer_consume_ms, const std::string& sign,
                                            int error_code, const std::string& x_ks_cache,
                                            std::string session_uuid, std::string download_uuid, std::string datasource_extra_msg) {

    if (listener_) {
        listener_->onTaskStopped(downloaded_bytes, transfer_consume_ms, sign.c_str());
    }
}

void OfflineCachedLister::OnSessionStarted(const std::string& key, uint64_t start_pos,
                                           int64_t cached_bytes,
                                           uint64_t total_bytes) {

    if (listener_) {
        listener_->onTaskStarted(start_pos, cached_bytes, total_bytes);
    }
}

void OfflineCachedLister::OnSessionClosed(int32_t error_code, uint64_t network_cost,
                                          uint64_t total_cost,
                                          uint64_t downloaded_bytes,
                                          const std::string& detail_stat,
                                          bool has_opened) {
}

OfflineCachedLister::~OfflineCachedLister() {
    LOG_DEBUG("~OfflineCachedLister()");
}

}
} // namespace kuaishou::cache
