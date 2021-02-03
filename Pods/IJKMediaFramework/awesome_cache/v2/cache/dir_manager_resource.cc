//
// Created by MarshallShuai on 2019-10-14.
//

#include <ac_log.h>
#include <file.h>
#include "dir_manager_resource.h"
#include "v2/cache/cache_v2_settings.h"
#include "v2/cache/cache_content_index_v2.h"
#include "cache_v2_file_manager.h"

HODOR_NAMESPACE_START
using namespace kuaishou::kpbase;

DirManagerResource::DirManagerResource() {
    index_ = new CacheContentIndexV2NonScope(CacheV2Settings::GetResourceCacheDirPath());
}

CacheContentIndexV2NonScope* DirManagerResource::Index() {
    return index_;
}

int64_t DirManagerResource::GetTotalCachedBytes() {
    return total_cached_bytes_;
}

int64_t DirManagerResource::GetCacheContentCachedBytes(const std::string& key) {
    std::lock_guard<std::recursive_mutex> lg(cache_content_map_mutex_);
    if (key.empty()) {
        LOG_ERROR("[DirManagerResource::GetCacheContentCachedBytes] input key is empty");
        return 0;
    }
    return cache_content_cached_bytes_map_[key];
}

DirManagerResource::RestoreResult
DirManagerResource::RestoreFileInfo() {
    // 确保load过一次
    if (index_->IsLoaded()) {
        index_->Load();
    }

    std::lock_guard<std::recursive_mutex> lg(cache_content_map_mutex_);
    std::string cache_dir = CacheV2Settings::GetResourceCacheDirPath();

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
            LOG_WARN("[DirManagerResource::RestoreFileInfo] file:%s file_size <= 0, remove it", file.base_name().c_str());
            ret.fail_content_file_name_invalid_cnt++;
            file.Remove();
            continue;
        }

        auto content = CacheContentV2NonScope::NewCacheContentFromFileName(file.file_name(), cache_dir);
        if (content != nullptr) {
            _AddCachedContent(*content, ret);
        } else {
            LOG_WARN("[DirManagerResource::RestoreFileInfo] NewScopeFromFileName for file:%s fail, remove it",
                     file.file_name().c_str());
            file.Remove();
            ret.fail_content_file_name_invalid_cnt++;
        }
    }

    // 本得文件夹不存在的cacheContent在这趁机清理掉
    index_->RemoveEmptyCacheContent([this](const std::string & key) {
        return cache_content_cached_bytes_map_.find(key) == cache_content_cached_bytes_map_.end();
    });

    return ret;
}

bool DirManagerResource::OnCacheContentFileFlushed(
    const CacheContentV2NonScope& content) {
    std::lock_guard<std::recursive_mutex> lg(cache_content_map_mutex_);
    RestoreResult ignore;
    return _AddCachedContent(content, ignore);
}

void DirManagerResource::OnCacheContentFileDeleted(
    const CacheContentV2NonScope& content) {
    std::lock_guard<std::recursive_mutex> lg(cache_content_map_mutex_);
    std::string content_file_name = content.GetCacheContentFileName();
    auto iter = cache_content_cached_bytes_map_.find(content_file_name);
    if (iter == cache_content_cached_bytes_map_.end()) {
        // do nothing
        LOG_WARN("[DirManagerResource::OnCacheContentFileDeleted] inner error, content(%s) not found in cache_content_cached_bytes_map_ record",
                 content_file_name.c_str());
    } else {
        // onCacheContentUpdated, update map and total_cached_bytes_
        int64_t origin_scope_cached_bytes = cache_content_cached_bytes_map_[content_file_name];
        cache_content_cached_bytes_map_[content_file_name] = 0;

        total_cached_bytes_ -= origin_scope_cached_bytes;
        cache_content_cached_bytes_map_[content.GetKey()] -= origin_scope_cached_bytes;
    }
}

bool DirManagerResource::_AddCachedContent(
    const CacheContentV2NonScope& content,
    DirManagerResource::RestoreResult& ret) {

    auto content_file_name = content.GetCacheContentFileName();
    File file = content.GetCacheContentCacheFile();
    int64_t content_cached_bytes = file.file_size();
    if (content_cached_bytes <= 0) {
        LOG_WARN("[DirManagerResource::_AddCachedContent]content (%s) file size(%lld) <= 0, not to add to map & delete file",
                 file.path().c_str(), content_cached_bytes);
        return false;
    }
    auto iter = cache_content_cached_bytes_map_.find(content_file_name);
    if (iter == cache_content_cached_bytes_map_.end()) {
        // 在Index里找不到记录的，就不加了，直接删除掉
        auto find_content = CacheV2FileManager::GetResourceDirManager()->Index()->FindCacheContent(content.GetKey());
        if (find_content == nullptr) {
            LOG_WARN("[DirManagerResource::_AddCachedContent]content (%s) does not found related content, not to add to map & delete file",
                     content_file_name.c_str());
            ret.fail_content_file_no_content_record_cnt++;
            content.DeleteCacheContentFile(false);
            return false;
        } else {
            // onContentAdded
            cache_content_cached_bytes_map_[content_file_name] = content_cached_bytes;
            total_cached_bytes_ += content_cached_bytes;
            // 之前没有的scope，直接加到cache_content的cached_bytes里即可
            cache_content_cached_bytes_map_[content.GetKey()] += content_cached_bytes;
        }
    } else {
        // onCacheContentUpdated, update map and total_cached_bytes_
        int64_t origin_scope_cached_bytes = cache_content_cached_bytes_map_[content_file_name];
        cache_content_cached_bytes_map_[content_file_name] = content_cached_bytes;

        total_cached_bytes_ -= origin_scope_cached_bytes;
        total_cached_bytes_ += content_cached_bytes;

        cache_content_cached_bytes_map_[content.GetKey()] -= origin_scope_cached_bytes;
        cache_content_cached_bytes_map_[content.GetKey()] += content_cached_bytes;
    }

    ret.success_content_file_cnt_++;
    return true;
}

HODOR_NAMESPACE_END
