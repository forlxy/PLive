//
// Created by liuyuxin on 2018/5/31.
//

#pragma once

#include <include/cache_session_listener.h>
#include <runloop.h>
#include "data_source.h"
#include "data_source_seekable.h"
#include "task.h"
#include "task_listener.h"
#include "file.h"

namespace kuaishou {
namespace cache {

class OfflineCachedLister : public CacheSessionListener {
  public:
    OfflineCachedLister(TaskListener* listener);
    ~OfflineCachedLister();
    void OnSessionStarted(const std::string& key, uint64_t start_pos,
                          int64_t cached_bytes, uint64_t total_bytes) override ;
    void OnSessionClosed(int32_t error_code, uint64_t network_cost,
                         uint64_t total_cost, uint64_t downloaded_bytes,
                         const std::string& detail_stat, bool has_opened) override ;
    void OnDownloadStarted(uint64_t position, const std::string& url, const std::string& host,
                           const std::string& ip, int response_code, uint64_t connect_time_ms) override ;
    void OnDownloadProgress(uint64_t download_position, uint64_t total_bytes) override ;
    void OnDownloadStopped(DownloadStopReason reason, uint64_t downloaded_bytes,
                           uint64_t transfer_consume_ms, const std::string& sign,
                           int error_code, const std::string& x_ks_cache,
                           std::string session_uuid, std::string download_uuid, std::string datasource_extra_msg) override ;
  private:
    TaskListener* listener_;
};

class OfflineCachedFileTask : public Task {
  public:

    OfflineCachedFileTask(std::unique_ptr<DataSourceSeekable> data_source,
                          const DataSpec& spec,
                          DataSourceType type,
                          TaskListener* listener);
    virtual ~OfflineCachedFileTask();



  private:
    virtual void RunInternal() override;
    DataSpec spec_;
    std::unique_ptr<DataSourceSeekable> data_source_;
    uint8_t* buf_;
    DataSourceType type_;
    TaskListener* listener_;
};

}
} // namespace kuaishou::cache
