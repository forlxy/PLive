//
//  AcCallbackInfo.h
//  IJKMediaFramework
//
//  Created by MarshallShuai on 2019/2/1.
//  Copyright © 2019 kuaishou. All rights reserved.
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSInteger, KSStopReason) {
    kKSStopReasonUnknown = 0,
    kKSStopReasonFinished,
    kKSStopReasonCancelled,
    kKSStopReasonFailed,
    kKSStopReasonTimeout,
    kKSStopReasonNoContentLength,
    kKSStopReasonContentLengthInvalid,
};

@interface KSAcCallbackInfo : NSObject

#pragma mark onSessionProgress 回调的时候可用
@property(nonatomic, copy) NSString* cacheKey;
@property(nonatomic) int64_t totalBytes;
@property(nonatomic) int64_t cachedBytes;
@property(nonatomic) int64_t downloadBytes;
@property(nonatomic) int64_t contentLength;
@property(nonatomic) int64_t progressPosition;

#pragma mark onDownloadFinish的时候更新
@property(nonatomic, copy) NSString* currentUri;
@property(nonatomic, copy) NSString* host;
@property(nonatomic, copy) NSString* ip;
@property(nonatomic, copy) NSString* kwaiSign;
@property(nonatomic, copy) NSString* xKsCache;
@property(nonatomic, copy) NSString* sessionUUID;
@property(nonatomic, copy) NSString* downloadUUID;
@property(nonatomic, copy) NSString* cdnStatJson;
@property(nonatomic) int httpResponseCode;
@property(nonatomic) KSStopReason stopReason;
@property(nonatomic) int errorCode;
@property(nonatomic) int transferConsumeMs;

/**
 * 表示是不是已经全部下载完成
 */
- (BOOL)isFullyCached;
@end

NS_ASSUME_NONNULL_END
