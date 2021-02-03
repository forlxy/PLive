//
// Created by MarshallShuai on 2019-10-16.
//


#pragma once

#include "utils/macro_util.h"
#include "cache_content_v2.h"
#include "cache_errors.h"
#include "v2/cache/cache_v2_settings.h"

HODOR_NAMESPACE_START

// 修改了这个地方记得修改MakeCacheFileName等所有跟kCacheScopeFileNameFormat有关的地方
#define kCacheScopeFileNameFormatSuffix "v2.4.acc" // 文件名必须以这个字符串结尾，不然不符合版本，每次改了文件格式，都要升级acc前面的数字
#define kCacheScopeFileNameFormatWithoutSuffix  "%lld_%lld_%lld_%d_%s" // contentLength_startPos_scopeMaxSize_evictStartegy_key
#define kCacheScopeFileNameFormat  kCacheScopeFileNameFormatWithoutSuffix "." kCacheScopeFileNameFormatSuffix

/**
 * 后缀名changeLog：
 * 2019.11.6
 * v2.3.acc -> v2.4.acc 添加 evictStartegy 字段，
 * contentLength_startPos_scopeMaxSize_key -> contentLength_startPos_scopeMaxSize_evictStartegy_key
 *
 *
 */

// 表示此内容的分片支持怎样被清理的方式
enum EvictStrategy {
    EvictStrategy_LRU = 1,   // 会被分片淘汰机制删除，也可以被cleanCacheDir触发
    EvictStrategy_NEVER = 2, // 只能通过Hodor.pruneStrategyNeverCacheContent的内部去重接口清除
};

class CacheContentV2WithScope final: public CacheContentV2 {
  public:

    static std::string MakeCacheFileName(const std::string& key, int64_t content_length, int64_t start_pos,
                                         EvictStrategy strategy, int64_t scope_max_size);

    /**
     * NOTE:这个函数理论上 只能被 CacheContentIndexV2 使用
     */
    CacheContentV2WithScope(std::string key, int64_t scope_max_size,
                            std::string cache_dir_path = CacheV2Settings::GetMediaCacheDirPath(), int64_t content_length = 0);

    /**
     *
     * @param force_check_file 目前只实现了 force_check_file = false的版本，所以传入true也没用，目前也没有使用true的场景
     * @return 已缓存的字节数
     */
    virtual int64_t GetCachedBytes(bool force_check_file = false) const override;


    /**
     * @return scopeMaxSize
     */
    int64_t GetScopeMaxSize() const;

    void SetEvictStrategy(EvictStrategy strategy);

    EvictStrategy GetEvictStrategy() const;

    /**
     * @return 获取CacheContent对应的CacheScope
     */
    std::vector<std::shared_ptr<CacheScope>> GetScopeList() const;

    /**
     * 根据规则生成 position和content_length对应的scope信息
     * @param position 位置
     * @return CacheScope
     */
    std::shared_ptr<CacheScope> ScopeForPosition(int64_t position) const;
    /**
     * 清理所有本CacheContent对应的文件分片
     */
    void DeleteScopeFiles() const;

    AcResultType VerifyMd5(const std::string& md5);

  private:
    const int64_t scope_max_size_{};
    EvictStrategy evict_strategy_;
};

HODOR_NAMESPACE_END
