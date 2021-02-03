//
// Created by MarshallShuai on 2019-10-14.
//

#pragma once

#include <stdint.h>
#include <string>
#include <map>
#include "utils/macro_util.h"
#include "v2/cache/cache_content_v2_non_scope.h"
#include "v2/cache/cache_content_index_v2_non_scope.h"

HODOR_NAMESPACE_START

class DirManagerResource {
  public:
    struct RestoreResult {
        int success_content_file_cnt_{};

        // 文件名不合法，删除
        int fail_content_file_name_invalid_cnt{};
        // cacheContentIndexV2查不到对应的cacheContent记录，删除掉
        int fail_content_file_no_content_record_cnt{};
    };

    DirManagerResource();

    CacheContentIndexV2NonScope* Index();
    /**
     * 当前总缓存的大小
     * @return 当前总缓存的大小
     */
    int64_t GetTotalCachedBytes();

    /**
     * 获取key对应的CacheContent已经缓存的字节数
     */
    int64_t GetCacheContentCachedBytes(const std::string& key);


    /**
     * 扫描本地media目录，恢复分片信息，在初始化模块调用一次即可
     */
    RestoreResult RestoreFileInfo();

    /**
     * 一旦有分片成功缓存flush到本地，调用此接口
     */
    bool OnCacheContentFileFlushed(const CacheContentV2NonScope& content);
    void OnCacheContentFileDeleted(const CacheContentV2NonScope& content);

  private:
    /**
     *
     * @return 失败返回false，并且会负责把相关scope file删除
     */
    bool _AddCachedContent(const CacheContentV2NonScope& content, RestoreResult& ret);

  private:
    CacheContentIndexV2NonScope* index_;

    std::recursive_mutex cache_content_map_mutex_;
    // key:cache_content的key，value：cache_content已经缓存的字节数
    std::map<std::string, int64_t> cache_content_cached_bytes_map_{};
    int64_t total_cached_bytes_{};

};

HODOR_NAMESPACE_END