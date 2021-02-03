//#include "cache_data_source.h"
//#include <algorithm>
//
//namespace kuaishou {
//namespace cache {
//namespace {
//static const uint32_t kProgressSpanUpdateThreshold = 2 * 1024 * 1024; // 2M
//}
//
//CacheDataSource::CacheDataSource(Cache* cache, std::shared_ptr<CacheSessionListener> listener) :
//    cache_(cache),
//    listener_(listener),
//    last_connection_open_time_(0),
//    last_pause_time_(0),
//    network_time_(0),
//    total_time_(0),
//    paused_time_(0),
//    total_bytes_(0),
//    error_code_(0),
//    downloaded_bytes_(0),
//    session_opened_(false),
//    session_close_reported_(false),
//    should_report_progress_(false),
//    key_(""),
//    runloop_(new kpbase::Runloop("cache_session_listener_callback")) {
//    span_list_.insert(Span{0, 0});
//}
//
//CacheDataSource::~CacheDataSource() {
//    LOG_DEBUG("CacheDataSource::~CacheDataSource()");
//    runloop_->Stop();
//}
//
//bool CacheDataSource::AlmostFullyCached(int64_t cached_bytes, uint64_t total_bytes) {
//    return total_bytes == 0 ? false : ((float)cached_bytes / (float)total_bytes > 0.98);
//}
//
//void CacheDataSource::DoReportProgressOnCallbackThread(uint64_t position) {
//    auto iter = span_list_.begin();
//    bool updated = false;
//    Span updated_span;
//    for (; iter != span_list_.end(); iter++) {
//        if (position >= iter->start && position <= iter->end) {
//            // if this span is not the last span in this set, report its end as position.
//            // if it is the last, don't report position.
//            auto next_span_iter = std::next(iter);
//            if (next_span_iter != span_list_.end()) {
//                span_list_.erase(next_span_iter, span_list_.end());
//                listener_->OnDownloadProgress(iter->end, total_bytes_);
//            }
//            return;
//        }
//        if (position > iter->end && position <= iter->end + kProgressSpanUpdateThreshold) {
//            // update progress span.
//            updated_span = Span{iter->start, position};
//            updated = true;
//            break;
//        } else if (position < iter->start) {
//            break;
//        }
//    }
//    // erase the spans that is bigger than current position.
//    span_list_.erase(iter, span_list_.end());
//
//    if (updated) {
//        span_list_.insert(updated_span);
//    } else {
//        span_list_.insert(Span{position, position});
//    }
//
//    listener_->OnDownloadProgress(position, total_bytes_);
//
//}
//
//void CacheDataSource::ReportProgress(uint64_t position) {
//    if (listener_ && total_bytes_ && should_report_progress_) {
//        runloop_->Post([ = ] {
//            DoReportProgressOnCallbackThread(position);
//        });
//    }
//}
//
//void CacheDataSource::ReportSessionOpened(uint64_t pos, int64_t cached_bytes, uint64_t total_bytes) {
//    if (!session_opened_) {
//        total_bytes_ = total_bytes;
//        should_report_progress_ = not AlmostFullyCached(cached_bytes, total_bytes);
//        if (listener_) {
//            runloop_->Post([ = ] {
//                listener_->OnSessionStarted(key_, pos, cached_bytes, total_bytes_);
//            });
//        }
//        session_opened_ = true;
//    }
//}
//
//void CacheDataSource::ReportError(int32_t error_code) {
//    error_code_ = error_code;
//}
//
//void CacheDataSource::ReportSessionClosed() {
//    // disable the possibility that mulitple thread is reporting session closed,
//    // that may cause the PostAndWait event call waiting forever.
//    std::lock_guard<std::mutex> lg(close_session_lock_);
//    network_time_ = total_time_ - paused_time_;
//    if (listener_ && !session_close_reported_ &&
//        AlmostFullyCached(cache_->GetContentCachedBytes(key_), total_bytes_)) {
//        should_report_progress_ = false;
//        auto stat = GetStats();
//        session_close_reported_ = true;
//        runloop_->PostAndWait([ = ] {
//            listener_->OnSessionClosed(error_code_, network_time_, total_time_, downloaded_bytes_, stat ? stat->ToString() : "", session_opened_);
//        });
//    }
//}
//
//void CacheDataSource::OnConnectionOpen(DownloadTaskWithPriority*, uint64_t position, const ConnectionInfo& info) {
//    last_connection_open_time_ = kpbase::SystemUtil::GetCPUTime();
//    ReportError(info.error_code);
//    if (listener_) {
//        runloop_->Post([ = ] {
//            listener_->OnDownloadStarted(position, info.uri, info.host, info.ip, info.response_code,
//                                         info.connection_used_time_ms);
//        });
//    }
//}
//
//void CacheDataSource::OnDownloadProgress(DownloadTaskWithPriority*, uint64_t position) {
//    ReportProgress(position);
//}
//
//void CacheDataSource::OnDownloadPaused(DownloadTaskWithPriority*) {
//    last_pause_time_ = kpbase::SystemUtil::GetCPUTime();
//}
//
//void CacheDataSource::OnDownloadResumed(DownloadTaskWithPriority*) {
//    if (last_pause_time_) {
//        paused_time_ += kpbase::SystemUtil::GetCPUTime() - last_pause_time_;
//        last_pause_time_ = 0;
//    }
//}
//
//void CacheDataSource::OnConnectionClosed(DownloadTaskWithPriority*, const ConnectionInfo& info,
//                                         DownloadStopReason reason, uint64_t downloaded_bytes, uint64_t transfer_consume_ms) {
//    auto now = kpbase::SystemUtil::GetCPUTime();
//    if (last_pause_time_) {
//        paused_time_ += now - last_pause_time_;
//        last_pause_time_ = 0;
//    }
//    if (last_connection_open_time_) {
//        total_time_ += now - last_connection_open_time_;
//        last_connection_open_time_ = 0;
//    }
//    downloaded_bytes_ += downloaded_bytes;
//    if (listener_) {
//        runloop_->Post([ = ] {
//            listener_->OnDownloadStopped(reason, downloaded_bytes, transfer_consume_ms,
//                                         info.sign, info.error_code, info.x_ks_cache);
//        });
//    }
//}
//
//} // namespace cache
//} // namespace kuaishou
