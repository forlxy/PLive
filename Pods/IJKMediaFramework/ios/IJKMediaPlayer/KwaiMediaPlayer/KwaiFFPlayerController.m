//
//  KwaiMediaPlayer.m
//  IJKMediaFramework
//
//  Created by 帅龙成 on 12/02/2018.
//  Copyright © 2018 kuaishou. All rights reserved.
//

#import "KwaiFFPlayerController.h"

#import <CommonCrypto/CommonDigest.h>
#import <Foundation/Foundation.h>
#import "IJKFFMoviePlayerController.h"
#include "KSCache/KSCacheSessionListenerBridge.h"

#include "ff_fferror.h"
#include "ff_ffmsg.h"
#import "ijkplayer.h"

#include <libavkwai/cJSON.h>
#import "ijkkwai/kwai_player_version_gennerated.h"
// cpu usage
#include <mach/mach.h>
#include <mach/processor_info.h>
#include <stdio.h>
#include <sys/time.h>
#import "AppQosLiveAdaptiveRealtime.h"
#import "AppQosLiveRealtime.h"
#import "KSAwesomeCacheCallbackDelegate.h"
#import "KwaiPlayerDebugInfo.h"
#import "ProductContext.h"
#import "cache_session_listener_c.h"
#include "cpu_resource.h"
#import "ios_awesome_cache_callback_c.h"

#define DEFAULT_MONITOR_INTERVAL (1.0)  // 单位: s

static KwaiLogCallback kwai_log_callback = NULL;

static NSString* sAlivePlayerCountLock = @"LOCK";
static int sAlivePlayerCount = 0;

@interface KwaiFFPlayerController ()
@property(nonatomic) KwaiPlayerPlaybackState kwaiPlaybackState;
@property(nonatomic) AppQosLiveRealtime* appQosLiveRealtime;
@property(nonatomic) AppQosLiveAdaptiveRealtime* appQosLiveAdaptiveRealtime;
@property(nonatomic) NSString* host;
@property(nonatomic) NSURL* url;
@property(nonatomic) BOOL isLiveManifest;

// cacheListenerProxyOfMyself是c语言代理本IJKFFMoviePlayerController的实例，生命周期也应该由IJKFFMoviePlayerController自己来管理
@property(nonatomic) CCacheSessionListener* cacheListenerProxyOfMyself;
@end

@implementation KwaiFFPlayerController {
    bool _isBuffering;
    double _lastTotalBufferTimeCurrentLive;
    NSTimeInterval _loadingStartTime;
}

@synthesize appVodQosDebugInfo = _appVodQosDebugInfo;
@synthesize appLiveQosDebugInfo = _appLiveQosDebugInfo;
@synthesize kwaiPlayerDebugInfo = _kwaiPlayerDebugInfo;

@synthesize cacheKey = _cacheKey;
@synthesize cacheMode = _cacheMode;
@synthesize cacheUpstreamType = _cacheUpstreamType;
@synthesize cacheBufferedDataSourceType = _cacheBufferedDataSourceType;
@synthesize cacheIgnoreOnError = _cacheIgnoreOnError;
@synthesize cacheBufferedDataSizeKb = _cacheBufferedDataSizeKb;
@synthesize cacheBufferedSeekThresholdKb = _cacheBufferedSeekThresholdKb;
@synthesize cacheCurlBufferSizeKb = _cacheCurlBufferSizeKb;

+ (NSString*)getVersion {
    return [NSString stringWithUTF8String:KWAI_PLAYER_VERSION];
}

+ (NSString*)getVersionExt {
    return [NSString stringWithUTF8String:KWAI_PLAYER_VERSION_EXT];
}

static void callback_proxy(int ijk_level, const char* msg) {
    KwaiLogCallback callback = kwai_log_callback;
    if (!callback) {
        return;
    }
    enum KwaiLogLevel kwai_level = KWAI_LOG_UNKNOWN;
    switch (ijk_level) {
        case IJK_LOG_UNKNOWN:
            kwai_level = KWAI_LOG_UNKNOWN;
            break;
        case IJK_LOG_DEFAULT:
            kwai_level = KWAI_LOG_UNKNOWN;
            break;
        case IJK_LOG_VERBOSE:
            kwai_level = KWAI_LOG_UNKNOWN;
            break;
        case IJK_LOG_DEBUG:
            kwai_level = KWAI_LOG_DEBUG;
            break;
        case IJK_LOG_INFO:
            kwai_level = KWAI_LOG_INFO;
            break;
        case IJK_LOG_ERROR:
            kwai_level = KWAI_LOG_ERROR;
            break;
        case IJK_LOG_FATAL:
            kwai_level = KWAI_LOG_FATAL;
            break;
        case IJK_LOG_SILENT:
            kwai_level = KWAI_LOG_SILENT;
            break;
        default:
            break;
    }

    callback(kwai_level, msg);
}

+ (void)setKwaiLogCallBack:(KwaiLogCallback)cb {
    if (cb) {
        inject_log_callback(callback_proxy);
    } else {
        inject_log_callback(NULL);
    }
    kwai_log_callback = cb;
}

+ (int)getAlivePlayerCount {
    return sAlivePlayerCount;
}

#pragma mark protocal IJKMediaPlayback
- (void)play {
    [super play];
}

#pragma mark protocal KwaiMediaPlayback
- (void)forExample_getQos {
    NSLog(@"forExample_getQos");
}

- (void)dealloc {
    @synchronized(sAlivePlayerCountLock) {
        sAlivePlayerCount--;
    }
    NSLog(@"[id:%d/alive:%d][PlayerLifeCycle][%s]", [super playerDebugId], sAlivePlayerCount,
          __func__);
}

- (KwaiPlayerPlaybackState)kwaiPlayBackState {
    IJKMPMoviePlaybackState state = [self playbackState];

    switch (state) {
        case IJKMPMoviePlaybackStateInited:
            _kwaiPlaybackState = KwaiPlayerPlaybackStateInited;
            break;
        case IJKMPMoviePlaybackStatePrepared:
        case IJKMPMoviePlaybackStatePlaying:
            _kwaiPlaybackState = KwaiPlayerPlaybackStatePlaying;
            break;
        case IJKMPMoviePlaybackStatePaused:
            _kwaiPlaybackState = KwaiPlayerPlaybackStatePaused;
            break;
        case IJKMPMoviePlaybackStateStopped:
            _kwaiPlaybackState = KwaiPlayerPlaybackStateStopped;
            break;

        default:
            // 如果没找到匹配的，则直接返回上次的_kwaiPlaybackState状态
            break;
    }

    return _kwaiPlaybackState;
}

