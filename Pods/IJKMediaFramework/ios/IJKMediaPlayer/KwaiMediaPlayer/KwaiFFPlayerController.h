//
//  KwaiMediaPlayer.h
//  IJKMediaPlayer
//
//  Created by 帅龙成 on 12/02/2018.
//  Copyright © 2018 kuaishou. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "IJKFFMoviePlayerController.h"
#import "KSAwesomeCacheCallbackDelegate.h"
#import "KSCacheSessionDelegate.h"
#import "KwaiMediaPlayback.h"

#import "AppLiveQosDebugInfo.h"
#import "AppVodQosDebugInfo.h"
#import "KSYQosInfo.h"  // to be deprecated, replaced by AppVodQosDebugInfo
#import "KwaiPlayerDebugInfo.h"
#import "KwaiPlayerLiveBuildParams.h"
#import "KwaiPlayerVodBuildParams.h"
#import "KwaiVodManifestConfig.h"
#import "PlayerTempDef.h"
#import "ProductContext.h"

typedef NS_ENUM(NSInteger, KwaiPlayerPlaybackState) {
    KwaiPlayerPlaybackStateInited,
    KwaiPlayerPlaybackStatePlaying,
    KwaiPlayerPlaybackStatePaused,
    KwaiPlayerPlaybackStateStopped
};

// 直播多码率手动自动切换模式
// 1.LIVE_MANIFEST_AUTO: 播放器做自适应切换; 其他的值播放器固定码率播放
// 2.LIVE_MANIFEST_REP_INDEX_1:指manifest中第一个url，该url对应的id不一定是1
// 3.LIVE_MANIFEST_REP_INDEX_2/3/4:以此类推
typedef NS_ENUM(NSInteger, KwaiLiveManifestSwitchMode) {
    LIVE_MANIFEST_AUTO = -1,
    LIVE_MANIFEST_REP_INDEX_1 = 0,
    LIVE_MANIFEST_REP_INDEX_2 = 1,
    LIVE_MANIFEST_REP_INDEX_3 = 2,
    LIVE_MANIFEST_REP_INDEX_4 = 3,
    LIVE_MANIFEST_REP_INDEX_5 = 4,
    LIVE_MANIFEST_REP_INDEX_6 = 5,
};

@interface KwaiFFPlayerController : IJKFFMoviePlayerController <KwaiMediaPlayback>

+ (NSString*)getVersion;
+ (NSString*)getVersionExt;
// 调试生命周期，内存泄漏用途
+ (int)getAlivePlayerCount;
+ (void)setKwaiLogCallBack:(KwaiLogCallback)cb;

#pragma mark protocal IJKMediaPlayback
- (void)play;

#pragma mark protocal KwaiMediaPlayback
- (void)forExample_getQos;

- (KwaiPlayerPlaybackState)kwaiPlayBackState;

// 此接口deprecated，请使用有 带withBuildParams的initVodPlayerWithURL接口
- (instancetype)initWithContentURL:(NSURL*)url
                       withHeaders:(NSDictionary*)headers
                   withCacheEnable:(BOOL)enableCache __deprecated;

#pragma mark vod builder api（deprecated)
// 此接口deprecated，请使用有 带withBuildParams的initVodPlayerWithURL接口
- (instancetype)initVodPlayerWithURL:(NSURL*)url
                         withHeaders:(NSDictionary*)header
                     withCacheEnable:(BOOL)enableCache;

// 此接口deprecated，请使用有 带withBuildParams的initVodPlayerWithURL接口
- (instancetype)initVodPlayerWithURL:(NSURL*)url
                         withHeaders:(NSDictionary*)header
                     withCacheEnable:(BOOL)enableCache
                        withMixAudio:(BOOL)enableMixAudio;

// 此接口deprecated，请使用有 带withBuildParams的initVodPlayerWithManifest接口
- (instancetype)initVodPlayerWithManifest:(NSString*)manifest
                              withHeaders:(NSDictionary*)header
                          withCacheEnable:(BOOL)enableCache
                        vodManifestConfig:(KwaiVodManifestConfig*)vodManifestConfig;

