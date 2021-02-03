//
// Created by 帅龙成 on 27/10/2017.
//

#pragma once

#include <string>
#include <memory>
#include <atomic>
#include "file.h"

using namespace kuaishou::kpbase;

namespace kuaishou {
namespace cache {

struct CacheSpan {
    /**
    * Creates a CacheSpan.
    *
    * @param key The cache key that uniquely identifies the original stream.
    * @param position The position of the {@link CacheSpan} in the original stream.
    * @param length The length of the {@link CacheSpan}, or {@link C#LENGTH_UNSET} if this is an
    *     open-ended hole.
    * @param lastAccessTimestamp The last access timestamp, or {@link C#TIME_UNSET} if
    *     {@link #isCached} is false.
    * @param file The file corresponding to this {@link CacheSpan}, or null if it's a hole.
    */
    CacheSpan(const std::string& key, int64_t position, int64_t length, int64_t lastAccessTimestamp,
              std::shared_ptr<File> file = nullptr);

    bool IsOpenEnded();

    bool IsHoleSpan();

    friend bool operator<(const CacheSpan& lhs, const CacheSpan& rhs);

    friend bool operator==(const CacheSpan& lhs, const CacheSpan& rhs);

    /**
      * The cache key that uniquely identifies the original stream.
      */
    const std::string key;
    /**
     * The position of the {@link CacheSpan} in the original stream.
     */
    const int64_t position;
    /**
     * The length of the {@link CacheSpan}, or {@link C#LENGTH_UNSET} if this is an open-ended hole.
     */
    const int64_t length;
    /**
     * The file corresponding to this {@link CacheSpan}, or null if {@link #isCached} is false.
     */
    const std::shared_ptr<File> file;
    /**
     * The last access timestamp, or {@link C#TIME_UNSET} if {@link #isCached} is false.
     */
    const int64_t last_access_timestamp;
    /**
     * Whether the {@link CacheSpan} is cached.
     */
    const bool is_cached;

    bool is_locked = false;
};

struct SharedPtrCacheSpanComp {
    bool operator()(const std::shared_ptr<CacheSpan>& a, const std::shared_ptr<CacheSpan>& b) const {
        return *a < *b;
    }
};

}
}
