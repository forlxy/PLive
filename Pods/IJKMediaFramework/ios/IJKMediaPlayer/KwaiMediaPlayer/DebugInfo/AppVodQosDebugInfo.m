//
//  AppVodQosDebugInfo.m
//  IJKMediaFramework
//
//  Created by 帅龙成 on 02/04/2018.
//  Copyright © 2018 kuaishou. All rights reserved.
//

#import "AppVodQosDebugInfo.h"
#import <Foundation/Foundation.h>
#import <MJExtension/MJExtension.h>
#include "DebugInfoUtil.h"
#include "KwaiFFPlayerController.h"
#include "ijkplayer.h"
#include "vod_qos_debug_info.h"

@interface AppVodQosDebugInfo ()
@property(weak, nonatomic) KwaiFFPlayerController* kwaiPlayer;
@property(nonatomic) VodQosDebugInfo qosDebugInfo;
@end

@implementation AppVodQosDebugInfo

- (NSMutableDictionary*)mj_keyValues {
    return [self mj_keyValuesWithIgnoredKeys:@[ @"kwaiPlayer", @"qosDebugInfo" ]];
}

- (instancetype)init {
    @throw [NSException exceptionWithName:NSInternalInconsistencyException
                                   reason:@"-init is not a valid initializer "
                                          @"for the class AppVodQosDebugInfo"
                                 userInfo:nil];
    return nil;
}

- (instancetype)initWith:(KwaiFFPlayerController*)kwaiPlayer {
    self = [super init];
    if (self) {
        _kwaiPlayer = kwaiPlayer;
        VodQosDebugInfo_init(&_qosDebugInfo);
    }
    return self;
}

- (void)dealloc {
    VodQosDebugInfo_release(&_qosDebugInfo);
    NSLog(@"AppVodQosDebugInfo dealloc");
}

- (NSString*)prettyDownloadSpeedInfo {
    return _downloadSpeedInfo;
}

- (void)refresh {
    struct IjkMediaPlayer* mp = _kwaiPlayer.mediaPlayer;
    if (!mp) {
        return;
    }

    ijkmp_get_vod_qos_debug_info(mp, &_qosDebugInfo);

    _alivePlayerCnt = _qosDebugInfo.alivePlayerCnt;

    _configMaxBufDurMs = _qosDebugInfo.configMaxBufDurMs;

    _metaFps = _qosDebugInfo.metaFps;
    _metaWidth = _qosDebugInfo.metaWidth;
    _metaHeight = _qosDebugInfo.metaHeight;
    _metaDurationMs = _qosDebugInfo.metaDurationMs;
    _bitrate = _qosDebugInfo.bitrate;

    _fileName = safe_from_c_string(_qosDebugInfo.fileName);
    _metaComment = safe_from_c_string(_qosDebugInfo.metaComment);
    _metaVideoDecoderInfo = safe_from_c_string(_qosDebugInfo.metaVideoDecoderInfo);
    _metaAudioDecoderInfo = safe_from_c_string(_qosDebugInfo.metaAudioDecoderInfo);

    _currentPositionMs = _qosDebugInfo.currentPositionMs;

    _fullErrorMsg = safe_from_c_string(_qosDebugInfo.fullErrorMsg);

    _transcodeType = safe_from_c_string(_qosDebugInfo.transcodeType);
    _lastError = _qosDebugInfo.lastError;
    _currentState = safe_from_c_string(_qosDebugInfo.currentState);
    _serverIp = safe_from_c_string(_qosDebugInfo.serverIp);
    _host = safe_from_c_string(_qosDebugInfo.host);
    _domain = safe_from_c_string(_qosDebugInfo.domain);
    _firstScreenStepCostInfo = safe_from_c_string(_qosDebugInfo.firstScreenStepCostInfo);

    _totalCostFirstScreen = _qosDebugInfo.totalCostFirstScreen;
    _firstScreenWithoutAppCost = _qosDebugInfo.firstScreenWithoutAppCost;

    _playableDurationMs = _qosDebugInfo.playableDurationMs;
    _blockStatus = safe_from_c_string(_qosDebugInfo.blockStatus);
    _dropFrame = safe_from_c_string(_qosDebugInfo.dropFrame);
    _avQueueStatus = safe_from_c_string(_qosDebugInfo.avQueueStatus);

    _usePreLoad = _qosDebugInfo.usePreLoad;
    _preLoadFinish = _qosDebugInfo.preLoadFinish;
    _preLoadMs = _qosDebugInfo.preLoadMs;

    _startPlayBlockUsed = _qosDebugInfo.startPlayBlockUsed;
    _startPlayBlockStatus = safe_from_c_string(_qosDebugInfo.startPlayBlockStatus);

    _cacheEnabled = _qosDebugInfo.cacheEnabled;
    if (_cacheEnabled) {
        _cacheCurrentReadingUri = safe_from_c_string(_qosDebugInfo.cacheCurrentReadingUri);
        _cacheTotalBytes = _qosDebugInfo.cacheTotalBytes;
        _cacheDownloadedBytes = _qosDebugInfo.cacheDownloadedBytes;
        _cacheReopenCntBySeek = _qosDebugInfo.cacheReopenCntBySeek;

        _cacheStopReason = _qosDebugInfo.cacheStopReason;
        _cacheErrorCode = _qosDebugInfo.cacheErrorCode;
        _cacheIsReadingCachedFile = _qosDebugInfo.cacheIsReadingCachedFile;

        _downloadCurrentSpeedKbps = _qosDebugInfo.downloadCurrentSpeedKbps;
        _downloadSpeedInfo = safe_from_c_string(_qosDebugInfo.downloadSpeedInfo);
        // vod adaptive
        _vodAdaptiveInfo = safe_from_c_string(_qosDebugInfo.vodAdaptiveInfo);
    }

    _dccAlgConfigEnabled = _qosDebugInfo.dccAlgConfigEnabled;
    _dccAlgUsed = _qosDebugInfo.dccAlgUsed;
    _dccAlgCurrentSpeedMarkKbps = _qosDebugInfo.dccAlgCurrentSpeedMarkKbps;
    _dccAlgStatus = safe_from_c_string(_qosDebugInfo.dccAlgStatus);

    _autoTestTags = safe_from_c_string(_qosDebugInfo.autoTestTags);

    _customString = safe_from_c_string(_qosDebugInfo.customString);
    _cacheV2Info = safe_from_c_string(_qosDebugInfo.cacheV2Info);
}

@end
