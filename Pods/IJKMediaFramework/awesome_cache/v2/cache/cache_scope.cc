//
// Created by MarshallShuai on 2019-07-01.
//

#include <ac_log.h>
#include "cache_defs.h"
#include "cache_def_v2.h"
#include "cache_scope.h"
#include "cache_v2_settings.h"
#include <assert.h>
#include <file.h>
#include <utility.h>
#include "cache_content_v2_with_scope.h"
#include "v2/cache/cache_v2_file_manager.h"

namespace kuaishou {
namespace cache {

CacheScope::CacheScope(int64_t start_position, std::shared_ptr<CacheContentV2WithScope> belonging_cache_content)
    : start_position_(start_position), is_last_scope_(false) {
    belonging_cache_content_ = std::make_shared<CacheContentV2WithScope>(*belonging_cache_content);
    // try guess end position
    if (belonging_cache_content_->GetContentLength() > 0) {
        GenerateEndPosition();
        UpdateFileName();
    } else {
        end_position_ = kLengthUnset;
    }
}

std::shared_ptr<CacheContentV2WithScope>  const& CacheScope::GetBelongingCacheContent() const {
    return belonging_cache_content_;
}

void CacheScope::UpdateFileName() {
    auto content_length = belonging_cache_content_->GetContentLength();
    auto key = belonging_cache_content_->GetKey();
    auto scope_max_size = belonging_cache_content_->GetScopeMaxSize();
    if (content_length <= 0) {
        LOG_ERROR("[CacheScope::UpdateFileName] content_length:%lld < 0, invalid! key:%s", content_length, key.c_str());
        file_name_ = "";
        return;
    }

    if (scope_max_size <= 0) {
        LOG_ERROR("[CacheScope::UpdateFileName] scope_max_size_:%lld < 0, invalid! key_:%s", scope_max_size, key.c_str());
        file_name_ = "";
        return;
    }

    file_name_ = CacheContentV2WithScope::MakeCacheFileName(key, content_length, start_position_,
                                                            belonging_cache_content_->GetEvictStrategy(), scope_max_size);
    file_path_ = belonging_cache_content_->GetCacheDirPath() + "/" + file_name_;
}

void CacheScope::GenerateEndPosition() {
    int64_t content_length = belonging_cache_content_->GetContentLength();
    int64_t scope_max_size = belonging_cache_content_->GetScopeMaxSize();
    assert(content_length > 0);
    if (content_length - start_position_ > scope_max_size) {
        end_position_ = start_position_ + scope_max_size - 1;
        is_last_scope_ = false;
    } else {
        end_position_ = content_length - 1;
        is_last_scope_ = true;
    }
}


int64_t CacheScope::GetStartPosition() {
    return start_position_;
}

int64_t CacheScope::GetContentLength() {
    return belonging_cache_content_->GetContentLength();
}

int64_t CacheScope::GetEndPosition() {
    return end_position_;
}

int64_t CacheScope::UpdateContentLength(int64_t content_length) {
    auto origin = belonging_cache_content_->GetContentLength();
    if (origin == content_length) {
        return origin;
    }

    if (content_length <= 0) {
        LOG_ERROR("[CacheScope::UpdateContentLengthAndFileName] inner error ，input content_length(%lld) invalid", content_length);
        return origin;
    }
    belonging_cache_content_->SetContentLength(content_length);
    GenerateEndPosition();
    UpdateFileName();

    return origin;
}

int64_t CacheScope::GetScopeMaxSize() {
    return belonging_cache_content_->GetScopeMaxSize();
}

int64_t CacheScope::GetActualLength() {
    return end_position_ != kLengthUnset ? end_position_ - start_position_ + 1 : kLengthUnset;
}


std::shared_ptr<CacheScope> CacheScope::NewScopeFromFileName(const std::string& file_name, const std::string& dir_path) {
    if (file_name.empty() || dir_path.empty()) {
        LOG_DEBUG("[CacheScope::NewScopeFromFileName]file_name:%s or dir_path:%s is empty",
                  file_name.c_str(), dir_path.c_str());
        return nullptr;
    }

    char key_buf[CACHE_V2_CACHE_FILE_NAME_MAX_LEN];
    int64_t content_length;
    int64_t start_pos;
    int64_t scope_max_size;
    int evict_strategy;

    if (!kpbase::StringUtil::EndWith(file_name, kCacheScopeFileNameFormatSuffix)) {
        LOG_ERROR_DETAIL("[CacheScope::NewScopeFromFileName] file(%s name not end with %s, return null",
                         file_name.c_str(), kCacheScopeFileNameFormatSuffix);
        return nullptr;
    }

    auto file_name_without_suffix = file_name.substr(0, file_name.length() - strlen(kCacheScopeFileNameFormatSuffix) - 1);

    std::sscanf(file_name_without_suffix.c_str(), kCacheScopeFileNameFormatWithoutSuffix,
                &content_length, &start_pos, &scope_max_size, &evict_strategy, key_buf);
    std::string key = std::string(key_buf);

    if (key.empty() || start_pos < 0 || content_length <= 0 || content_length <= start_pos || scope_max_size <= 0
        || (evict_strategy != EvictStrategy_LRU && evict_strategy != EvictStrategy_NEVER)) {
        LOG_ERROR_DETAIL("[CacheScope::NewScopeFromFileName] file(file_name:%s, file_name_without_suffix:%s) parse fail, "
                         "key:%s, start_pos:%lld, content_length:%lld, scope_max_size:%lld, evict_strategy:%d, return null",
                         file_name.c_str(), file_name_without_suffix.c_str(),
                         key.c_str(), start_pos, content_length, scope_max_size, evict_strategy);
        return nullptr;
    }

    auto cache_content = std::make_shared<CacheContentV2WithScope>(key, scope_max_size, dir_path, content_length);
    cache_content->SetEvictStrategy(static_cast<EvictStrategy>(evict_strategy));
    auto scope = std::make_shared<CacheScope>(start_pos, cache_content);

    return scope;
}

std::string CacheScope::GetCacheFileName() const {
    return file_name_;
}

std::string CacheScope::GetCacheFilePath() const {
    return file_path_;
}

std::string CacheScope::GetKey() const {
    return belonging_cache_content_->GetKey();
}

int64_t CacheScope::DeleteCacheFile(bool should_notify_file_manager) const {
    kpbase::File file(file_path_);
    int64_t exist_file_size = file.file_size();
    file.Remove();
    if (should_notify_file_manager && exist_file_size > 0) {
        CacheV2FileManager::GetMediaDirManager()->OnCacheScopeFileDeleted(*this);
    }
    return exist_file_size > 0 ? exist_file_size : 0;
}

int64_t CacheScope::GetCachedFileBytes() const {
    return CacheV2FileManager::GetMediaDirManager()->GetCacheScopeCachedBytes(*this);
}

bool CacheScope::IsScopeFulllyCached() const {
    if (end_position_ == kLengthUnset) {
        return false;
    }

    int64_t cached_bytes = GetCachedFileBytes();
    return cached_bytes == end_position_ - start_position_ + 1;
}

kpbase::File CacheScope::GetCacheFile() const {
    kpbase::File file(file_path_);
    return file;
}

bool CacheScope::IsLastScope() const {
    return is_last_scope_;
}


} // namespace cache
} // namespace kuaishou
