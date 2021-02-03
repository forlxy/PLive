//
//  PlayerTempDef.h
//  IJKMediaPlayer
//
//  Created by 帅龙成 on 12/02/2018.
//  Copyright © 2018 kuaishou. All rights reserved.
//

// 本文件等新的接口类集成到app中稳定后，既可以重新处理了，当前需要同事被新老接口类include
// -- kikyo 2018.2.27

#import <CoreMedia/CMSampleBuffer.h>
#import <MediaPlayer/MediaPlayer.h>

/**
 @abstract 视频数据回调
 */
typedef void (^KSYPlyVideoDataBlock)(CVPixelBufferRef pixelBuffer, int rotation);

/**
 @abstract 音频数据回调
 */
typedef void (^KSYPlyAudioDataBlock)(CMSampleBufferRef sampleBuffer);

/**
 @abstract Qos回调
 */
typedef void (^KSYPlyQosStatBlock)(NSString* jsonStat);

enum KwaiLogLevel {
    KWAI_LOG_UNKNOWN = 0,
    KWAI_LOG_DEFAULT,
    KWAI_LOG_VERBOSE,
    KWAI_LOG_DEBUG,
    KWAI_LOG_INFO,
    KWAI_LOG_WARN,
    KWAI_LOG_ERROR,
    KWAI_LOG_FATAL,
    KWAI_LOG_SILENT,
};

typedef void (*KwaiLogCallback)(enum KwaiLogLevel level, const char* msg);

typedef NS_ENUM(NSInteger, KwaiCacheMode) {
    /**
     * Cache sync mode, cache 模块会根据player的需要下载数据
     */
    KwaiCacheModeSync = 0,
    /**
     * Cache async mode, cache 模块会尽量快的下载数据
     */
    KwaiCacheModeAsync = 1,
    /**
     * Live normal mode, 直播flv类型
     */
    KwaiCacheModeLiveNormal = 2,
    /**
     * Live manifest mode, 直播多码率类型
     */
    KwaiCacheModeLiveAdaptive = 3,
    /**
     * segment manifest mode, hls
     */
    KwaiCacheModeSegment = 4,
    /**
     * segment manifest mode, hls
     */
    KwaiCacheModeAsyncV2 = 5,

};

typedef NS_ENUM(NSInteger, VodAdaptive) {
    /**
     * 网络类型: 不知道
     */
    NET_WORK_TYPE_UNKNOW = 0,
    /**
     * 网络类型: wifi
     */
    NET_WORK_TYPE_WIFI = 1,
    /**
     * 网络类型: 4G
     */
    NET_WORK_TYPE_FOUR_G = 2,
    /**
     * 网络类型: 3G
     */
    NET_WORK_TYPE_THREE_G = 3,
    /**
     * 网络类型: 2G
     */
    NET_WORK_TYPE_TWO_G = 4,
};

typedef NS_ENUM(NSInteger, KwaiCacheUpstreamType) {
    /**
     *  默认的http下载方式
     */
    KwaiCacheUpstreamDefaultHttp = 0,
    /**
     * 多task式的http下载方式
     */
    KwaiCacheUpstreamMultiHttp = 1,
    KwaiCacheUpstreamFFUrlHttp = 2,
    KwaiCacheUpstreamP2spHttp = 3,    // ios暂不支持
    KwaiCacheUpstreamCronetHttp = 4,  // ios暂不支持
};

typedef NS_ENUM(NSInteger, KwaiCacheBufferedDataSourceType) {
    /**
     *   Cache原来老版的的buffer层
     */
    KwaiCacheBufferedDataSource = 0,
    /**
     * Cache优化过的buffer层
     */
    KwaiCacheBufferedDataSourceOpt = 1,
};

// Posted when the playback state changes, either programatically or by the
// user.
MP_EXTERN NSString* const MPMoviePlayerPlaybackStateDidChangeNotification;

// Posted when movie playback ends or a user exits playback. when in loop mode,
// this notification will never be posted, see alse
// MPMoviePlayerPlayToEndNotification:
MP_EXTERN NSString* const MPMoviePlayerPlaybackDidFinishNotification;
// every time the video is play to end,this notification is posted
MP_EXTERN NSString* const MPMoviePlayerPlayToEndNotification;

MP_EXTERN NSString* const MPMoviePlayerPlaybackDidFinishReasonUserInfoKey;  // NSNumber
// (MPMovieFinishReason)

MP_EXTERN NSString* const KSYMPMoviePlayerPlaybackDidFinishErrorCodeUserInfoKey;  // NSNumber
// (KSYMPErrorCode).
// This key exists
// only if the value
// of key
// "MPMoviePlayerPlaybackDidFinishReasonUserInfoKey"
// is
// "MPMovieFinishReasonPlaybackError"

// Posted when the network load state changes.
MP_EXTERN NSString* const MPMoviePlayerLoadStateDidChangeNotification;

// 以下的常量已经deprecated，不应该再使用，上面的MPMoviePlayerLoadStateDidChangeNotification在客户端使用了，暂时保持支持
MP_EXTERN NSString* const MPMovieNaturalSizeAvailableNotification;

MP_EXTERN NSString* const MPMoviePlayerFirstVideoFrameRenderedNotification;

MP_EXTERN NSString* const MPMoviePlayerFirstAudioFrameRenderedNotification;

MP_EXTERN NSString* const MPMoviePlayerPlayerAccurateSeekCompleteNotification;
MP_EXTERN NSString* const MPMoviePlayerAccurateSeekCompleteCurPosNotification;

MP_EXTERN const NSString* const kKSYPLYFormat;

MP_EXTERN const NSString* const kKSYPLYHttpFirstDataTime;

MP_EXTERN const NSString* const kKSYPLYHttpAnalyzeDns;

MP_EXTERN const NSString* const kKSYPLYHttpConnectTime;
