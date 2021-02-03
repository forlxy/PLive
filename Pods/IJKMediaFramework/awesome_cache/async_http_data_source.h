#pragma once
#include "http_data_source.h"
#include "transfer_listener.h"
#include "download/download_task.h"
#include "download/download_manager.h"
#include "default_http_data_source_stats.h"
#include "download/connection_info.h"
#include "awesome_cache_interrupt_cb_c.h"
#include "awesome_cache_runtime_info_c.h"

namespace kuaishou {
namespace cache {

class AsyncHttpDataSource final : public HttpDataSource, public HasConnectionInfo {
  public:
    AsyncHttpDataSource(std::shared_ptr<DownloadManager> download_manager,
                        std::shared_ptr<TransferListener<HttpDataSource> > listener,
                        const DownloadOpts& opts,
                        AwesomeCacheRuntimeInfo* ac_rt_info);

    virtual ~AsyncHttpDataSource();

    virtual int64_t Open(const DataSpec& spec) override;

    virtual int64_t Read(uint8_t* buf, int64_t offset, int64_t len) override;

    virtual AcResultType Close() override;

    virtual Stats* GetStats() override;

    const ConnectionInfo& GetConnectionInfo() override;

  private:
    std::shared_ptr<TransferListener<HttpDataSource> > listener_;
    std::shared_ptr<DownloadManager> download_manager_;
    std::unique_ptr<DownloadTaskWithPriority> download_task_;
    std::shared_ptr<InputStream> input_stream_;

    std::mutex download_task_mutex_;
    std::unique_ptr<DefaultHttpDataSourceStats> stats_;
    int32_t last_error;

    const DownloadOpts opts_;
    ConnectionInfo connectInfo_;
    std::string uri_;
    AwesomeCacheInterruptCB interrupt_cb_;

    int http_max_retry_cnt_;
    CurlType curl_type_;
    bool async_enable_reuse_manager_;

    AwesomeCacheRuntimeInfo* ac_rt_info_;
};

}
}

