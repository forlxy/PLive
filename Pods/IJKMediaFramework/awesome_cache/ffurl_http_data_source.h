#ifndef AWESOMECACHE_FFURL_HTTP_DATA_SOURCE_H
#define AWESOMECACHE_FFURL_HTTP_DATA_SOURCE_H value

#include "cache_opts.h"
#include "http_data_source.h"
#include "download/connection_info.h"
#include "awesome_cache_interrupt_cb_c.h"
#include "awesome_cache_runtime_info_c.h"
#include "download/download_task.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "libavformat/url.h"

#ifdef __cplusplus
}
#endif

namespace kuaishou {
namespace cache {


class FFUrlHttpDataSource: public HttpDataSource, public HasConnectionInfo {
  public:
    FFUrlHttpDataSource(DownloadOpts const& opts,
                        AwesomeCacheRuntimeInfo* ac_rt_info);

    virtual int64_t Open(DataSpec const& spec) override;
    virtual int64_t Read(uint8_t* buf, int64_t offset, int64_t len) override;
    virtual AcResultType Close() override;
    virtual Stats* GetStats() override { return nullptr; }
    virtual const ConnectionInfo& GetConnectionInfo() override;

  private:
    DownloadOpts const opts_;
    ConnectionInfo connection_info_;
    AwesomeCacheRuntimeInfo* ac_rt_info_;

    URLContext* ctx_ = nullptr;
    int64_t timestamp_start_download_ = 0;
    int64_t downloaded_bytes_ = 0;

    AVIOInterruptCB avio_interrupt_cb_;
};

}

}

#endif /* ifndef AWESOMECACHE_FFURL_HTTP_DATA_SOURCE_H */
