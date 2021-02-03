//
// Created by MarshallShuai on 2019-07-11.
//
#pragma

#include <stdint.h>
#include <mutex>
#include <map>
#include "v2/cache/cache_v2_settings.h"
#include "v2/cache/cache_content_v2.h"
#include "cache_errors.h"
#include "dir_manager_media.h"
#include "dir_manager_resource.h"


namespace kuaishou {
namespace cache {

/**
 *
 * 职能0：查询缓存文件大小
 * 原因：不能让 CacheContentIndexV2 去做这些文件查询和大小统计等可能耗时的操作，应该尽量保持 CacheContentIndexV2
 * 的一个轻量，因为 CacheContentIndexV2 内部有一个锁来互斥对map的操作，所以应该分担一些可以分担的查询等操作给 本CacheV2FileManager类，
 * CacheV2FileManager内的接口可以大部分是相对耗时的
 *
 * 职能1：清理，对CacheV2的文件做清理工作,主要包括以下几个职责：
 *  1.定期对@see CacheContextIndexV2 里查不到的文件做删除
 *  2.定期evict CacheContent对应的文件，保持CacheV2的文件总大小不会超过上线，
 *      目前的策略不会马上清理，而是允许有一定延迟，比如每5分钟清理一次，这样两个好处：
 *      a.可以让清理文件的耗时操作和CacheContentIndexV2的init load操作解耦合
 *      b.可以让ImportToCache很大文件(比如超过cacheMaxDirSize的文件，在一定时间内是可用的）,
 *          解决定佳报的刚上传完并ImportToCache的视频就不能完全从本地缓存文件下载的问题
 *  3.维护本地文件的已缓存大小的信息，支持外部Query/Update/Delete 已缓存文件信息
 *
 */
class CacheV2FileManager {
  public:
    /**
     * only for unit test purpose
     */
    static void DeleteCacheV2RootDir();

    static DirManagerMedia* GetMediaDirManager();

    static DirManagerResource* GetResourceDirManager();
  private:
    static std::mutex instance_mutex_;

    static DirManagerMedia* media_dir_manager_;
    static DirManagerResource* resource_dir_manager_;
};


}
}
