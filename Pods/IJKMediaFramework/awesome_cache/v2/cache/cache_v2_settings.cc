//
// Created by MarshallShuai on 2019-07-01.
//

#include "cache_v2_settings.h"
#include "cache_def_v2.h"
#include "ac_log.h"
#include <file.h>

namespace kuaishou {
namespace cache {
int64_t CacheV2Settings::s_scope_size_ = kDefaultScopeBytes;
int64_t CacheV2Settings::s_cache_byts_limit_ = kDefaultCacheBytesLimit;
std::string CacheV2Settings::s_root_cache_dir_;
std::string CacheV2Settings::s_media_cache_dir_;
std::string CacheV2Settings::s_resource_cache_dir_;
int CacheV2Settings::s_tcp_max_connects_ = 0;

int64_t CacheV2Settings::GetScopeMaxSize() {
    return s_scope_size_;
}

void CacheV2Settings::SetScopeMaxSize(int64_t scope_size) {
    if (scope_size > kMaxScopeBytes || scope_size < kMinScopeBytes) {
        LOG_WARN("[CacheV2Settings::SetScopeMaxSize]invalid scope_size:%lld", scope_size);
        return;
    }
    LOG_INFO("[CacheV2Settings::SetScopeMaxSize] scope_size:%dKB", scope_size / KB);
    s_scope_size_ = scope_size;
}

void CacheV2Settings::SetCacheRootDirPath(const std::string& path) {
    s_root_cache_dir_ = path + "/cache_v2";
    s_media_cache_dir_ = s_root_cache_dir_ + "/media";
    s_resource_cache_dir_ = s_root_cache_dir_ + "/resource";
    MakeSureMediaCacheDirExists();
    MakeSureResourceCacheDirExists();
    LOG_INFO("[CacheV2Settings::SetCacheRootDirPath] finish, s_root_cache_dir_:%s", s_root_cache_dir_.c_str());
}

bool CacheV2Settings::MakeSureResourceCacheDirExists() {
    return MakeSureCacheDirExists(s_resource_cache_dir_);
}

bool CacheV2Settings::MakeSureMediaCacheDirExists() {
    return MakeSureCacheDirExists(s_media_cache_dir_);
}

bool CacheV2Settings::MakeSureCacheDirExists(const std::string& path) {
    if (path.empty()) {
        LOG_ERROR("[CacheV2Settings::MakeSureCacheDirExists] invalid path, path is empty");
        return false;
    }
    kpbase::File dir(path);
    if (!dir.Exists() || !dir.IsDirectory()) {
        LOG_INFO("[CacheV2Settings::MakeSureCacheDirExists] init makeDir for :%s", dir.path().c_str());
        bool ret = kpbase::File::MakeDirectories(dir);
        if (!ret) {
            // todo 可能要考虑全局disable cache了
            LOG_ERROR("[CacheV2Settings::MakeSureCacheDirExists] init makeDir fail");
        }
        return ret;
    } else {
        return true;
    }
}

std::string CacheV2Settings::GetCacheRootDirPath() {
    return s_root_cache_dir_;
}

std::string CacheV2Settings::GetMediaCacheDirPath() {
    return s_media_cache_dir_;
}

std::string CacheV2Settings::GetResourceCacheDirPath() {
    return s_resource_cache_dir_;
}

int64_t CacheV2Settings::GetCacheBytesLimit() {
    return s_cache_byts_limit_;
}

void CacheV2Settings::SetCacheBytesLimit(int64_t bytes_limit) {
    if (bytes_limit < kMinCacheBytesLimit || bytes_limit > kMaxCacheBytesLimit) {
        return;
    }
    s_cache_byts_limit_ = bytes_limit;
}


int CacheV2Settings::GetTcpMaxConnects() {
    return s_tcp_max_connects_;
}


int CacheV2Settings::SetTcpMaxConnects(int max) {
    if (max >= 0) {
        s_tcp_max_connects_ = max;
    }
    return s_tcp_max_connects_;
}



} // namespace cache
} // namespace kuaishou2