- (void)configHeaders:(NSDictionary*)headers {
    if (headers != nil && [headers count] > 0) {
        _host = [headers objectForKey:@"Host"];
        if (_host) {
            ijkmp_set_option(super.mediaPlayer, FFP_OPT_CATEGORY_PLAYER, "host",
                             [_host UTF8String]);
        }
        NSString* headerString = [[NSString alloc] init];
        for (NSString* key in headers) {
            NSString* val = [headers objectForKey:key];
            // 经测headers的格式必须是 "key: value", 不能是
            // key:value，即value前有一个空格
            headerString = [headerString
                stringByAppendingString:[NSString stringWithFormat:@"%@: %@\r\n", key, val]];
        }
        ijkmp_set_option(super.mediaPlayer, FFP_OPT_CATEGORY_FORMAT, "headers",
                         [headerString UTF8String]);
    }
}

- (void)onInited:(NSURL*)url {
    _url = url;
    _kwaiPlaybackState = KwaiPlayerPlaybackStateInited;
    @synchronized(sAlivePlayerCountLock) {
        sAlivePlayerCount++;
    }
}

- (void)configCache:(BOOL)enableCache {
    ijkmp_set_option_int(super.mediaPlayer, FFP_OPT_CATEGORY_PLAYER, "cache-enabled",
                         enableCache ? 1 : 0);
    if (enableCache) {
        _cacheListenerProxyOfMyself = CCacheSessionListener_create(self);
        ijkmp_setup_cache_session_listener(super.mediaPlayer, _cacheListenerProxyOfMyself);
    }
}

// todo useHwCodec change的时候，需要 调用一次
- (void)configUserAgent {
    // set user-agent
    NSString* shortVersion =
        [[[NSBundle mainBundle] infoDictionary] objectForKey:@"CFBundleShortVersionString"];
    NSString* bundleId = [[NSBundle mainBundle] bundleIdentifier];
    NSString* ua = [NSString stringWithFormat:@"i/%@/%@/%@", bundleId, shortVersion,
                                              [KwaiFFPlayerController getVersion]];

    ijkmp_set_option(super.mediaPlayer, FFP_OPT_CATEGORY_FORMAT, "user-agent", [ua UTF8String]);
}

- (void)applyVodConfig {
    _isLive = FALSE;
    ijkmp_set_option_int(super.mediaPlayer, FFP_OPT_CATEGORY_PLAYER, "islive", 0);
}

- (void)applyVodManifestConfig:(KwaiVodManifestConfig*)vodManifestConfig {
    if (!super.mediaPlayer) {
        return;
    }
    [self applyVodConfig];
    ijkmp_set_option_int(super.mediaPlayer, IJKMP_OPT_CATEGORY_PLAYER, "enable-vod-manifest", 1);
    ijkmp_set_option_int(super.mediaPlayer, IJKMP_OPT_CATEGORY_FORMAT, "device-resolution-width",
                         vodManifestConfig.deviceWidth);
    ijkmp_set_option_int(super.mediaPlayer, IJKMP_OPT_CATEGORY_FORMAT, "device-resolution-height",
                         vodManifestConfig.deviceHeight);
    ijkmp_set_option_int(super.mediaPlayer, IJKMP_OPT_CATEGORY_FORMAT, "device-network-type",
                         vodManifestConfig.netType);
    ijkmp_set_option(super.mediaPlayer, IJKMP_OPT_CATEGORY_FORMAT, "abr-config-string",
                     [vodManifestConfig.rateConfig UTF8String]);
    ijkmp_set_option_int(super.mediaPlayer, IJKMP_OPT_CATEGORY_FORMAT, "low-device",
                         vodManifestConfig.lowDevice);
    ijkmp_set_option_int(super.mediaPlayer, IJKMP_OPT_CATEGORY_FORMAT, "signal-strength",
                         vodManifestConfig.signalStrength);
    ijkmp_set_option_int(super.mediaPlayer, IJKMP_OPT_CATEGORY_FORMAT, "switch-code",
                         vodManifestConfig.switchCode);
    ijkmp_set_option(super.mediaPlayer, IJKMP_OPT_CATEGORY_FORMAT, "adaptive-a1-config-string",
                     [vodManifestConfig.rateConfigA1 UTF8String]);
    ijkmp_set_option_int(super.mediaPlayer, IJKMP_OPT_CATEGORY_FORMAT, "adaptive-algorithm-mode",
                         vodManifestConfig.algorithm_mode);
}

- (void)applyLiveConfig {
    _isLive = YES;

    ijkmp_set_option_int(super.mediaPlayer, FFP_OPT_CATEGORY_PLAYER, "framedrop", 150);
    ijkmp_set_option_int(super.mediaPlayer, FFP_OPT_CATEGORY_PLAYER, "islive", 1);
}

- (void)applyLiveManifestConfig:(NSString*)adaptConfig {
    [self applyLiveConfig];
    _isLiveManifest = YES;

    ijkmp_set_option_int(super.mediaPlayer, FFP_OPT_CATEGORY_PLAYER, "enable-live-manifest", 1);
    ijkmp_set_option(super.mediaPlayer, FFP_OPT_CATEGORY_FORMAT, "liveAdaptConfig",
                     [adaptConfig UTF8String]);
}

#pragma mark 点播 init 接口 -- deprecated
- (instancetype)initVodPlayerWithURL:(NSURL*)url
                         withHeaders:(NSDictionary*)header
                     withCacheEnable:(BOOL)enableCache {
    KwaiPlayerVodBuildParams* params = [[KwaiPlayerVodBuildParams alloc] init];
    params.enableCache = enableCache;
    return [self initVodPlayerWithURL:url withHeaders:header withBuildParams:params];
}

- (instancetype)initVodPlayerWithURL:(NSURL*)url
                         withHeaders:(NSDictionary*)header
                     withCacheEnable:(BOOL)enableCache
                        withMixAudio:(BOOL)enableMixAudio {
    KwaiPlayerVodBuildParams* params = [[KwaiPlayerVodBuildParams alloc] init];
    params.enableCache = enableCache;
    params.ijkFFOptions.enableMixAudio = enableMixAudio;
    return [self initVodPlayerWithURL:url withHeaders:header withBuildParams:params];
}

- (instancetype)initVodPlayerWithManifest:(NSString*)manifest
                              withHeaders:(NSDictionary*)header
                          withCacheEnable:(BOOL)enableCache
                        vodManifestConfig:(KwaiVodManifestConfig*)vodManifestConfig {
    KwaiPlayerVodBuildParams* params = [[KwaiPlayerVodBuildParams alloc] init];
    params.enableCache = enableCache;
    return [self initVodPlayerWithManifest:manifest
                               withHeaders:header
                         vodManifestConfig:vodManifestConfig
                           withBuildParams:params];
}

- (instancetype)initVodPlayerWithIndexContent:(NSString*)content
                               withRequestUrl:(NSURL*)requestUrl
                               withPrefixPath:(NSString*)prefixPath
                                  withHeaders:(NSDictionary*)header
                              withCacheEnable:(BOOL)enableCache {
    KwaiPlayerVodBuildParams* params = [[KwaiPlayerVodBuildParams alloc] init];
    params.enableCache = enableCache;
    return [self initVodPlayerWithIndexContent:content
                                withRequestUrl:requestUrl
                                withPrefixPath:prefixPath
                                   withHeaders:header
                               withBuildParams:params];
}