/**
 *
 * 此接口deprecated，请使用有 带withBuildParams的initVodPlayerWithIndexContent接口
 * Sets the data source when the input data is index content.
 * 主要主站使用
 *
 * @param requestUrl     the Uri to be reported
 * @param prefixPath     the prefix path of ts segment
 * @param content        ts segment playlist 传入的是标准的m3u8内容
 */
- (instancetype)initVodPlayerWithIndexContent:(NSString*)content
                               withRequestUrl:(NSURL*)requestUrl
                               withPrefixPath:(NSString*)prefixPath
                                  withHeaders:(NSDictionary*)header
                              withCacheEnable:(BOOL)enableCache;

/**
 * 此接口deprecated，请使用有 带withBuildParams的initVodPlayerWithIndexContent接口
 * Sets the data source when the input data is index content.
 * Hls私有协议，主要A站使用
 *
 * @param manifest       hls manifest
 * 私有协议，私有协议wiki：https://wiki.corp.kuaishou.com/pages/viewpage.action?pageId=137982547
 * @param requestUrl     the Uri to be reported
 * @param header         http request header
 * @param enableCache    enableCache
 */
- (instancetype)initVodPlayerWithHlsManifest:(NSString*)manifest
                              withRequestUrl:(NSURL*)requestUrl
                                 withHeaders:(NSDictionary*)header
                             withCacheEnable:(BOOL)enableCache;

#pragma mark vod builder api（new)
/**
 * 普通单url视频接口
 *
 * @param buildParams    所有和dataSource地址无关的buildParams
 */
- (instancetype)initVodPlayerWithURL:(NSURL*)url
                         withHeaders:(NSDictionary*)header
                     withBuildParams:(KwaiPlayerVodBuildParams*)buildParams;

/**
 * 短视频多码率接口
 *
 * @param buildParams    所有和dataSource地址无关的buildParams
 */
- (instancetype)initVodPlayerWithManifest:(NSString*)manifest
                              withHeaders:(NSDictionary*)header
                        vodManifestConfig:(KwaiVodManifestConfig*)vodManifestConfig
                          withBuildParams:(KwaiPlayerVodBuildParams*)buildParams;

/**
 *
 * Sets the data source when the input data is index content.
 * 主要主站使用
 *
 * @param requestUrl     the Uri to be reported
 * @param prefixPath     the prefix path of ts segment
 * @param content        ts segment playlist 传入的是标准的m3u8内容
 * @param buildParams    所有和dataSource地址无关的buildParams
 */
- (instancetype)initVodPlayerWithIndexContent:(NSString*)content
                               withRequestUrl:(NSURL*)requestUrl
                               withPrefixPath:(NSString*)prefixPath
                                  withHeaders:(NSDictionary*)header
                              withBuildParams:(KwaiPlayerVodBuildParams*)buildParams;

/**
 * Sets the data source when the input data is index content.
 * Hls私有协议，主要A站使用
 *
 * @param manifest       hls manifest
 * ，私有协议，私有协议wiki：https://wiki.corp.kuaishou.com/pages/viewpage.action?pageId=137982547
 * @param requestUrl     the Uri to be reported
 * @param header         http request header
 * @param buildParams    所有和dataSource地址无关的buildParams
 */
- (instancetype)initVodPlayerWithHlsManifest:(NSString*)manifest
                              withRequestUrl:(NSURL*)requestUrl
                                 withHeaders:(NSDictionary*)header
                             withBuildParams:(KwaiPlayerVodBuildParams*)buildParams;

#pragma mark live builder api(deprecated)
/*
 * 此接口deprecated，请使用有 带withBuildParams的initLivePlayerWithURL接口
 */
- (instancetype)initLivePlayerWithURL:(NSURL*)url withHeaders:(NSDictionary*)header;
/*
 * 此接口deprecated，请使用有 带withBuildParams的initLivePlayerWithManifest接口
 */
- (instancetype)initLivePlayerWithManifest:(NSString*)manifest
                               withHeaders:(NSDictionary*)header
                                withConfig:(NSString*)adaptConfig;

