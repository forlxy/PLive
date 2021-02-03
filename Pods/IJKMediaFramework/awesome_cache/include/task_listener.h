#pragma once

#include <stdint.h>
namespace kuaishou {
namespace cache {

class TaskListener {
  public:
    virtual ~TaskListener() {}
    virtual void OnTaskSuccessful() {}
    virtual void OnTaskCancelled() {}
    virtual void OnTaskFailed(int32_t fail_reason) {}
    virtual void OnTaskComplete(int32_t reason) {}
    virtual void onTaskProgress(int64_t position, int64_t totalsize) {}
    virtual void onTaskStopped(int64_t download_bytes, int64_t transfer_consume_ms) {} // 这个已经废弃了
    virtual void onTaskStopped(int64_t download_bytes, int64_t transfer_consume_ms, const char* sign) {}
    virtual void onTaskStarted(int64_t start_pos, int64_t cached_bytes, int64_t totalsize) {}
};
}
}
