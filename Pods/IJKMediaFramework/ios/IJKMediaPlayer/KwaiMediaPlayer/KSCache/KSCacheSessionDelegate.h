//
//  CacheSessionListener.h
//  IJKMediaPlayer
//
//  Created by 帅龙成 on 21/12/2017.
//  Copyright © 2017 kuaishou. All rights reserved.
//

#import <Foundation/Foundation.h>

// 修改这里的值必须同事修改 STOP_REASON_NAME[]的内容
typedef NS_ENUM(NSInteger, KSDownloadStopReason) {
    kKSDownloadStopReasonUnknown = 0,
    kKSDownloadStopReasonFinished,
    kKSDownloadStopReasonCancelled,
    kKSDownloadStopReasonFailed,
    kKSDownloadStopReasonTimeout,
    kKSDownloadStopReasonNoContentLength,
    kKSDownloadStopReasonContentLengthInvalid,
};

@interface KSCacheSessionDelegateUtil : NSObject
+ (BOOL)needRetryOnThisStopReason:(KSDownloadStopReason)stopReason;
+ (NSString*)stopReasonToString:(KSDownloadStopReason)stopReason;
@end

@protocol KSCacheSessionDelegate <NSObject>
- (void)onSessionStartWithKey:(const char*)key
                          pos:(uint64_t)startPos
                  cachedBytes:(int64_t)cachedBytes
                   totalBytes:(uint64_t)totalBytes;
- (void)onDownloadStarted:(uint64_t)position
                      url:(const char*)url
                     host:(const char*)host
                       ip:(const char*)ip
             responseCode:(int)responseCode
              connectTime:(uint64_t)timeMs;
- (void)onDownloadProgress:(uint64_t)downloadPosition totalBytes:(uint64_t)bytes;
- (void)onDownloadStopped:(KSDownloadStopReason)reason
            downloadBytes:(uint64_t)bytes
          transferConsume:(uint64_t)consumeMs
                 kwaiSign:(const char*)sign
                errorCode:(int)code
                 xKsCache:(const char*)x_ks_cache
              sessionUUID:(const char*)session_uuid
             downloadUUID:(const char*)download_uuid
                    extra:(const char*)extra_msg;
- (void)onSessionClosed:(int32_t)errorCode
            networkCost:(uint64_t)netWorkCost
              totalCost:(uint64_t)totalCost
        downloadedBytes:(uint64_t)bytes
                  stats:(const char*)stats
                 opened:(BOOL)hasOpened;

// APP should not concern the callbacks below
- (void)onDownloadPaused;
- (void)onDownloadResumed;
@end
