//
// Created by MarshallShuai on 2019/1/16.
//

#pragma once

#include <string>
#include <cstdint>
#include "cache_defs.h"
#include <json/json.h>
#include <v2/net/connection_info_v2.h>
#include "ac_log.h"

using std::string;
using json = nlohmann::json;

namespace kuaishou {
namespace cache {

class AcCallbackInfo {
  public:
    virtual ~AcCallbackInfo() {};


    static void CopyConnectionInfoIntoAcCallbackInfo(ConnectionInfoV2 const& info, AcCallbackInfo& ac_cb_info);

    // 连接前就能确认
    void SetDataSourceType(int32_t dataSourceType);
    virtual void SetCacheKey(string key);
    virtual void SetCachedBytes(int64_t bytes);
    virtual void SetUpstreamType(int32_t upstreamType);

    // 连接前和连接后都需要尝试设置的信息
    virtual void SetTotalBytes(int64_t totalBytes);
    virtual void SetProgressPosition(int64_t progressPosition);

    // 发生连接后才拥有的信息
    virtual void SetDownloadBytes(int64_t downloadBytes);
    virtual void SetContentLength(int64_t contentLength);

    virtual void SetCurrentUri(string uri);
    virtual void SetHost(string host);
    virtual void SetIp(string ip);
    virtual void SetKwaiSign(string kwaiSign);
    virtual void SetXKsCache(string xKsCache);
    virtual void SetSessionUUID(string sessionUUID);
    virtual void SetDownloadUUID(string downloadUUID);
    virtual void SetStopReason(DownloadStopReason stopReason);
    virtual void SetErrorCode(int32_t errorCode);
    virtual void SetTransferConsumeMs(int32_t costMs);
    virtual void SetHttpVersion(string httpVersion);

    void SetHttpRedirectCount(int httpRedirectCount);
    void SetEffectiveUrl(string httpEffectiveUrl);
    // 这个字段应该可以改为对上层不可见
    virtual void SetHttpResponseCode(int httpResponseCode);

    void SetRangeRequestStart(int64_t start);
    void SetRangeRequestEnd(int64_t end);
    void SetRangeResponseStart(int64_t start);
    void SetRangeResponseEnd(int64_t end);
    void SetDnsCost(int64_t cost);
    void SetConnectCost(int64_t cost);
    void SetFirstDataCost(int64_t cost);
    void SetProductContext(string productContext);
    void SetTcpClimbingInfo(string tcpClimbingInfo);
    void SetOsErrno(long osErrno);

    // 拼装的Json信息
    virtual void SetCdnStatJson(string cdnStatJson) = 0;
    virtual string GetCdnStatJson();

  private:
    std::string GetStopReasonStr(DownloadStopReason reason);

  PUBLIC_FOR_UNIT_TEST:
    string cacheKey;                // 上层目前应该用不到
    int64_t cachedBytes{};            // deprecated fixme 这个字段不应该存在这里,因为下载开始的时候已经缓存了多少字节不重要，尤其是在一次下载有多段分次下载的场景下
    int32_t dataSourceType = kDataSourceTypeUnknown;

    int64_t totalBytes{};             // pb, event.totalFileSize
    int64_t downloadBytes{};          // pb, event.downloadedSize
    int64_t contentLength{};          // pb, event.expectedSize

    int64_t progressPosition{};       // 总进度的主要参考指标

    // onDownloadFinish的时候更新
    string currentUri;              // pb, event.url
    string host;                    // pb, event.host
    string ip;                      // pb, event.ip
    string kwaiSign;                // pb, event.kwaiSignature
    string xKsCache;                // pb, event.xKsCache
    string sessionUUID;             // pb, event.mExtJson.sessionUUID
    string downloadUUID;            // pb, event.mExtJson.downloadUUID
    DownloadStopReason stopReason = kDownloadStopReasonUnset;  // pb, event.mExtJson.stopReason

    int errorCode{};                  // pb, event.mExtJson.errorCode
    int transferConsumeMs{};          // pb, event.networkCost/totalCost

    int32_t httpRedirectCount{};
    string httpEffectiveUrl;
    int httpResponseCode{};
    long osErrno{};
    int64_t rangeRequestStart{};
    int64_t rangeRequestEnd{};
    int64_t rangeResponseStart{};
    int64_t rangeResponseEnd{};
    int64_t dnsCost{};
    int64_t connectCost{};
    int64_t firstDataCost{};
    string productContext;
#if defined(__ANDROID__)
    string tcpClimbingInfo;
#endif
    int32_t upstreamType {};
    string httpVersion;
    // string cdnStatJson;           // pb, event.cdnStatJson，这个字段值保存在Java/OC层，长度较大，也没必要存在native层
};

class AcCallbackInfoFactory {
  public:
    static AcCallbackInfo* CreateCallbackInfo();
};


class AwesomeCacheCallback {
  public:
    AwesomeCacheCallback() {
        // todo 日志先留着，后续这个类作为关键 alive_cnt 的监控类
        // LOG_DEBUG("AwesomeCacheCallback::AwesomeCacheCallback");
    }
    virtual ~AwesomeCacheCallback() {
        // LOG_DEBUG("AwesomeCacheCallback::~AwesomeCacheCallback");
    }

    virtual void onSessionProgress(std::shared_ptr<AcCallbackInfo> info) = 0;
    virtual void onDownloadFinish(std::shared_ptr<AcCallbackInfo> info) = 0;
};

}
}
