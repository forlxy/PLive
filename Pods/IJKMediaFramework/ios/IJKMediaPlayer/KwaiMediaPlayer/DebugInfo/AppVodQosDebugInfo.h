//
//  AppVodQosDebugInfo.h
//  IJKMediaPlayer
//
//  Created by 帅龙成 on 02/04/2018.
//  Copyright © 2018 kuaishou. All rights reserved.
//

#import <Foundation/Foundation.h>

@class KwaiFFPlayerController;

// 短视频debug info
@interface AppVodQosDebugInfo : NSObject

- (instancetype)initWith:(KwaiFFPlayerController*)kwaiPlayer NS_DESIGNATED_INITIALIZER;

- (void)refresh;

@property(nonatomic, readonly) int alivePlayerCnt;

// config
@property(nonatomic, readonly) int configMaxBufDurMs;

@property(nonatomic, readonly) float metaFps;
@property(nonatomic, readonly) int metaWidth;
@property(nonatomic, readonly) int metaHeight;
@property(nonatomic, readonly) long metaDurationMs;
@property(nonatomic, readonly) long bitrate;

@property(nonatomic, readonly) NSString* fileName;
@property(nonatomic, readonly) NSString* metaComment;
@property(nonatomic, readonly) NSString* metaVideoDecoderInfo;
@property(nonatomic, readonly) NSString* metaAudioDecoderInfo;
@property(nonatomic, readonly) NSString* firstScreenStepCostInfo;

@property(nonatomic, readonly) long currentPositionMs;

@property(nonatomic, readonly) int mediaType;

@property(nonatomic, readonly) NSString* fullErrorMsg;

@property(nonatomic, readonly) NSString* transcodeType;
@property(nonatomic, readonly) int lastError;
@property(nonatomic, readonly) NSString* currentState;
@property(nonatomic, readonly) NSString* serverIp;
@property(nonatomic, readonly) NSString* host;
@property(nonatomic, readonly) NSString* domain;

// 首屏相关，单位都是毫秒
@property(nonatomic, readonly) int64_t totalCostFirstScreen;
@property(nonatomic, readonly) int64_t firstScreenWithoutAppCost;

@property(nonatomic, readonly) int playableDurationMs;
@property(nonatomic, copy) NSString* blockStatus;
@property(nonatomic, copy) NSString* dropFrame;
@property(nonatomic, copy) NSString* avQueueStatus;

@property(nonatomic, readonly) int videoFrameDropCnt;
@property(nonatomic, readonly) int audioFrameDropCnt;

@property(nonatomic, readonly) int ffpLoopCnt;

@property(nonatomic, readonly) BOOL usePreLoad;
@property(nonatomic, readonly) BOOL preLoadFinish;
@property(nonatomic, readonly) int preLoadMs;

@property(nonatomic, readonly) BOOL startPlayBlockUsed;
@property(nonatomic, copy) NSString* startPlayBlockStatus;

// cache打开的时候才有
@property(nonatomic, readonly) BOOL cacheEnabled;
@property(nonatomic, copy) NSString* cacheCurrentReadingUri;
@property(nonatomic, readonly) long cacheTotalBytes;
@property(nonatomic, readonly) long cacheDownloadedBytes;
@property(nonatomic, readonly) int cacheReopenCntBySeek;

@property(nonatomic, readonly) int cacheStopReason;
@property(nonatomic, readonly) int cacheErrorCode;
@property(nonatomic, readonly) BOOL cacheIsReadingCachedFile;

@property(nonatomic, readonly) int downloadCurrentSpeedKbps;
@property(nonatomic, copy) NSString* downloadSpeedInfo;

@property(nonatomic, readonly) BOOL dccAlgConfigEnabled;
@property(nonatomic, readonly) BOOL dccAlgUsed;
@property(nonatomic, readonly) int dccAlgCurrentSpeedMarkKbps;
@property(nonatomic, copy) NSString* dccAlgStatus;

@property(nonatomic, readonly) NSString* prettyDownloadSpeedInfo;

@property(nonatomic, copy) NSString* customString;

@property(nonatomic, copy) NSString* cacheV2Info;

@property(nonatomic, copy) NSString* autoTestTags;

// vod adaptive
@property(nonatomic, readonly) NSString* vodAdaptiveInfo;

@end