- (instancetype)initVodPlayerWithHlsManifest:(NSString*)manifest
                              withRequestUrl:(NSURL*)requestUrl
                                 withHeaders:(NSDictionary*)header
                             withCacheEnable:(BOOL)enableCache {
    KwaiPlayerVodBuildParams* params = [[KwaiPlayerVodBuildParams alloc] init];
    params.enableCache = enableCache;
    return [self initVodPlayerWithHlsManifest:manifest
                               withRequestUrl:requestUrl
                                  withHeaders:header
                              withBuildParams:params];
}

#pragma mark 点播 init 接口(new)
- (instancetype)initVodPlayerWithURL:(NSURL*)url
                         withHeaders:(NSDictionary*)header
                     withBuildParams:(KwaiPlayerVodBuildParams*)buildParams {
    if (self = [super initWithContentURL:url withOptions:buildParams.ijkFFOptions]) {
        [self onInited:url];
        [self applyVodConfig];
        [self configHeaders:header];
        [self configUserAgent];
        [self configCache:buildParams.enableCache];
    }
    return self;
}

- (instancetype)initVodPlayerWithManifest:(NSString*)manifest
                              withHeaders:(NSDictionary*)header
                        vodManifestConfig:(KwaiVodManifestConfig*)vodManifestConfig
                          withBuildParams:(KwaiPlayerVodBuildParams*)buildParams {
    if (self = [super initWithContentURLString:manifest withOptions:buildParams.ijkFFOptions]) {
        [self onInited:nil];
        [self applyVodManifestConfig:vodManifestConfig];
        [self configHeaders:header];
        [self configUserAgent];
        [self configCache:buildParams.enableCache];
    }
    return self;
}

- (instancetype)initVodPlayerWithIndexContent:(NSString*)content
                               withRequestUrl:(NSURL*)requestUrl
                               withPrefixPath:(NSString*)prefixPath
                                  withHeaders:(NSDictionary*)header
                              withBuildParams:(KwaiPlayerVodBuildParams*)buildParams {
    if (self = [super initWithContentURL:requestUrl withOptions:buildParams.ijkFFOptions]) {
        [self onInited:requestUrl];
        [self applyVodConfig];
        [self configHeaders:header];
        [self configUserAgent];
        [self configCache:buildParams.enableCache];

        ijkmp_set_option_int(super.mediaPlayer, IJKMP_OPT_CATEGORY_PLAYER, "input-data-type",
                             INPUT_DATA_TYPE_INDEX_CONTENT);
        ijkmp_set_option(super.mediaPlayer, IJKMP_OPT_CATEGORY_PLAYER, "index-content.pre_path",
                         [prefixPath UTF8String]);
        ijkmp_set_option(super.mediaPlayer, IJKMP_OPT_CATEGORY_PLAYER, "index-content.content",
                         [content UTF8String]);
    }
    return self;
}

- (instancetype)initVodPlayerWithHlsManifest:(NSString*)manifest
                              withRequestUrl:(NSURL*)requestUrl
                                 withHeaders:(NSDictionary*)header
                             withBuildParams:(KwaiPlayerVodBuildParams*)buildParams {
    if (self = [super initWithContentURL:requestUrl withOptions:buildParams.ijkFFOptions]) {
        [self onInited:requestUrl];
        [self applyVodConfig];
        [self configHeaders:header];
        [self configUserAgent];
        [self configCache:buildParams.enableCache];

        ijkmp_set_option_int(super.mediaPlayer, IJKMP_OPT_CATEGORY_PLAYER, "input-data-type",
                             INPUT_DATA_TYPE_HLS_CUSTOME_MANIFEST);
        ijkmp_set_option(super.mediaPlayer, IJKMP_OPT_CATEGORY_PLAYER, "index-content.content",
                         [manifest UTF8String]);
    }
    return self;
}

#pragma mark 直播 init 接口
- (instancetype)initLivePlayerWithURL:(NSURL*)url withHeaders:(NSDictionary*)header {
    KwaiPlayerLiveBuildParams* params = [[KwaiPlayerLiveBuildParams alloc] init];
    return [self initLivePlayerWithURL:url withHeaders:header withBuildParams:params];
}

- (instancetype)initLivePlayerWithManifest:(NSString*)manifest
                               withHeaders:(NSDictionary*)header
                                withConfig:(NSString*)adaptConfig {
    KwaiPlayerLiveBuildParams* params = [[KwaiPlayerLiveBuildParams alloc] init];
    return [self initLivePlayerWithManifest:manifest
                                withHeaders:header
                                 withConfig:adaptConfig
                            withBuildParams:params];
}

#pragma mark live builder api(new)
- (instancetype)initLivePlayerWithURL:(NSURL*)url
                          withHeaders:(NSDictionary*)header
                      withBuildParams:(KwaiPlayerLiveBuildParams*)buildParams {
    if (self = [super initWithContentURL:url withOptions:buildParams.ijkFFOptions]) {
        [self onInited:url];
        [self applyLiveConfig];
        [self configHeaders:header];
        [self configUserAgent];
        [self configCache:buildParams.enableCache];
        if (buildParams.enableCache) {
            [self setCacheMode:KwaiCacheModeLiveNormal];
        }
    }
    return self;
}

- (instancetype)initLivePlayerWithManifest:(NSString*)manifest
                               withHeaders:(NSDictionary*)header
                                withConfig:(NSString*)adaptConfig
                           withBuildParams:(KwaiPlayerLiveBuildParams*)buildParams {
    if (self = [super initWithContentURLString:manifest withOptions:buildParams.ijkFFOptions]) {
        [self onInited:nil];
        [self applyLiveManifestConfig:adaptConfig];
        [self configHeaders:header];
        [self configUserAgent];
        [self configCache:buildParams.enableCache];
        if (buildParams.enableCache) {
            [self setCacheMode:KwaiCacheModeLiveAdaptive];
        }
    }

    return self;
}

- (MPMovieLoadState)convertLoadState:(IJKMPMovieLoadState)ijkState {
    MPMovieLoadState state = MPMovieLoadStateUnknown;

    if (ijkState & IJKMPMovieLoadStatePlayable) {
        state |= MPMovieLoadStatePlayable;
    }
    if (ijkState & IJKMPMovieLoadStatePlaythroughOK) {
        state |= MPMovieLoadStatePlaythroughOK;
    }
    if (ijkState & IJKMPMovieLoadStateStalled) {
        state |= MPMovieLoadStateStalled;
    }

    return state;
}

- (NSTimeInterval)getCurrentTime {
    //    NSLog(@"current time: %f", [self currentPlaybackTime]);
    return [[NSDate date] timeIntervalSince1970];
}

