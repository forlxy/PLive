//
//  connection_info_.h
//  IJKMediaFramework
//
//  Created by 帅龙成 on 2018/7/23.
//  Copyright © 2018 kuaishou. All rights reserved.
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
struct ConnectionInfo {
    uint32_t id_ = -1;
// content length of the downloadable contents, may be kLengthUnset. 这个值是已经减掉 droped_bytes_的
    int64_t content_length = kLengthUnset;
// response code of the request.
    int response_code = 0;
// the number of redirects
    int redirect_count = 0;
// the last used URL
    std::string effective_url = "";
// 0 if no error, positive value for error we set, negative value for error from other library(libcurl).
    int error_code = 0;
// used time of make connection.
    int connection_used_time_ms = 0;
// dns resolve time
    int http_dns_analyze_ms = 0;
// http read first data time
    int http_first_data_ms = 0;
// uri.
    std::string uri = "";
// host.
    std::string host = "";
// ip address.
    std::string ip = "";
// kwaisign
    std::string sign = "";
// kwai k_ks_cache
    std::string x_ks_cache = "";
// os errno
    long os_errno = 0;
#if defined(__ANDROID__)
    std::string tcp_climbing_info = "";
#endif
    int64_t range_request_start = -1;
    int64_t range_request_end = -1;

    int64_t range_response_start = -1;
    int64_t range_response_end = -1;

    std::string session_uuid = "";
    std::string download_uuid = "";

// file total length of the downloading file. for byte-range.
    int64_t file_length = kLengthUnset;
    DownloadStopReason stop_reason_ = kDownloadStopReasonUnknown;

    int64_t downloaded_bytes_ = 0;  // deprecated use downloaded_bytes_from_curl_
    // transfer_consume_ms_ 这个字段很重要，会影响cdn resource上报统计逻辑，所以重构的时候需谨慎，一定要测试100%正确
    int32_t transfer_consume_ms_ = 0;
    bool connection_closed = false;
    int64_t  need_drop_bytes_ = 0;  //兼容后台不支持断点情况
    int32_t  droped_bytes_ = 0;

    int64_t content_length_from_curl_ = kLengthUnset;
    int64_t downloaded_bytes_from_curl_ = 0;

    bool download_complete_ = false; // 表示当前下载是否下载完了
    bool is_gzip = false;

    ConnectionInfo(uint32_t id): id_(id) {
//        LOG_DEBUG("id:%d, ConnectionInfo(id) Ctor", id_);
    }

    ConnectionInfo() {
//        LOG_DEBUG("id:%d, ConnectionInfo() Ctor", id_);
    }
//  这块日志暂时需要供SyncCacheDataSource随时用来分析，等确认线上这块没生命周期的崩溃就删掉
//    ~ConnectionInfo() {
//        LOG_DEBUG("id:%d, ConnectionInfo() Dtor", id_);
//    }
//
//    ConnectionInfo(const ConnectionInfo& other) {
//        LOG_DEBUG("id:%d, ConnectionInfo() copy Ctor", other.id_);
//    }

    void UpdateDownloadedSize(int64_t downloaded_size) {
        downloaded_bytes_from_curl_ = downloaded_size;
        if ((content_length_from_curl_ >= 0) && (content_length_from_curl_ <= downloaded_bytes_from_curl_)) {
            download_complete_ = true;
        }
    }

    bool IsDownloadComplete() const {
        return download_complete_;
    }

    bool IsNeedDropData() const {
        return need_drop_bytes_ > 0 || droped_bytes_ > 0;
    }

    // 平均每秒下载的字节数
    int32_t GetAvgDownloadSpeed() const {
        return (int32_t)(transfer_consume_ms_ == 0 ? -1 : (GetDownloadedBytes() * 1000 / transfer_consume_ms_));
    }

    bool IsResponseCodeSuccess() const {
        return (response_code >= 200 && response_code <= 299);
    }

    int64_t GetUnDownloaedBytes() const {
        return content_length_from_curl_ - downloaded_bytes_from_curl_;
    }

    int64_t GetDownloadedBytes() const {
        return downloaded_bytes_from_curl_;
    }
    int64_t GetContentLength() const {
        return content_length_from_curl_;
    }



  private:
};

class HasConnectionInfo {
  public:
    virtual ~HasConnectionInfo() {};

    virtual const ConnectionInfo& GetConnectionInfo() = 0;
};


}
}
