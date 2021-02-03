//
//  AppLiveDebugInfo.m
//  IJKMediaFramework
//
//  Created by wangtao03 on 2018/4/26.
//  Copyright © 2018年 kuaishou. All rights reserved.
//

#import "AppLiveQosDebugInfo.h"
#import <Foundation/Foundation.h>
#import <MJExtension/MJExtension.h>
#import "KwaiFFPlayerController.h"
#import "ijkplayer.h"
#import "live_qos_debug_info.h"

@interface AppLiveQosDebugInfo ()
@property(weak, nonatomic) KwaiFFPlayerController* kwaiPlayer;
@property(nonatomic) LiveQosDebugInfo qosDebugInfo;
@end

@implementation AppLiveQosDebugInfo

- (NSMutableDictionary*)mj_keyValues {
    return [self mj_keyValuesWithIgnoredKeys:@[ @"kwaiPlayer", @"qosDebugInfo" ]];
}

- (instancetype)initWith:(KwaiFFPlayerController*)kwaiPlayer {
    self = [super init];
    if (self) {
        _kwaiPlayer = kwaiPlayer;
        LiveQosDebugInfo_init(&_qosDebugInfo);
    }
    return self;
}

- (void)dealloc {
    LiveQosDebugInfo_release(&_qosDebugInfo);
    NSLog(@"AppLiveQosDebugInfo dealloc");
}

- (void)refresh {
    struct IjkMediaPlayer* mp = _kwaiPlayer.mediaPlayer;
    if (!mp) {
        return;
    }
    ijkmp_get_live_qos_debug_info(mp, &_qosDebugInfo);

    _audioBufferByteLength = _qosDebugInfo.audioBufferByteLength;
    _videoBufferByteLength = _qosDebugInfo.videoBufferByteLength;
    _audioTotalDataSize = _qosDebugInfo.audioTotalDataSize;
    _videoTotalDataSize = _qosDebugInfo.videoTotalDataSize;
    _totalDataSize = _qosDebugInfo.totalDataBytes;
    _blockCnt = _qosDebugInfo.blockCnt;
    _blockDuration = _qosDebugInfo.blockDuration;
    _videoReadFramesPerSecond = _qosDebugInfo.videoReadFramesPerSecond;
    _videoDecodeFramesPerSecond = _qosDebugInfo.videoDecodeFramesPerSecond;
    _videoDisplayFramesPerSecond = _qosDebugInfo.videoDisplayFramesPerSecond;

    _firstFrameReceived = _qosDebugInfo.stepCostFirstFrameReceived;
    _audioBufferTimeLength = _qosDebugInfo.audioBufferTimeLength;
    _videoBufferTimeLength = _qosDebugInfo.videoBufferTimeLength;

    _isLiveManifest = _qosDebugInfo.isLiveManifest;
    _sourceDeviceType = _qosDebugInfo.sourceDeviceType;
    _speedupThresholdMs = _qosDebugInfo.speedupThresholdMs;

    if (_isLiveManifest) {
        _kflvPlayingBitrate = _qosDebugInfo.kflvPlayingBitrate;
        _kflvBandwidthCurrent = _qosDebugInfo.kflvBandwidthCurrent;
        _kflvBandwidthFragment = _qosDebugInfo.kflvBandwidthFragment;
        _kflvCurrentBufferMs = _qosDebugInfo.kflvCurrentBufferMs;
        _kflvEstimateBufferMs = _qosDebugInfo.kflvEstimateBufferMs;
        _kflvPredictedBufferMs = _qosDebugInfo.kflvPredictedBufferMs;
        _kflvSpeedupThresholdMs = _qosDebugInfo.kflvSpeedupThresholdMs;
    }

    // 首屏完成后就不再重复获取
    if (_firstScreenTimeTotal <= 0) {
        _firstScreenTimeTotal = (int)_qosDebugInfo.totalCostFirstScreen;
        _firstScreenTimeWaitForPlay = (int)_qosDebugInfo.stepCostWaitForPlay;
        _firstScreenTimeInputOpen = (int)_qosDebugInfo.stepCostOpenInput;
        _firstScreenTimeDnsAnalyze = (int)_qosDebugInfo.costDnsAnalyze;
        _firstScreenTimeHttpConnect = (int)_qosDebugInfo.costHttpConnect;
        _firstScreenTimeStreamFind = (int)_qosDebugInfo.stepCostFindStreamInfo;
        _firstScreenTimeCodecOpen = (int)_qosDebugInfo.stepCostOpenDecoder;
        _firstScreenTimePktReceive = (int)_qosDebugInfo.costFirstHttpData;
        _firstScreenTimePreDecode = (int)_qosDebugInfo.stepCostPreFirstFrameDecode;
        _firstScreenTimeDecode = (int)_qosDebugInfo.stepCostAfterFirstFrameDecode;
        _firstScreenTimeRender = (int)_qosDebugInfo.stepCostFirstFrameRender;
        _firstScreenTimeDroppedDuration = _qosDebugInfo.droppedDurationBefFirstScreen;
    }

    _totalDroppedDuration = _qosDebugInfo.droppedDurationTotal;
    _audioDelay = _qosDebugInfo.audioDelay;
    _videoDelayRecv = _qosDebugInfo.videoDelayRecv;
    _videoDelayBefDec = _qosDebugInfo.videoDelayBefDec;
    _videoDelayAftDec = _qosDebugInfo.videoDelayAftDec;
    _videoDelayRender = _qosDebugInfo.videoDelayRender;

    _playUrl =
        _qosDebugInfo.playUrl ? [NSString stringWithUTF8String:_qosDebugInfo.playUrl] : @"N/A";
    _firstScreenStepCostInfo =
        [NSString stringWithUTF8String:_qosDebugInfo.firstScreenStepCostInfo];
    _hostInfo =
        _qosDebugInfo.hostInfo ? [NSString stringWithUTF8String:_qosDebugInfo.hostInfo] : @"N/A";
    _vencInit =
        _qosDebugInfo.vencInit ? [NSString stringWithUTF8String:_qosDebugInfo.vencInit] : @"N/A";
    _aencInit =
        _qosDebugInfo.aencInit ? [NSString stringWithUTF8String:_qosDebugInfo.aencInit] : @"N/A";
    _host = _qosDebugInfo.host ? [NSString stringWithUTF8String:_qosDebugInfo.host] : @"N/A";
    _comment = _qosDebugInfo.metaComment ? [NSString stringWithUTF8String:_qosDebugInfo.metaComment]
                                         : @"N/A";
    _videoDecoder = _qosDebugInfo.metaVideoDecoderInfo
                        ? [NSString stringWithUTF8String:_qosDebugInfo.metaVideoDecoderInfo]
                        : @"N/A";
    _audioDecoder = _qosDebugInfo.metaAudioDecoderInfo
                        ? [NSString stringWithUTF8String:_qosDebugInfo.metaAudioDecoderInfo]
                        : @"N/A";
    _vencDynamic = _qosDebugInfo.metaVideoDecoderDynamicInfo
                       ? [NSString stringWithUTF8String:_qosDebugInfo.metaVideoDecoderDynamicInfo]
                       : @"N/A";

    _serverAddress = _kwaiPlayer.serverAddress;
    _naturalSizeString = NSStringFromCGSize(_kwaiPlayer.naturalSize);
    _readSize = _kwaiPlayer.readSize;
}

@end