- (void)onPlayerLoadStateChangedOld:(NSNotification*)notification {
    MPMovieLoadState _state = [self convertLoadState:[super loadState]];
    if (MPMovieLoadStateStalled & _state) {
        _isBuffering = YES;
        _lastTotalBufferTimeCurrentLive = 0;
        _bufferEmptyCountOld++;

        _loadingStartTime = [self getCurrentTime];
    }
    if (MPMovieLoadStatePlayable & _state || MPMovieLoadStatePlaythroughOK & _state) {
        if (_loadingStartTime > 0) {
            _isBuffering = NO;
            _bufferEmptyDurationOld += [self getCurrentTime] - _loadingStartTime;
            _loadingStartTime = 0;

            NSLog(@"loading cost %f seconds with %d times", _bufferEmptyDurationOld,
                  (int)_bufferEmptyCountOld);
        }
    }
    [[NSNotificationCenter defaultCenter]
        postNotificationName:MPMoviePlayerLoadStateDidChangeNotification
                      object:self];
}

- (void)prepareToPlay {
    [super prepareToPlay];
    [self startQosLiveRealtimeIfNeeded:_qosLiveRealtimeReportIntervalMs withBlock:_qosStatBlock];
    [self startQosLiveAdaptiveRealtimeIfNeeded:_qosLiveAdaptiveRealtimeReportIntervalMs
                                     withBlock:_qosLiveAdaptiveStatBlock
                                     withBlock:_qosStatBlock];
}

- (void)stop {
    [self stopQosLiveRealtimeIfNeeded];
    [self stopQosLiveAdaptiveRealtimeIfNeeded];
    [super stop];
}

- (void)shutdown {
    [self stopQosLiveRealtimeIfNeeded];
    [self stopQosLiveAdaptiveRealtimeIfNeeded];
    [super shutdown];
    NSLog(@"[id:%d/alive:%d][PlayerLifeCycle][%s]", [super playerDebugId], sAlivePlayerCount,
          __func__);
}

- (void)didShutdown {
    [super didShutdown];
    CCacheSessionListener_freep(&_cacheListenerProxyOfMyself);
    self.cacheSessionDelegate = nil;
}

#pragma final Qos
- (KSYQosInfo*)qosInfo {
    KsyQosInfo qosInfo;
    memset(&qosInfo, 0, sizeof(KsyQosInfo));
    NSInteger ret = [self getQosInfo:&qosInfo];
    if (ret != 0) {
        return nil;
    }

    if (!_qosInfo) {
        _qosInfo = [KSYQosInfo new];
    }

    _qosInfo.audioBufferByteLength = qosInfo.audioBufferByteLength;
    _qosInfo.audioBufferTimeLength = qosInfo.audioBufferTimeLength;
    _qosInfo.audioTotalDataSize = qosInfo.audioTotalDataSize;
    _qosInfo.videoBufferByteLength = qosInfo.videoBufferByteLength;
    _qosInfo.videoBufferTimeLength = qosInfo.videoBufferTimeLength;
    _qosInfo.videoTotalDataSize = qosInfo.videoTotalDataSize;
    _qosInfo.totalDataSize = qosInfo.totalDataBytes;
    _qosInfo.audioDelay = qosInfo.audioDelay;
    _qosInfo.videoDelayRecv = qosInfo.videoDelayRecv;
    _qosInfo.videoDelayBefDec = qosInfo.videoDelayBefDec;
    _qosInfo.videoDelayAftDec = qosInfo.videoDelayAftDec;
    _qosInfo.videoDelayRender = qosInfo.videoDelayRender;
    _qosInfo.firstScreenTimeTotal = qosInfo.fst_total;
    _qosInfo.firstScreenTimeDnsAnalyze = qosInfo.fst_dns_analyze;
    _qosInfo.firstScreenTimeHttpConnect = qosInfo.fst_http_connect;
    _qosInfo.firstScreenTimeInputOpen = qosInfo.fst_input_open;
    _qosInfo.firstScreenTimeStreamFind = qosInfo.fst_stream_find;
    _qosInfo.firstScreenTimeCodecOpen = qosInfo.fst_codec_open;
    _qosInfo.firstScreenTimeAllPrepared = qosInfo.fst_all_prepared;
    _qosInfo.firstScreenTimeWaitForPlay = qosInfo.fst_wait_for_play;
    _qosInfo.firstScreenTimePktReceive = qosInfo.fst_video_pkt_recv;
    _qosInfo.firstScreenTimePreDecode = qosInfo.fst_video_pre_dec;
    _qosInfo.firstScreenTimeDecode = qosInfo.fst_video_dec;
    _qosInfo.firstScreenTimeRender = qosInfo.fst_video_render;
    _qosInfo.firstScreenTimeDroppedDuration = qosInfo.fst_dropped_duration;
    _qosInfo.totalDroppedDuration = qosInfo.dropped_duration;
    _qosInfo.hostInfo = [NSString stringWithUTF8String:qosInfo.hostInfo];
    _qosInfo.vencInit = [NSString stringWithUTF8String:qosInfo.vencInit];
    _qosInfo.aencInit = [NSString stringWithUTF8String:qosInfo.aencInit];
    _qosInfo.vencDynamic = [NSString stringWithUTF8String:qosInfo.vencDynamic];
    _qosInfo.comment = [NSString stringWithUTF8String:(qosInfo.comment ? qosInfo.comment : "")];

    // for cache
    _qosInfo.currentReadUri = [NSString stringWithUTF8String:qosInfo.current_read_path];
    _qosInfo.totalBytes = qosInfo.total_bytes;
    _qosInfo.cachedBytes = qosInfo.cached_bytes;
    _qosInfo.reopenCntBySeek = qosInfo.reopen_cnt_by_seek;

    /* free qosInfo */
    [self freeQosInfo:&qosInfo];

    return _qosInfo;
}

- (KwaiPlayerDebugInfo*)kwaiPlayerDebugInfo {
    if (_kwaiPlayerDebugInfo == nil) {
        _kwaiPlayerDebugInfo = [[KwaiPlayerDebugInfo alloc] initWith:self];
    }

    [_kwaiPlayerDebugInfo refresh];
    return _kwaiPlayerDebugInfo;
}

- (AppVodQosDebugInfo*)appVodQosDebugInfo {
    if (_appVodQosDebugInfo == nil) {
        _appVodQosDebugInfo = [[AppVodQosDebugInfo alloc] initWith:self];
    }

    [_appVodQosDebugInfo refresh];
    return _appVodQosDebugInfo;
}

- (AppLiveQosDebugInfo*)appLiveQosDebugInfo {
    if (_appLiveQosDebugInfo == nil) {
        _appLiveQosDebugInfo = [[AppLiveQosDebugInfo alloc] initWith:self];
    }

    [_appLiveQosDebugInfo refresh];
    return _appLiveQosDebugInfo;
}

#pragma mark API delegate
- (void)setDataReadTimeout:(int)timeout {
    if (super.mediaPlayer) {
        ijkmp_set_readtimeout(super.mediaPlayer, timeout);
    }
}

- (void)setPreferBandwidth:(int)bandWidth {
    if (super.mediaPlayer) {
        ijkmp_set_option_int(super.mediaPlayer, IJKMP_OPT_CATEGORY_PLAYER, "prefer-bandwidth",
                             bandWidth);
    }
}

