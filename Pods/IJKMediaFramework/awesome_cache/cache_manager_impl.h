#pragma once

#include <memory>
#include <atomic>
#include <include/offline_cache_task_spec.h>
#include "cache_manager.h"
#include "download/download_manager.h"
#include "http_data_source_factory.h"
#include "file_data_source_factory.h"
#include "cache/cache.h"
#include "cache/cache_evictor.h"
#include "cache/cached_content_index.h"
#include "cache/cache_data_source_factory.h"
#include "cache_factory_impl.h"
#include "awesome_cache_runtime_info_c.h"

namespace kuaishou {
namespace cache {

class CacheManagerImpl : public CacheManager {
  public:
    CacheManagerImpl();

    virtual ~CacheManagerImpl();

    virtual std::unique_ptr<DataSource> CreateDataSource(DataSourceOpts& options,
                                                         shared_ptr<CacheSessionListener> listener,
                                                         AwesomeCacheRuntimeInfo* ac_rt_info,
                                                         bool* seekable = nullptr) override;

    virtual std::unique_ptr<Task> CreateExportCachedFileTask(const std::string& uri, const std::string& key,
                                                             DataSourceOpts& options, const std::string& file_name,
                                                             TaskListener* listener) override;

    virtual std::unique_ptr<Task> CreateOfflineCachedFileTask(const std::string& uri, const std::string& key,
                                                              DataSourceOpts& options, TaskListener* listener) override;

    virtual std::unique_ptr<Task> CreateOfflineCachePreloadManifestTask(OfflienCacheDataSpec data_spec,
                                                                        OfflienCacheVodAdaptiveInit vod_adaptive_init,
                                                                        DataSourceOpts& options,
                                                                        TaskListener* listener) override;
    virtual std::unique_ptr<Task> CreateOfflineCachePreloadFileTask(OfflienCacheDataSpec data_spec,
                                                                    DataSourceOpts& options,
                                                                    TaskListener* listener);

    virtual bool ImportFileToCache(const std::string& path, const std::string& key) override;
    virtual void SetOptInt(CacheOpt opt, int64_t value) override;

    virtual void SetOptStr(CacheOpt opt, const std::string value) override;

    virtual int64_t GetOptInt(CacheOpt opt) override;

    virtual std::string GetOptStr(CacheOpt opt) override;

    virtual void Init() override;

    virtual bool Initialized() override ;

    virtual void ClearCacheForKey(const std::string& key) override;

    virtual void ClearCacheDir() override;

    virtual bool IsFullyCached(const std::string& key) override;

    virtual int64_t GetCachedBytesForKey(const std::string& key) override;

    virtual int64_t GetTotalBytesForKey(const std::string& key) override;

    virtual int64_t GetCachedBytes() override;

    virtual void SetEnabled(bool enabled) override;

    virtual bool IsEnabled() override;

    virtual bool hasOccurCacheError() override;

    virtual void onCacheErrorOccur() override;

    // Cache V2 related
    virtual void SetCacheV2Enabled(bool enabled) override;

    virtual bool IsCacheV2Enabled() override;

    virtual bool ImportFileToCacheV2(const std::string& path, const std::string& key) override;

  private:
    // return: false if not globally enabled or init fail, otherwise return true.
    // inner function, not thread-safe
    virtual bool _InitOnceIfEnabled();
    std::unique_ptr<Task> CreateOfflineCachePreloadTask(const std::string& url, const std::string& key,
                                                        const std::string& host,
                                                        DataSourceOpts& options,
                                                        TaskListener* listener,
                                                        int64_t pos = 0, int64_t len = -1);


  private:
    bool inited_;
    std::mutex mutex_;
    bool enabled_;
    bool cache_v2_enabled_;
    File directory_;
    std::shared_ptr<CachedContentIndex> index_;
    std::unique_ptr<Cache> cache_;
    std::shared_ptr<DownloadManager> download_manager_;
    std::shared_ptr<HttpDataSourceFactory> http_data_source_factory_;
    std::shared_ptr<FileDataSourceFactory> file_data_source_factory_;
    std::shared_ptr<CacheDataSourceFactory> cache_data_source_factory_;
    std::shared_ptr<TransferListener<HttpDataSource> > transfer_listener_;
    int64_t max_cache_bytes_;
    int64_t max_cache_file_size_;
    bool enable_detailed_stats_;
    CacheEvictorStrategy evictor_stratergy_;
    DownloadStratergy download_stratergy_;
    // 在有些Android机型上，文件系统会有时可用有时不可用，这个时候我们如果在一次app生命周期遇到cache error，之后的流程就不走cache了
    bool has_occur_cache_error_;
    std::string proxy_address_;
};

} // cache
} // kuaishou
