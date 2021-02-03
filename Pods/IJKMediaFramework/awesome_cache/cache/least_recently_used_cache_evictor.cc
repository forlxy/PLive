//
// Created by 帅龙成 on 30/10/2017.
//

#include "least_recently_used_cache_evictor.h"
#include <assert.h>
#include "ac_log.h"

namespace kuaishou {
namespace cache {


static const bool VERBOSE = false;
LeastRecentlyUsedCacheEvictor::LeastRecentlyUsedCacheEvictor(long max_bytes) :
    max_bytes_(max_bytes),
    current_size_(0) {

}

LeastRecentlyUsedCacheEvictor::~LeastRecentlyUsedCacheEvictor() {
}


void LeastRecentlyUsedCacheEvictor::OnCacheInitialized() {
    // Do nothing.
}

void LeastRecentlyUsedCacheEvictor::OnStartFile(Cache* cache, std::string key, long position,
                                                long max_length) {
    EvictCache(cache, max_length);
}

void LeastRecentlyUsedCacheEvictor::OnClearCache(Cache* cache) {
    EvictCache(cache, max_bytes_);
}

void LeastRecentlyUsedCacheEvictor::OnSpanAdded(Cache* cache, std::shared_ptr<CacheSpan> span) {
    assert(cache);
    assert(span);
    least_recently_used.insert(span);
    current_size_ += span->length;
    if (span->length <= 0) {
        LOG_WARN("LeastRecentlyUsedCacheEvictor::OnSpanAdded, span->length(%d) <= 0,is_cached:%d, key:%s",
                 span->length, span->is_cached, span->key.c_str());
    }
    if (VERBOSE) {
        LOG_DEBUG("OnSpanAdded, to call EvictCache(cache, 0), span->position:%lld, span->length:%lld",
                  span->position, span->length);
    }
    EvictCache(cache, 0);
}

void LeastRecentlyUsedCacheEvictor::OnSpanRemoved(Cache* cache, std::shared_ptr<CacheSpan> span) {
    assert(cache);
    assert(span);
    least_recently_used.erase(span);
    current_size_ -= span->length;
    if (VERBOSE) {
        LOG_DEBUG(
            "OnSpanRemoved, to call EvictCache(cache, 0), span->position:%lld, span->length:%lld",
            span->position, span->length);
        PrintCacheStatus("OnSpanRemoved", 0);
    }
}

void LeastRecentlyUsedCacheEvictor::OnSpanTouched(Cache* cache, std::shared_ptr<CacheSpan> oldSpan,
                                                  std::shared_ptr<CacheSpan> newSpan) {
    assert(cache);
    assert(oldSpan);
    assert(newSpan);

    if (VERBOSE) {
        LOG_DEBUG(
            "OnSpanTouched, to call OnSpanRemoved/OnSpanAdded, oldSpan->length:%lld, newSpan->length:%lld",
            oldSpan->length, newSpan->length);
    }
    OnSpanRemoved(cache, oldSpan);
    OnSpanAdded(cache, newSpan);
}

void LeastRecentlyUsedCacheEvictor::EvictCache(Cache* cache, long required_space) {
    assert(cache);

    PrintCacheStatus("Before EvictCache", required_space);
    int max_remove_op_cnt = (int)least_recently_used.size();
    int remove_opt_cnt = 0; // protection from infinite loop(which may be caused by bug)
    while (current_size_ + required_space > max_bytes_
           && !least_recently_used.empty()
           && remove_opt_cnt <= max_remove_op_cnt
          ) {
        auto to_remove = *least_recently_used.begin();
        cache->RemoveSpan(to_remove);
        remove_opt_cnt ++;
        PrintCacheStatus("Doing EvictCache", required_space);
    }

    PrintCacheStatus("Finished EvictCache", required_space);
}

void LeastRecentlyUsedCacheEvictor::PrintCacheStatus(const char* tag, long required_space) {
    if (VERBOSE) {
        LOG_INFO("[%24s][PrintCacheStatus]: lru.size():%3d, current_size_/max_bytes_:%d/%d(%3ldMB/%3ldMB), required_space:%3ldMB",
                 tag, least_recently_used.size(),
                 current_size_, max_bytes_,
                 current_size_ / (1024 * 1024), max_bytes_ / (1024 * 1024),
                 required_space / (1024 * 1024));
    }
}

long LeastRecentlyUsedCacheEvictor:: current_size() {
    return current_size_;
}

}
}
