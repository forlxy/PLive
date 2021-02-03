//
// Created by MarshallShuai on 2019-07-01.
//

#pragma once

#include <stdint.h>
#include <string>

namespace kuaishou {
namespace cache {

class CacheV2Settings {

  public:
    static int64_t GetScopeMaxSize();
    static void SetScopeMaxSize(int64_t scope_size);

    static void SetCacheRootDirPath(const std::string& path);

    static std::string GetCacheRootDirPath();
    /**
     * 多媒体缓存文件夹， 这个目录的文件会按照LRU的规则做分片淘汰
     * @return 视频文件缓存路径
     */
    static std::string GetMediaCacheDirPath();
    /**
     * 主要用于存放静态资源文件这个目录的文件永不删除
     * @return 静态资源缓存路径
     */
    static std::string GetResourceCacheDirPath();

    /**
     * @return 确保资源缓存文件夹创建好了
     */
    static bool MakeSureResourceCacheDirExists();
    /**
     * @return 确保多媒体缓存文件夹创建好了
     */
    static bool MakeSureMediaCacheDirExists();

    /**
     *  获取tcp当前复用重连数，目前暂时只在预加载task场景demo使用，后续会全局使用
     */
    static int GetTcpMaxConnects();

    /**
     *  设置tcp当前复用重连数，目前暂时只在预加载task场景demo使用，后续会全局使用
     */
    static int SetTcpMaxConnects(int max);

    static void SetCacheBytesLimit(int64_t bytes_limit);
    static int64_t GetCacheBytesLimit();

  private:
    static bool MakeSureCacheDirExists(const std::string& path);
  private:
    static int64_t s_scope_size_;
    static int64_t s_cache_byts_limit_;

    static std::string s_root_cache_dir_;;
    static std::string s_media_cache_dir_;;
    static std::string s_resource_cache_dir_;

    static int s_tcp_max_connects_;
};


} // namespace cache
} // namespace kuaishou
