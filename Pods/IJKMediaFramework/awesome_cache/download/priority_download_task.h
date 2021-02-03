#pragma once

#include <include/awesome_cache_runtime_info_c.h>
#include "download/download_task.h"
#include "download/download_task_listener.h"
#include "listener.h"
#include "cache_opts.h"

namespace kuaishou {
namespace cache {

static const int KDownload_Log_Interval_Ms = 300;    //300ms 打印一次
static const int KDownload_Log_Bytes_Threshold = 10 * 1024;    //10k

class DownloadTaskWithPriority : public DownloadTask, public kpbase::Listener<DownloadTaskListener, false>  {
  public:
    ~DownloadTaskWithPriority() {}

    DownloadTaskWithPriority() : priority_(DownloadPriority::kPriorityDefault) {}

    void SetPriority(DownloadPriority priority) {
        priority_ = priority;
    };

    DownloadPriority priority() {
        return priority_;
    }

    virtual void Pause() = 0;

    virtual void Resume() = 0;

  private:
    DownloadPriority priority_;
};

class DownloadTaskWithPriorityFactory {
  public:
    virtual ~DownloadTaskWithPriorityFactory() {}

    virtual DownloadTaskWithPriority* CreateTask(const DownloadOpts& opts,
                                                 AwesomeCacheRuntimeInfo* ac_rt_info);
};
}
}
