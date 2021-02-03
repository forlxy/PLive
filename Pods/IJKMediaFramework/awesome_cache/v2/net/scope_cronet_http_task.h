#ifdef CONFIG_AEGON

#ifndef CACHE_V2_NET_SCOPE_CRONET_HTTP_TASK_H
#define CACHE_V2_NET_SCOPE_CRONET_HTTP_TASK_H


#include "scope_task.h"
#include "http_task_progress_helper.h"
#include "connection_info_v2.h"
#include "cache_opts.h"
#include "data_source.h"

#include <mutex>
#include <chrono>
#include <condition_variable>
#include <aegon/aegon.h>

namespace kuaishou {
namespace cache {

class ScopeCronetHttpTask: public ScopeTask {

  public:
    static bool IsEnabled();

  public:
    ScopeCronetHttpTask(const DownloadOpts& opts, ScopeTaskListener* listener,
                        AwesomeCacheRuntimeInfo* ac_rt_info = nullptr);
    ~ScopeCronetHttpTask();

    int64_t Open(const DataSpec& spec) override;
    void Close() override;
    void Abort() override;
    void WaitForTaskFinish() override;

    ConnectionInfoV2 const& GetConnectionInfo() override {
        return this->connection_info_;
    }

  public:
    // some callbacks from cronet
    void OnRedirectReceived(Cronet_UrlResponseInfoPtr info, Cronet_String new_location_url);
    void OnResponseStarted(Cronet_UrlResponseInfoPtr info);
    void OnReadComplete(Cronet_UrlResponseInfoPtr info, Cronet_BufferPtr buffer, uint64_t bytes_read);
    void OnSucceeded(Cronet_UrlResponseInfoPtr info);
    void OnFailed(Cronet_UrlResponseInfoPtr info, Cronet_ErrorPtr error);
    void OnCancelled(Cronet_UrlResponseInfoPtr info);

    // Call onDownloadComplete
    void OnFinished(int32_t error, int32_t stop_reason);

  private:
    int id_;
    ConnectionInfoV2 connection_info_;

    DownloadOpts opts_;
    DataSpec spec_;
    ScopeTaskListener* listener_ = nullptr;
    AwesomeCacheRuntimeInfo* ac_rt_info_ = nullptr;

    HttpTaskProgressHelper progress_helper_;

    std::chrono::steady_clock::time_point start_t_;

    Cronet_UrlRequestCallbackPtr cronet_callback_ = nullptr;
    Cronet_UrlRequestPtr cronet_request_ = nullptr;

    std::mutex mtx_;
    std::condition_variable cond_;
    bool finished_ = false;

    // 整个表示通过onReceive回传了多少长度数据
    int64_t recv_valid_bytes_ = 0;
    // 兼容服务器不支持range请求的情况
    bool server_not_support_range_ = false;
    int64_t to_skip_bytes_ = 0;
    int64_t skipped_bytes_ = 0;
};

}
}


#endif

#endif