- (void)setNetWorkConnectionTimeout:(int)timeout {
    if (super.mediaPlayer) {
        ijkmp_set_connectiontimeout(super.mediaPlayer, timeout);
    }
}

- (void)setVolume:(float)leftVolume rigthVolume:(float)rightVolume {
    if (super.mediaPlayer) {
        ijkmp_set_volume(super.mediaPlayer, leftVolume, rightVolume);
    }
}

- (void)setConfigJson:(NSString*)configJson {
    if (super.mediaPlayer) {
        ijkmp_set_config_json(super.mediaPlayer, [configJson UTF8String]);
    }
}

- (void)setLiveSpeedChangeConfigJson:(NSString*)configJson {
    if (super.mediaPlayer) {
        ijkmp_set_config_json(super.mediaPlayer, [configJson UTF8String]);
    }
}
- (void)setLiveLowDelayConfigJson:(NSString*)configJson {
    if (super.mediaPlayer) {
        ijkmp_set_live_low_delay_config_json(super.mediaPlayer, [configJson UTF8String]);
    }
}

- (void)updateCurrentWallClock:(int64_t)epochMs {
    if (super.mediaPlayer) {
        ijkmp_set_wall_clock(super.mediaPlayer, epochMs);
    }
}

- (void)updateCurrentMaxWallClockOffset:(int64_t)epochMs {
    if (super.mediaPlayer) {
        ijkmp_set_max_wall_clock_offset(super.mediaPlayer, epochMs);
    }
}

- (void)setFadeinEndTimeMs:(int)fadeinEndTimeMs {
    if (super.mediaPlayer && fadeinEndTimeMs > 0) {
        ijkmp_set_option_int(super.mediaPlayer, IJKMP_OPT_CATEGORY_PLAYER, "fade-in-end-time-ms",
                             fadeinEndTimeMs);
    }
}

- (void)setAsyncStreamOpenMode:(BOOL)enableAsync {
    if (super.mediaPlayer) {
        ijkmp_set_option_int(super.mediaPlayer, IJKMP_OPT_CATEGORY_PLAYER,
                             "async-stream-component-open", enableAsync ? 1 : 0);
    }
}

- (void)setLibfdkForAac:(BOOL)enableLibfdkForAac {
    if (!super.mediaPlayer) return;
    ijkmp_set_option_int(super.mediaPlayer, IJKMP_OPT_CATEGORY_PLAYER, "aac-libfdk",
                         enableLibfdkForAac ? 1 : 0);
}

- (void)setH264HWCodec:(BOOL)enableH264HWCodec {
    if (!super.mediaPlayer) return;
    ijkmp_set_option_int(super.mediaPlayer, IJKMP_OPT_CATEGORY_PLAYER, "vtb-h264",
                         enableH264HWCodec ? 1 : 0);
}

- (float)probeFps {
    if (!super.mediaPlayer) return 0.0f;

    return ijkmp_get_probe_fps(super.mediaPlayer);
}

- (void)setH265HWCodec:(BOOL)enableH265HWCodec {
    if (!super.mediaPlayer) return;
    ijkmp_set_option_int(super.mediaPlayer, IJKMP_OPT_CATEGORY_PLAYER, "vtb-h265",
                         enableH265HWCodec ? 1 : 0);
}

- (void)setLiveManifestSwitchMode:(KwaiLiveManifestSwitchMode)mode {
    if (super.mediaPlayer) {
        ijkmp_set_live_manifest_switch_mode(super.mediaPlayer, (int)mode);
    }
}

- (void)setVtbAutoRotate:(BOOL)enableVtbRotate {
    if (!super.mediaPlayer) return;
    ijkmp_set_option_int(super.mediaPlayer, IJKMP_OPT_CATEGORY_PLAYER, "vtb-auto-rotate",
                         enableVtbRotate ? 1 : 0);
}

- (void)setShouldMute:(BOOL)shouldMute {
    if (!super.mediaPlayer)
        return;
    else {
        if (!ijkmp_set_mute(super.mediaPlayer, shouldMute ? 1 : 0)) {
            _shouldMute = shouldMute;
        }
    }
}

- (void)setUseAlignedPts:(BOOL)enableAlignedPts {
    if (!super.mediaPlayer) return;
    ijkmp_set_option_int(super.mediaPlayer, IJKMP_OPT_CATEGORY_PLAYER, "use-aligned-pts",
                         enableAlignedPts ? 1 : 0);
}

- (BOOL)isHWCodecEnabled {
    if (super.mediaPlayer) {
        return ijkmp_is_hw(super.mediaPlayer);
    }
    NSLog(@"failed to get status of hardware decoder");
    return false;
}

- (int)videoDecodeType {
    if (super.mediaPlayer) {
        return (int)ijkmp_get_property_int64(super.mediaPlayer, FFP_PROP_INT64_VIDEO_DECODER,
                                             FFP_PROPV_DECODER_UNKNOWN);
    }
    return FFP_PROPV_DECODER_UNKNOWN;
}

- (BOOL)shouldLoop {
    BOOL bRet = FALSE;
    if (!super.mediaPlayer) bRet = FALSE;
    if (super.mediaPlayer) {
        int ret = ijkmp_get_loop(super.mediaPlayer);
        bRet = (ret == 0 ? TRUE : FALSE);
    }
    return bRet;
}

- (void)setShouldLoop:(BOOL)shouldLoop {
    if (!super.mediaPlayer)
        return;
    else {
        // XXX native loop return 1 on unloop, 0 for loop, strange
        ijkmp_set_loop(super.mediaPlayer, shouldLoop ? 0 : 1);
        //_shouldLoop = shouldLoop;
    }
}

- (void)setShouldLoopOnError:(BOOL)shouldLoopOnError {
    if (!super.mediaPlayer) {
        return;
    }
    ijkmp_set_option_int(super.mediaPlayer, IJKMP_OPT_CATEGORY_PLAYER, "enable-loop-on-error",
                         shouldLoopOnError ? 1 : 0);
}

- (void)setShouldExitOnDecError:(BOOL)shouldExitOnDecError {
    if (!super.mediaPlayer) {
        return;
    }
    ijkmp_set_option_int(super.mediaPlayer, IJKMP_OPT_CATEGORY_PLAYER, "exit-on-dec-error",
                         shouldExitOnDecError ? 1 : 0);
}

- (int)bufferEmptyCount {
    if (!super.mediaPlayer) {
        return 0;
    }
    if (_isLive || ijkmp_is_live_manifest(super.mediaPlayer)) {
        return (int)ijkmp_get_property_int64(super.mediaPlayer, FFP_PROP_INT64_BLOCKCNT, 0);
    } else {
        // todo add video stituation
        return 0;
    }
}

- (int64_t)bufferEmptyDuration {
    if (!super.mediaPlayer) {
        return 0;
    }

    if (_isLive || ijkmp_is_live_manifest(super.mediaPlayer)) {
        return ijkmp_get_property_int64(super.mediaPlayer, FFP_PROP_INT64_BUFFERTIME, 0);
    } else {
        // todo add video stituation
        return 0;
    }
}