#pragma mark live builder api(new)
- (instancetype)initLivePlayerWithURL:(NSURL*)url
                          withHeaders:(NSDictionary*)header
                      withBuildParams:(KwaiPlayerLiveBuildParams*)buildParams;
- (instancetype)initLivePlayerWithManifest:(NSString*)manifest
                               withHeaders:(NSDictionary*)header
                                withConfig:(NSString*)adaptConfig
                           withBuildParams:(KwaiPlayerLiveBuildParams*)buildParams;

- (void)setPreferBandwidth:(int)bandWidth;

#pragma mark API delegate
- (void)setDataReadTimeout:(int)timeout;
- (void)setNetWorkConnectionTimeout:(int)timeout;  // app未使用到
- (void)setVolume:(float)leftVolume rigthVolume:(float)rightVolume;
// @deprecated, will be replaced by setLiveSpeedChangeConfigJson
- (void)setConfigJson:(NSString*)configJson;

- (void)setLiveSpeedChangeConfigJson:(NSString*)configJson;
- (void)setLiveLowDelayConfigJson:(NSString*)configJson;

- (void)updateCurrentWallClock:(int64_t)epochMs;
- (void)updateCurrentMaxWallClockOffset:(int64_t)epochMs;

@property(nonatomic, readonly) BOOL isLive;
@property(nonatomic) BOOL shouldMute;
@property(nonatomic) BOOL shouldLoop;

// enable loop for io error
@property(nonatomic) BOOL shouldLoopOnError;
// for A1,解码出错是否自动退出播放
@property(nonatomic) BOOL shouldExitOnDecError;

// @deprecated, will be replaced by videoDecodeType
@property(nonatomic, readonly) BOOL isHWCodecEnabled;

// 统计软硬解码比例使用
//  0: FFP_PROPV_DECODER_UNKNOWN, 表示解码器未初始化完成
//  1: FFP_PROPV_DECODER_AVCODEC, 表示使用软解码
//  2: FFP_PROPV_DECODER_MEDIACODEC, 表示使用MC硬解码
//  3: FFP_PROPV_DECODER_VIDEOTOOLBOX, 表示使用VTB硬解码
@property(nonatomic, readonly) int videoDecodeType;

@property(nonatomic, readonly) float probeFps;

#pragma mark QosLiveRealtime
@property(nonatomic, copy) KSYPlyQosStatBlock qosStatBlock;
@property(nonatomic, assign) BOOL enableQosLiveRealtime;
@property(nonatomic, assign) int64_t qosLiveRealtimeReportIntervalMs;

#pragma mark QosLiveAdaptiveRealtime
@property(nonatomic, copy) KSYPlyQosStatBlock qosLiveAdaptiveStatBlock;
@property(nonatomic, assign) BOOL enableQosLiveAdaptiveRealtime;
// bandwidth_estimate, bitrate_playing and bitrate_downloading will be
// reported depending on server's config for debug purpose, the config
// will be registered in "enableQosLiveAdaptiveAdditionalRealtimereport as
// default"
@property(nonatomic, assign) BOOL enableQosLiveAdaptiveAdditionalRealtime;
@property(nonatomic, assign) int64_t qosLiveAdaptiveRealtimeReportIntervalMs;

#pragma mark qos report
@property(nonatomic, readonly) KwaiPlayerDebugInfo* kwaiPlayerDebugInfo;
@property(nonatomic, readonly) AppVodQosDebugInfo* appVodQosDebugInfo;
@property(nonatomic, readonly) AppLiveQosDebugInfo* appLiveQosDebugInfo;
@property(nonatomic, strong) KSYQosInfo* qosInfo;
// statJson目前是客户端的live的汇总上报
// @deprecated, will be replaced by liveStatJson
@property(nonatomic, readonly) NSString* statJson;
@property(nonatomic, readonly) NSString* liveStatJson;
@property(nonatomic, readonly) NSString* videoStatJson;
@property(nonatomic, readonly) NSString* briefVideoStatJson;

#pragma mark 向前兼容，最终等新版上报稳定了会删掉
@property(nonatomic, readonly) int bufferEmptyCount;
@property(nonatomic, readonly) int64_t bufferEmptyDuration;

