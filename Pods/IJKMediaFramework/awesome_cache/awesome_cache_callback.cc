#include "include/awesome_cache_callback.h"

namespace kuaishou {
namespace cache {

string AcCallbackInfo::GetCdnStatJson() {
    json cdnJson;
    {
        json request;
        request["url"] = currentUri;
        request["host"] = host;
        request["ua"] = "";    // to do
        request["range_start"] = rangeRequestStart;
        request["range_end"] = rangeRequestEnd;
        cdnJson["request"] = request;
    }
    {
        json response;
        response["code"] = httpResponseCode;
        response["range_start"] = rangeResponseStart;
        response["range_end"] = rangeResponseEnd;
        response["total_bytes"] = totalBytes;
        response["kwai_sign"] = kwaiSign;
        response["x_ks_cache"] = xKsCache;
        response["content_length"] = contentLength;
        response["effective_url"] = httpEffectiveUrl;
        response["http_ver"] = httpVersion;
        cdnJson["response"] = response;
    }
    {
        json config;
        config["session_uuid"] = sessionUUID;
        config["download_uuid"] = downloadUUID;
        config["data_source_type"] = dataSourceType;
        if (!productContext.empty()) {
            config["product_context"] = productContext;
        }
        config["upstream_type"] = upstreamType;
        cdnJson["config"] = config;
    }
    {
        json stat;
        stat["server_ip"] = ip;
        stat["stop_reason"] = GetStopReasonStr(stopReason);
        stat["error_code"] = errorCode;
        stat["downloaded_bytes"] = downloadBytes;
        stat["net_cost"] = transferConsumeMs;
        stat["dns_cost"] = dnsCost;
        stat["connect_cost"] = connectCost;
        stat["fst_data_cost"] = firstDataCost;
        stat["redirect_cnt"] = httpRedirectCount;
#if defined(__ANDROID__)
        stat["tcp_climbing"] = tcpClimbingInfo;
#endif
        stat["os_errno"] = osErrno;
        cdnJson["stat"] = stat;
    }
    return cdnJson.dump();
}

std::string AcCallbackInfo::GetStopReasonStr(DownloadStopReason reason) {
    switch (reason) {
        case kDownloadStopReasonFinished:
            return "FINISHED";
        case kDownloadStopReasonCancelled:
            return "CANCEL";
        case kDownloadStopReasonFailed:
        case kDownloadStopReasonTimeout:
        case kDownloadStopReasonNoContentLength:
        case kDownloadStopReasonContentLengthInvalid:
        case kDownloadStopReasonByteRangeInvalid:
            return "FAILED";
        case kDownloadStopReasonUnset:
        case kDownloadStopReasonUnknown:
        default:
            return "N/A";
    }
}

void AcCallbackInfo::SetDnsCost(int64_t cost) {
    this->dnsCost = cost;
}

void AcCallbackInfo::SetConnectCost(int64_t cost) {
    this->connectCost = cost;
}

void AcCallbackInfo::SetFirstDataCost(int64_t cost) {
    this->firstDataCost = cost;
}

void AcCallbackInfo::SetRangeRequestStart(int64_t start) {
    this->rangeRequestStart = start;
}

void AcCallbackInfo::SetRangeRequestEnd(int64_t end) {
    this->rangeRequestEnd = end;
}

void AcCallbackInfo::SetRangeResponseStart(int64_t start) {
    this->rangeResponseStart = start;
}

void AcCallbackInfo::SetRangeResponseEnd(int64_t end) {
    this->rangeResponseEnd = end;
}

void AcCallbackInfo::SetProductContext(string productContext0) {
    this->productContext = productContext0;
}

void AcCallbackInfo::SetCacheKey(string key) {
    this->cacheKey = key;
}

void AcCallbackInfo::SetCachedBytes(int64_t bytes) {
    this->cachedBytes = bytes;
}

void AcCallbackInfo::SetContentLength(int64_t contentLength0) {
    this->contentLength = contentLength0;
}

void AcCallbackInfo::SetDownloadBytes(int64_t downloadBytes0) {
    this->downloadBytes = downloadBytes0;
}

void AcCallbackInfo::SetProgressPosition(int64_t progressPosition0) {
    this->progressPosition = progressPosition0;
}

void AcCallbackInfo::SetTotalBytes(int64_t totalBytes0) {
    this->totalBytes = totalBytes0;
}

void AcCallbackInfo::SetCurrentUri(string uri0) {
    this->currentUri = uri0;
}

void AcCallbackInfo::SetHost(string host0) {
    this->host = host0;
}

void AcCallbackInfo::SetIp(string ip0) {
    this->ip = ip0;
}

void AcCallbackInfo::SetKwaiSign(string kwaiSign0) {
    this->kwaiSign = kwaiSign0;
}

void AcCallbackInfo::SetXKsCache(string xKsCache0) {
    this->xKsCache = xKsCache0;
}

void AcCallbackInfo::SetSessionUUID(string sessionUUID0) {
    this->sessionUUID = sessionUUID0;
}

void AcCallbackInfo::SetDownloadUUID(string downloadUUID0) {
    this->downloadUUID = downloadUUID0;
}

void AcCallbackInfo::SetHttpResponseCode(int httpResponseCode0) {
    this->httpResponseCode = httpResponseCode0;
}

void AcCallbackInfo::SetHttpRedirectCount(int httpRedirectCount0) {
    this->httpRedirectCount = httpRedirectCount0;
}

void AcCallbackInfo::SetEffectiveUrl(string httpEffectiveUrl0) {
    this->httpEffectiveUrl = httpEffectiveUrl0;
}

void AcCallbackInfo::SetStopReason(DownloadStopReason stopReason0) {
    this->stopReason = stopReason0;
}

void AcCallbackInfo::SetErrorCode(int32_t errorCode0) {
    this->errorCode = errorCode0;
}

void AcCallbackInfo::SetTransferConsumeMs(int32_t costMs) {
    this->transferConsumeMs = costMs;
}

void AcCallbackInfo::SetDataSourceType(int32_t dataSourceType0) {
    this->dataSourceType = dataSourceType0;
}

#if defined(__ANDROID__)
void AcCallbackInfo::SetTcpClimbingInfo(string tcpClimbingInfo0) {
    this->tcpClimbingInfo = tcpClimbingInfo0;
}
#endif

void AcCallbackInfo::SetOsErrno(long osErrno0) {
    this->osErrno = osErrno0;
}

void AcCallbackInfo::CopyConnectionInfoIntoAcCallbackInfo(const ConnectionInfoV2& info,
                                                          AcCallbackInfo& ac_cb_info) {
    ac_cb_info.SetDownloadBytes(info.downloaded_bytes_from_curl);
    ac_cb_info.SetContentLength(info.content_length);

    ac_cb_info.SetHost(info.http_dns_host);
    ac_cb_info.SetIp(info.ip);
    ac_cb_info.SetKwaiSign(info.sign);
    ac_cb_info.SetXKsCache(info.x_ks_cache);
    ac_cb_info.SetSessionUUID(info.session_uuid);
    ac_cb_info.SetDownloadUUID(info.download_uuid);
    ac_cb_info.SetTransferConsumeMs(info.transfer_consume_ms);

    ac_cb_info.SetHttpRedirectCount(info.redirect_count);
    ac_cb_info.SetEffectiveUrl(info.effective_url);
    ac_cb_info.SetHttpResponseCode(info.response_code);

    ac_cb_info.SetRangeRequestStart(info.range_request_start);
    ac_cb_info.SetRangeRequestEnd(info.range_request_end);
    ac_cb_info.SetRangeResponseStart(info.range_response_start);
    ac_cb_info.SetRangeResponseEnd(info.range_response_end);
    ac_cb_info.SetDnsCost(info.http_dns_analyze_ms);
    ac_cb_info.SetConnectCost(info.connection_used_time_ms);
    ac_cb_info.SetFirstDataCost(info.http_first_data_ms);
}

void AcCallbackInfo::SetHttpVersion(string httpVersion0) {
    this->httpVersion = httpVersion0;
}

void AcCallbackInfo::SetUpstreamType(int32_t upstreamType0) {
    this->upstreamType = upstreamType0;
}

}
}

