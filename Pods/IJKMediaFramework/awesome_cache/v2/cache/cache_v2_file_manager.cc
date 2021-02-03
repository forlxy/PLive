//
// Created by MarshallShuai on 2019-07-11.
//

#include <ac_log.h>
#include "cache_v2_file_manager.h"
#include "utils/macro_util.h"
#include "v2/cache/cache_def_v2.h"
#include "v2/cache/cache_content_index_v2.h"
#include "v2/cache/cache_scope.h"


HODOR_NAMESPACE_START

using namespace kuaishou::kpbase;

DirManagerMedia* CacheV2FileManager::media_dir_manager_;
DirManagerResource* CacheV2FileManager::resource_dir_manager_;
std::mutex CacheV2FileManager::instance_mutex_;

void CacheV2FileManager::DeleteCacheV2RootDir() {
    File file(CacheV2Settings::GetCacheRootDirPath());
    File::RemoveAll(file);
}

DirManagerMedia* CacheV2FileManager::GetMediaDirManager() {
    if (media_dir_manager_ == nullptr) {
        std::lock_guard<std::mutex> lock(instance_mutex_);
        if (media_dir_manager_ == nullptr) {
            media_dir_manager_ = new DirManagerMedia();
        }
    }
    return media_dir_manager_;
}

DirManagerResource* CacheV2FileManager::GetResourceDirManager() {
    if (resource_dir_manager_ == nullptr) {
        std::lock_guard<std::mutex> lock(instance_mutex_);
        if (resource_dir_manager_ == nullptr) {
            resource_dir_manager_ = new DirManagerResource();
        }
    }
    return resource_dir_manager_;
}


HODOR_NAMESPACE_END
