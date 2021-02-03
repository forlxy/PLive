//
// Created by 帅龙成 on 30/10/2017.
//
#pragma once

#include "cache_listener.h"
#include "cache.h"

namespace kuaishou {
namespace cache {

/**
 * Evicts data from a {@link Cache}. Implementations should call {@link Cache#removeSpan(CacheSpan)}
 * to evict cache entries based on their eviction policies.
 */
class CacheEvictor : public CacheListener {
  public:
    virtual ~CacheEvictor() {};

    /**
     * Called when cache has been initialized.
     */
    virtual void OnCacheInitialized() = 0;

    /**
     * Called when a writer starts writing to the cache.
     *
     * @param cache The source of the event.
     * @param key The key being written.
     * @param position The starting position of the data being written.
     * @param max_length The maximum length of the data being written.
     */
    virtual void OnStartFile(Cache* cache, std::string key, long position, long max_length) = 0;

    /**
     * Called when clear the cache.
     */
    virtual void OnClearCache(Cache* cache) = 0;
};
}
}
