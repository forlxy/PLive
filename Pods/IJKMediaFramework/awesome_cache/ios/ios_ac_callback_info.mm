//
//  ios_ac_callback_info.m
//  IJKMediaFramework
//
//  Created by MarshallShuai on 2019/9/3.
//  Copyright © 2019 kuaishou. All rights reserved.
//

#include "include/awesome_cache_callback.h"
#import "ios_ac_callback_info.h"

HODOR_NAMESPACE_START

IOSAcCallbackInfo::IOSAcCallbackInfo() {
    this->info_oc_obj_ = [[KSAcCallbackInfo alloc] init];
}
IOSAcCallbackInfo::~IOSAcCallbackInfo() {
}

void IOSAcCallbackInfo::SetCdnStatJson(string cdnStatJson){
    info_oc_obj_.cdnStatJson = [NSString stringWithUTF8String:cdnStatJson.c_str()];
}

void IOSAcCallbackInfo::SetCacheKey(string key) {
    AcCallbackInfo::SetCacheKey(key);
    info_oc_obj_.cacheKey = [NSString stringWithUTF8String:key.c_str()];
};

void IOSAcCallbackInfo::SetCachedBytes(int64_t bytes) {
    AcCallbackInfo::SetCachedBytes(bytes);
    info_oc_obj_.cachedBytes = bytes;
};

void IOSAcCallbackInfo::SetTotalBytes(int64_t totalBytes) {
    AcCallbackInfo::SetTotalBytes(totalBytes);
    info_oc_obj_.totalBytes = totalBytes;
};

void IOSAcCallbackInfo::SetDownloadBytes(int64_t downloadBytes) {
    AcCallbackInfo::SetDownloadBytes(downloadBytes);
    info_oc_obj_.downloadBytes = downloadBytes;
}

void IOSAcCallbackInfo::SetProgressPosition(int64_t progressPosition) {
    AcCallbackInfo::SetProgressPosition(progressPosition);
    info_oc_obj_.progressPosition = progressPosition;
}

void IOSAcCallbackInfo::SetContentLength(int64_t contentLength) {
    AcCallbackInfo::SetContentLength(contentLength);
    info_oc_obj_.contentLength = contentLength;
}

void IOSAcCallbackInfo::SetCurrentUri(string uri) {
    AcCallbackInfo::SetCurrentUri(uri);
    info_oc_obj_.currentUri = [NSString stringWithUTF8String:uri.c_str()];
};

void IOSAcCallbackInfo::SetHost(string host) {
    AcCallbackInfo::SetHost(host);
    info_oc_obj_.host = [NSString stringWithUTF8String:host.c_str()];
};

void IOSAcCallbackInfo::SetIp(string ip) {
    AcCallbackInfo::SetIp(ip);
    info_oc_obj_.ip = [NSString stringWithUTF8String:ip.c_str()];
};

void IOSAcCallbackInfo::SetKwaiSign(string kwaiSign) {
    AcCallbackInfo::SetKwaiSign(kwaiSign);
    info_oc_obj_.kwaiSign = [NSString stringWithUTF8String:kwaiSign.c_str()];
};

void IOSAcCallbackInfo::SetXKsCache(string xKsCache) {
    AcCallbackInfo::SetXKsCache(xKsCache);
    info_oc_obj_.xKsCache = [NSString stringWithUTF8String:xKsCache.c_str()];
};

void IOSAcCallbackInfo::SetSessionUUID(string sessionUUID) {
    AcCallbackInfo::SetSessionUUID(sessionUUID);
    info_oc_obj_.sessionUUID = [NSString stringWithUTF8String:sessionUUID.c_str()];
};

void IOSAcCallbackInfo::SetDownloadUUID(string downloadUUID) {
    AcCallbackInfo::SetDownloadUUID(downloadUUID);
    info_oc_obj_.downloadUUID = [NSString stringWithUTF8String:downloadUUID.c_str()];
};

void IOSAcCallbackInfo::SetHttpResponseCode(int httpResponseCode) {
    AcCallbackInfo::SetHttpResponseCode(httpResponseCode);
    info_oc_obj_.httpResponseCode = httpResponseCode;
};

void IOSAcCallbackInfo::SetStopReason(DownloadStopReason stopReason) {
    AcCallbackInfo::SetStopReason(stopReason);
    info_oc_obj_.stopReason = (KSStopReason)stopReason;
};

void IOSAcCallbackInfo::SetErrorCode(int32_t errorCode) {
    AcCallbackInfo::SetErrorCode(errorCode);
    info_oc_obj_.errorCode = errorCode;
};

void IOSAcCallbackInfo::SetTransferConsumeMs(int32_t costMs) {
    AcCallbackInfo::SetTransferConsumeMs(costMs);
    info_oc_obj_.transferConsumeMs = costMs;
}


AcCallbackInfo* kuaishou::cache::AcCallbackInfoFactory::CreateCallbackInfo() {
    return new IOSAcCallbackInfo();
}

HODOR_NAMESPACE_END
