#pragma once
#include <vector>
#include <mutex>
#include <thread>
#include "download/download_manager.h"

namespace kuaishou {
namespace cache {
class SimplePriorityDownloadManager : public DownloadManager {
  public:
    SimplePriorityDownloadManager();
    ~SimplePriorityDownloadManager();

  private:
    virtual void OnConnectionOpen(DownloadTaskWithPriority*, uint64_t position, const ConnectionInfo& info) override;
    virtual void OnConnectionClosed(DownloadTaskWithPriority*, const ConnectionInfo& info, DownloadStopReason reason, uint64_t, uint64_t transfer_consume_ms) override;

    void OnDestroy();

    void RePriorization();

    void Enqueue(std::vector<DownloadTaskWithPriority*>* queue, DownloadTaskWithPriority* task);
    void Dequeue(std::vector<DownloadTaskWithPriority*>* queue, DownloadTaskWithPriority* task);

    void ResumeAll(std::vector<DownloadTaskWithPriority*>* queue);
    void PauseAll(std::vector<DownloadTaskWithPriority*>* queue);

    std::vector<DownloadTaskWithPriority*> low_priority_queue_;
    std::vector<DownloadTaskWithPriority*> normal_priority_queue_;
    std::vector<DownloadTaskWithPriority*> high_priority_queue_;
    std::mutex mutex_;
};

} // namespace cache
} // namespace kuaishou
