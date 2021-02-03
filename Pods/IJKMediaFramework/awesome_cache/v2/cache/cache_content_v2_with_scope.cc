//
// Created by MarshallShuai on 2019-10-16.
//

#include "cache_content_v2_with_scope.h"
#include "cache_v2_file_manager.h"
#include "v2/data_sink/cache_scope_data_sink.h"
#include "cache_util.h"
#include <stdio.h>
extern "C" {
#include <libavutil/md5.h>
}

HODOR_NAMESPACE_START


std::string CacheContentV2WithScope::MakeCacheFileName(const std::string& key, int64_t content_length, int64_t start_pos,
                                                       EvictStrategy strategy, int64_t scope_max_size) {
    char name[CACHE_V2_CACHE_FILE_NAME_MAX_LEN];
    snprintf(name, CACHE_V2_CACHE_FILE_NAME_MAX_LEN, kCacheScopeFileNameFormat,
             content_length, start_pos, scope_max_size, strategy, key.c_str());
    return name;
}

CacheContentV2WithScope::CacheContentV2WithScope(std::string key, int64_t scope_max_size,
                                                 std::string cache_dir_path, int64_t content_length)
    : CacheContentV2(key, cache_dir_path, content_length),
      scope_max_size_(scope_max_size) {
    evict_strategy_ = EvictStrategy_LRU;
}

int64_t CacheContentV2WithScope::GetCachedBytes(bool force_check_file) const {
    return CacheV2FileManager::GetMediaDirManager()->GetCacheContentCachedBytes(key_);
}


int64_t CacheContentV2WithScope::GetScopeMaxSize() const {
    return scope_max_size_;
}

void CacheContentV2WithScope::DeleteScopeFiles() const {
    for (const auto& cache_scope : GetScopeList()) {
        cache_scope->DeleteCacheFile(true);
    }
}

std::vector<std::shared_ptr<CacheScope>> CacheContentV2WithScope::GetScopeList() const {
    std::vector<std::shared_ptr<CacheScope>> ret_vec;

    int64_t start_pos = 0;
    while (start_pos < content_length_) {
        auto cache_scope = ScopeForPosition(start_pos);

        ret_vec.push_back(cache_scope);

        start_pos = cache_scope->GetEndPosition() + 1;
    }
    return ret_vec;
}

std::shared_ptr<CacheScope> CacheContentV2WithScope::ScopeForPosition(int64_t position) const {
    assert(position >= 0);
    int64_t start_pos = position / scope_max_size_ * scope_max_size_;
    return std::make_shared<CacheScope>(start_pos, std::make_shared<CacheContentV2WithScope>(*this));
}

void CacheContentV2WithScope::SetEvictStrategy(EvictStrategy strategy) {
    evict_strategy_ = strategy;
}

EvictStrategy CacheContentV2WithScope::GetEvictStrategy() const {
    return evict_strategy_;
}

AcResultType CacheContentV2WithScope::VerifyMd5(const std::string& md5) {
    if (content_length_ < 0) {
        LOG_ERROR("[CacheContentV2WithScope::VerifyMd5] content_length invalid!");
        return kCacheContentV2WithScopeMD5ContentLengthInvalid;
    }
    AVMD5* ctx = av_md5_alloc();
    if (!ctx) {
        LOG_WARN("[CacheContentV2WithScope::VerifyMd5] av_md5_alloc faile!");
        return kCacheContentV2WithScopeMD5InitFail;
    }
    av_md5_init(ctx);
    std::vector<std::shared_ptr<CacheScope>> scope_list = GetScopeList();
    uint8_t* buf = new uint8_t[scope_max_size_];
    for (auto& scope : scope_list) {
        std::string file_path = scope->GetCacheFilePath();
        int64_t len = CacheUtil::ReadFile(file_path, buf, scope_max_size_);
        if (len <= 0) {
            LOG_WARN("[CacheContentV2WithScope::VerifyMd5] read data fail len:%d! key:%s file_name:%s startPos:%lld endPos:%lld",
                     len, key_.c_str(), file_name_.c_str(), scope->GetStartPosition(), scope->GetEndPosition());
            return (int)len;
        }
        av_md5_update(ctx, buf, (int)len);
    }
    std::string res(128, 0);
    uint8_t rst[16];
    av_md5_final(ctx, rst);
    char tmp[32] = {0};
    for (int i = 0; i < 16; i++) {
        snprintf(tmp + 2 * i, 32, "%02x", rst[i]);
    }
    if (md5 != std::string(tmp)) {
        return kCacheContentV2WithScopeMD5VerfityFail;
    }
    return 0;
}


HODOR_NAMESPACE_END
