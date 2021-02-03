//
// Created by MarshallShuai on 2019-10-14.
//

#include "dir_manager_media.h"
#include <ac_log.h>
#include "cache_v2_file_manager.h"
#include "v2/cache/cache_def_v2.h"
#include "v2/cache/cache_content_index_v2.h"

HODOR_NAMESPACE_START

using namespace kuaishou::kpbase;

static const bool kVerboseKeep = false;
static const bool kVerboseDelete = true;


DirManagerMedia::DirManagerMedia() {
    index_ = new CacheContentIndexV2WithScope(CacheV2Settings::GetMediaCacheDirPath());
}

CacheContentIndexV2WithScope* DirManagerMedia::Index() {
    return index_;
}

void DirManagerMedia::PruneWithCacheTotalBytesLimit(int64_t bytes_limit) {
    std::lock_guard<std::recursive_mutex> lock(cache_scopes_map_mutex_);
    auto cache_content_list = index_->GetCacheContentListOfEvictStrategy(EvictStrategy_LRU);

    LOG_DEBUG("[DirManagerMedia::PruneWithCacheTotalBytesLimit] bytes_limit:%lld, cache_content_list.size:%d", bytes_limit, cache_content_list.size());

    std::sort(cache_content_list.begin(), cache_content_list.end(), &DirManagerMedia::CacheContentSortAsc);

    if (total_cached_bytes_ > bytes_limit) {
        for (const auto& cache_content : cache_content_list) {

            auto scope_list = cache_content->GetScopeList();
            for (auto& scope : scope_list) {
                if (scope->GetCachedFileBytes() > 0) {
                    scope->DeleteCacheFile(false);
                    if (kVerboseDelete) {
                        LOG_DEBUG("[DirManagerMedia::PruneWithCacheTotalBytesLimit]total_cached_bytes_(%.1f) > bytes_limit(%.1f), "
                                  "to delete cache_scope(cached:%.1fMB, key:%s, "
                                  "file_name:%s, ts:%lld)",
                                  total_cached_bytes_ * 1.f / MB,
                                  bytes_limit * 1.f / MB, scope->GetCachedFileBytes() * 1.f /
                                  MB, scope->GetKey().c_str(), scope->GetCacheFileName().c_str(), cache_content->GetLastAccessTimestamp());
                    }

                    // check again after delete cache_scope
                    OnCacheScopeFileDeleted(*scope);
                    if (scope->GetBelongingCacheContent()->GetCachedBytes() == 0) {
                        index_->RemoveCacheContent(scope->GetBelongingCacheContent(), false);
                        if (kVerboseDelete) {
                            LOG_DEBUG("[DirManagerMedia::PruneWithCacheTotalBytesLimit]cache_content has no cache file left, to remove from index, key:%s, content_length:%lld",
                                      scope->GetKey().c_str(), scope->GetContentLength())
                        }
                    }
                }
                if (total_cached_bytes_ <= bytes_limit) {
                    break;
                }
            }
            // check again after delete cache_content
            if (total_cached_bytes_ <= bytes_limit) {
                break;
            }
        }
    }
}

int64_t DirManagerMedia::GetTotalCachedBytes() {
    return total_cached_bytes_;
}

int64_t DirManagerMedia::GetCacheBytesLimit() {
    return CacheV2Settings::GetCacheBytesLimit();
}

DirManagerMedia::RestoreResult DirManagerMedia::RestoreFileInfo() {
    // 确保load过一次
    if (!index_->IsLoaded()) {
        index_->Load();
    }
    std::lock_guard<std::recursive_mutex> lg(cache_scopes_map_mutex_);
    std::string cache_dir = CacheV2Settings::GetMediaCacheDirPath();

    cache_scope_cached_bytes_map_.clear();
    cache_content_cached_bytes_map_.clear();
    total_cached_bytes_ = 0;

    RestoreResult ret{};

    std::vector<File> files = File::ListRegularFiles(cache_dir);
    for (auto& file : files) {
        if (file.file_name() == kCacheV2IndexFileName
            || file.file_name() == kCacheV2IndexBackupFileName) {
            continue;
        }

        if (file.file_size() <= 0) {
            LOG_WARN("[DirManagerMedia::RestoreFileInfo] file:%s file_size <= 0, remove it", file.base_name().c_str());
            ret.fail_scope_file_name_invalid_cnt++;
            file.Remove();
            continue;
        }

        auto scope = CacheScope::NewScopeFromFileName(file.file_name(), cache_dir);
        if (scope != nullptr) {
            _AddCachedScope(*scope, ret);
        } else {
            LOG_WARN("[DirManagerMedia::RestoreFileInfo] NewScopeFromFileName for file:%s fail, remove it",
                     file.file_name().c_str());
            file.Remove();
            ret.fail_scope_file_name_invalid_cnt++;
        }
    }

    // 文件分片里不存在的cacheContent在这趁机清理掉
    index_->RemoveEmptyCacheContent([this](const std::string & key) {
        return cache_content_cached_bytes_map_.find(key) == cache_content_cached_bytes_map_.end();
    });


    return ret;
}

