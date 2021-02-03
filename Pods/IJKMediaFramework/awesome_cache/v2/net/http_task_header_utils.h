#pragma once

#include <functional>
#include <string>
#include "cache_opts.h"
#include "connection_info_v2.h"


namespace kuaishou {
namespace cache {
namespace http {


// Take dataspec and download options, setup connection info, generate request headers
// 根据dataspec和download options生成需要的请求http头，同时设置相应的connection info字段
// Including:
// - Range reuqest related
// - Host related, including http_dns_host
//
// Return error code
void SetRequestHeaders(const DataSpec& spec,
                       const DownloadOpts& options,
                       ConnectionInfoV2* info,
                       std::function<void(std::string const&, std::string const&)> add_header_cb);

// Take response headers, fill connection info
// 根据返回的http头，填相应的connection info和其他信息
// Including:
// - Content length related
// - Range request related
// - x-ks, kwai-sign
//
// `get_next_header` is used to get a list of response headers.
// Return (name, value) on each call, return ("", "") on end.
AcResultType ParseResponseHeaders(const DataSpec& spec,
                                  ConnectionInfoV2* info,
                                  bool* server_not_support_range,
                                  int64_t* to_skip_bytes,
                                  std::function<std::pair<std::string, std::string>()> get_next_header);


}
}
}
