//
//  KSCacheSessionDelegateUtil.m
//  IJKMediaFramework
//
//  Created by 帅龙成 on 2018/7/16.
//  Copyright © 2018 kuaishou. All rights reserved.
//

#import "KSCacheSessionDelegate.h"

@implementation KSCacheSessionDelegateUtil

NSString* const STOP_REASON_NAME[] = {
    @"kKSDownloadStopReasonUnknown",
    @"kKSDownloadStopReasonFinished",
    @"kKSDownloadStopReasonCancelled",
    @"kKSDownloadStopReasonFailed",
    @"kKSDownloadStopReasonTimeout",
    @"kKSDownloadStopReasonNoContentLength",
    @"kKSDownloadStopReasonContentLengthInvalid",
};

+ (NSString*)stopReasonToString:(KSDownloadStopReason)stopReason {
    return STOP_REASON_NAME[stopReason];
}

+ (BOOL)needRetryOnThisStopReason:(KSDownloadStopReason)stopReason {
    switch (stopReason) {
        case kKSDownloadStopReasonUnknown:
        case kKSDownloadStopReasonFailed:
        case kKSDownloadStopReasonTimeout:
        case kKSDownloadStopReasonNoContentLength:
        case kKSDownloadStopReasonContentLengthInvalid:
            return YES;
        default:
            return NO;
    }
    return NO;
}

@end
