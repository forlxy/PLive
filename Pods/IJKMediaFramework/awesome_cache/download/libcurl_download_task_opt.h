#pragma once

#include <memory>
#include <string>
#include <map>
#include <vector>
#include <thread>
#include <atomic>
#include <include/awesome_cache_runtime_info_c.h>
#include "event.h"
#include "download/priority_download_task.h"
#include "download/default_input_stream.h"
#include "download/blocking_input_stream.h"
#include "speed_calculator.h"

#include "listener.h"
#include "tcp_climing.h"

extern "C" {
#include <curl/curl.h>
}

namespace kuaishou {
namespace cache {

class LibcurlDownloadTaskOpt : public DownloadTaskWithPriority {
  public:
    LibcurlDownloadTaskOpt(const DownloadOpts& opts, AwesomeCacheRuntimeInfo* ac_rt_info);

    virtual ~LibcurlDownloadTaskOpt();

    virtual ConnectionInfo MakeConnection(const DataSpec& spec) override;

    virtual void Pause() override;

    virtual void Resume() override;

    virtual void Close() override;

    virtual std::shared_ptr<InputStream> GetInputStream() override;

    virtual const ConnectionInfo& GetConnectionInfo() override;

  private:
    void Run();
    void ParseHeader();
    bool IsInterrupted();
    void GetHttpXKsJsonString();
    void onTransferOver();
    void UpdateDownloadedSizeFromCurl();
    void UpdateDownloadBytes();
    void UpdateSpeedCalculator();
    void CopyConnectionInfoToRuntimeInfo(AwesomeCacheRuntimeInfo::ConnectInfo& ac_rt_info, ConnectionInfo& info);

    std::string user_agent_;
    const DownloadOpts options_;
    CURL* curl_;
    DataSpec spec_;
    std::string http_header_;
    std::map<std::string, std::string> http_headers_;
    std::vector<std::string> http_x_ks_headers_;
    // std::shared_ptr<DefaultInputStream> input_stream_; // 因为现在InputStream返回0直接报错并结束播放，所以这个类暂时用不了
    std::shared_ptr<BlockingInputStream> input_stream_;
    int buffer_size_;

    curl_off_t last_dlnow_;
    int64_t last_progress_callback_ts_ms_;
#if defined(__ANDROID__)
    TcpCliming tcp_climing_;
#endif
    uint64_t task_make_connection_ts_ms_;
    uint64_t task_start_download_ts_ms_;
    int64_t last_log_time_;
    int64_t last_log_size_;
    int task_total_consume_ms_;
    int feed_data_consume_ms_;
    bool transfer_over_recorded_;

    atomic<int> state_;
    atomic<bool> paused_;
    atomic<bool> pending_paused_;
    atomic<bool> pending_resumed_;
    atomic<bool> abort_;
    atomic<bool> first_open_;
    atomic<bool> terminate_thread_;
    atomic<bool> close_waiting;

    kpbase::Event open_event_;
    kpbase::Event close_event_;
    std::mutex state_mutex_;

    ConnectionInfo connection_info_;
    kpbase::Event connection_opened_;

    std::thread thread_;
    AwesomeCacheRuntimeInfo* ac_rt_info_;
    int context_id_;

    std::unique_ptr<SpeedCalculator> speed_cal_;
    int64_t speed_cal_total_downloaded_size_;

    friend size_t WriteCallback(char* buffer, size_t size, size_t nitems, LibcurlDownloadTaskOpt* task);
    friend size_t HeaderCallback(char* buffer, size_t size, size_t nitems, LibcurlDownloadTaskOpt* task);
    friend int ProgressCallback(LibcurlDownloadTaskOpt* data, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow);

// for new cache
    AwesomeCacheInterruptCB interrupt_callback_;

};

}
}
