//#pragma once
//#include <atomic>
//#include <set>
//#include "download/download_task_listener.h"
//#include "data_source.h"
//#include "cache/cache.h"
//#include "cache_session_listener.h"
//#include "runloop.h"
//#include "utility.h"
//
//namespace kuaishou {
//namespace cache {
//
//class CacheDataSource : public DownloadTaskListener, public DataSource {
//  public:
//    CacheDataSource(Cache* cache, std::shared_ptr<CacheSessionListener> listener = nullptr);
//    virtual ~CacheDataSource();
//
//  protected:
//    // will be called and must be called when opening this session.
//    virtual void ReportSessionOpened(uint64_t pos, int64_t cached_bytes, uint64_t total_bytes);
//    // will be called when closing this session or the download thread stopped.
//    virtual void ReportSessionClosed();
//    // will be called when an error happened and cannot be recovered.
//    virtual void ReportError(int32_t error_code);
//    // will be called when opening any data source.
//    virtual void ReportProgress(uint64_t position);
//
//    Cache* cache_;
//    std::string key_;
//
//    // Tell whether or not this video is almost fully cached.
//    // 某些视频后面可能有一些冗余信息，在一次视频播放中，可能没有读到最后，所以加一个保护.
//    bool AlmostFullyCached(int64_t cached_bytes, uint64_t total_bytes);
//
//  private:
//    void DoReportProgressOnCallbackThread(uint64_t position);
//    virtual void OnConnectionOpen(DownloadTaskWithPriority*, uint64_t position, const ConnectionInfo& info);
//    virtual void OnDownloadProgress(DownloadTaskWithPriority*, uint64_t position);
//    virtual void OnDownloadPaused(DownloadTaskWithPriority*);
//    virtual void OnDownloadResumed(DownloadTaskWithPriority*);
//    virtual void OnConnectionClosed(DownloadTaskWithPriority*, const ConnectionInfo& info, DownloadStopReason reason, uint64_t downloaded_bytes, uint64_t transfer_consume_ms);
//
//    std::shared_ptr<CacheSessionListener> listener_;
//    uint64_t last_connection_open_time_;
//    uint64_t last_pause_time_;
//    uint64_t network_time_; // network time is the total download time excluding paused time.
//    uint64_t total_time_; // total time is the total time including paused time.
//    uint64_t paused_time_;
//    int32_t error_code_;
//    std::atomic<uint64_t> total_bytes_;
//    uint64_t downloaded_bytes_;
//    bool session_opened_;
//    bool session_close_reported_;
//    std::atomic_bool should_report_progress_;
//    std::mutex close_session_lock_;
//
//    std::unique_ptr<kpbase::Runloop> runloop_;
//    // variables used in cache callback runloop.
//    struct Span {
//        uint64_t start;
//        uint64_t end;
//    };
//    struct SpanComp {
//        bool operator()(const Span& a, const Span& b) const {
//            return a.start == b.start ? a.end < b.end : a.start < b.start;
//        }
//    };
//    std::set<Span, SpanComp> span_list_;
//};
//
//} // namespace cache
//} // namespace kuaishou
