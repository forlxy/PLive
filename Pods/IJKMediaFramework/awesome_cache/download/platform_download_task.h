#pragma once
#include "download/priority_download_task.h"

#ifdef __APPLE__
#include <TargetConditionals.h>
#endif


namespace kuaishou {
namespace cache {

#if (TARGET_OS_IPHONE) && !defined(TESTING)
#define HAS_IMPLEMENTATION
#endif

class PlatformDownloadTask : public DownloadTaskWithPriority {
  public:
    static bool HasPlatformImplementation() {
#if defined(HAS_IMPLEMENTATION)
        return false;
#else
        return false;
#endif
    }
    static PlatformDownloadTask* Create(const DownloadOpts& opts);
};

#if !defined(HAS_IMPLEMENTATION)
PlatformDownloadTask* PlatformDownloadTask::Create(const DownloadOpts& opts) {
    return nullptr;
}
#endif

#undef HAS_IMPLEMENTATION

} // namespace cache
} // namespace kuaishou
