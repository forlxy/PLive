//
// Created by 帅龙成 on 27/10/2017.
//

#pragma once

#include <string>
#include <set>
#include <memory>
#include "cache_span.h"
#include "cache_listener.h"
#include "constant.h"
#include "file.h"

using namespace kuaishou::kpbase;

namespace kuaishou {
namespace cache {

class Cache {
  public:
    class Factory {
      public:
        virtual Cache* CreateCache() = 0;
        virtual ~Factory() {};
    };

  public:
    virtual ~Cache() {};

    /**
    * Registers a listener to listen for changes to a given key.
    * <p>
    * No guarantees are made about the thread or threads on which the listener is called, but it is
    * guaranteed that listener methods will be called in a serial fashion (i.e. one at a time) and in
    * the same order as events occurred.
    *
    * @param key The key to listen to.
    * @param listener The listener to add.
    * @return The current spans for the key.
    */
    virtual std::set<std::shared_ptr<CacheSpan> > AddListener(const std::string& key, CacheListener* listener) = 0;

    /**
     * Unregisters a listener.
     *
     * @param key The key to stop listening to.
     * @param listener The listener to remove.
     */
    virtual void RemoveListener(const std::string& key, CacheListener* listener) = 0;

    /**
     * Returns the cached spans for a given cache key.
     *
     * @param key The key for which spans should be returned.
     * @return The spans for the key. May be null if there are no such spans.
     */
    virtual std::set<std::shared_ptr<CacheSpan> > GetCachedSpans(const std::string& key) = 0;

    /**
     * Returns all keys in the cache.
     *
     * @return All the keys in the cache.
     */
    virtual std::set<std::string> GetKeys() = 0;

    /**
     * Returns the total disk space in bytes used by the cache.
     *
     * @return The total disk space in bytes.
     */
    virtual int64_t GetCacheSpace() = 0;

    /**
     * Clear the total disk space in bytes used by the cache.
     */
    virtual void ClearCacheSpace() = 0;

    virtual std::shared_ptr<CacheSpan> StartRead(const std::string& key, int64_t position, AcResultType& result) = 0;

    /**
     * A caller should invoke this method when they require data from a given position for a given
     * key.
     * <p>
     * If there is a cache entry that overlaps the position, then the returned {@link CacheSpan}
     * defines the file in which the data is stored. {@link CacheSpan#isCached} is true. The caller
     * may read from the cache file, but does not acquire any locks.
     * <p>
     * If there is no cache entry overlapping {@code offset}, then the returned {@link CacheSpan}
     * defines a hole in the cache starting at {@code position} into which the caller may write as it
     * obtains the data from some other source. The returned {@link CacheSpan} serves as a lock.
     * Whilst the caller holds the lock it may write data into the hole. It may split data into
     * multiple files. When the caller has finished writing a file it should commit it to the cache
     * by calling {@link #commitFile(File)}. When the caller has finished writing, it must release
     * the lock by calling {@link #releaseHoleSpan}.
     *
     * @param key The key of the data being requested.
     * @param position The position of the data being requested.
     * @return The {@link CacheSpan}.
     * @throws InterruptedException
     */
    virtual std::shared_ptr<CacheSpan> StartReadWrite(const std::string& key, int64_t position, AcResultType& result,
                                                      int64_t max_write_length = kLengthUnset) = 0;

    /**
     * Same as {@link #startReadWrite(String, int64_t)}. However, if the cache entry is locked, then
     * instead of blocking, this method will return null as the {@link CacheSpan}.
     *
     * @param key The key of the data being requested.
     * @param position The position of the data being requested.
     * @return The {@link CacheSpan}. Or null if the cache entry is locked.
     */
    virtual std::shared_ptr<CacheSpan>
    StartReadWriteNonBlocking(const std::string& key, int64_t position, AcResultType& result, int64_t max_write_length = kLengthUnset) = 0;

    /**
     * Obtains a cache file into which data can be written. Must only be called when holding a
     * corresponding hole {@link CacheSpan} obtained from {@link #startReadWrite(String, int64_t)}.
     *
     * @param key The cache key for the data.
     * @param position The starting position of the data.
     * @param maxLength The maximum length of the data to be written. Used only to ensure that there
     *     is enough space in the cache.
     * @return The file into which data should be written.
     */
    virtual File
    StartFile(const std::string& key, int64_t position, int64_t maxLength, AcResultType& result) = 0;

    /**
     * Commits a file into the cache. Must only be called when holding a corresponding hole
     * {@link CacheSpan} obtained from {@link #startReadWrite(String, int64_t)}
     *
     * @param file A newly written cache file.
     */
    virtual AcResultType CommitFile(File& file) = 0;

    /**
     * Releases a {@link CacheSpan} obtained from {@link #startReadWrite(String, int64_t)} which
     * corresponded to a hole in the cache.
     *
     * @param holeSpan The {@link CacheSpan} being released.
     */
    virtual void ReleaseHoleSpan(std::shared_ptr<CacheSpan>) = 0;

    /**
     * Removes a cached {@link CacheSpan} from the cache, deleting the underlying file.
     *
     * @param span The {@link CacheSpan} to remove.
     */
    virtual void RemoveSpan(std::shared_ptr<CacheSpan>) = 0;

    /**
     * Queries if a range is entirely available in the cache.
     *
     * @param key The cache key for the data.
     * @param position The starting position of the data.
     * @param length The length of the data.
     * @return true if the data is available in the Cache otherwise false;
     */
    virtual bool IsCached(const std::string& key, int64_t position, int64_t length) = 0;

    /**
     * Returns the length of the cached data block starting from the {@code position} to the block end
     * up to {@code length} bytes. If the {@code position} isn't cached then -(the length of the gap
     * to the next cached data up to {@code length} bytes) is returned.
     *
     * @param key The cache key for the data.
     * @param position The starting position of the data.
     * @param length The maximum length of the data to be returned.
     * @return the length of the cached or not cached data block length.
     */
    virtual int64_t GetCachedBytes(const std::string& key, int64_t position, int64_t length) = 0;

    /**
     * Sets the content length for the given key.
     *
     * @param key The cache key for the data.
     * @param length The length of the data.
     */
    virtual AcResultType SetContentLength(const std::string& key, int64_t length) = 0;

    /**
     * Returns the content length for the given key if one set, or {@link
     * com.google.android.exoplayer2.C#LENGTH_UNSET} otherwise.
     *
     * @param key The cache key for the data.
     */
    virtual int64_t GetContentLength(const std::string& key) = 0;

    /**
     * Remove spans that exists in memory but the file does not exist for a given key.
     * @return result type.
     */
    virtual AcResultType RemoveStaleSpans(const std::string& key) = 0;

    /**
     * Tells whether a key is fully cached.
     * @return whether a key is fully cached.
     */
    virtual bool IsFullyCached(const std::string& key) = 0;

    /**
     * Returns the cached bytes of content for the given key if one set, or {@link
     * com.google.android.exoplayer2.C#LENGTH_UNSET} otherwise.
     *
     * @param key The cache key for the data.
     */
    virtual int64_t GetContentCachedBytes(const std::string& key) = 0 ;

};

}
}




