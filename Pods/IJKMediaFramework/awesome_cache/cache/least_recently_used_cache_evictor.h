//
// Created by 帅龙成 on 30/10/2017.
//
#pragma once

#include "cache_span.h"
#include "cache_evictor.h"

namespace kuaishou {
namespace cache {

class LeastRecentlyUsedCacheEvictor final : public CacheEvictor {
  public:
    LeastRecentlyUsedCacheEvictor(long max_bytes);

    virtual ~LeastRecentlyUsedCacheEvictor();

    virtual void OnCacheInitialized() override;

    virtual void OnStartFile(Cache* cache, std::string key, long position, long max_length) override;

    virtual void OnClearCache(Cache* cache) override;

    virtual void OnSpanAdded(Cache* cache, std::shared_ptr<CacheSpan> span) override;

    virtual void OnSpanRemoved(Cache* cache, std::shared_ptr<CacheSpan> span) override;

    virtual void OnSpanTouched(Cache* cache, std::shared_ptr<CacheSpan> oldSpan,
                               std::shared_ptr<CacheSpan> newSpan) override;

    long current_size();
  private:
    void EvictCache(Cache* cache, long required_space);
    void PrintCacheStatus(const char* tag, long required_space);

    struct CacheSpanLruComp {
        bool operator()(const std::shared_ptr<CacheSpan> lhs, const std::shared_ptr<CacheSpan> rhs) const {
            int64_t timestamp_delta = lhs->last_access_timestamp - rhs->last_access_timestamp;
            if (timestamp_delta == 0) {
                return *lhs < *rhs;
            }
            return lhs->last_access_timestamp < rhs->last_access_timestamp;
        }
    };
  private:
    const long max_bytes_;
    std::set<std::shared_ptr<CacheSpan>, CacheSpanLruComp> least_recently_used;
    long current_size_;
};

}
}

