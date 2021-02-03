#pragma once

#include <runloop.h>
#include "include/awesome_cache_runtime_info_c.h"
#include "include/cache_opts.h"
#include "data_source.h"
#include "http_data_source.h"
#include "constant.h"
#include "stats/stage_stats.h"
#include "sync_cache_data_source_session_stats.h"
#include "cache_session_listener.h"
#include "download/download_task_listener.h"
#include "include/awesome_cache_callback.h"

namespace kuaishou {
namespace cache {

class HttpProxyDataSource : public DataSource {

  public:
    HttpProxyDataSource(std::shared_ptr<HttpDataSource> upstream,
                        std::shared_ptr<CacheSessionListener> session_listener,
                        const DataSourceOpts& opts, AwesomeCacheRuntimeInfo* ac_rt_info);

    virtual ~HttpProxyDataSource();

    virtual int64_t Open(const DataSpec& spec) override;

    virtual int64_t Read(uint8_t* buf, int64_t offset, int64_t read_len) override;

    virtual AcResultType Close() override;

    virtual Stats* GetStats() override;

  private:
    void ReportDownloadStarted(uint64_t position, const ConnectionInfo& info);
    void ReportDownloadStopped(const char* tag, const ConnectionInfo& info);

  private:
    std::shared_ptr<CacheSessionListener> cache_session_listener_;
    AwesomeCacheCallback* cache_callback_;
    std::shared_ptr<AcCallbackInfo> callbackInfo_;
    std::shared_ptr<HttpDataSource> upstream_data_source_;
    DownloadStopReason stop_reason_for_qos_;
    AwesomeCacheRuntimeInfo* ac_rt_info_;
    int32_t error_code_;
    bool download_stop_need_to_report_;
    int64_t read_position_;
    std::string data_source_extra_;
    std::string product_context_;
    std::unique_ptr<kpbase::Runloop> runloop_;
};

}
}