#pragma mark qos report
- (NSString*)statJson {
    return [self liveStatJson];
}

- (NSString*)liveStatJson {
    if (nil == super.mediaPlayer) {
        return @"";
    }

    char* stat_json = ijkmp_get_live_stat_json_str(super.mediaPlayer);
    if (stat_json) {
        NSString* ret = [NSString stringWithUTF8String:stat_json];
        free(stat_json);
        return ret;
    }

    return @"";
}

- (NSString*)videoStatJson {
    if (nil == super.mediaPlayer) {
        return @"";
    }

    char* stat_json = ijkmp_get_video_stat_json_str(super.mediaPlayer);
    if (stat_json) {
        NSString* ret = [NSString stringWithUTF8String:stat_json];
        free(stat_json);
        return ret;
    }

    return nil;
}

- (NSString*)briefVideoStatJson {
    if (nil == super.mediaPlayer) {
        return @"";
    }

    char* brief_stat_json = ijkmp_get_brief_video_stat_json_str(super.mediaPlayer);
    if (brief_stat_json) {
        NSString* ret = [NSString stringWithUTF8String:brief_stat_json];
        free(brief_stat_json);
        return ret;
    }

    return nil;
}

- (NSInteger)getQosInfo:(KsyQosInfo*)p_qosInfo {
    if (nil == super.mediaPlayer || nil == p_qosInfo) return -1;

    int ret = ijkmp_get_qos_info(super.mediaPlayer, p_qosInfo);
    return ret;
}

- (NSInteger)freeQosInfo:(KsyQosInfo*)p_qosInfo {
    if (nil == super.mediaPlayer || nil == p_qosInfo) return -1;

    int ret = ijkmp_free_qos_info(super.mediaPlayer, p_qosInfo);
    return ret;
}

#pragma mark PROP getter/setter
- (CGFloat)fpsAtDecoder {
    if (!super.mediaPlayer) return 0.0f;

    return ijkmp_get_property_float(super.mediaPlayer,
                                    FFP_PROP_FLOAT_VIDEO_DECODE_FRAMES_PER_SECOND, .0f);
}

- (double)readSize {
    if (NULL == super.mediaPlayer) return 0.0;

    NSTimeInterval kb = 0;
    if (super.mediaPlayer)
        kb = ijkmp_get_property_int64(super.mediaPlayer, FFP_PROP_LONG_DOWNLOAD_SIZE, 0);

    return kb / 1024.0;
}

- (int64_t)dtsDuration {
    if (NULL == super.mediaPlayer) return 0;

    int64_t ret = 0;
    if (super.mediaPlayer)
        ret = ijkmp_get_property_int64(super.mediaPlayer, FFP_PROP_INT64_DTS_DURATION, 0);

    return ret;
}

- (NSTimeInterval)bufferTimeMax {
    float max = ijkmp_get_property_float(super.mediaPlayer, FFP_PROP_FLOAT_BUFFERSIZE_MAX, .0f);
    return max;
}

- (void)setBufferTimeMax:(NSTimeInterval)max {
    ijkmp_set_property_float(super.mediaPlayer, FFP_PROP_FLOAT_BUFFERSIZE_MAX, max);
}

- (float)averageDisplayFps {
    if (NULL == super.mediaPlayer) return 0.0;

    return ijkmp_get_property_float(super.mediaPlayer, FFP_PROP_FLOAT_AVERAGE_DISPLAYED_FPS, 0);
}

- (NSString*)serverAddress {
    if (NULL == super.mediaPlayer) return NULL;
    char* ip = ijkmp_get_property_string(super.mediaPlayer, FFP_PROP_STRING_SERVER_IP);
    if (NULL == ip) {
        return nil;
    }
    NSString* serverIp = [NSString stringWithUTF8String:ip];
    return serverIp;
}

- (NSString*)liveManifestCurUrl {
    if (NULL == super.mediaPlayer) return nil;
    char* url = ijkmp_get_property_string(super.mediaPlayer, FFP_PROP_STRING_PLAYING_URL);
    if (NULL == url) {
        return nil;
    }
    NSString* curUrl = [NSString stringWithUTF8String:url];
    return curUrl;
}

- (void)setDisableVodAudio:(BOOL)disableVodAudio {
    if (super.mediaPlayer) {
        ijkmp_set_option_int(super.mediaPlayer, FFP_OPT_CATEGORY_PLAYER, "an",
                             disableVodAudio ? 1 : 0);
    }
}

- (void)seekAtStart:(int)msec {
    [self setPlayerOptionIntValue:msec forKey:@"seek-at-start"];
}

- (void)setEnableSeekForwardOffset:(BOOL)enableSeekForwardOffset {
    if (super.mediaPlayer) {
        ijkmp_set_option_int(super.mediaPlayer, FFP_OPT_CATEGORY_PLAYER,
                             "enable-seek-forward-offset", enableSeekForwardOffset ? 1 : 0);
    }
}

- (void)setMaxBufferDuration:(int)msec {
    [self setPlayerOptionIntValue:msec forKey:@"max-buffer-dur-ms"];
}

- (int64_t)getCurAbsoluteTime {
    if (super.mediaPlayer) {
        return ijkmp_get_property_int64(super.mediaPlayer, FFP_PROP_INT64_CURRENT_ABSOLUTE_TIME, 0);
    }
    return 0;
}

#pragma mark buffer strategy
- (void)setBufferStrategy:(int)value {
    [super setPlayerOptionIntValue:value forKey:@"buffer-strategy"];
}

- (void)setFirstBufferTime:(int)value {
    [super setPlayerOptionIntValue:value forKey:@"first-high-water-mark-ms"];
}

- (void)setMinBufferTime:(int)value {
    [super setPlayerOptionIntValue:value forKey:@"next-high-water-mark-ms"];
}

- (void)setMaxBufferTime:(int)value {
    [super setPlayerOptionIntValue:value forKey:@"last-high-water-mark-ms"];
}

- (void)setBufferIncrementStep:(int)value {
    [super setPlayerOptionIntValue:value forKey:@"buffer-increment-step"];
}

- (void)setBufferSmoothTime:(int)value {
    [super setPlayerOptionIntValue:value forKey:@"buffer-smooth-time"];
}

- (void)setStartTime:(long)startTime {
    [super setPlayerOptionIntValue:startTime forKey:@"app-start-time"];
}

- (void)preLoad:(int64_t)preLoadDurationMs {
    if (super.mediaPlayer) {
        ijkmp_enable_pre_demux(super.mediaPlayer, 1, preLoadDurationMs);
    }
}

- (void)enableAbLoop:(int64_t)startMs endMs:(int64_t)endMs {
    if (super.mediaPlayer) {
        ijkmp_enable_ab_loop(super.mediaPlayer, startMs, endMs);
    }
}

