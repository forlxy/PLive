//
// Created by MarshallShuai on 2019-06-26.
//

#pragma once

#include <stdint.h>
#include <map>
#include <thread>
#include "stats/session_stats.h"
#include "event.h"
#include "data_source.h"
#include "cache_opts.h"
#include "cache_errors.h"
#include "connection_info_v2.h"
#include "download/speed_calculator.h"
#include "http_task_progress_helper.h"
#include "scope_task.h"

extern "C" {
#include <curl/curl.h>
}

namespace kuaishou {
namespace cache {

/**
 *  只处理有上限大小的下载任务
 *  1.提供一个边下边写的
 */
class ScopeCurlHttpTask: public ScopeTask {

  public:
    ScopeCurlHttpTask(const DownloadOpts& opts, ScopeTaskListener* listener,
                      AwesomeCacheRuntimeInfo* ac_rt_info = nullptr);

    ~ScopeCurlHttpTask();

    /**
     * 如果出错则返回error，如果成功open，则返回content_length（注意，不等于总长度，只有spec.position=0的时候才等于总长度）
     * @param spec DataSpec
     * @return open返回值
     */
    int64_t Open(const DataSpec& spec) override;

    /**
      * 立即终止下载进度,并清理资源（包括curl）
      */
    void Close() override;

    void Abort() override;

    /**
     * 等curl自然结束
     */
    void WaitForTaskFinish() override;

    // can only access by unit test,todo，后续unit test改成使用GetConnectionInfo
    ConnectionInfoV2 connection_info_;

    ConnectionInfoV2 const& GetConnectionInfo() override {
        return this->connection_info_;
    }

  private:
    int id() {return id_;};
    void SetupCurl();
    void CleanupCurl();

    int ParseHeader();
    /**
     *
     * @return 如果有表示错误的ResponseCode，则返回非kResultOK值，如果没有则返回kResultOK
     */
    AcResultType CheckResponseCode();
    /**
     * 把header里的xks相关的key-value信息组装成jsonString
     */
    void GetHttpXKsJsonString();
    /**
     * 解析header，并负责调用ScopeTaskListener
     * 这个不仅要在writeCallback里调用，还需要在curl_easy_perform完了也调用，
     * 因为如果 range_start_position >= content-length的时候，不会进入writeCallback回调
     * @return 解析header
     */
    int ParseCheckHeadersOnce();

    inline void ReportInvalidResponseHeader();

    static void CopyConnectionInfoToRuntimeInfo(
        AwesomeCacheRuntimeInfo::ConnectInfo& ac_rt_connect_info, ConnectionInfoV2& info);

    /**
     * 实际下载线程的流程
     */
    void DoDownload();

    /**
     * 在curl连接彻底结束后，收集错误码、invalid repsonse code等
     * @param curl_ret curl的返回值
     */
    void CheckErrorAfterCurlPerform(int curl_ret);

    /**
     * valid data表示兼容server不支持range请求的场景下已经skip过的有效数据
     * @param buf buffer
     * @param len 长度
     */
    void OnReceiveValidData(uint8_t* buf, int64_t len);
    void OnReceiveData(uint8_t* buf, int64_t len);

    /**
     * 这个接口不耗时，0ms
     */
    void CollectCurlInfo();
    bool IsInterrupted();

    void SetErrorCode(int error_code);
    void SetStopReason(DownloadStopReason reason);

    static size_t WriteCallback(char* buffer, size_t size, size_t nitems, ScopeCurlHttpTask* task);
    static size_t HeaderCallback(char* buffer, size_t size, size_t nitems, ScopeCurlHttpTask* task);
    static int ProgressCallback(ScopeCurlHttpTask* data, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow);
    static int SockOptCallback(void* clientp,
                               curl_socket_t curlfd,
                               curlsocktype purpose);

    static void* DownloadThread(void*);

  private:
    int id_;
    ScopeTaskListener* const task_listener_;

    DataSpec spec_;

    // request options
    const DownloadOpts options_;
    int context_id_;
    std::string user_agent_;

    // error result
    DownloadStopReason stop_reason_;
    int last_error_;

    bool abort_;
    AwesomeCacheInterruptCB interrupt_callback_;

    // read timeout related
    int64_t last_dlnow_;
    int64_t last_progress_callback_ts_ms_;

    enum  {
        State_Inited,
        State_Conneced
    } state_;

    pthread_t thread_id_;
    bool thread_joined;

    CURL* curl_;
    // curl_header_list_ 在curl请求之后才能释放，不然会发生SIGSEGV
    struct curl_slist* curl_header_list_;

    std::string http_header_;
    std::map<std::string, std::string> http_headers_;
    bool http_header_parsed_;

    AwesomeCacheRuntimeInfo* ac_rt_info_;

    std::unique_ptr<HttpTaskProgressHelper> progress_helper_;

    // 整个表示通过onReceive回传了多少长度数据
    int64_t recv_valid_bytes_;

    // 兼容服务器不支持range请求的情况
    bool server_not_support_range_;
    int64_t to_skip_bytes_;
    int64_t skipped_bytes_;
};


}
}

