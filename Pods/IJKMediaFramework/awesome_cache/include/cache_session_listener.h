#pragma once
#include <stdint.h>
#include <string>
#include "cache_defs.h"

namespace kuaishou {
namespace cache {

namespace {
static const char* sDownloadStopReasonToString[] = {
    [kDownloadStopReasonUnknown] = "kDownloadStopReasonUnknown",
    [kDownloadStopReasonFinished] = "kDownloadStopReasonFinished",
    [kDownloadStopReasonCancelled] = "kDownloadStopReasonCancelled",
    [kDownloadStopReasonFailed] = "kDownloadStopReasonFailed",
    [kDownloadStopReasonTimeout] = "kDownloadStopReasonTimeout",
    [kDownloadStopReasonNoContentLength] = "kDownloadStopReasonNoContentLength",
    [kDownloadStopReasonContentLengthInvalid] = "kDownloadStopReasonContentLengthInvalid",
};
}

class CacheSessionListener {
  public:
    static bool NeedRetryOnStopReason(DownloadStopReason stopReason) {
        switch (stopReason) {
            case kDownloadStopReasonFinished:
            case kDownloadStopReasonCancelled:
                return false;
            default:
                return true;
        }
    }

    static const char* DownloadStopReasonToString(DownloadStopReason stopReason) {
        return sDownloadStopReasonToString[stopReason];
    }

  private:

  public:
    virtual ~CacheSessionListener() {}

    /**
     * The callback when a cache session started.
     * @param start_pos the session start position
     * @param cached_bytes length already cached of this uri
     * @param total_bytes The total bytes of this url.
     */
    virtual void OnSessionStarted(const std::string& key, uint64_t start_pos,
                                  int64_t cached_bytes, uint64_t total_bytes) {}

    /**
     * The callback when a download task started. There may be multiple
     * download tasks start/stop in a single cache session.
     * @param position the start position of this download task.
     * @param url the url to download
     * @param host the hostname of this download task
     * @param ip the ip of this download task
     * @param connect_time_ms the connection time used of this download task.
     */
    virtual void OnDownloadStarted(uint64_t position, const std::string& url, const std::string& host,
                                   const std::string& ip, int response_code, uint64_t connect_time_ms) {}

    /**
     * The callback when download progressed.
     * @param download_position Download start position
     * @param total_bytes Total bytes of this url
     */
    virtual void OnDownloadProgress(uint64_t download_position, uint64_t total_bytes) {}

    /**
     * The callback when the download task is paused.
     */
    virtual void OnDownloadPaused() {}

    /**
     * The callback when the download task is resumed.
     */
    virtual void OnDownloadResumed() {}

    /**
     * The callback when the download task is stopped. There may be multiple
     * download tasks start/stop in a single cache session
     * @param reason the reason why the download task stopped.
     */
    virtual void OnDownloadStopped(DownloadStopReason reason, uint64_t downloaded_bytes,
                                   uint64_t transfer_consume_ms, const std::string& sign,
                                   int error_code, const std::string& x_ks_cache,
                                   std::string session_uuid, std::string download_uuid, std::string datasource_extra_msg) {}

    /**
     * The callback when cache session closed
     * @param error_code Error code of this session. 0 if no error.
     * @param network_cost The time used of downloading bytes
     * @param total_cost The time used of download task including task's waiting time(between pause/resume)
     * @param detail_stat The detailed statistics in string.
     * @param has_opened The cache session has been opened.
     *
     * 这个接口目前 同框/拍同款 会依赖回调后才会开始下载offlineCache，所以这块后续要替换的话要注意一下
     */
    virtual void OnSessionClosed(int32_t error_code, uint64_t network_cost,
                                 uint64_t total_cost, uint64_t downloaded_bytes,
                                 const std::string& detail_stat, bool has_opened) {}
};

} // namespace cache
} // namespace kuaishou