- (void)enableLoopWithBufferStartPercent:(int)bufferStartPercent
                               bufferEnd:(int)bufferEndPercent
                               loopBegin:(int64_t)loopBegin {
    if (super.mediaPlayer) {
        ijkmp_enable_loop_on_block(super.mediaPlayer, bufferStartPercent, bufferEndPercent,
                                   loopBegin);
    }
}

- (void)setStartPlayBlockBufferMs:(int)bufferMs bufferMaxCostMs:(int)bufferMaxCostMs {
    if (super.mediaPlayer) {
        ijkmp_set_start_play_block_ms(super.mediaPlayer, bufferMs, bufferMaxCostMs);
    }
}

- (BOOL)checkCanStartPlay {
    if (!super.mediaPlayer) {
        return FALSE;
    }
    return ijkmp_check_can_start_play(super.mediaPlayer);
}

- (int)getDownloadedPercent {
    if (!super.mediaPlayer) {
        return 0;
    }
    return ijkmp_get_downloaded_percent(super.mediaPlayer);
}

- (void)setDccAlgorithm:(BOOL)enable {
    if (super.mediaPlayer) {
        ijkmp_set_option_int(super.mediaPlayer, FFP_OPT_CATEGORY_PLAYER, "dcc-alg.config_enabled",
                             enable ? 1 : 0);
    }
}

- (void)setDccAlgMBTh_10:(int)th_10 {
    if (super.mediaPlayer) {
        ijkmp_set_option_int(super.mediaPlayer, FFP_OPT_CATEGORY_PLAYER,
                             "dcc-alg.config_mark_bitrate_th_10", th_10);
    }
}

- (void)setDccAlgPreReadMs:(int)preReadMs {
    if (super.mediaPlayer) {
        ijkmp_set_option_int(super.mediaPlayer, FFP_OPT_CATEGORY_PLAYER,
                             "dcc-alg.config_dcc_pre_read_ms", preReadMs);
    }
}

#pragma mark custom API
- (void)reprepareBackgournd:(KwaiFFPlayerController*)mySelf is_flush:(bool)is_flush {
    if (super.mediaPlayer) ijkmp_reprepare_async(super.mediaPlayer, is_flush);
}

- (void)applyOptions:(IJKFFOptions*)options {
    if (!self.mediaPlayer || !options) {
        return;
    }
    [options applyTo:super.mediaPlayer];
}

- (void)setTag1:(int)tag1 {
    if (!super.mediaPlayer) {
        return;
    }
    ijkmp_set_option_int(super.mediaPlayer, IJKMP_OPT_CATEGORY_PLAYER, "tag1", tag1);
}

- (void)setProductContext:(ProductContext*)productContext {
    if (!super.mediaPlayer) {
        return;
    }
    ijkmp_set_option(super.mediaPlayer, IJKMP_OPT_CATEGORY_FORMAT, "product-context",
                     [productContext.stringJson UTF8String]);
}

- (void)setIsSF2020EncryptSource:(BOOL)isEncryptSource aesKey:(NSString*)aesKey {
    if (!super.mediaPlayer) {
        return;
    }
    ijkmp_set_option_int(super.mediaPlayer, IJKMP_OPT_CATEGORY_FORMAT, "is-sf2020-encrypt-source",
                         isEncryptSource ? 1 : 0);
    ijkmp_set_option(super.mediaPlayer, IJKMP_OPT_CATEGORY_FORMAT, "sf2020-aes-key",
                     [aesKey UTF8String]);
}

#pragma mark AwesomeCache
- (void)setCacheKey:(NSString*)cacheKey {
    if (!super.mediaPlayer) {
        return;
    }
    _cacheKey = cacheKey;
    ijkmp_set_option(super.mediaPlayer, IJKMP_OPT_CATEGORY_PLAYER, "cache-key",
                     [cacheKey UTF8String]);
}

- (void)setCacheMode:(KwaiCacheMode)cacheMode {
    if (!super.mediaPlayer) {
        return;
    }
    _cacheMode = cacheMode;
    ijkmp_set_option_int(super.mediaPlayer, IJKMP_OPT_CATEGORY_FORMAT, "cache-mode", cacheMode);
}

- (void)setAsyncCacheByteRangeSize:(int)byteRangeSize {
    if (!super.mediaPlayer) {
        return;
    }
    ijkmp_set_option_int(super.mediaPlayer, IJKMP_OPT_CATEGORY_FORMAT, "byte-range-size",
                         byteRangeSize);
}

- (void)setAsyncCacheFirstByteRangeSize:(int)firstByteRangeSize {
    if (!super.mediaPlayer) {
        return;
    }
    ijkmp_set_option_int(super.mediaPlayer, IJKMP_OPT_CATEGORY_FORMAT, "first-byte-range-size",
                         firstByteRangeSize);
}

- (void)setCacheIgnoreOnError:(BOOL)cacheIgnoreOnError {
    if (!super.mediaPlayer) {
        return;
    }
    _cacheIgnoreOnError = cacheIgnoreOnError;
    ijkmp_set_option_int(super.mediaPlayer, IJKMP_OPT_CATEGORY_FORMAT, "cache-flags",
                         cacheIgnoreOnError ? kFlagIgnoreCacheOnError : kFlagBlockOnCache);
}

- (void)setCacheUpstreamType:(KwaiCacheUpstreamType)cacheUpstreamType {
    if (!super.mediaPlayer) {
        return;
    }
    _cacheUpstreamType = cacheUpstreamType;
    ijkmp_set_option_int(super.mediaPlayer, IJKMP_OPT_CATEGORY_FORMAT, "cache-upstream-type",
                         cacheUpstreamType);
}

- (void)setCacheBufferedDataSourceType {
    // deprecated
}

- (void)setCacheBufferedDataSizeKb:(int)cacheBufferedDataSizeKb {
    if (!super.mediaPlayer) {
        return;
    }
    _cacheBufferedDataSizeKb = cacheBufferedDataSizeKb;
    ijkmp_set_option_int(super.mediaPlayer, IJKMP_OPT_CATEGORY_FORMAT,
                         "buffered-datasource-size-kb", cacheBufferedDataSizeKb);
}

- (void)setCacheBufferedSeekThresholdKb:(int)cacheBufferedSeekThresholdKb {
    if (!super.mediaPlayer) {
        return;
    }
    _cacheBufferedSeekThresholdKb = cacheBufferedSeekThresholdKb;
    ijkmp_set_option_int(super.mediaPlayer, IJKMP_OPT_CATEGORY_FORMAT,
                         "datasource-seek-reopen-threshold-kb", cacheBufferedSeekThresholdKb);
}

- (void)setCacheCurlBufferSizeKb:(int)cacheCurlBufferSizeKb {
    if (!super.mediaPlayer) {
        return;
    }
    _cacheCurlBufferSizeKb = cacheCurlBufferSizeKb;
    ijkmp_set_option_int(super.mediaPlayer, IJKMP_OPT_CATEGORY_FORMAT, "curl-buffer-size-kb",
                         cacheCurlBufferSizeKb);
}

