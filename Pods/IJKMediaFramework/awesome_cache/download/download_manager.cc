#include "download_manager.h"
#include <assert.h>
#include "ac_log.h"
#include "download/no_op_download_manager.h"
#include "download/simple_priority_download_manager.h"
#include "awesome_cache_runtime_info_c.h"

namespace kuaishou {
namespace cache {

std::unique_ptr<DownloadTaskWithPriority> DownloadManager::CreateDownloadTask(const DownloadOpts& opts,
                                                                              AwesomeCacheRuntimeInfo* ac_rt_info) {
    if (!task_factory_) {
        LOG_ERROR_DETAIL("download factory must be set before creating tasks.");
        assert(false);
    }
    DownloadTaskWithPriority* task = task_factory_->CreateTask(opts, ac_rt_info);
    // 暂时不用优先级下载管理，避免没必要的流程
//  task->AddListener(this);
    return std::unique_ptr<DownloadTaskWithPriority>(task);
}

DownloadManager* DownloadManager::CreateDownloadManager(DownloadStratergy stratergy) {
    switch (stratergy) {
        case kDownloadNoOp: {
            return new NoOpDownloadManager();
        }
        case kDownloadSimplePriority: {
            return new SimplePriorityDownloadManager();
        }
        default: {
            assert(false);
        }
    }
    return nullptr;
}


} // cache
} // kuaishou
