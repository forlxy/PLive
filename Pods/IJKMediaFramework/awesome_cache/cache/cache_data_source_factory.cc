#include "cache_data_source_factory.h"
#include "cache/sync_cache_data_source.h"
#include "cache/async_cache_data_source.h"
#include "cache/cache_data_sink_factory.h"
#include "cache/http_proxy_data_source.h"
#include "v2/data_source/async_cache_data_source_v2.h"
#include "live_http_data_source_wrapper.h"

namespace kuaishou {
namespace cache {
CacheDataSourceFactory::CacheDataSourceFactory(Cache* cache,
                                               std::shared_ptr<HttpDataSourceFactory> http_datasource_factory,
                                               std::shared_ptr<FileDataSourceFactory> file_datasource_factory,
                                               int32_t max_cache_file_size) :
    cache_(cache),
    http_source_factory_(http_datasource_factory),
    file_souce_factory_(file_datasource_factory),
    cache_data_sink_factory_(std::make_shared<CacheDataSinkFactory>(cache, max_cache_file_size)),
    max_cache_file_size_(max_cache_file_size) {
    assert(cache_);
}

DataSource* CacheDataSourceFactory::CreateCacheDataSource(const DataSourceOpts& opts,
                                                          shared_ptr<CacheSessionListener> listener,
                                                          AwesomeCacheRuntimeInfo* ac_rt_info) {
    switch (opts.type) {
        case kDataSourceTypeDefault: {
            return new SyncCacheDataSource(cache_,
                                           std::shared_ptr<HttpDataSource>(
                                               http_source_factory_->CreateDataSource(opts.download_opts, ac_rt_info)),
                                           std::shared_ptr<DataSource>(
                                               file_souce_factory_->CreateDataSource()),
                                           std::shared_ptr<DataSink>(
                                               cache_data_sink_factory_->CreateDataSink(ac_rt_info)),
                                           opts,
                                           nullptr,
                                           listener,
                                           ac_rt_info);
        }
        case kDataSourceTypeAsyncDownload: {
            return new AsyncCacheDataSource(cache_,
                                            std::shared_ptr<HttpDataSource>(
                                                http_source_factory_->CreateDataSource(opts.download_opts, ac_rt_info, opts.type)),
                                            std::shared_ptr<DataSource>(
                                                file_souce_factory_->CreateDataSource()),
                                            opts,
                                            nullptr, listener,
                                            ac_rt_info);
        }
        case kDataSourceTypeSegment: {
            DownloadOpts download_opts = opts.download_opts;
            download_opts.allow_content_length_unset = true;
            return new HttpProxyDataSource(std::shared_ptr<HttpDataSource>(http_source_factory_->CreateDataSource(download_opts, ac_rt_info)),
                                           listener, opts, ac_rt_info);
        }
        case kDataSourceTypeAsyncV2:
            return new AsyncCacheDataSourceV2(opts, listener, ac_rt_info);
        case kDataSourceTypeLiveNormal:
        case kDataSourceTypeLiveAdaptive:  // should not reach here
        default: {
            LOG_DEBUG("CacheDataSourceFactory::CreateCacheDataSource, opts.type(%d) invalid, return nullptr", opts.type);
            return nullptr;
        }
    }
}


DataSink* CacheDataSourceFactory::CreateCacheDataSink(AwesomeCacheRuntimeInfo* ac_rt_info) {
    return cache_data_sink_factory_->CreateDataSink(ac_rt_info);
}

} // namespace cache
} // namespace kuaishou
