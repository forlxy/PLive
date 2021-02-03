//
// Created by 帅龙成 on 30/10/2017.
//

#pragma once

#include "constant.h"
#include "cache_span.h"
#include "file.h"

using namespace kuaishou::kpbase;

namespace kuaishou {
namespace cache {

class CachedContentIndex;

class SimpleCacheSpan : public CacheSpan {
  public:

    /**
     * Creates a cache span from an underlying cache file. Upgrades the file if necessary.
     *
     * @param file The cache file.
     * @param index Cached content index.
     * @return The span, or null if the file name is not correctly formatted, or if the id is not
     *     present in the content index.
     */
    static SimpleCacheSpan* CreateCacheEntry(File& file, CachedContentIndex* index);

    static File
    GetCacheFile(const File& cache_dir, int id, int64_t position, int64_t last_access_timestamp);

    static SimpleCacheSpan* CreateLookup(const std::string& key, int64_t position) {
        return new SimpleCacheSpan(key, position);
    }

    static SimpleCacheSpan* CreateOpenHole(const std::string& key, int64_t position) {
        return new SimpleCacheSpan(key, position);
    }

    static SimpleCacheSpan* CreateClosedHole(const std::string& key, int64_t position, int64_t length) {
        return new SimpleCacheSpan(key, position, length, kTimeoutUnSet);
    }

    SimpleCacheSpan* CopyWithUpdatedLastAccessTime(int id);

#ifndef TESTING
  private:
#endif

    SimpleCacheSpan(const std::string& key, int64_t position,
                    int64_t length = kLengthUnset,
                    int64_t lastAccessTimestamp = kTimeoutUnSet,
                    std::shared_ptr<File> file = nullptr);
};

}
}
