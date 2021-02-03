//
// Created by MarshallShuai on 2019-11-10.
//

#include "strategy_never_cache_content_evictor.h"
#include "ac_log.h"
#include "v2/cache/cache_v2_file_manager.h"

HODOR_NAMESPACE_START


void StrategyNeverCacheContentEvictor::OnStrategyNeverTaskAdded(const std::string& key) {
    std::lock_guard<std::mutex> lg(mutex_);
    key_set_.insert(key);
}

int StrategyNeverCacheContentEvictor::PruneStrategyNeverCacheContent(bool clear_all) {
    int prune_count = 0;
    std::lock_guard<std::mutex> lg(mutex_);
    if (key_set_.empty() & !clear_all) {
        LOG_ERROR("[StrategyNeverCacheContentEvictor::PruneStrategyNeverCacheContent] invalid op, int [clear_all==false] mode, current key_set_ is empty");
        return prune_count;
    }

    auto index = CacheV2FileManager::GetMediaDirManager()->Index();
    auto list = index->GetCacheContentListOfEvictStrategy(EvictStrategy_NEVER);

    for (auto& content : list) {
        if (key_set_.find(content->GetKey()) == key_set_.end() || clear_all) {
            // 没有记录在案，删除相关content对应的缓存文件
            // 这个信息比较重要，也比较低频，所以日志打印出来
            LOG_INFO("[StrategyNeverCacheContentEvictor::PruneStrategyNeverCacheContent]delete deprecated media file, key:%s, cachedBytes:%lld ",
                     content->GetKey().c_str(), content->GetCachedBytes())
            content->DeleteScopeFiles();
            index->RemoveCacheContent(content, true);
            prune_count++;
        }
    }
    key_set_.clear();
    return prune_count;
}

HODOR_NAMESPACE_END

