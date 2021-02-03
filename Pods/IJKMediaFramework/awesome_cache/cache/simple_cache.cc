//
// Created by 帅龙成 on 30/10/2017.
//

#include "simple_cache.h"
#include "simple_cache_span.h"
#include "cached_content.h"
#include "utility.h"
#include "ac_log.h"

using namespace kuaishou::kpbase;

namespace kuaishou {
namespace cache {

SimpleCache::SimpleCache(File cache_dir,
                         std::shared_ptr<CacheEvictor> evcitor,
                         std::shared_ptr<CachedContentIndex> cache_content_index)
    : cache_dir_(cache_dir),
      evictor_(evcitor),
      index_(cache_content_index),
      total_space_(0),
      init_result_(kResultOK) {

    std::lock_guard<std::recursive_mutex> lg(cache_mutex_);
    init_result_ = _initialize();
    evictor_->OnCacheInitialized();
}

std::set<std::shared_ptr<CacheSpan> > SimpleCache::AddListener(const std::string& key, CacheListener* listener) {
    assert(listener);
    {
        std::lock_guard<std::recursive_mutex> lg(cache_mutex_);
        auto iter = listeners_.find(key);
        if (iter != listeners_.end()) {
            iter->second.push_back(listener);
        }
    }
    return GetCachedSpans(key);
}

void SimpleCache::RemoveListener(const std::string& key, CacheListener* listener) {
    std::lock_guard<std::recursive_mutex> lg(cache_mutex_);
    listeners_.erase(key);
}

std::set<std::shared_ptr<CacheSpan>> SimpleCache::GetCachedSpans(const std::string& key) {
    std::lock_guard<std::recursive_mutex> lg(cache_mutex_);
    auto cached_content = index_->Get(key);
    std::set<std::shared_ptr<CacheSpan> > result;
    if (cached_content != nullptr && !cached_content->IsEmptyOrInValid()) {
        auto spans = cached_content->GetSpans();
        for (auto iter = spans.begin(); iter != spans.end(); iter++) {
            result.insert(*iter);
        }
    }
    return result;
}

std::set<std::string> SimpleCache::GetKeys() {
    std::lock_guard<std::recursive_mutex> lg(cache_mutex_);
    return index_->GetKeys();
}

int64_t SimpleCache::GetCacheSpace() {
    std::lock_guard<std::recursive_mutex> lg(cache_mutex_);
    return total_space_;
}

void SimpleCache::ClearCacheSpace() {
    std::lock_guard<std::recursive_mutex> lg(cache_mutex_);
    if (evictor_) {
        evictor_->OnClearCache(this);
    }
}

std::shared_ptr<CacheSpan> SimpleCache::StartRead(const std::string& key, int64_t position, AcResultType& result) {
    std::lock_guard<std::recursive_mutex> lg(cache_mutex_);
    if (init_result_ != kResultOK) {
        result = init_result_;
        return nullptr;
    }
    result = kResultOK;
    std::shared_ptr<SimpleCacheSpan> cache_span = _GetSpan(key, position, result);
    if (result != kResultOK) {
        return nullptr;
    }
    assert(cache_span);
    if (cache_span->is_cached) {
        // Obtain a new span with updated last access timestamp.
        auto new_span = index_->Get(key)->Touch(cache_span, result);
        if (result != kResultOK) {
            return nullptr;
        }
        _NotifySpanTouched(cache_span, new_span);
        return new_span;
    } else {
        // just return the hole span without locking it, user should not write with this span.
        return cache_span;
    }
}

/**
 * 这个方法实现比较特殊，可以看成是加锁了的
 */
std::shared_ptr<CacheSpan> SimpleCache::StartReadWrite(const std::string& key, int64_t position, AcResultType& result,
                                                       int64_t max_write_length) {
    while (true) {
        auto span = StartReadWriteNonBlocking(key, position, result, max_write_length);
        if (result != kResultOK) {
            return nullptr;
        }

        if (span != nullptr && !span->is_locked) {
            return span;
        } else {
            // Write case, lock not available. We'll be woken up when a locked span is released (if the
            // released lock is for the requested key then we'll be able to make progress) or when a
            // span is added to the cache (if the span is for the requested key and covers the requested
            // position, then we'll become a read and be able to make progress).
            event_.Wait();
        }
    }
}

std::shared_ptr<CacheSpan>
SimpleCache::StartReadWriteNonBlocking(const std::string& key, int64_t position, AcResultType& result,
                                       int64_t max_write_length) {
    std::lock_guard<std::recursive_mutex> lg(cache_mutex_);
    if (init_result_ != kResultOK) {
        result = init_result_;
        return nullptr;
    }

    result = kResultOK;
    std::shared_ptr<SimpleCacheSpan> cache_span = _GetSpan(key, position, result);
    if (result != kResultOK) {
        return nullptr;
    }

    assert(cache_span);
    // Read case.
    if (cache_span->is_cached) {
        // Obtain a new span with updated last access timestamp.
        auto new_span = index_->Get(key)->Touch(cache_span, result);
        if (result != kResultOK) {
            return nullptr;
        }
        _NotifySpanTouched(cache_span, new_span);
        return new_span;
    }
    auto constrained_span = _ConstrainWriteSpanMaxLength(cache_span, max_write_length);

    // Write case, lock available.
    if (locked_spans_.find(key) == locked_spans_.end()) {
        std::set<std::shared_ptr<CacheSpan> > span_set;
        span_set.insert(constrained_span);
        locked_spans_[key] = span_set;
    } else {
        auto span_set = locked_spans_[key];
        // locked span means that it is locked by somebody else and not useable.
        std::shared_ptr<SimpleCacheSpan> locked_span = nullptr;
        for (auto span : span_set) {
            if (span->IsOpenEnded() && span->position <= constrained_span->position) {
                locked_span = constrained_span;
                break;
            } else if (!span->IsOpenEnded() && constrained_span->position >= span->position
                       && constrained_span->position < span->position + span->length) {
                int64_t length;
                if (constrained_span->IsOpenEnded()) {
                    length = span->position + span->length - constrained_span->position;
                } else {
                    length = std::min(constrained_span->length, span->position + span->length - constrained_span->position);
                }
                locked_span = std::shared_ptr<SimpleCacheSpan>(SimpleCacheSpan::CreateClosedHole(constrained_span->key, constrained_span->position, length));
                break;
            }
            if (constrained_span->IsOpenEnded() && constrained_span->position < span->position) {
                constrained_span = _ConstrainWriteSpanMaxLength(constrained_span, span->position -
                                                                constrained_span->position);
            } else if (!constrained_span->IsOpenEnded() && constrained_span->position < span->position &&
                       constrained_span->position + constrained_span->length > span->position) {
                constrained_span = _ConstrainWriteSpanMaxLength(constrained_span, span->position -
                                                                constrained_span->position);
            }
        }
        if (locked_span) {
            // Write case, spans is locked by somebody.
            locked_span->is_locked = true;
            return locked_span;
        } else {
            locked_spans_[key].insert(constrained_span);
        }
    }

    return constrained_span;
}

File
SimpleCache::StartFile(const std::string& key, int64_t position, int64_t maxLength, AcResultType& result) {
    std::lock_guard<std::recursive_mutex> lg(cache_mutex_);
    result = kResultOK;
    assert(locked_spans_.find(key) != locked_spans_.end());
    if (!cache_dir_.Exists()) {
        _RemoveStaleSpansAndCachedContents();
        if (!File::MakeDirectories(cache_dir_)) {
            LOG_ERROR_DETAIL("[SimpleCache], make cache_dir_ fail, %s", cache_dir_.path().c_str());
            result = kResultFileExceptionCreateDirFail;
            return File("");
        } else {
            LOG_DEBUG("[SimpleCache], make cache_dir_ success, %s", cache_dir_.path().c_str());
        }
    } else {
        // LOG_DEBUG("[SimpleCache], cache_dir_ already exist, %s", cache_dir_.path().c_str());
    }
    evictor_->OnStartFile(this, key, position, maxLength);
    return SimpleCacheSpan::GetCacheFile(cache_dir_, index_->AssignIdForKey(key), position,
                                         SystemUtil::GetEpochTime());
}

AcResultType SimpleCache::CommitFile(File& file) {
    std::lock_guard<std::recursive_mutex> lg(cache_mutex_);

    // If the file doesn't exist, don't add it to the in-memory representation.
    if (!file.Exists()) {
        return kResultOK;
    }
    auto span = std::shared_ptr<SimpleCacheSpan>(SimpleCacheSpan::CreateCacheEntry(file, index_.get()));
    if (span == nullptr) {
        return kResultCacheCreateSpanEntryFail;
    }

    // If the file has length 0, delete it and don't add it to the in-memory representation.
    if (file.file_size() == 0) {
        file.Remove();
    }

    if (locked_spans_.find(span->key) == locked_spans_.end()) {
        file.Remove();
        return kResultOK;
    }

    // Check if the span conflicts with the set content length
    int64_t length = GetContentLength(span->key);
    if (length != kLengthUnset && (span->position + span->length) > length) {
        file.Remove();
        return kResultOK;
    }

    _AddSpan(span);
    int32_t ret = index_->Store();
    event_.Signal();
    return ret;
}

void SimpleCache::ReleaseHoleSpan(std::shared_ptr<CacheSpan> holeSpan) {
    std::lock_guard<std::recursive_mutex> lg(cache_mutex_);
    assert(!holeSpan->is_locked);
    auto iter = locked_spans_.find(holeSpan->key);
    if (iter != locked_spans_.end()) {
        assert(iter->second.find(holeSpan) != iter->second.end());
        locked_spans_[holeSpan->key].erase(holeSpan);
        if (locked_spans_[holeSpan->key].empty()) {
            locked_spans_.erase(holeSpan->key);
        }
    }
    event_.Signal();
}

void SimpleCache::RemoveSpan(std::shared_ptr<CacheSpan> span) {
    std::lock_guard<std::recursive_mutex> lg(cache_mutex_);
    _RemoveSpan(span, true);
}

bool SimpleCache::IsCached(const std::string& key, int64_t position, int64_t length) {
    std::lock_guard<std::recursive_mutex> lg(cache_mutex_);
    auto cached_content = index_->Get(key);
    return cached_content != nullptr && cached_content->GetCachedBytes(position, length) > length;
}

int64_t SimpleCache::GetCachedBytes(const std::string& key, int64_t position, int64_t length) {
    std::lock_guard<std::recursive_mutex> lg(cache_mutex_);
    auto cached_content = index_->Get(key);
    return cached_content != nullptr ? cached_content->GetCachedBytes(position, length) : -length;
}

AcResultType SimpleCache::SetContentLength(const std::string& key, int64_t length) {
    std::lock_guard<std::recursive_mutex> lg(cache_mutex_);
    index_->SetContentLength(key, length);
    return index_->Store();
}

int64_t SimpleCache::GetContentLength(const std::string& key) {
    std::lock_guard<std::recursive_mutex> lg(cache_mutex_);
    return index_->GetContentLength(key);
}


bool SimpleCache::IsFullyCached(const std::string& key) {
    std::lock_guard<std::recursive_mutex> lg(cache_mutex_);
    auto cached_content = index_->Get(key);
    return cached_content != nullptr ? cached_content->IsFullyCached() : false;
}

int64_t SimpleCache::GetContentCachedBytes(const std::string& key) {
    std::lock_guard<std::recursive_mutex> lg(cache_mutex_);
    return index_->GetContentCachedBytes(key);
}

#pragma mark private methods
AcResultType SimpleCache::_initialize() {
    if (!cache_dir_.Exists()) {
        if (!File::MakeDirectories(cache_dir_)) {
            return kResultFileExceptionCreateDirFail;
        } else {
            return kResultOK;
        }
    }

    index_->Load();

    std::vector<File> files = File::ListRegularFiles(cache_dir_);
    for (auto iter = files.begin(); iter != files.end(); iter++) {
        if (iter->file_name() == CachedContentIndex::kFileName) {
            continue;
        }

        std::shared_ptr<SimpleCacheSpan> span =
            iter->file_size() > 0 ?
            std::shared_ptr<SimpleCacheSpan>(SimpleCacheSpan::CreateCacheEntry(*iter, index_.get()))
            : nullptr;
        if (span != nullptr) {
            _AddSpan(span);
        } else {
            iter->Remove();
        }
    }

    index_->RemoveEmptyAndInvalid();
    index_->Store();
    return kResultOK;
}


void SimpleCache::_AddSpan(std::shared_ptr<SimpleCacheSpan> span) {
    assert(span);
    index_->Add(span->key)->AddSpan(span);
    total_space_ += span->length;
    _NotifySpanAdded(span);
}

AcResultType SimpleCache::RemoveStaleSpans(const std::string& key) {
    std::lock_guard<std::recursive_mutex> lg(cache_mutex_);
    std::vector<std::shared_ptr<CacheSpan> > spans_to_be_removed;
    auto cached_content = index_->Get(key);
    if (cached_content) {
        for (auto span : cached_content->GetSpans()) {
            if (!span->file->Exists()) {
                LOG_DEBUG("SimpleCache::RemoveStaleSpans, file not exist, to remove");
                spans_to_be_removed.push_back(span);
            }
        }
    }

    for (auto span : spans_to_be_removed) {
        AcResultType ret = _RemoveSpan(span, false);
        if (ret != kResultOK) {
            return ret;
        }
    }
    index_->RemoveEmpty(_GetLockedKeySet());
    return index_->Store();
}

/**
 * Scans all of the cached spans in the in-memory representation, removing any for which files
 * no longer exist.
 */
AcResultType SimpleCache::_RemoveStaleSpansAndCachedContents() {
    std::vector<std::shared_ptr<CacheSpan> > spans_to_be_removed;

    for (auto content : index_->GetAll()) {
        for (auto span : content->GetSpans()) {
            if (!span->file->Exists()) {
                spans_to_be_removed.push_back(span);
            }
        }
    }

    for (auto span : spans_to_be_removed) {
        AcResultType ret = _RemoveSpan(span, false);
        if (ret != kResultOK) {
            return ret;
        }
    }

    index_->RemoveEmpty(_GetLockedKeySet());
    return index_->Store();
}

AcResultType SimpleCache::_RemoveSpan(std::shared_ptr<CacheSpan> span,
                                      bool remove_empty_cached_content) {
    assert(span);
    auto content = index_->Get(span->key);
    if (content == nullptr || !content->RemoveSpan(span)) {
        _NotifySpanRemoved(span);
        return kResultOK;
    }

    AcResultType ret = kResultOK;
    total_space_ -= span->length;
    if (remove_empty_cached_content && content->IsEmptyOrInValid()) {
        index_->RemoveEmpty(content->key());
        ret = index_->Store();
    }
    _NotifySpanRemoved(span);
    return ret;
}

void SimpleCache::_NotifySpanRemoved(std::shared_ptr<CacheSpan> span) {
    _NotifyListenerInternal(span, [this, span](CacheListener * l) { l->OnSpanRemoved(this, span); });
    evictor_->OnSpanRemoved(this, span);
}

void SimpleCache::_NotifySpanAdded(std::shared_ptr<CacheSpan> span) {
    _NotifyListenerInternal(span, [this, span](CacheListener * l) { l->OnSpanAdded(this, span); });
    evictor_->OnSpanAdded(this, span);
}

void SimpleCache::_NotifySpanTouched(std::shared_ptr<CacheSpan> old_span,
                                     std::shared_ptr<CacheSpan> new_span) {
    _NotifyListenerInternal(old_span, [this, old_span, new_span](CacheListener * l) {
        l->OnSpanTouched(this, old_span, new_span);
    });
    evictor_->OnSpanTouched(this, old_span, new_span);
}

void SimpleCache::_NotifyListenerInternal(std::shared_ptr<CacheSpan> span,
                                          SimpleCache::ListenerAction action) {
    auto listeners_for_key = listeners_.find(span->key);
    if (listeners_for_key != listeners_.end()) {
        for (CacheListener* l : listeners_for_key->second) {
            action(l);
        }
    }
}

std::shared_ptr<SimpleCacheSpan> SimpleCache::_GetSpan(const string& key, int64_t position,
                                                       AcResultType& error) {
    error = kResultOK;
    auto cache_content = index_->Get(key);
    if (cache_content == nullptr) {
        return std::shared_ptr<SimpleCacheSpan>(SimpleCacheSpan::CreateOpenHole(key, position));
    }

    while (true) {
        auto span = cache_content->GetSpan(position);
        if (span->is_cached && !span->file->Exists()) {
            // The file has been deleted from under us. It's likely that other files will have been
            // deleted too, so scan the whole in-memory representation.
            _RemoveStaleSpansAndCachedContents();
            continue;
        }
        return span;
    }
}

std::set<std::string> SimpleCache::_GetLockedKeySet() {
    std::set<std::string> key_set;
    for (auto it : locked_spans_) {
        key_set.insert(it.first);
    }
    return key_set;
}

std::shared_ptr<SimpleCacheSpan> SimpleCache::_ConstrainWriteSpanMaxLength(
    std::shared_ptr<SimpleCacheSpan> span, int64_t max_length) {
    assert(span->IsHoleSpan());
    if (max_length == kLengthUnset) {
        return span;
    }
    int64_t length = max_length;
    if (!span->IsOpenEnded()) {
        length = std::min(max_length, span->length);
    }
    return std::shared_ptr<SimpleCacheSpan>(SimpleCacheSpan::CreateClosedHole(span->key, span->position, length));
}


}
}
