#pragma once

#include <set>
#include "http_data_source.h"
#include "transfer_listener.h"
#include "download/download_task.h"
#include "download/download_manager.h"
#include "stats/default_data_stats.h"

namespace kuaishou {
namespace cache {
namespace {

struct DownloadTaskWrapper {
    std::unique_ptr<DownloadTaskWithPriority> download_task;
    int64_t position = 0;
    int64_t bytes_remaining = 0;
    uint64_t ts = 0;
};

struct DownloadTaskWrapperPtrComp {
    bool operator()(const std::shared_ptr<DownloadTaskWrapper>& a, const std::shared_ptr<DownloadTaskWrapper>& b) {
        return a->ts < b->ts;
    }
};
typedef std::set<std::shared_ptr<DownloadTaskWrapper>, DownloadTaskWrapperPtrComp> DownloadTaskContainer;

}

class MultiDownloadHttpDataSource final : public HttpDataSource {
  private:

  public:
    MultiDownloadHttpDataSource(std::shared_ptr<DownloadManager> download_manager,
                                std::shared_ptr<TransferListener<HttpDataSource> > listener,
                                const DownloadOpts& opts,
                                AwesomeCacheRuntimeInfo* ac_rt_info);

    virtual ~MultiDownloadHttpDataSource();

    virtual int64_t Open(const DataSpec& spec) override;

    virtual int64_t Read(uint8_t* buf, int64_t offset, int64_t len) override;

    virtual AcResultType Close() override;

    virtual Stats* GetStats() override;

    void ClearDownloadTasks();

  private:
    std::shared_ptr<DownloadTaskWrapper> SelectDownloadTask(int64_t position);
    void MaybeEnqueueDownloadTaskOrClose(std::shared_ptr<DownloadTaskWrapper> download_task);
    void CheckEnqueuedDownloadTaskTimeout();

    std::shared_ptr<TransferListener<HttpDataSource> > listener_;
    std::shared_ptr<DownloadManager> download_manager_;
    DownloadTaskContainer download_task_set_;
    std::shared_ptr<DownloadTaskWrapper> current_download_task_;
    std::shared_ptr<InputStream> input_stream_;
    std::unique_ptr<HttpDataSourceStats> stats_;
    const DownloadOpts opts_;
    ConnectionInfo conn_info_;
    DataSpec spec_;
    uint8_t* buf_;
    AwesomeCacheRuntimeInfo* ac_rt_info_;
};
}
}
