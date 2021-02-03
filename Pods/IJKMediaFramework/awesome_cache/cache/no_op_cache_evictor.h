//
// Created by 帅龙成 on 30/10/2017.
//

#pragma once

#include <string>
#include <memory>
#include "cache_span.h"
#include "cache_evictor.h"

namespace kuaishou {
namespace cache {

class NoOpCacheEvictor final : public CacheEvictor {
  public:
    virtual void OnCacheInitialized() override {};

    virtual void OnStartFile(Cache* cache, std::string key, long position, long max_length) override {};

    virtual void OnClearCache(Cache* cache) override {};

    virtual void OnSpanAdded(Cache* cache, std::shared_ptr<CacheSpan> span) override {};

    virtual void OnSpanRemoved(Cache* cache, std::shared_ptr<CacheSpan> span) override {};

    virtual void OnSpanTouched(Cache* cache, std::shared_ptr<CacheSpan> oldSpan,
                               std::shared_ptr<CacheSpan> newSpan) override {};
};

}
}
