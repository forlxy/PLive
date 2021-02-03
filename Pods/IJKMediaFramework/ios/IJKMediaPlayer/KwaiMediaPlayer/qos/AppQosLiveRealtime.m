//
//  AppQosLiveRealtime.m
//  IJKMediaFramework
//
//  Created by 帅龙成 on 26/02/2018.
//  Copyright © 2018 kuaishou. All rights reserved.
//

#import "AppQosLiveRealtime.h"
#import <Foundation/Foundation.h>
#include <libavkwai/cJSON.h>
#import "AppQosUtil.h"
#import "cpu_resource.h"
#import "qos_live_adaptive_realtime.h"

#define JSON_NAME_MAX_LEN 64

#define DEFAULT_REALTIME_UPLOAD_INTERVAL_MS 10000

// Live manifest auto mode
#define LIVE_MANIFEST_AUTO -1
// Live manifest switch Qos Report
#define LIVE_MANIFEST_SWITCH_FLAG_AUTO 1
#define LIVE_MANIFEST_SWITCH_FLAG_MANUAL 2
#define LIVE_MANIFEST_SWITCH_FLAG_AUTOMANUAL 3

@interface AppQosLiveRealtime ()
@property(weak, nonatomic) KwaiFFPlayerController* kwaiPlayer;
@end

@implementation AppQosLiveRealtime {
    int64_t _lastRecordTimestamp;
    int64_t _tickStartTimestamp;  // 如果_qosUploadBlock耗时不高的话
                                  // _tickStartTimestamp等价于_lastRecordTimestamp

    int64_t _reportIntervalMs;
    KSYPlyQosStatBlock _qosUploadBlock;
    NSTimer* _timer;

    bool mIsFirstQosStatReport;
    bool mIsLastQosStatReport;
    bool mIsRunTimeCostReport;
}

- (instancetype)init {
    @throw [NSException exceptionWithName:NSInternalInconsistencyException
                                   reason:@"-init is not a valid initializer "
                                          @"for the class AppQosLiveRealtime"
                                 userInfo:nil];
    return nil;
}

- (instancetype)initWith:(KwaiFFPlayerController*)kwaiPlayer {
    self = [super init];
    if (self) {
        _kwaiPlayer = kwaiPlayer;
        mIsFirstQosStatReport = true;
        mIsLastQosStatReport = false;
        mIsRunTimeCostReport = false;
    }

    return self;
}

/**
 * NOTE:如果启用实时上报，必须在startPlay的时候startReport。因为_playStartTime依赖于此逻辑
 */
- (void)startReport:(KSYPlyQosStatBlock)qosStatBlock reportIntervalMs:(int64_t)intervalMs {
    @synchronized(self) {
        if (_timer != nil) {
            return;
        }

        _reportIntervalMs = intervalMs >= 0 ? intervalMs : DEFAULT_REALTIME_UPLOAD_INTERVAL_MS;
        _qosUploadBlock = qosStatBlock;

        _lastRecordTimestamp = _tickStartTimestamp = [AppQosUtil getTimestamp];
        _timer = [NSTimer scheduledTimerWithTimeInterval:_reportIntervalMs / 1000
                                                  target:self
                                                selector:@selector(uploadReport:)
                                                userInfo:nil
                                                 repeats:YES];
    }
}

- (void)stopReport {
    @synchronized(self) {
        if (_timer == nil) {
            return;
        }

        [_timer invalidate];

        mIsLastQosStatReport = true;

        // upload tail report
        [self uploadReport:nil];

        _qosUploadBlock = nil;
        _kwaiPlayer = nil;
        _timer = nil;
    }
}

- (void)uploadReport:(NSTimer*)t {
    if (!_kwaiPlayer) {
        return;
    }
    if (_qosUploadBlock) {
        _qosUploadBlock([self getQosJsonStr:_kwaiPlayer.mediaPlayer]);
    }
    _tickStartTimestamp = [AppQosUtil getTimestamp];
}

- (NSString*)getQosJsonStr:(IjkMediaPlayer*)mp {
    int64_t curRecordTimestamp = [AppQosUtil getTimestamp];
    int64_t tickDuration = curRecordTimestamp - _lastRecordTimestamp;

    int first = mIsFirstQosStatReport ? 1 : 0;
    int last = mIsLastQosStatReport ? 1 : 0;

    _lastRecordTimestamp = curRecordTimestamp;

    if (mIsFirstQosStatReport) {
        mIsFirstQosStatReport = false;
    }

    char* qos_str = ijkmp_get_qos_live_realtime_json_str(mp, first, last, _tickStartTimestamp,
                                                         tickDuration, _reportIntervalMs);

    NSString* jsonStr = [NSString stringWithUTF8String:qos_str];
    free(qos_str);
    return jsonStr;
}

@end
