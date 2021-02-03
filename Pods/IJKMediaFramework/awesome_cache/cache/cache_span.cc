//
// Created by 帅龙成 on 27/10/2017.
//

#include "constant.h"
#include "cache_span.h"
using namespace kuaishou::kpbase;

namespace kuaishou {
namespace cache {

CacheSpan::CacheSpan(const std::string& key, int64_t position, int64_t length,
                     int64_t last_access_timestamp,
                     std::shared_ptr<File> file) :
    key(key),
    position(position),
    length(length),
    file(file),
    last_access_timestamp(last_access_timestamp),
    is_cached(file != nullptr) {
}

bool CacheSpan::IsOpenEnded() {
    return length == kLengthUnset;
}

bool CacheSpan::IsHoleSpan() {
    return !is_cached;
}

bool operator<(const CacheSpan& lhs, const CacheSpan& rhs) {
    if (lhs.key != rhs.key) {
        return lhs.key < rhs.key;
    }
    return lhs.position < rhs.position;
}

bool operator==(const CacheSpan& lhs, const CacheSpan& rhs) {
    return lhs.key == rhs.key && lhs.position == rhs.position;
}


}
}
