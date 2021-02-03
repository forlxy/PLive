#pragma once

#ifdef CONFIG_VOD_P2SP

#include "./scope_task.h"
#include "./scope_curl_http_task.h"

#include <map>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <json/json.h>

namespace kuaishou {
namespace cache {

class ScopeP2spTaskCdnListenerProxy;

class ScopeP2spTask: public ScopeTask {
  public:

    ScopeP2spTask(DownloadOpts const& opts,
                  ScopeTaskListener* listener,
                  AwesomeCacheRuntimeInfo* ac_rt_info);
    virtual ~ScopeP2spTask();

    int64_t Open(DataSpec const& spec) override;
    void Abort() override;
    void Close() override;
    void WaitForTaskFinish() override;
    bool CanReopen() override { return true; }
    ConnectionInfoV2 const& GetConnectionInfo() override { return connection_info_; }

    // callback (events) from p2sp and cdn datasource and player
    int p2sp_data_callback(int64_t offset, uint8_t const* data, uint32_t len);
    int p2sp_timeout_callback(int64_t offset, uint32_t len);
    int p2sp_error_callback(int code, char const* msg);

    void cdn_connected_callback(ConnectionInfoV2 const& info);
    void cdn_data_callback(uint8_t* data, int64_t data_len);
    void cdn_finish_callback(int32_t err, int32_t stop_reason);

    void update_player_statistic(PlayerStatistic const* statistic);

  private:
    // All methods in this class DOES NOT block (except WaitForExit() and close())
    // They are triggered by external events above

    // For emitting p2sp data, used in runloop only
    void try_emit_p2sp_data(std::unique_lock<std::mutex>& lock);

    // Manage cdn tasks
    // note: before entering cdn_datasource_open/close, mtx_ must be unlocked
    void cdn_datasource_close();
    int64_t cdn_datasource_open(int64_t next_valid_data);

    // Some helper functions
    // They do not lock mtx_
    void on_receive_data(uint8_t* data, int64_t data_len);
    void on_download_complete(int err, DownloadStopReason stop_reason);

    void p2sp_submit_if_required();
    void p2sp_clean_chunks();

    // include_cached:
    // if false, return the length of player buffer only (which in memory)
    // if true, also adds the estimated duration from cached (downloaded) bytes (but not in player buffer yet), using bitrate
    int64_t get_current_player_buffer_ms(bool includes_cached);
    // If true, we should not keep waiting for p2sp. E.g., buffer < 50%
    bool is_buffer_below_p2sp_off_threshold();
    // If true, we can start some p2sp task. E.g., buffer > 90%
    bool is_buffer_above_p2sp_on_threshold();

    std::thread runloop_;
    std::condition_variable runloop_cond_;
    bool runloop_abort_ = false;

    std::mutex mtx_;

    // requested bytes range
    DataSpec dataspec_orig_;
    int64_t data_start_ = -1, data_end_ = -1;
    int64_t file_length_ = -1;
    int64_t current_read_pos_ = 0;

    std::shared_ptr<ScopeCurlHttpTask> cdn_task_;
    int64_t cdn_submitted_start_ = 0, cdn_submitted_end_ = 0;

    size_t p2sp_wrapper_id_ = 0;
    std::string p2sp_url_;
    int p2sp_error_code_ = 0;
    int64_t p2sp_datasource_submitted_start_ = 0;
    int64_t p2sp_datasource_submitted_end_ = 0;
    int64_t p2sp_datasource_submitted_timestamp_ = 0;
    std::map<int64_t, std::vector<uint8_t>> p2sp_chunks_;
    int64_t p2sp_chunks_bytes_ = 0;
    int64_t p2sp_chunks_max_bytes_ = 0;

    ScopeTaskListener* listener_;
    AwesomeCacheRuntimeInfo* ac_rt_info_;
    ConnectionInfoV2 connection_info_;
    DownloadOpts opts_;
    std::unique_ptr<ScopeP2spTaskCdnListenerProxy> cdn_listener_proxy_;
    int64_t transfer_start_timestamp_ = 0;

    PlayerStatistic::listeners_iterator_t player_statistic_it_;
    int64_t player_bitrate_ = 0;
    int64_t player_read_pos_ = 0;
    int64_t player_buffer_ms_ = 0;
    int64_t player_pre_read_ms_ = 0;
    int64_t player_last_updated_ = 0;

    void collect_sdk_details(bool force = false);
    std::chrono::steady_clock::time_point collect_sdk_details_last_time;
};

class ScopeP2spTaskCdnListenerProxy: public ScopeTaskListener {
  private:
    ScopeP2spTask* task;
  public:
    ScopeP2spTaskCdnListenerProxy(ScopeP2spTask* task): task(task) {}
    void OnConnectionInfoParsed(const ConnectionInfoV2& info) override {
        return this->task->cdn_connected_callback(info);
    }
    void OnReceiveData(uint8_t* data_buf, int64_t data_len) override {
        return this->task->cdn_data_callback(data_buf, data_len);
    }
    void OnDownloadComplete(int32_t error, int32_t stop_reason) override {
        return this->task->cdn_finish_callback(error, stop_reason);
    }
};


}
}

#endif
