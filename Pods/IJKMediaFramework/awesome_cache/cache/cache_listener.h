//
// Created by 帅龙成 on 27/10/2017.
//

#pragma once

#include "cache_span.h"
#include "file.h"
#include <memory>

namespace kuaishou {
namespace cache {

class Cache;

class CacheListener {
  public:
    virtual ~CacheListener() {};

    /**
     * Called when a {@link CacheSpan} is added to the cache.
     *
     * @param cache The source of the event.
     * @param span The added {@link CacheSpan}.
     */
    virtual void OnSpanAdded(Cache* cache, std::shared_ptr<CacheSpan> span) = 0;

    /**
     * Called when a {@link CacheSpan} is removed from the cache.
     *
     * @param cache The source of the event.
     * @param span The removed {@link CacheSpan}.
     */
    virtual void OnSpanRemoved(Cache* cache, std::shared_ptr<CacheSpan> span) = 0;

    /**
     * Called when an existing {@link CacheSpan} is accessed, causing it to be replaced. The new
     * {@link CacheSpan} is guaranteed to represent the same data as the one it replaces, however
     * {@link CacheSpan#file} and {@link CacheSpan#lastAccessTimestamp} may have changed.
     * <p>
     * Note that for span replacement, {@link #onSpanAdded(Cache, CacheSpan)} and
     * {@link #onSpanRemoved(Cache, CacheSpan)} are not called in addition to this method.
     *
     * @param cache The source of the event.
     * @param oldSpan The old {@link CacheSpan}, which has been removed from the cache.
     * @param newSpan The new {@link CacheSpan}, which has been added to the cache.
     */
    virtual void OnSpanTouched(Cache* cache, std::shared_ptr<CacheSpan> oldSpan, std::shared_ptr<CacheSpan> newSpan) = 0;
};

}
}
