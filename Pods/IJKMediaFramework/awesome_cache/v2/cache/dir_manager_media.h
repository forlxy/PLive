//
// Created by MarshallShuai on 2019-10-14.
//

#pragma once

#include "utils/macro_util.h"
#include <stdint.h>
#include <mutex>
#include <map>
#include "v2/cache/cache_v2_settings.h"
#include "v2/cache/cache_content_v2_with_scope.h"
#include "v2/cache/cache_content_index_v2_with_scope.h"
#include "cache_errors.h"

HODOR_NAMESPACE_START

class DirManagerMedia {
  public:
    struct RestoreResult {
        int success_scope_file_cnt_{};

        // 文件名不合法，删除
        int fail_scope_file_name_invalid_cnt{};
        // cacheContentIndexV2查不到对应的cacheContent记录，删除掉
        int fail_scope_file_no_content_record_cnt{};
    };
    DirManagerMedia();

    CacheContentIndexV2WithScope* Index();

    /**
     * 按规则清理CacheContentV2，把总的缓存文件的大小 清理到 bytes_limit 的范围以内
     * 这个函数可能会耗时（删文件，以及更新CacheContentIndexV2），所以到时候要放到单独线程做
     * @param bytes_limit 缓存上限阈值
     */
    void PruneWithCacheTotalBytesLimit(int64_t bytes_limit = CacheV2Settings::GetCacheBytesLimit());

    /**
     * 当前总缓存的大小
     * @return 当前总缓存的大小
     */
    int64_t GetTotalCachedBytes();

    /**
     * 缓存上限阈值RestoreFileInfoForMediaCacheDir
     * @return 缓存上限阈值
     */
    int64_t GetCacheBytesLimit();

    /**
     * 获取key对应的CacheContent已经缓存的字节数
     */
    int64_t GetCacheContentCachedBytes(const std::string& key);

    /**
     * 获取key对应的CacheScope已经缓存的字节数
     */
    int64_t GetCacheScopeCachedBytes(const CacheScope& scope);

    /**
     * 扫描本地media目录，恢复分片信息，在初始化模块调用一次即可
     */
    RestoreResult RestoreFileInfo();


    /**
     * 表示一个scope对应的文件缓存到本地了，需要通知fileManager进行注册相关信息，并触发LRU清理
     */
    AcResultType CommitScopeFile(const CacheScope& scope);
    /**
     * 一旦有分片成功缓存flush到本地，调用此接口
     */
    bool OnCacheScopeFileFlushed(const CacheScope& scope);
    void OnCacheScopeFileDeleted(const CacheScope& scope);

  private:
    /**
     *
     * @return 失败返回false，并且会负责把相关scope file删除
     */
    bool _AddCachedScope(const CacheScope& scope, RestoreResult& ret);

    static bool CacheContentSortAsc(std::shared_ptr<CacheContentV2WithScope> c1,
                                    std::shared_ptr<CacheContentV2WithScope> c2) {
        return c1->GetLastAccessTimestamp() < c2->GetLastAccessTimestamp();
    }

  private:
    CacheContentIndexV2WithScope* index_;

    std::recursive_mutex cache_scopes_map_mutex_;
    // key:scope的filename，value：scope已经缓存的字节数
    std::map<std::string, int64_t> cache_scope_cached_bytes_map_;
    // key:cache_content的key，value：cache_content已经缓存的字节数
    std::map<std::string, int64_t> cache_content_cached_bytes_map_;
    int64_t total_cached_bytes_{};
};

HODOR_NAMESPACE_END