int64_t DirManagerMedia::GetCacheContentCachedBytes(const std::string& key) {
    std::lock_guard<std::recursive_mutex> lg(cache_scopes_map_mutex_);
    if (key.empty()) {
        LOG_ERROR("[DirManagerMedia::GetCacheContentCachedBytes] input key is empty")
        return 0;
    }
    return cache_content_cached_bytes_map_[key];
}


int64_t DirManagerMedia::GetCacheScopeCachedBytes(const CacheScope& scope) {
    std::lock_guard<std::recursive_mutex> lg(cache_scopes_map_mutex_);
    std::string file_name = scope.GetCacheFileName();
    if (file_name.empty()) {
        LOG_ERROR("[DirManagerMedia::GetCacheScopeCachedBytes] scope key is empty")
        return 0;
    }
    return cache_scope_cached_bytes_map_[file_name];
}

bool DirManagerMedia::_AddCachedScope(const CacheScope& scope, RestoreResult& ret) {
    std::string scope_file_name = scope.GetCacheFileName();
    File file = scope.GetCacheFile();
    int64_t scope_cached_bytes = file.file_size();
    if (scope_cached_bytes <= 0) {
        LOG_WARN("[DirManagerMedia::_AddCachedScope]scope (%s) file size(%lld) <= 0, not to add to map & delete file",
                 scope_file_name.c_str(), scope_cached_bytes);
        return false;
    }
    auto iter = cache_scope_cached_bytes_map_.find(scope_file_name);
    if (iter == cache_scope_cached_bytes_map_.end()) {
        auto content = CacheV2FileManager::GetMediaDirManager()->Index()->FindCacheContent(scope.GetKey());
        if (content == nullptr) {
            LOG_WARN("[DirManagerMedia::_AddCachedScope]scope(filename:%s) does not found belonging content(key:%s), not to add to map & delete file",
                     scope_file_name.c_str(), scope.GetKey().c_str());
            ret.fail_scope_file_no_content_record_cnt++;
            scope.DeleteCacheFile(false);
            return false;
        } else {
            // onScopeAdded
            cache_scope_cached_bytes_map_[scope_file_name] = scope_cached_bytes;
            total_cached_bytes_ += scope_cached_bytes;
            // 之前没有的scope，直接加到cache_content的cached_bytes里即可
            cache_content_cached_bytes_map_[scope.GetKey()] += scope_cached_bytes;
        }
    } else {
        // onScopeUpdated, update map and total_cached_bytes_
        int64_t origin_scope_cached_bytes = cache_scope_cached_bytes_map_[scope_file_name];
        cache_scope_cached_bytes_map_[scope_file_name] = scope_cached_bytes;

        total_cached_bytes_ -= origin_scope_cached_bytes;
        total_cached_bytes_ += scope_cached_bytes;

        cache_content_cached_bytes_map_[scope.GetKey()] -= origin_scope_cached_bytes;
        cache_content_cached_bytes_map_[scope.GetKey()] += scope_cached_bytes;
    }

    ret.success_scope_file_cnt_++;
    return true;
}

AcResultType DirManagerMedia::CommitScopeFile(const CacheScope& scope) {
    // 更新CacheIndex
    CacheV2FileManager::GetMediaDirManager()->Index()->Store();

    bool b_ret = CacheV2FileManager::GetMediaDirManager()->OnCacheScopeFileFlushed(scope);
    if (b_ret) {
        CacheV2FileManager::GetMediaDirManager()->PruneWithCacheTotalBytesLimit();
        return kResultOK;
    } else {
        return kCacheDirManagerMediaCommitScopeFileFail;
    }
}

bool DirManagerMedia::OnCacheScopeFileFlushed(const CacheScope& scope) {
    std::lock_guard<std::recursive_mutex> lg(cache_scopes_map_mutex_);
    RestoreResult ignore;
    return _AddCachedScope(scope, ignore);
}

void DirManagerMedia::OnCacheScopeFileDeleted(const CacheScope& scope) {
    std::lock_guard<std::recursive_mutex> lg(cache_scopes_map_mutex_);
    std::string scope_file_name = scope.GetCacheFileName();
    auto iter = cache_scope_cached_bytes_map_.find(scope_file_name);
    if (iter == cache_scope_cached_bytes_map_.end()) {
        // do nothing
        LOG_WARN("[DirManagerMedia::OnCacheScopeFileDeleted] inner error, scope(%s) not found in cache_scope_cached_bytes_map_ record",
                 scope_file_name.c_str());
    } else {
        // onScopeUpdated, update map and total_cached_bytes_
        int64_t origin_scope_cached_bytes = cache_scope_cached_bytes_map_[scope_file_name];
        cache_scope_cached_bytes_map_[scope_file_name] = 0;

        total_cached_bytes_ -= origin_scope_cached_bytes;

        cache_content_cached_bytes_map_[scope.GetKey()] -= origin_scope_cached_bytes;
    }
}


HODOR_NAMESPACE_END
