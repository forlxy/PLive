#include "download/libcurl_download_task.h"
#include "download/libcurl_download_task_opt.h"
#include "download/priority_download_task.h"
#include "download/platform_download_task.h"

namespace kuaishou {
namespace cache {

DownloadTaskWithPriority* DownloadTaskWithPriorityFactory::CreateTask(const DownloadOpts& opts,
                                                                      AwesomeCacheRuntimeInfo* ac_rt_info) {
    DownloadTaskWithPriority* task;
    if (PlatformDownloadTask::HasPlatformImplementation()) {
        task = PlatformDownloadTask::Create(opts);
    } else {
        if (opts.curl_type == kCurlTypeAsyncDownload) {
            task = new LibcurlDownloadTaskOpt(opts, ac_rt_info);
        } else {
            task = new LibcurlDownloadTask(opts, ac_rt_info);
        }
    }
    return task;
}

} // namespace cache
} // namespace kuaishou
