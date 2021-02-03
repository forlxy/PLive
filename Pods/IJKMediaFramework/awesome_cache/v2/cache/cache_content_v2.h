//
// Created by MarshallShuai on 2019-07-01.
//

#pragma once

#include "file.h"
#include "v2/cache/cache_scope.h"

namespace kuaishou {
namespace cache {

class CacheScope;

class CacheContentV2 {
  public:
    static std::string GenerateKey(std::string url);

    CacheContentV2() = delete;

    CacheContentV2(const CacheContentV2& other);

    CacheContentV2(std::string key, std::string dir_path, int64_t content_length = 0);

    std::string GetKey() const;

    int64_t GetContentLength() const;

    /**
     * @return 此CacheContent储存的位置（可能是media_cache/resource_cahce)
     */
    std::string GetCacheDirPath() const;

    int SimpleHashCode();
    /**
     * @param len 必须大于0
     */
    void SetContentLength(int64_t len);

    /**
     * 20M的文件，这个函数扫描 在mac单元只需要不到1ms,美图手机上只要1ms
     * @param force_check_file 强制检测文件系统，如果为false则会参考FileManager里的缓存值，性能会更高
     * @return 已缓存大小
     */
    virtual int64_t GetCachedBytes(bool force_check_file = false) const {return 0;};

    /**
     * 20M的文件，这个函数扫描 在mac单元只需要不到1ms,美图手机上只要1ms
     * @param force_check_file 强制检测文件系统，如果为false则会参考FileManager里的缓存值，性能会更高
     * @return 是否完全缓存完
     */
    bool IsFullyCached(bool force_check_file = false) const;

    void UpdateLastAccessTimestamp();

    int64_t GetLastAccessTimestamp() const;

    void SetLastAccessTimestamp(int64_t ts);

  protected:
    /**
     * 以下几个值是cacheContent的特征值。cacheContent是不合文件系统状态绑定的（无文件状态性）的，
     * 只有以下几个变量来唯一确定一个cacheContent，往CacheContentV2里加成员变量的时候一定要谨慎，不能破坏无文件状态性
     *
     * 理论上：
     * 1.key_是uri一一对应的
     * 2.content_length_是固定不变的
     * 3.last_access_timestamp_是给分片淘汰用的
     * 4.scope_max_size_确定了缓存分片的方式以及补洞的性能（range)
     */
    const std::string key_;
    int64_t content_length_{};
    int64_t last_access_timestamp_{};

    // caching property
    const std::string cache_dir_path_;

    std::string file_name_;
};


} // namespace cache
} // namespace kuaishou
