#pragma once

#include <memory>
#include <string>
#include <map>
#include <vector>
#include <thread>
#include <atomic>
#include <pthread.h>
#include <include/awesome_cache_runtime_info_c.h>
#include "event.h"
#include "download/priority_download_task.h"
#include "download/default_input_stream.h"
#include "download/blocking_input_stream.h"
#include "connection_info.h"
#include "awesome_cache_interrupt_cb_c.h"
#include "avg_sampler.h"
#include "speed_calculator.h"

#include "listener.h"
#include "tcp_climing.h"

extern "C" {
#include <curl/curl.h>
}

namespace kuaishou {
namespace cache {

class LibcurlDownloadTask : public DownloadTaskWithPriority {
  public:
    LibcurlDownloadTask(const DownloadOpts& opts, AwesomeCacheRuntimeInfo* ac_rt_info);

    virtual ~LibcurlDownloadTask();

    virtual ConnectionInfo MakeConnection(const DataSpec& spec) override;

    virtual void Pause() override;

    virtual void Resume() override;

    virtual void Close() override;

    virtual void LimitCurlSpeed() override;

    virtual std::shared_ptr<InputStream> GetInputStream() override;

    virtual const ConnectionInfo& GetConnectionInfo() override;

  private:
    bool IsInterrupted();
    void Run();
    void ParseHeader();
    void OnTransferOver();
    void UpdateDownloadedSizeFromCurl();
    void GetHttpXKsJsonString();
    void OnAfterFeedData(int32_t buffer_cur_used_bytes);
    void UpdateSpeedCalculator();
    void UpdateDownloadBytes();
    void CollectCurlInfoOnce();
    void SetErrorCodeAndStopReason(int error_code, DownloadStopReason reason);
    void SetErrorCode(int error_code);
    void SetStopReason(DownloadStopReason reason);
    void CopyConnectionInfoToRuntimeInfo(AwesomeCacheRuntimeInfo::ConnectInfo& ac_info,
                                         ConnectionInfo& info);
    void SetOsErrno(long os_errno);
    bool ParseAndReplaceIpWithDomain(std::string& url_str, std::string& ip);
    bool IsHttpsScheme(std::string& url_str);

    std::string user_agent_;
    const DownloadOpts options_;
    CURL* curl_;
    bool curl_info_collected_;
    DataSpec spec_;
    std::string http_header_;
    std::map<std::string, std::string> http_headers_;
    std::vector<std::string> http_x_ks_headers_;
    uint64_t last_progress_time_ms_;
    uint64_t last_progress_bytes_;
//  std::shared_ptr<DefaultInputStream> input_stream_;  // 因为现在InputStream返回0直接报错并结束播放，所以这个类暂时用不了
    std::shared_ptr<BlockingInputStream> input_stream_;
    int32_t input_stream_buffer_max_used_bytes_;

    int64_t last_dlnow_;
    int64_t last_progress_callback_ts_ms_;
#if defined(__ANDROID__)
    TcpCliming tcp_climing_;
#endif
    // 下面这4个变量很重要，不要随便删
    int64_t task_start_download_ts_ms_;
    int task_total_consume_ms_;
    int feed_data_consume_ms_;
    bool transfer_over_recorded_;

    int state_;

    bool abort_;
    bool http_header_parsed_;

    ConnectionInfo connection_info_;
    kpbase::Event connection_opened_;

    pthread_t thread_id_;
    bool thread_joined;
    AwesomeCacheInterruptCB interrupt_callback_;
    int buffer_size_;

    AwesomeCacheRuntimeInfo* ac_rt_info_;

    std::shared_ptr<SpeedCalculator> speed_cal_;

    int context_id_;
    int64_t last_log_time_;
    int64_t last_log_size_;

    friend size_t WriteCallback(char* buffer, size_t size, size_t nitems, LibcurlDownloadTask* task);
    friend size_t HeaderCallback(char* buffer, size_t size, size_t nitems, LibcurlDownloadTask* task);
    friend int ProgressCallback(LibcurlDownloadTask* data, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow);
    friend int SockOptCallback(void* clientp,
                               curl_socket_t curlfd,
                               curlsocktype purpose);
    friend void* run_thread(void*);
};

}
}
