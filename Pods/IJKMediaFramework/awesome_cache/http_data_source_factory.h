#pragma once

#include <include/awesome_cache_runtime_info_c.h>
#include "http_data_source.h"
#include "transfer_listener.h"
#include "cache_opts.h"
#include "download/download_manager.h"
#include "awesome_cache_runtime_info_c.h"

namespace kuaishou {
namespace cache {

class HttpDataSourceFactory {
  public:
    virtual ~HttpDataSourceFactory() {};
    HttpDataSourceFactory(std::shared_ptr<DownloadManager> download_manager,
                          std::shared_ptr<TransferListener<HttpDataSource> > listener);
    HttpDataSource* CreateDataSource(const DownloadOpts& opts,
                                     AwesomeCacheRuntimeInfo* ac_rt_info,
                                     DataSourceType type = kDataSourceTypeDefault);

    void SetDefaultRequestProperties(const std::string& name, const std::string& value);

    void ClearRequestProperty(const std::string& name);

    void ClearAllRequestProperties();

  private:
    std::shared_ptr<TransferListener<HttpDataSource> > listener_;
    std::shared_ptr<DownloadManager> download_manager_;
    std::map<std::string, std::string> request_property_map_;
};
}
}
