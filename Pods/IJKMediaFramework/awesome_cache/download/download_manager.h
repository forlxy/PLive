#pragma once
#include <memory>
#include <vector>
#include "cache_opts.h"
#include "download/download_task_listener.h"
#include "download/priority_download_task.h"

namespace kuaishou {
namespace cache {

/**
 * Abstract download manager manages download task.
 */
class DownloadManager : public DownloadTaskListener {
  public:
    static DownloadManager* CreateDownloadManager(DownloadStratergy stratergy);

    std::unique_ptr<DownloadTaskWithPriority> CreateDownloadTask(const DownloadOpts& opts,
                                                                 AwesomeCacheRuntimeInfo* ac_rt_info);

    void SetDownloadTaskFactory(std::shared_ptr<DownloadTaskWithPriorityFactory> factory) {
        task_factory_ = factory;
    }

    DownloadManager() {};
    virtual ~DownloadManager() {};

  private:
    std::shared_ptr<DownloadTaskWithPriorityFactory> task_factory_;
};

} // cache
} // kuaishou