- (void)setCacheDownloadConnectTimeoutMs:(int)timeoutMs {
    if (!super.mediaPlayer) {
        return;
    }
    ijkmp_set_option_int(super.mediaPlayer, IJKMP_OPT_CATEGORY_FORMAT, "cache-connect-timeout-ms",
                         timeoutMs);
}

- (void)setCacheDownloadReadTimeoutMs:(int)timeoutMs {
    if (!super.mediaPlayer) {
        return;
    }
    ijkmp_set_option_int(super.mediaPlayer, IJKMP_OPT_CATEGORY_FORMAT, "cache-read-timeout-ms",
                         timeoutMs);
}

- (void)setCacheHttpConnectRetry:(int)cacheHttpConnectRetry {
    if (!super.mediaPlayer) {
        return;
    }
    ijkmp_set_option_int(super.mediaPlayer, IJKMP_OPT_CATEGORY_FORMAT,
                         "cache-http-connect-retry-cnt", cacheHttpConnectRetry);
}

- (void)setMaxSpeedKbps:(int)maxSpeedKbps {
    if (!super.mediaPlayer) {
        return;
    }
    ijkmp_set_option_int(super.mediaPlayer, IJKMP_OPT_CATEGORY_FORMAT, "max-speed-kbps",
                         maxSpeedKbps);
}

- (void)setAwesomeCacheCallbackDelegate:
    (id<KSAwesomeCacheCallbackDelegate>)awesomeCacheCallbackDelegate {
    ijkmp_setup_awesome_cache_callback(
        super.mediaPlayer, AwesomeCacheCallback_Opaque_new(awesomeCacheCallbackDelegate));
}

- (void)setLastTryFlag:(BOOL)is_last_try {
    if (!super.mediaPlayer) {
        return;
    }
    ijkmp_set_last_try_flag(super.mediaPlayer, is_last_try);
}

- (void)setEnableAudioGain:(BOOL)enableAudioGain {
    if (!super.mediaPlayer) {
        return;
    }
    ijkmp_set_option_int(super.mediaPlayer, IJKMP_OPT_CATEGORY_PLAYER, "audio-gain.enable",
                         enableAudioGain ? 1 : 0);
}

- (void)setEnableModifyBlock:(BOOL)enableModifyBlock {
    if (!super.mediaPlayer) {
        return;
    }
    ijkmp_set_option_int(super.mediaPlayer, IJKMP_OPT_CATEGORY_PLAYER, "enable-modify-block",
                         enableModifyBlock ? 1 : 0);
}

- (void)setEnableSegmentCache:(BOOL)enableSegmentCache {
    if (!super.mediaPlayer) {
        return;
    }
    ijkmp_set_option_int(super.mediaPlayer, IJKMP_OPT_CATEGORY_PLAYER, "enable-segment-cache",
                         enableSegmentCache ? 1 : 0);
}

// vod adaptive
- (NSString*)getVodAdaptiveUrl {
    if (!super.mediaPlayer) {
        return NULL;
    }
    char url[DATA_SOURCE_URI_MAX_LEN] = "";
    ijkmp_get_vod_adaptive_url(super.mediaPlayer, url);
    NSString* vod_adaptive_url = [NSString stringWithUTF8String:url];
    return vod_adaptive_url;
}

- (NSString*)getVodAdaptiveCacheKey {
    if (!super.mediaPlayer) {
        return NULL;
    }
    char key[DATA_SOURCE_URI_MAX_LEN] = "";
    ijkmp_get_vod_adaptive_cache_key(super.mediaPlayer, key);
    NSString* vod_adaptive_cache_key = [NSString stringWithUTF8String:key];
    return vod_adaptive_cache_key;
}

- (NSString*)getVodAdaptiveHostName {
    if (!super.mediaPlayer) {
        return NULL;
    }
    char host[DATA_SOURCE_IP_MAX_LEN] = "";
    ijkmp_get_vod_adaptive_host_name(super.mediaPlayer, host);
    NSString* vod_adaptive_host = [NSString stringWithUTF8String:host];
    return vod_adaptive_host;
}

- (int32_t)getVodAdaptiveRepID {
    if (!super.mediaPlayer) {
        return 0;
    }

    return (int32_t)ijkmp_get_property_int64(super.mediaPlayer, FFP_PROP_INT64_VOD_ADAPTIVE_REP_ID,
                                             0);
}

#pragma mark QosLiveRealtime
- (void)startQosLiveRealtimeIfNeeded:(int64_t)intervalMs
                           withBlock:(KSYPlyQosStatBlock)qosStatBlock {
    @synchronized(self) {
        if (_isLive && _enableQosLiveRealtime && _appQosLiveRealtime == nil) {
            _appQosLiveRealtime = [[AppQosLiveRealtime alloc] initWith:self];
            [_appQosLiveRealtime startReport:qosStatBlock reportIntervalMs:intervalMs];
        }
    }
}

- (void)stopQosLiveRealtimeIfNeeded {
    @synchronized(self) {
        if (_appQosLiveRealtime) {
            [_appQosLiveRealtime stopReport];
            _appQosLiveRealtime = nil;
        }
    }
}

- (void)setAppQosStatJson:(NSString*)qosJson {
    ijkmp_set_live_app_qos_info(super.mediaPlayer, [qosJson UTF8String]);
}

#pragma mark QosLiveAdaptiveRealtime
- (void)startQosLiveAdaptiveRealtimeIfNeeded:(int64_t)intervalMs
                                   withBlock:(KSYPlyQosStatBlock)qosLiveAdaptiveStatBlock
                                   withBlock:(KSYPlyQosStatBlock)qosStatBlock {
    @synchronized(self) {
        if (_isLiveManifest && _enableQosLiveAdaptiveRealtime &&
            _appQosLiveAdaptiveRealtime == nil) {
            _appQosLiveAdaptiveRealtime = [[AppQosLiveAdaptiveRealtime alloc] initWith:self];
            _appQosLiveAdaptiveRealtime.host = _host;
            [_appQosLiveAdaptiveRealtime startReport:qosLiveAdaptiveStatBlock
                                    reportIntervalMs:intervalMs
                                    enableAdditional:_enableQosLiveAdaptiveAdditionalRealtime];
        }

        if (_enableQosLiveRealtime && _appQosLiveRealtime == nil) {
            _appQosLiveRealtime = [[AppQosLiveRealtime alloc] initWith:self];
            [_appQosLiveRealtime startReport:qosStatBlock
                            reportIntervalMs:_qosLiveRealtimeReportIntervalMs];
        }
    }
}

- (void)stopQosLiveAdaptiveRealtimeIfNeeded {
    @synchronized(self) {
        if (_appQosLiveAdaptiveRealtime) {
            [_appQosLiveAdaptiveRealtime stopReport];
            _appQosLiveAdaptiveRealtime = nil;
        }

        if (_appQosLiveRealtime) {
            [_appQosLiveRealtime stopReport];
            _appQosLiveRealtime = nil;
        }
    }
}

@end
