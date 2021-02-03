//
//  connection_info_.h
//  IJKMediaFramework
//
//  Created by 帅龙成 on 2019/6/27.
//  Copyright © 2019 kuaishou. All rights reserved.
//

#pragma once
#include <memory>
#include "download/input_stream.h"
#include "data_spec.h"
#include "../include/cache_defs.h"
#include "ac_log.h"

namespace kuaishou {
namespace cache {

/**
*  所有下载一些连接/下载进度等数据
*/
struct ConnectionInfoV2 {
    uint32_t id_ = -1;
// content length of the downloadable contents, may be kLengthUnset.
    int64_t content_length = kLengthUnset;
// response code of the request.
    int response_code = 0;
// 0 if no error, positive value for error we set, negative value for error from other library(libcurl).
    int error_code = 0;
// used time of make connection.
    int connection_used_time_ms = 0;
// dns resolve time
    int http_dns_analyze_ms = 0;
// http read first data time
    int http_first_data_ms = 0;
// the number of redirects
    int redirect_count = 0;
// the last used URL
    std::string effective_url = "";
// uri.
    std::string uri = "";
// host.
    std::string http_dns_host = "";
// ip address.
    std::string ip = "";
// kwaisign
    std::string sign = "";
// kwai k_ks_cache
    std::string x_ks_cache = "";

    std::string session_uuid = "";
    std::string download_uuid = "";

    bool is_range_request = false;
    int64_t range_request_start = -1;
    int64_t range_request_end = -1;
    int64_t range_response_start = -1;
    int64_t range_response_end = -1;
// file total length of the downloading file. for byte-range.
    int64_t range_response_file_length = kLengthUnset;

    DownloadStopReason stop_reason = kDownloadStopReasonUnknown;

    // transfer_consume_ms_ 这个字段很重要，会影响cdn resource上报统计逻辑，所以重构的时候需谨慎，一定要测试100%正确
    int32_t transfer_consume_ms = 0;

    int64_t downloaded_bytes_from_curl = 0;

    bool is_gzip = false;

    /**
     *
     * @return 平均下载速度，单位kbps
     */
    int64_t GetAvgDownloadSpeedkbps() {
        return transfer_consume_ms > 0 ? downloaded_bytes_from_curl * 8 / transfer_consume_ms : 0;
    }
    bool IsResponseCodeSuccess() const {
        return (response_code >= 200 && response_code <= 299);
    }

    bool IsDownloadComplete() const {
        return content_length > 0 && (downloaded_bytes_from_curl >= content_length);
    }

    /**
     * 如果是range请求，则返回range里的file_length，否则返回Content-length
     */
    int64_t GetFileLength() const {
        return is_range_request ? range_response_file_length : content_length;
    }

};

}
}
