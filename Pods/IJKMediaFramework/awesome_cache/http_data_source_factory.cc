#include "http_data_source_factory.h"
#include "default_http_data_source.h"
#include "async_http_data_source.h"
#include "multi_download_http_data_source.h"
#include "ffurl_http_data_source.h"

#ifdef CONFIG_LIVE_P2SP
#include "live_xyp2sp_http_data_source.h"
#endif

#include <assert.h>
#include "constant.h"

#ifdef __APPLE__
#include <TargetConditionals.h>
#endif

namespace kuaishou {
namespace cache {

HttpDataSourceFactory::HttpDataSourceFactory(std::shared_ptr<DownloadManager> download_manager, std::shared_ptr<TransferListener<HttpDataSource> > listener) :
    download_manager_(download_manager),
    listener_(listener) {
}

HttpDataSource* HttpDataSourceFactory::CreateDataSource(const DownloadOpts& opts,
                                                        AwesomeCacheRuntimeInfo* ac_rt_info,
                                                        DataSourceType type) {
    HttpDataSource* data_source = nullptr;

    if (type == kDataSourceTypeLiveNormal) {
        switch (opts.upstream_type) {
#ifdef CONFIG_LIVE_P2SP
            case kP2spHttpDataSource:
                data_source = new LiveXyP2spHttpDataSource(download_manager_, listener_, opts, ac_rt_info);
                LOG_DEBUG("HttpDataSourceFactory::CreateDataSource use LiveXyP2spHttpDataSource for live normal type");
                break;
#endif
#if (TARGET_OS_IPHONE || __ANDROID__)
            case kFFUrlHttpDataSource:
                data_source = new FFUrlHttpDataSource(opts, ac_rt_info);
                LOG_DEBUG("HttpDataSourceFactory::CreateDataSource use FFUrlHttpDataSource for live normal type");
                break;
#endif
            default:
                data_source = new DefaultHttpDataSource(download_manager_, listener_, opts, ac_rt_info);
                LOG_DEBUG("HttpDataSourceFactory::CreateDataSource use DefaultHttpDataSource for live normal type");
                break;
        }
    } else if (type == kDataSourceTypeAsyncDownload) {
        switch (opts.upstream_type) {
            case kDefaultHttpDataSource:
            default:
                data_source = new AsyncHttpDataSource(download_manager_, listener_, opts, ac_rt_info);
                LOG_DEBUG("HttpDataSourceFactory::CreateDataSource use AsyncHttpDataSource in Async Mode");
                break;
        }
    } else {
        switch (opts.upstream_type) {
            case kMultiDownloadHttpDataSource:
                data_source = new MultiDownloadHttpDataSource(download_manager_, listener_, opts, ac_rt_info);
                LOG_DEBUG("HttpDataSourceFactory::CreateDataSource use MultiDownloadHttpDataSource");
                break;
#if (TARGET_OS_IPHONE || __ANDROID__)
            case kFFUrlHttpDataSource:
                data_source = new FFUrlHttpDataSource(opts, ac_rt_info);
                LOG_DEBUG("HttpDataSourceFactory::CreateDataSource use FFUrlHttpDataSource");
                break;
#endif
            case kDefaultHttpDataSource:
            default:
                data_source = new DefaultHttpDataSource(download_manager_, listener_, opts, ac_rt_info);
                LOG_DEBUG("HttpDataSourceFactory::CreateDataSource use DefaultHttpDataSource");
                break;
        }
    }

    for (auto it : request_property_map_) {
        data_source->SetRequestProperties(it.first, it.second);
    }
    return data_source;
}

void HttpDataSourceFactory::SetDefaultRequestProperties(const std::string& name, const std::string& value) {
    request_property_map_[name] = value;
}

void HttpDataSourceFactory::ClearRequestProperty(const std::string& name) {
    auto it = request_property_map_.find(name);
    if (it != request_property_map_.end()) {
        request_property_map_.erase(it);
    }
}

void HttpDataSourceFactory::ClearAllRequestProperties() {
    request_property_map_.clear();
}


}
}
