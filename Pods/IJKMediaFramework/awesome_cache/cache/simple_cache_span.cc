//
// Created by 帅龙成 on 30/10/2017.
//

#include "simple_cache_span.h"
#include "utility.h"
#include "cached_content_index.h"
#include "ac_log.h"

using namespace kuaishou::kpbase;
namespace kuaishou {
namespace cache {

// acc = awesome_cache_cachefile
const std::string kSuffix = "v1.acc";
const std::string kFileNameFormat = "%d.%lld.%lld." + kSuffix;

SimpleCacheSpan::SimpleCacheSpan(const std::string& key, int64_t position, int64_t length,
                                 int64_t lastAccessTimestamp, std::shared_ptr<File> file)
    : CacheSpan(key, position, length, lastAccessTimestamp, file) {}


SimpleCacheSpan* SimpleCacheSpan::CreateCacheEntry(File& file, CachedContentIndex* index) {
    assert(index);
    std::string filename = file.file_name();
    int64_t file_size = file.file_size();
    if (file_size <= 0) {
        file.Remove();
        LOG_ERROR_DETAIL("SimpleCacheSpan::CreateCacheEntry, file size(%lld) <= 0", file_size);
        return nullptr;
    }
    if (!StringUtil::EndWith(filename, kSuffix)) {
        // 第一版不考虑缓存文件文件名升级的事情，不符合SUFFIX的直接删除
        LOG_ERROR_DETAIL("SimpleCacheSpan::CreateCacheEntry, file name not right:%s", filename.c_str());
        file.Remove();
        return nullptr;
    }

    // to parse key from file name
    int id = -1;
    int64_t position = -1;
    int64_t timestamp = -1;
    std::sscanf(filename.c_str(), kFileNameFormat.c_str(), &id, &position, &timestamp);
    string key = index->GetKeyForId(id);
    if (key.empty() || id < 0 || position < 0 || timestamp <= 0) {
        LOG_ERROR_DETAIL("SimpleCacheSpan::CreateCacheEntry, pasrse filename fail, key:%s, id:%d, position:%lld, timestamp:%lld",
                         key.c_str(), id, position, timestamp);
        return nullptr;
    }

    return new SimpleCacheSpan(key, position, file_size, timestamp, std::make_shared<File>(file));
}

File SimpleCacheSpan::GetCacheFile(const File& cache_dir, int id, int64_t position,
                                   int64_t last_access_timestamp) {
    char buf[256] = {0};
    std::sprintf(buf, kFileNameFormat.c_str(), id, position, last_access_timestamp);
    return File(cache_dir, std::string(buf));
}

SimpleCacheSpan* SimpleCacheSpan::CopyWithUpdatedLastAccessTime(int id) {
    assert(is_cached);
    int64_t now = SystemUtil::GetEpochTime();
    File new_cache_file = GetCacheFile(file->parent().path(), id, position, now);
    return new SimpleCacheSpan(key, position, length, now, std::make_shared<File>(new_cache_file));
}

}
}