@property(nonatomic, readonly) NSInteger bufferEmptyCountOld;
@property(nonatomic, readonly) NSTimeInterval bufferEmptyDurationOld;

#pragma mark PROP getter/setter
@property(nonatomic, readonly) CGFloat fpsAtDecoder;
@property(nonatomic, readonly) double readSize;
@property(nonatomic, readonly) int64_t dtsDuration;
@property NSTimeInterval bufferTimeMax;

@property(nonatomic, readonly) NSString* serverAddress;
@property(nonatomic, readonly) float averageDisplayFps;

// 直播多码率当前播放的url
@property(nonatomic, readonly) NSString* liveManifestCurUrl;

// VOD播放是否disable Audio，For A1
- (void)setDisableVodAudio:(BOOL)disableVodAudio;

- (void)seekAtStart:(int)msec;
/**
 *是否支持seek前向偏差，默认支持，该设置只对非精准seek有效,目前只有hls支持
 *支持该功能，seek将跳到距离seekpos最近的一个关键帧（可能向前也可能向后），缩小非精准seek误差
 */
- (void)setEnableSeekForwardOffset:(BOOL)enableSeekForwardOffset;

// max buffer duration should be pre-read, 当前仅VOD使用
- (void)setMaxBufferDuration:(int)msec;

- (int64_t)getCurAbsoluteTime;

/**
 * water_mark buffer strategy相关接口
 * 当前仅直播业务使用了相关接口
 */
// buffer-strategy: water_mark buffer策略
// 1:使用老策略，2:使用新策略
// 当前下发 2:新策略
- (void)setBufferStrategy:(int)value;
// first-high-water-mark-ms: water_mark
// buffer time的初始值，当前下发500ms
- (void)setFirstBufferTime:(int)value;
// next-high-water-mark-ms: water_mark
// buffer time的最小值，默认为200ms
// 当前未使用
- (void)setMinBufferTime:(int)value;
// last-high-water-mark-ms: water_mark
// buffer time的最大值，默认为4000ms
- (void)setMaxBufferTime:(int)value;
// buffer-increment-step: water_mark
// buffer time动态增长的步长
// 策略1默认值为100ms，策略2默认值为500ms
// 当前下发500ms
- (void)setBufferIncrementStep:(int)value;
// buffer-smooth-time: water_mark
// smooth buffer time, 默认20000ms
// 当前下发20000ms
- (void)setBufferSmoothTime:(int)value;

// 设置APP 播放起始时间，用于数据分析，关联播放行为，针对重试场景多次创建播放器，startTime应该一致
- (void)setStartTime:(long)startTime;

// 默认关闭，只供点播使用，直播无效
- (void)preLoad:(int64_t)preLoadDurationMs;
// 比如想在1秒和3秒之间循环播放则 [player enableAbLoop:1000 b:3000],
// endMs:对纯音频多媒体无效 设置一些无效值，abloop则不会生效
- (void)enableAbLoop:(int64_t)start endMs:(int64_t)endMs;

// 针对春节项目，在bufferStart 和 bufferEnd 之间出现卡顿或者报错直接跳到loopBegin循环播放,
// bufferEndPercent:-1 为文件结尾, bufferStartPercent 和 bufferEndPercent
// 分别为起止位置占视频总长度百分比
- (void)enableLoopWithBufferStartPercent:(int)bufferStartPercent
                               bufferEnd:(int)bufferEndPercent
                               loopBegin:(int64_t)loopBegin;

// 起播buffer策略
- (void)setStartPlayBlockBufferMs:(int)bufferMs bufferMaxCostMs:(int)bufferMaxCostMs;

// 检测启播buffer是否缓存好
- (BOOL)checkCanStartPlay;

// 获取视频文件下载百分比[0-100]
- (int)getDownloadedPercent;

// 设置针对AAC是否使用libfdk
- (void)setLibfdkForAac:(BOOL)enableLibfdkForAac;

// 淡入效果结束时间，为0时关闭淡入效果，单位毫秒，最大值9999毫秒
- (void)setFadeinEndTimeMs:(int)fadeinEndTimeMs;

