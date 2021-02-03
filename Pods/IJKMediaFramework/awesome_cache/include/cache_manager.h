#pragma once
#include <memory>
#include <mutex>
#include "cache_opts.h"
#include "cache_session_listener.h"
#include "data_source_seekable.h"
#include "task.h"
#include "task_listener.h"
#include "awesome_cache_runtime_info_c.h"
#include "offline_cache_task_spec.h"

namespace kuaishou {
namespace cache {

class CacheManager {
  public:
    static CacheManager* GetInstance();

    virtual std::unique_ptr<DataSource> CreateDataSource(DataSourceOpts& options,
                                                         std::shared_ptr<CacheSessionListener> listener,
                                                         AwesomeCacheRuntimeInfo* ac_rt_info,
                                                         bool* seekable = nullptr) = 0;
    virtual std::unique_ptr<Task> CreateExportCachedFileTask(const std::string& uri, const std::string& key,
                                                             DataSourceOpts& options, const std::string& file_name,
                                                             TaskListener* listener) = 0;
    virtual std::unique_ptr<Task> CreateOfflineCachedFileTask(const std::string& uri, const std::string& key,
                                                              DataSourceOpts& options, TaskListener* listener) = 0;

    virtual std::unique_ptr<Task> CreateOfflineCachePreloadManifestTask(OfflienCacheDataSpec data_spec,
                                                                        OfflienCacheVodAdaptiveInit vod_adaptive_init,
                                                                        DataSourceOpts& options,
                                                                        TaskListener* listener) = 0;
    virtual std::unique_ptr<Task> CreateOfflineCachePreloadFileTask(OfflienCacheDataSpec data_spec,
                                                                    DataSourceOpts& options,
                                                                    TaskListener* listener) = 0;

    virtual bool ImportFileToCache(const std::string& path, const std::string& key) = 0;
    virtual void SetOptInt(CacheOpt opt, int64_t value) = 0;
    virtual void SetOptStr(CacheOpt opt, const std::string value) = 0;
    virtual int64_t GetOptInt(CacheOpt opt) = 0;
    virtual std::string GetOptStr(CacheOpt opt) = 0;
    virtual void Init() = 0;
    virtual bool Initialized() = 0;
    virtual void SetEnabled(bool enabled) = 0;
    virtual bool IsEnabled() = 0;

    virtual void ClearCacheForKey(const std::string& key) = 0;
    virtual void ClearCacheDir() = 0;
    virtual int64_t GetTotalBytesForKey(const std::string& key) = 0;
    virtual int64_t GetCachedBytes() = 0;
    virtual bool IsFullyCached(const std::string& key) = 0;
    virtual int64_t GetCachedBytesForKey(const std::string& key) = 0;

    virtual bool hasOccurCacheError() = 0;
    virtual void onCacheErrorOccur() = 0;

    // Cache V2 related
    virtual void SetCacheV2Enabled(bool enabled) = 0;
    virtual bool IsCacheV2Enabled() = 0;
    virtual bool ImportFileToCacheV2(const std::string& path, const std::string& key) = 0;

  protected:
    CacheManager() {};
    virtual ~CacheManager() {};

    CacheManager(CacheManager const&) = delete;
    CacheManager& operator=(CacheManager const&) = delete;
    CacheManager(CacheManager&&) = delete;
    CacheManager& operator=(CacheManager&&) = delete;

  private:
    static CacheManager* instance_;
    static std::mutex mutex_;
};

} // cache
} // kuaishou
