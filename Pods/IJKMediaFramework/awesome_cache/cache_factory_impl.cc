#include "cache/simple_cache.h"
#include "cache/no_op_cache_evictor.h"
#include "cache/least_recently_used_cache_evictor.h"
#include "cache_factory_impl.h"

namespace kuaishou {
namespace cache {

CacheFactoryImpl::CacheFactoryImpl(File cache_dir, std::shared_ptr<CachedContentIndex> index,
                                   CacheEvictorStrategy evictor_strategy, int64_t max_cache_bytes):
    cache_dir_(cache_dir),
    index_(index),
    evictor_strategy_(evictor_strategy),
    max_cache_bytes_(max_cache_bytes) {
    assert(max_cache_bytes_ > 0);
    if (!cache_dir.Exists() || cache_dir_.IsDirectory()) {
        File::MakeDirectories(cache_dir);
    }
    assert(index_);
}

Cache* CacheFactoryImpl::CreateCache() {
    switch (evictor_strategy_) {
        case CacheEvictorStrategy::kEvictorNoOp:
            return new SimpleCache(cache_dir_, std::make_shared<NoOpCacheEvictor>(), index_);

        case CacheEvictorStrategy::kEvictorLru:
        default:
            return new SimpleCache(cache_dir_, std::make_shared<LeastRecentlyUsedCacheEvictor>(max_cache_bytes_), index_);
    }
}

}
}