// 设置是否打开异步初始化stream component
- (void)setAsyncStreamOpenMode:(BOOL)enableAsync;

// 打开H264,H265的硬件解码
- (void)setH264HWCodec:(BOOL)enableH264HWCodec;
- (void)setH265HWCodec:(BOOL)enableH265HWCodec;

// 设置直播多码率自动／手动切换模式
- (void)setLiveManifestSwitchMode:(KwaiLiveManifestSwitchMode)mode;

// 客户端统计部分qosJson后，调用该接口设置到实时上报
- (void)setAppQosStatJson:(NSString*)qosJson;

// videotoolbox 解码自动旋转
- (void)setVtbAutoRotate:(BOOL)enableVtbRotate;

// 带宽控制
- (void)setDccAlgorithm:(BOOL)enable;
- (void)setDccAlgMBTh_10:(int)th_10;
- (void)setDccAlgPreReadMs:(int)preReadMs;

// ProductContext，客户端相关设置，用于数据分析，如重试次数，播放场景（短视频/长视频/小课堂等）
- (void)setProductContext:(ProductContext*)productContext;

// isEncryptSource 为true表示是春季活动的加密视频源，底层会做解密播放
- (void)setIsSF2020EncryptSource:(BOOL)isEncryptSource aesKey:(NSString*)aesKey;

// Async Cache
- (void)setAsyncCacheByteRangeSize:(int)byteRangeSize;
- (void)setAsyncCacheFirstByteRangeSize:(int)firstByteRangeSize;

// vod adaptive
//短视频多码率获取url
- (NSString*)getVodAdaptiveUrl;

//短视频多码率获取cache key
- (NSString*)getVodAdaptiveCacheKey;

// 短视频多码率获取host name
- (NSString*)getVodAdaptiveHostName;

// 短视频多码率获取视频ID
- (int32_t)getVodAdaptiveRepID;

#pragma mark custom API
- (void)applyOptions:(IJKFFOptions*)options;

//表示本次播放时第几次 network 失败后的重试了，目前需要app来告诉Player
@property(nonatomic) int tag1;

#pragma mark AwesomeCache
@property(nonatomic, strong) NSString* cacheKey;
@property(nonatomic) KwaiCacheMode cacheMode;
@property(nonatomic) KwaiCacheUpstreamType cacheUpstreamType;
@property(nonatomic) KwaiCacheBufferedDataSourceType cacheBufferedDataSourceType;
@property(nonatomic) BOOL cacheIgnoreOnError;
@property(nonatomic) int cacheBufferedDataSizeKb;
@property(nonatomic) int cacheBufferedSeekThresholdKb;
@property(nonatomic) int cacheCurlBufferSizeKb;
@property(nonatomic) int cacheDownloadConnectTimeoutMs;
@property(nonatomic) int cacheDownloadReadTimeoutMs;
@property(nonatomic) int cacheHttpConnectRetry;
// cache下载模块的最大下载速度, 单位kbps，1k=1024, 默认为-1，不限速
@property(nonatomic) int maxSpeedKbps;

@property(nonatomic, strong) id<KSCacheSessionDelegate> cacheSessionDelegate;
@property(nonatomic, strong) id<KSAwesomeCacheCallbackDelegate> awesomeCacheCallbackDelegate;

// is_last_try表示本次播放是否为最后一次重试，用户上报统计
// 一次播放行为可能会有多次重试，重复创建播放器，只有最后一次为YES
// 在调用videoStatJson之前调用
// is_last_try  YES:最后一次重试 NO:本次播放为重试
- (void)setLastTryFlag:(BOOL)is_last_try;

- (void)setEnableAudioGain:(BOOL)enableAudioGain;

// 是否开启开播时将音频pts对齐视频，默认值为true
- (void)setUseAlignedPts:(BOOL)enableAlignedPts;

- (void)setEnableModifyBlock:(BOOL)enableModifyBlock;
// 长视频是否打开缓存，如hls 是否缓存ts分片
- (void)setEnableSegmentCache:(BOOL)enableSegmentCache;
// ===== kwai code end =====
@end
