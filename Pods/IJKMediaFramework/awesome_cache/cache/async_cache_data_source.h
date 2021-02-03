#pragma once
#include <thread>
#include <mutex>
#include <algorithm>
#include <include/awesome_cache_runtime_info_c.h>
#include <runloop.h>
#include <include/cache_opts.h>
#include "data_source.h"
#include "data_sink.h"
#include "cache/cache.h"
#include "file.h"
#include "io_stream.h"
#include "constant.h"
#include "event.h"
#include "file_writer_memory_data_source.h"
#include "stats/session_stats.h"
#include "cache_session_listener.h"
#include "cache_data_source.h"
#include "http_data_source.h"
#include "download/download_task_listener.h"
#include "include/awesome_cache_callback.h"

namespace kuaishou {
namespace cache {
namespace internal {
class AsyncCacheDataSourceStageStats : public StageStats {
  public:
    AsyncCacheDataSourceStageStats(int stage) : StageStats(stage) {
    }
    std::string uri;
    int64_t pos;
    int64_t bytes_total;
    int error;
    void AppendWriteThreadHttpDataSourceStats(json stat);
    void AppendWriteThreadFileDataSourceStats(json stat);
    void AppendReadDataSourceStats(json stat);
  private:
    virtual void FillJson() override;
    json write_data_source_stats_;
    json read_data_source_stats_;
};
} // namespace internal

class AsyncCacheDataSource : public DataSource {
//class AsyncCacheDataSource : public CacheDataSource {
  public:
    /**
     * Listener of {@link CacheDataSource} events.
     */
    struct EventListener {
        virtual ~EventListener() {};

        /**
         * Called when bytes have been read from the cache.
         *
         * @param cache_size_bytes Current cache size in bytes.
         * @param cached_bytes_read Total bytes read from the cache since this method was last called.
         */
        virtual void OnCachedBytesRead(long cache_size_bytes, long cached_bytes_read) = 0;
    };

    enum CacheSpanVisitStatus {
        NotVisited = 0,
        WriteVisited,
        ReadVisited
    };

    enum DownloadThreadExitType {
        DownloadOK = 0,
        DownloadSpanLocked,             // span被lock了，此时不缓存该视频，直接从网络下载；
        DownloadSpanNull,               // 获取span失败，此时不缓存该视频，直接从网络下载；
        DownloadCacheStartRWFailed,     // cache打开失败
        DownloadUpstreamOpenFailed,     // upstream打开失败
        DownloadCacheStartFileFailed,   // 获取cache file失败
        DownloadCacheWriteBufFailed,    // 写cache buf失败
        DownloadCacheWriteExceedMax,    // 超过cache buf最大值
        DownloadByteRangeContentInvalid,  // CDN返回的数据长度与请求的range长度不一致
        DownloadPosNotZero,           // position don't 0.
        DownloadPosNotCache,           // position don't cached.
        DownloadCacheError,              // cache本身出错，无法缓存数据。
        DownloadFileWriteError,           // 文件写入错误
        DownloadCacheFileDirError,        // cache目录出错
        CacheRemoveStaleSpansFailed       // span对应的文件不存在
    };

  public:
    AsyncCacheDataSource(Cache* cache, std::shared_ptr<HttpDataSource> upstream,
                         std::shared_ptr<DataSource> file_datasource,
                         const DataSourceOpts& opts,
                         AsyncCacheDataSource::EventListener* listener = nullptr,
                         std::shared_ptr<CacheSessionListener> session_listener = nullptr,
                         AwesomeCacheRuntimeInfo* ac_rt_info = nullptr);
    virtual ~AsyncCacheDataSource();

    virtual int64_t Open(const DataSpec& spec) override;

    virtual int64_t Read(uint8_t* buf, int64_t offset, int64_t read_len) override;

    virtual AcResultType Close() override;

    virtual Stats* GetStats() override;

  private:
    // clear any resources before closing this data source.
    int32_t HandleFileError();
    void HandleErrorBeforeReturn(AcResultType ret);
    void HandleDownloadError(DownloadStopReason download_error);
    bool IsCacheError(AcResultType ret);
    void DownloadThread(int64_t read_position, int64_t download_remaining);
    int64_t GetContentLength(int64_t read_position);
    int setContentLength(int64_t read_position);
    int64_t FindDownloadReadPosition(int64_t read_position, int64_t* download_remaining);
    void AbrUpdateDownloadInfo(uint64_t start_time_ms, uint64_t bytes_transferred);
    int64_t OpenHttpDataSource(int64_t len);

