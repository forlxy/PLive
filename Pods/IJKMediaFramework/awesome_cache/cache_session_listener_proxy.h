#pragma once
#include "cache_session_listener.h"
#include "cache_session_listener_c.h"
#include "awesome_cache_c.h"
#include "ac_log.h"
#include "ac_datasource.h"

namespace kuaishou {
namespace cache {

class CacheSessionListenerProxy : public CacheSessionListener {
  public:
    CacheSessionListenerProxy(const CCacheSessionListener* c_listener, const ac_data_source_t ac_data_source) : c_listener_(c_listener), ac_data_source_(
            ac_data_source) {}
    virtual ~CacheSessionListenerProxy() {}
    virtual void OnSessionStarted(const std::string& key, uint64_t start_pos, int64_t cached_bytes, uint64_t total_bytes) override {
        if (!ac_data_source_->need_report) {
            return;
        }
        ac_data_source_->need_report = false;
        if (c_listener_ && c_listener_->on_session_started) {
            c_listener_->on_session_started(c_listener_, key.c_str(), start_pos, cached_bytes, total_bytes);
        }
    }

    virtual void OnDownloadStarted(uint64_t position, const std::string& url, const std::string& host,
                                   const std::string& ip, int response_code, uint64_t connect_time_ms) override {
        if (c_listener_ && c_listener_->on_download_started) {

            c_listener_->on_download_started(c_listener_, position, url.c_str(),
                                             host.c_str(), ip.c_str(), response_code, connect_time_ms);
        }
    }

    virtual void OnDownloadProgress(uint64_t download_position, uint64_t total_bytes) override {
        if (c_listener_ && c_listener_->on_download_progress) {
            c_listener_->on_download_progress(c_listener_, download_position, total_bytes);
        }
    }

    virtual void OnDownloadPaused() override {
        if (c_listener_ && c_listener_->on_download_paused) {
            c_listener_->on_download_paused(c_listener_);
        }
    }

    virtual void OnDownloadResumed() override {
        if (c_listener_ && c_listener_->on_download_resumed) {
            c_listener_->on_download_resumed(c_listener_);
        }
    }

    virtual void OnDownloadStopped(DownloadStopReason reason, uint64_t downloaded_bytes,
                                   uint64_t transfer_consume_ms, const std::string& sign,
                                   int error_code, const std::string& x_ks_cache,
                                   std::string session_uuid, std::string download_uuid, std::string datasource_extra_msg) override {
        if (c_listener_ && c_listener_->on_download_stopped) {
            c_listener_->on_download_stopped(c_listener_, reason, downloaded_bytes,
                                             transfer_consume_ms, sign.c_str(),
                                             error_code, x_ks_cache.c_str(),
                                             session_uuid.c_str(), download_uuid.c_str(), datasource_extra_msg.c_str());
        }
    }

    virtual void OnSessionClosed(int32_t error_code, uint64_t network_cost_ms,
                                 uint64_t total_cost_ms, uint64_t downloaded_bytes,
                                 const std::string& detail_stat, bool has_opened) override {
        if (!ac_data_source_->need_report) {
            return;
        }
        ac_data_source_->need_report = false;
        if (c_listener_ && c_listener_->on_session_closed) {
            c_listener_->on_session_closed(c_listener_, error_code, network_cost_ms, total_cost_ms, downloaded_bytes, detail_stat.c_str(), has_opened);
        }
    }

  private:
    const CCacheSessionListener* c_listener_;
    ac_data_source_t ac_data_source_;

};

} // namespace cache
} // namespace kuaishou
