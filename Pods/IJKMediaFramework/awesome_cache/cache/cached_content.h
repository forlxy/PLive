//
// Created by 帅龙成 on 30/10/2017.
//

#pragma once

#include <string>
#include <set>
#include <memory>
#include "constant.h"
#include "simple_cache_span.h"
#include "data_io_stream.h"

namespace kuaishou {
namespace cache {

/**
 * Defines the cached content for a single stream.
 */
class CachedContent {

  public:
    CachedContent(int id, std::string key, int64_t length);

    static std::shared_ptr<CachedContent> NewFromDataInputStream(kpbase::DataInputStream* input);

    AcResultType WriteToStream(kpbase::DataOutputStream* data_output_stream);

    void SetLength(int64_t l);

    void AddSpan(std::shared_ptr<SimpleCacheSpan> span);

    std::set<std::shared_ptr<SimpleCacheSpan>> GetSpans() const;

    std::shared_ptr<SimpleCacheSpan> GetSpan(int64_t position);

    /**
     * Returns the length of the cached data block starting from the {@code position} to the block end
     * up to {@code length_} bytes. If the {@code position_} isn't cached then -(the length of the gap
     * to the next cached data up to {@code length_} bytes) is returned.
     *
     * @param position The starting position of the data.
     * @param length The maximum length of the data to be returned.
     * @return the length of the cached or not cached data block length.
     */
    int64_t GetCachedBytes(int64_t position, int64_t length);

    /**
     * Copies the given span with an updated last access time. Passed span becomes invalid after this
     * call.
     *
     * @param span to be copied and updated. span with the updated last access time.
     * @param error the error of the operation.
     * @return touched span related result or success
     */
    std::shared_ptr<SimpleCacheSpan> Touch(std::shared_ptr<SimpleCacheSpan> span, AcResultType& error);

    bool IsEmptyOrInValid();

    /** Removes the given span from cache. return true if the span is contained*/
    bool RemoveSpan(std::shared_ptr<CacheSpan> span);

    /** Calculates a hash code for the header of this {@code CachedContent}. */
    int32_t HeaderHashCode();

    // Getters
    int id() { return id_; }

    std::string key() { return key_; }

    int64_t length() { return length_; }

    int64_t cached_bytes() {return cached_bytes_;}

    bool IsFullyCached();
  private:
    // Get a span that is less or equal to the given span.
    std::shared_ptr<SimpleCacheSpan> GetFloorSpan(std::shared_ptr<SimpleCacheSpan> lookup_span);
    // Get a span that is bigger than the given span.
    std::shared_ptr<SimpleCacheSpan> GetCeilingSpan(std::shared_ptr<SimpleCacheSpan> lookup_span);

    const int id_;
    const std::string key_;
    std::set<std::shared_ptr<SimpleCacheSpan>, SharedPtrCacheSpanComp> cached_spans_;
    int64_t length_;
    int64_t cached_bytes_;
};

}
}
