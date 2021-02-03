//
//  ios_ac_callback_info.h
//  IJKMediaFramework
//
//  Created by MarshallShuai on 2019/9/3.
//  Copyright © 2019 kuaishou. All rights reserved.
//

#pragma once

#import <Foundation/Foundation.h>
#import "utils/macro_util.h"
#import "awesome_cache_callback.h"
#import "KSCache/KSAcCallbackInfo.h"

NS_ASSUME_NONNULL_BEGIN

HODOR_NAMESPACE_START

class IOSAcCallbackInfo : public AcCallbackInfo {
  public:
    IOSAcCallbackInfo();
    ~IOSAcCallbackInfo();

    void SetCdnStatJson(string cdnStatJson);

    KSAcCallbackInfo* oc_object() {
        return info_oc_obj_;
    }

    void SetCacheKey(string key);

    void SetCachedBytes(int64_t bytes);
    void SetTotalBytes(int64_t totalBytes);
    void SetDownloadBytes(int64_t downloadBytes);
    void SetContentLength(int64_t contentLength);
    void SetProgressPosition(int64_t progressPosition);

    void SetCurrentUri(string uri);
    void SetHost(string host);
    void SetIp(string ip);
    void SetKwaiSign(string kwaiSign);
    void SetXKsCache(string xKsCache);
    void SetSessionUUID(string sessionUUID);
    void SetDownloadUUID(string downloadUUID);
    void SetHttpResponseCode(int httpResponseCode);
    void SetStopReason(DownloadStopReason stopReason);
    void SetErrorCode(int32_t errorCode);
    void SetTransferConsumeMs(int32_t costMs);

  private:
    KSAcCallbackInfo* info_oc_obj_;
};

HODOR_NAMESPACE_END

NS_ASSUME_NONNULL_END


