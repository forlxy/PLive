#pragma once
#include <stdint.h>
#include "cache_defs.h"
#include "download/download_task.h"

namespace kuaishou {
namespace cache {
static const int kProgressBytesThreshold = 10 * 1024;

class DownloadTaskWithPriority;

class DownloadTaskListener {
  public:
    virtual ~DownloadTaskListener() {}
    virtual void OnConnectionOpen(DownloadTaskWithPriority*, uint64_t position, const ConnectionInfo& info) {}
    virtual void OnDownloadProgress(DownloadTaskWithPriority*, uint64_t position) {}
    virtual void OnConnectionClosed(DownloadTaskWithPriority*, const ConnectionInfo& info,
                                    DownloadStopReason reason, uint64_t downloaded_bytes, uint64_t transfer_consume_ms) {}

    virtual void OnDownloadPaused(DownloadTaskWithPriority*) {}   // will be removed later
    virtual void OnDownloadResumed(DownloadTaskWithPriority*) {}  // will be removed later
};

} // cache
} // kuaishou

