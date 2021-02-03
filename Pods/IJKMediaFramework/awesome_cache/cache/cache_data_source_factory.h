#pragma once

#include <include/awesome_cache_runtime_info_c.h>
#include "data_source.h"
#include "data_sink.h"
#include "cache.h"
#include "http_data_source_factory.h"
#include "file_data_source_factory.h"
#include "cache_session_listener.h"
#include "cache_data_sink_factory.h"
#include "file_data_source_factory.h"

namespace kuaishou {
namespace cache {

class CacheDataSourceFactory {
  public:
    CacheDataSourceFactory(Cache* cache,
                           std::shared_ptr<HttpDataSourceFactory> http_datasource_factory,
                           std::shared_ptr<FileDataSourceFactory> file_datasource_factory,
                           int32_t max_cache_file_size = kDefaultMaxCacheFileSize);

    virtual ~CacheDataSourceFactory() {}

    DataSource* CreateCacheDataSource(const DataSourceOpts& opts,
                                      shared_ptr<CacheSessionListener> listener,
                                      AwesomeCacheRuntimeInfo* ac_rt_info);

    DataSink* CreateCacheDataSink(AwesomeCacheRuntimeInfo* ac_rt_info);

  private:
    Cache* const cache_;
    std::shared_ptr<HttpDataSourceFactory> http_source_factory_;
    std::shared_ptr<FileDataSourceFactory> file_souce_factory_;
    std::shared_ptr<CacheDataSinkFactory> cache_data_sink_factory_;
    int32_t max_cache_file_size_;
};

} // namespace cache
} // namespace kuaishou
