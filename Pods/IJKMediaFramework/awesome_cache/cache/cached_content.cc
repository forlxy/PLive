//
// Created by 帅龙成 on 30/10/2017.
//

#include <constant.h>
#include "cached_content.h"
#include "ac_log.h"

namespace kuaishou {
namespace cache {

CachedContent::CachedContent(int id, std::string key, int64_t length) :
    id_(id), key_(key), length_(length), cached_bytes_(0) {
}

#define CHECK_OK_RETURN_NULL(err) if (err != 0) return nullptr;
std::shared_ptr<CachedContent>
CachedContent::NewFromDataInputStream(kpbase::DataInputStream* input) {
    int err;
    int id = input->ReadInt(err);
    CHECK_OK_RETURN_NULL(err);
    std::string key = input->ReadString(err);
    CHECK_OK_RETURN_NULL(err);
    int64_t length = input->ReadInt64(err);
    CHECK_OK_RETURN_NULL(err);
    return std::make_shared<CachedContent>(id, key, length);
}

AcResultType CachedContent::WriteToStream(kpbase::DataOutputStream* data_output_stream) {
    data_output_stream->WriteInt(id_);
    data_output_stream->WriteString(key_);
    data_output_stream->WriteInt64(length_);
    if (!data_output_stream->Good()) {
        LOG_ERROR_DETAIL("CachedContent::WriteToStream, fail");
        return kResultCachedContentWriteToStreamFail;
    }
    return kResultOK;
}

void CachedContent::SetLength(int64_t l) {
    if (l < 0) {
        LOG_ERROR("CachedContent::SetLength:%lld < 0! previous length_:%lld, key:%s", l, length_, key_.c_str());
    }
    length_ = l;
}

void CachedContent::AddSpan(std::shared_ptr<SimpleCacheSpan> span) {
    assert(span->is_cached);
    assert(span->length != kLengthUnset);
    cached_spans_.insert(span);
    cached_bytes_ += span->length;
}

std::set<std::shared_ptr<SimpleCacheSpan> > CachedContent::GetSpans() const {
    return std::set<std::shared_ptr<SimpleCacheSpan> >(cached_spans_.begin(), cached_spans_.end());
}

std::shared_ptr<SimpleCacheSpan> CachedContent::GetSpan(int64_t position) {
    auto lookup_span = std::shared_ptr<SimpleCacheSpan>(SimpleCacheSpan::CreateLookup(key_, position));
    auto floor_span = GetFloorSpan(lookup_span);
    if (floor_span && floor_span->position + floor_span->length > position) {
        return floor_span;
    }
    auto ceil_span = GetCeilingSpan(lookup_span);
    SimpleCacheSpan* span = ceil_span == nullptr ?
                            SimpleCacheSpan::CreateOpenHole(key_, position) :
                            SimpleCacheSpan::CreateClosedHole(key_, position, ceil_span->position - position);
    return std::shared_ptr<SimpleCacheSpan>(span);
}

int64_t CachedContent::GetCachedBytes(int64_t position, int64_t length) {
    auto span = GetSpan(position);
    if (span->IsHoleSpan()) {
        if (span->IsOpenEnded()) {
            return -length;
        } else {
            return -std::min(span->length, length);
        }
    }
    /**           query_pos                                    query_end
     *               |********************************************|
     *    span1_pos              span1_end
     *       |***********************|
     *                   span2_pos                span2_end
     *                      |************************|
     *                                                       span3_pos             span3_end
     *                                                           |*********************|
     * In this case, current_end_pos will  become span2_end
     */
    int64_t query_end_pos = position + length;
    int64_t current_end_pos = span->position + span->length;
    if (current_end_pos < query_end_pos) {
        auto ceil_span_it = cached_spans_.upper_bound(span);
        std::set<std::shared_ptr<SimpleCacheSpan>, SharedPtrCacheSpanComp> tail_set(ceil_span_it, cached_spans_.end());
        for (auto next : tail_set) {
            if (next->position > current_end_pos) {
                break;
            }
            current_end_pos = std::max(current_end_pos, next->position + next->length);
            if (current_end_pos >= query_end_pos) {
                break;
            }
        }
    }
    return std::min(current_end_pos - position, length);
}

std::shared_ptr<SimpleCacheSpan> CachedContent::Touch(std::shared_ptr<SimpleCacheSpan> span, AcResultType& error) {
    assert(cached_spans_.find(span) != cached_spans_.end());

    auto new_span = std::shared_ptr<SimpleCacheSpan>(span->CopyWithUpdatedLastAccessTime(id_));
    if (new_span->file->file_name() == span->file->file_name()) {
        error = kResultOK;
        return span;
    }

    if (!span->file->RenameTo(*new_span->file)) {
        error = kResultCacheExceptionTouchSpan;
        return nullptr;
    }
    RemoveSpan(span);
    AddSpan(new_span);
    error = kResultOK;

    return new_span;
}

bool CachedContent::IsEmptyOrInValid() {
    return cached_spans_.empty() || length_ < 0 || length_ < cached_bytes_;
}

int32_t CachedContent::HeaderHashCode() {
    int32_t result = id_;
    int32_t key_hash = 0;
    for (int i = 0; i < key_.size(); ++i) {
        key_hash += key_[i] * (i + 1);
    }
    result = 31 * result + key_hash;
    result = 31 * result + length_;
    return result;
}

bool CachedContent::RemoveSpan(std::shared_ptr<CacheSpan> span) {
    auto simple_cache_span = std::static_pointer_cast<SimpleCacheSpan>(span);
    if (cached_spans_.find(simple_cache_span) != cached_spans_.end()) {
        simple_cache_span->file->Remove();
        cached_spans_.erase(simple_cache_span);
        cached_bytes_ -= simple_cache_span->length;
        return true;
    }
    return false;
}

std::shared_ptr<SimpleCacheSpan> CachedContent::GetFloorSpan(std::shared_ptr<SimpleCacheSpan> lookup_span) {
    auto upper_span = cached_spans_.upper_bound(lookup_span);
    if (upper_span == cached_spans_.begin()) {
        return nullptr;
    }
    upper_span--;
    return *upper_span;
}

std::shared_ptr<SimpleCacheSpan> CachedContent::GetCeilingSpan(std::shared_ptr<SimpleCacheSpan> lookup_span) {
    auto upper_span = cached_spans_.upper_bound(lookup_span);
    if (upper_span == cached_spans_.end()) {
        return nullptr;
    }
    return *upper_span;
}


bool CachedContent::IsFullyCached() {
    if (cached_bytes_ > length_) {
        LOG_ERROR_DETAIL("WARNING: CachedContent::IsFullyCached cached_bytes(%lld) > length_(%lld)", cached_bytes_, length_);
    }
    return cached_bytes_ >= length_;
}
}
}

