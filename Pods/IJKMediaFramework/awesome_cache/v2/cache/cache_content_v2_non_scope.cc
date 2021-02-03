//
// Created by MarshallShuai on 2019-10-16.
//

#include <utility.h>
#include "cache_content_v2_non_scope.h"
#include "ac_log.h"
#include "v2/cache/cache_v2_file_manager.h"
#include "v2/cache/cache_def_v2.h"

HODOR_NAMESPACE_START

CacheContentV2NonScope::CacheContentV2NonScope(std::string key, std::string cache_dir_path,
                                               int64_t content_length) :
    CacheContentV2(key, cache_dir_path, content_length) {
    if (key_.empty()) {
        LOG_DEBUG("[CacheContentV2::CacheContentV2] strong warning key is empty");
    } else {
        if (content_length > 0) {
            UpdateFileName();
        }
    }
}

int64_t CacheContentV2NonScope::GetCachedBytes(bool force_check_file) const {
    int64_t ret = 0;
    if (force_check_file) {
        kpbase::File file(cache_dir_path_, file_name_);
        ret = file.file_size();
        ret = ret <= 0 ? 0 : ret;
        return ret;
    } else {
        return CacheV2FileManager::GetResourceDirManager()->GetCacheContentCachedBytes(key_);
    }
}

std::shared_ptr<CacheContentV2NonScope>
CacheContentV2NonScope::NewCacheContentFromFileName(const std::string& file_name, const std::string& cache_dir) {
    char key_buf[CACHE_V2_CACHE_FILE_NAME_MAX_LEN];
    int64_t content_length;

    if (!kpbase::StringUtil::EndWith(file_name, kCacheContentFileNameFormatSuffix)) {
        LOG_ERROR_DETAIL("[CacheContentV2NonScope::NewCacheContentFromFileName] file(%s name not end with %s, return null",
                         file_name.c_str(), kCacheContentFileNameFormatSuffix);
        return nullptr;
    }

    auto file_name_without_suffix = file_name.substr(0, file_name.length() - strlen(kCacheContentFileNameFormatSuffix) - 1);

    std::sscanf(file_name_without_suffix.c_str(), kCacheContentFileNameFormatWithoutSuffix, &content_length, key_buf);
    std::string key = std::string(key_buf);

    if (key.empty() ||  content_length <= 0) {
        LOG_ERROR_DETAIL("[CacheContentV2NonScope::NewCacheContentFromFileName] file(file_name:%s, file_name_without_suffix:%s) parse fail, "
                         "key:%s, content_length:%lld, return null",
                         file_name.c_str(), file_name_without_suffix.c_str(), key.c_str(), content_length);
        return nullptr;
    }

    return std::make_shared<CacheContentV2NonScope>(key, cache_dir, content_length);

}

std::string CacheContentV2NonScope::GetCacheContentFileName() const {
    return file_name_;
}

int64_t CacheContentV2NonScope::DeleteCacheContentFile(bool should_notify_file_manager) const {
    kpbase::File file(cache_dir_path_, file_name_);
    int64_t exist_file_size = file.file_size();
    file.Remove();
    if (should_notify_file_manager && exist_file_size > 0) {
        // 这里如果后续有多个GetResourceDirManager的话，即需要做成listener的形式
        CacheV2FileManager::GetResourceDirManager()->OnCacheContentFileDeleted(*this);
    }
    return exist_file_size > 0 ? exist_file_size : 0;
}

std::string CacheContentV2NonScope::GetCacheContentValidFilePath() const {
    if (file_name_.empty()) {
        return "";
    }
    return cache_dir_path_ + "/" + file_name_;
}

kpbase::File CacheContentV2NonScope::GetCacheContentCacheFile() const {
    return kpbase::File(cache_dir_path_, file_name_);
}

void CacheContentV2NonScope::UpdateContentLengthAndFileName(int64_t content_length) {
    if (content_length <= 0) {
        LOG_ERROR("[CacheContentV2NonScope::UpdateContentLengthAndFileName] invalid input content_length:%lld", content_length);
        return;
    }
    CacheContentV2::SetContentLength(content_length);
    UpdateFileName();
}

void CacheContentV2NonScope::UpdateFileName() {
    char name[CACHE_V2_CACHE_FILE_NAME_MAX_LEN];
    snprintf(name, CACHE_V2_CACHE_FILE_NAME_MAX_LEN, kCacheContentFileNameFormat,
             content_length_, key_.c_str());
    file_name_ = name;
}

HODOR_NAMESPACE_END
