#pragma once

#include <memory>
#include "cache/cached_content_index.h"
#include "cache/cache.h"
#include "cache_defs.h"
#include "file.h"

namespace kuaishou {
namespace cache {

class CacheFactoryImpl : public Cache::Factory {
  public:
    CacheFactoryImpl(File cache_dir, std::shared_ptr<CachedContentIndex> index,
                     CacheEvictorStrategy evictor_strategy = CacheEvictorStrategy::kEvictorLru,
                     int64_t max_cache_bytes = kDefaultMaxCacheBytes);

    Cache* CreateCache() override;

  private:
    const int64_t max_cache_bytes_;
    const File cache_dir_;
    const std::shared_ptr<CachedContentIndex> index_;
    const CacheEvictorStrategy evictor_strategy_;
};

}
}
