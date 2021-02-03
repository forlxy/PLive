//
// Created by 帅龙成 on 30/10/2017.
//
#pragma once

#include <map>
#include <mutex>
#include "cache.h"
#include "file.h"
#include "cache_evictor.h"
#include "cached_content_index.h"
#include "simple_cache_span.h"
#include "event.h"

namespace kuaishou {
namespace cache {

/**
 * 为了让此类多线程更清晰，特作出以下命名约定:
 * 1.non-lock的方法都用下划线开头
 * 2.public方法都是 has-lock 的方法
 */
class SimpleCache : public Cache {
  public:
    SimpleCache(File cache_dir, std::shared_ptr<CacheEvictor> evcitor,
                std::shared_ptr<CachedContentIndex> cache_content_index);

    virtual std::set<std::shared_ptr<CacheSpan> >
    AddListener(const std::string& key, CacheListener* listener) override;

    virtual void RemoveListener(const std::string& key, CacheListener* listener) override;

    virtual std::set<std::shared_ptr<CacheSpan> > GetCachedSpans(const std::string& key) override;

    virtual std::set<std::string> GetKeys() override;

    virtual int64_t GetCacheSpace() override;

    virtual void ClearCacheSpace() override;

    virtual std::shared_ptr<CacheSpan> StartRead(const std::string& key, int64_t position, AcResultType& result) override;

    virtual std::shared_ptr<CacheSpan> StartReadWrite(const std::string& key, int64_t position, AcResultType& result, int64_t max_write_length = kLengthUnset) override;

    virtual std::shared_ptr<CacheSpan> StartReadWriteNonBlocking(const std::string& key, int64_t position, AcResultType& result, int64_t max_write_length = kLengthUnset) override;

    virtual File StartFile(const std::string& key, int64_t position, int64_t maxLength, AcResultType& result) override;

    virtual AcResultType CommitFile(File& file) override;

    virtual void ReleaseHoleSpan(std::shared_ptr<CacheSpan> holeSpan) override;

    virtual void RemoveSpan(std::shared_ptr<CacheSpan> span) override;

    virtual bool IsCached(const std::string& key, int64_t position, int64_t length) override;

    virtual int64_t GetCachedBytes(const std::string& key, int64_t position, int64_t length) override;

    virtual AcResultType SetContentLength(const std::string& key, int64_t length) override;

    virtual int64_t GetContentLength(const std::string& key) override;

    virtual AcResultType RemoveStaleSpans(const std::string& key) override;

    virtual bool IsFullyCached(const std::string& key) override;

    virtual int64_t GetContentCachedBytes(const std::string& key) override;
  private:
    AcResultType _initialize();

    void _AddSpan(std::shared_ptr<SimpleCacheSpan> pSpan);

    AcResultType _RemoveStaleSpansAndCachedContents();

    AcResultType _RemoveSpan(std::shared_ptr<CacheSpan> span, bool remove_empty_cached_content);

    std::shared_ptr<SimpleCacheSpan> _GetSpan(const string& key, int64_t position, AcResultType& error);

    using ListenerAction = std::function<void(CacheListener*)>;

    void _NotifySpanAdded(std::shared_ptr<CacheSpan> span);

    void _NotifySpanRemoved(std::shared_ptr<CacheSpan> span);

    void _NotifySpanTouched(std::shared_ptr<CacheSpan> old_span,
                            std::shared_ptr<CacheSpan> new_span);

    void _NotifyListenerInternal(std::shared_ptr<CacheSpan> span, ListenerAction action);

    std::set<std::string> _GetLockedKeySet();

    std::shared_ptr<SimpleCacheSpan> _ConstrainWriteSpanMaxLength(
        std::shared_ptr<SimpleCacheSpan> span, int64_t max_length);

  private:
    const File cache_dir_;
    const std::shared_ptr<CacheEvictor> evictor_;
    std::map<std::string, std::set<std::shared_ptr<CacheSpan> > > locked_spans_;
    std::map<std::shared_ptr<CacheSpan>, int, SharedPtrCacheSpanComp> reading_span_refs_;
    const std::shared_ptr<CachedContentIndex> index_;
    std::map<std::string, std::vector<CacheListener*>> listeners_;
    int64_t total_space_;
    AcResultType init_result_;

    std::recursive_mutex cache_mutex_; // 所有的public接口都应该是synchronized的
    Event event_;
};

}
}