  private:
    std::shared_ptr<HttpDataSource> upstream_data_source_;
    std::shared_ptr<DataSource> cache_read_data_source_;
    EventListener* const event_listener_;

    const bool ignore_cache_on_error_;
    const bool ignore_cache_for_unset_length_requests_;

    std::thread download_thread_;
    std::atomic<bool> terminate_download_thread_;
    std::atomic<bool> download_thread_exit_;
    std::atomic<bool> directly_read_from_upstream_;
    std::atomic<bool> is_span_error_;
    std::atomic<bool> download_thread_is_waiting_;
    std::mutex cache_write_data_source_mutex_;
    std::shared_ptr<FileWriterMemoryDataSource> cache_write_data_source_;
    int32_t flag_;
    std::string uri_ = "";
    int64_t read_position_;
    std::atomic<int64_t> write_position_;
    int64_t bytes_remaining_;
    bool seen_cache_error_;
    bool current_request_ignore_cache_;
    std::unique_ptr<SessionStats<internal::AsyncCacheDataSourceStageStats> > stats_;
    const int USE_STAT = false;
    std::atomic<bool> first_init_source_;
    std::atomic<bool> current_request_unbounded_;
// byte range
    int32_t byte_range_length_;
    int32_t first_byte_range_length_;
    int64_t last_cached_bytes_read_;
    Event download_pause_event_;
    Event read_pause_event_;
    Event tytes_remaining_event_;
    Event cache_write_data_source_event_;
    int32_t transfer_consume_ms_;
    int enable_vod_adaptive_;
    AwesomeCacheRuntimeInfo* ac_rt_info_;
    int64_t last_progress_time_ms_;
    int64_t last_progress_pos_;
    bool abort_;
    long progress_cb_interval_ms_;

  private:
    bool AlmostFullyCached(int64_t cached_bytes, uint64_t total_bytes);
    void ReportSessionOpened(uint64_t pos, int64_t cached_bytes);
    void ReportSessionClosed();
    void ReportDownloadStarted(uint64_t position, const ConnectionInfo& info);
    void CheckAndCloseCurrentDataSource(int error_code, DownloadStopReason stop_reason);
    void ReportDownloadStopped(const char* tag, const ConnectionInfo& info);
    void DoReportProgressOnCallbackThread(uint64_t position);
    void ReportProgress(uint64_t position);

    void OnDataSourceOpened(int64_t open_ret, int64_t position);
    void OnDataSourceReadResult(int64_t read_len, int32_t& got_transfer_consume_ms);
    void OnDataSourceCancel();
    void HandleCdnError(DownloadStopReason stop_reason, int error_code);

    std::string key_;
    std::shared_ptr<CacheSessionListener> cache_session_listener_;
    AwesomeCacheCallback* cache_callback_;
    std::shared_ptr<AcCallbackInfo> callbackInfo_;
    std::recursive_mutex current_data_source_lock_;
    std::mutex close_session_lock_;
    std::unique_ptr<kpbase::Runloop> runloop_;
    std::atomic_bool should_report_progress_;
    // variables used in cache callback runloop.
    struct Span {
        uint64_t start;
        uint64_t end;
    };
    struct SpanComp {
        bool operator()(const Span& a, const Span& b) const {
            return a.start == b.start ? a.end < b.end : a.start < b.start;
        }
    };
    std::set<Span, SpanComp> span_list_;

    Cache* cache_;
    int32_t error_code_;
    int64_t downloaded_bytes_;
    bool download_stop_need_to_report_;
    int64_t total_bytes_;
    bool session_close_reported_;
    int64_t session_close_ts_ms_;
    int64_t network_cost_ms_;
    int64_t session_open_ts_ms_;
    bool session_opened_;
    std::string data_source_extra_;
    std::string product_context_;
    DataSpec current_upstream_data_spec_;
    bool is_reading_from_cache_;

  private:
    struct {
        int cache_read_source_cnt;
        int cache_write_source_cnt;
        int cache_upstream_source_cnt;
    } qos_;
    void UpdateDataSourceQos();
    std::mutex key_lock_;
};

} // namespace cache
} // namespace kuaishou
