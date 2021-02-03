//
// Created by MarshallShuai on 2019-07-01.
//

#include <ac_log.h>
#include <utility>
#include <file.h>
#include <utility.h>
#include "cache_content_v2.h"
#include "cache_v2_settings.h"
#include "v2/cache/cache_def_v2.h"
#include "v2/cache/cache_v2_file_manager.h"

namespace kuaishou {
namespace cache {
static const bool kVerbose = false;


std::string CacheContentV2::GenerateKey(std::string url) {
    // 暂时实现, fixme
    std::replace_if(url.begin(), url.end(), ::ispunct, '_');
    if (url.size() >= 64) {
        return url.substr(url.size() - 64, 64);
    } else {
        return url;
    }
}

CacheContentV2::CacheContentV2(std::string key, std::string dir_path, int64_t content_length):
    key_(std::move(key)),
    last_access_timestamp_(0),
    cache_dir_path_(dir_path) {

    assert(!dir_path.empty());

    if (content_length > 0) {
        SetContentLength(content_length);
    }

    if (kVerbose) {
        LOG_DEBUG("[CacheContentV2::CacheContentV2] key:%s", key_.c_str());
    }
    UpdateLastAccessTimestamp();
}

CacheContentV2::CacheContentV2(const CacheContentV2& other):
    CacheContentV2(other.key_, other.cache_dir_path_, other.content_length_) {
    last_access_timestamp_ = other.last_access_timestamp_;
}

std::string CacheContentV2::GetKey() const {
    return key_;
}

int64_t CacheContentV2::GetContentLength() const {
    return content_length_;
}


std::string CacheContentV2::GetCacheDirPath() const {
    return cache_dir_path_;
}

void CacheContentV2::SetContentLength(int64_t len) {
    assert(len > 0);
    if (len == content_length_) {
        return;
    }
    if (content_length_ > 0 && len != content_length_) {
        LOG_WARN("[CacheContentV2::SetContentLength] warning, new len(%lld) != current content_length_(%lld)",
                 len, content_length_);
    }
    content_length_ = len;
}

int CacheContentV2::SimpleHashCode() {
    return static_cast<int>(key_.size());
}

bool CacheContentV2::IsFullyCached(bool force_check_file) const {
    if (content_length_ <= 0) {
        return false;
    }

    auto cached_bytes = GetCachedBytes(force_check_file);
    if (cached_bytes > content_length_) {
        LOG_ERROR("[CacheContentV2::IsFullyCached]internal bug, key:%s, cached_bytes(%lld) > content_length_(%lld)",
                  key_.c_str(), cached_bytes, content_length_);
    }
    return cached_bytes == content_length_;
}

void CacheContentV2::UpdateLastAccessTimestamp() {
    last_access_timestamp_ = kpbase::SystemUtil::GetEpochTime();
}

int64_t CacheContentV2::GetLastAccessTimestamp() const {
    return last_access_timestamp_;
}

void CacheContentV2::SetLastAccessTimestamp(int64_t ts) {
    last_access_timestamp_ = ts;
}



} // namespace cache
} // namespace kuaishou
