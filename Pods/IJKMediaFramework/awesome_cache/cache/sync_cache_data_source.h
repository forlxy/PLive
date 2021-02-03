//
// Created by 帅龙成 on 30/10/2017.
//
#pragma once

#include <memory>
#include <mutex>
#include <runloop.h>
#include <include/awesome_cache_runtime_info_c.h>
#include <include/cache_opts.h>
#include <include/awesome_cache_callback.h>
#include "data_source.h"
#include "http_data_source.h"
#include "data_sink.h"
#include "cache.h"
#include "constant.h"
#include "stats/stage_stats.h"
#include "sync_cache_data_source_session_stats.h"
#include "cache_session_listener.h"
#include "download/download_task_listener.h"

namespace kuaishou {
namespace cache {

class SyncCacheDataSource : public DataSource {

  private:
    // will be called and must be called when opening this session.
    void ReportSessionOpened(uint64_t pos, int64_t cached_bytes, int64_t total_bytes);
    // will be called when closing this session or the download thread stopped.
    void ReportSessionClosed();
    void ReportDownloadStarted(uint64_t position, const ConnectionInfo& info);
    void ReportDownloadStopped(const char* tag, const ConnectionInfo& info);
    void ReportProgress(const ConnectionInfo& info);
    void CheckAndCloseCurrentDataSource(int error_code, DownloadStopReason stop_reason);

    Cache* cache_;
    std::string key_;

    // Tell whether or not this video is almost fully cached.
    // 某些视频后面可能有一些冗余信息，在一次视频播放中，可能没有读到最后，所以加一个保护.
    bool AlmostFullyCached(int64_t cached_bytes, uint64_t total_bytes);

    void DoReportProgressOnCallbackThread(uint64_t position, int64_t content_length);

    // -------- CacheDataSource part START --------
    std::shared_ptr<CacheSessionListener> cache_session_listener_;
    AwesomeCacheCallback* cache_callback_;

    std::shared_ptr<AcCallbackInfo> callbackInfo_;
    int64_t cached_bytes_;

    int32_t error_code_;
    DownloadStopReason stop_reason_for_qos_;

    int64_t total_bytes_;
    int64_t downloaded_bytes_; // deprecated，等CacheCallback回调的时候要删掉
    int64_t session_open_ts_ms_;
    int64_t session_close_ts_ms_;
    int64_t network_cost_ms_;
    int64_t last_progress_time_ms_;
    int64_t last_progress_pos_;

    bool session_opened_;
    bool session_close_reported_;
    bool download_stop_need_to_report_;
    std::atomic_bool should_report_progress_;
    std::mutex close_session_lock_;

    std::unique_ptr<kpbase::Runloop> runloop_;
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
    // -------- CacheDataSource part END --------

    void OnDataSourceOpened(int64_t open_ret);
    void OnDataSourceReadResult(int64_t read_ret);
    void OnUpstreamDataSourceOpened(uint64_t position, const ConnectionInfo& info);
    void OnUpstreamDataSourceRead(int64_t read_ret, const ConnectionInfo& info);

    void AbrUpdateDownloadInfo(uint64_t start_time_ms, uint64_t bytes_transferred);

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

  public:
    /**
     * Constructs an instance with arbitrary {@link DataSource} and {@link DataSink} instances for
     * reading and writing the cache. One use of this constructor is to allow data to be transformed
     * before it is written to disk.
     *
     * @param cache The cache.
     * @param upstream A {@link DataSource} for reading data not in the cache.
     * @param cacheReadDataSource A {@link DataSource} for reading data from the cache.
     * @param cacheWriteDataSink A {@link DataSink} for writing data to the cache. If null, cache is
     *     accessed read-only.
     * @param flags A combination of {@link #FLAG_BLOCK_ON_CACHE}, {@link #FLAG_IGNORE_CACHE_ON_ERROR}
     *     and {@link #FLAG_IGNORE_CACHE_FOR_UNSET_LENGTH_REQUESTS}, or 0.
     * @param eventListener An optional {@link EventListener} to receive events.
     */
    SyncCacheDataSource(Cache* cache, std::shared_ptr<HttpDataSource> upstream,
                        std::shared_ptr<DataSource> cache_read_data_source,
                        std::shared_ptr<DataSink> cache_write_data_sink, const DataSourceOpts& opts,
                        EventListener* listener, std::shared_ptr<CacheSessionListener> session_listener,
                        AwesomeCacheRuntimeInfo* ac_rt_info);
    virtual ~SyncCacheDataSource();

    virtual int64_t Open(const DataSpec& spec) override;

    virtual int64_t Read(uint8_t* buf, int64_t offset, int64_t read_len) override;

    virtual AcResultType Close() override;

    virtual void LimitCurlSpeed() override;

    virtual Stats* GetStats() override;

  private:
    // clear any resources before closing this data source.
    void OnClose();
    int64_t OpenNextSource(bool initial);

    int64_t OpenNextSourceWithCache(bool use_cache, bool initial);

    AcResultType CloseCurrentSource();

    AcResultType SetContentLength(int64_t length);

    void HandleErrorBeforeReturn(int64_t ret);


    bool IsCacheError(int64_t ret);

    void NotifyBytesRead();

  private:
    std::shared_ptr<DataSource> cache_read_data_source_;
    std::shared_ptr<DataSource> cache_write_data_source_;
    std::shared_ptr<HttpDataSource> upstream_data_source_;
    EventListener* const event_listener_;

    struct {
        int cache_read_source_cnt;
        int cache_write_source_cnt;
        int cache_upstream_source_cnt;
        bool is_reading_file_data_source;
    } qos_;

    AwesomeCacheRuntimeInfo* ac_rt_info_;

    const bool block_on_cache_;

    std::shared_ptr<DataSource> current_data_source_;
    std::recursive_mutex current_data_source_lock_;
    bool current_request_unbonded_;
    std::string uri_;
    DataSpec spec_;
    DataSpec current_spec_; //current_data_source_ open dataSpec
    int flags_;
    int64_t read_position_;
    int64_t bytes_remaining_;
    bool current_request_ignore_cache_;
    long total_cached_bytes_read_;
    std::mutex key_lock_;
    int32_t progress_cb_interval_ms_;
    int enable_vod_adaptive_;
    int64_t last_download_start_time_;
    bool is_update_abr_;
    int64_t curl_buffer_size_kb_;
    std::string data_source_extra_;

    std::string product_context_;

    std::unique_ptr<internal::SyncCacheDataSourceSessionStats> stats_;
};
}
}
