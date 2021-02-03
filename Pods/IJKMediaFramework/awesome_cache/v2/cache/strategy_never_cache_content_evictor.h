//
// Created by MarshallShuai on 2019-11-10.
//

#pragma once

#include <set>
#include "utils/macro_util.h"
#include "hodor_downloader/download_priority_step_task.h"

HODOR_NAMESPACE_START

/**
 * 清理 类型为 Strategy_Never的 Media类型的DownloadTask下载的文件
 */
class StrategyNeverCacheContentEvictor {

  public:
    /**
     * 有新任务加入，添加到记录集合
     * @param key task的cacheKey
     */
    void OnStrategyNeverTaskAdded(const std::string& key);

    /**
     * 调用此接口的时候，会把不在记录集合中的任务对应的缓存都删除
     * 注：如果当前任务集合为空，则不会做任何操作，约定不会有这种行为
     * @param true表示全部清除，false表示只清理不在下发列表中的预热视频
     * @return 实际删除了几个不在记录的资源
     */
    int PruneStrategyNeverCacheContent(bool clear_all = false);

  private:
    std::mutex mutex_;
    std::set<std::string> key_set_;
};

HODOR_NAMESPACE_END

