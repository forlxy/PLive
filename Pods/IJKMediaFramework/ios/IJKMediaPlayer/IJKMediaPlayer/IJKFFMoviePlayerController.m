/*
 * IJKFFMoviePlayerController.m
 *
 * Copyright (c) 2013 Zhang Rui <bbcallen@gmail.com>
 *
 * This file is part of ijkPlayer.
 *
 * ijkPlayer is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * ijkPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with ijkPlayer; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#import "IJKFFMoviePlayerController.h"

#import <UIKit/UIKit.h>
#import "IJKAudioKit.h"
#import "IJKFFMoviePlayerDef.h"
#import "IJKMediaModule.h"
#import "IJKMediaPlayback.h"
#import "IJKNotificationManager.h"
#import "NSString+IJKMedia.h"
#import "ijkkwai/kwai_player_version_gennerated.h"

#include "ijkplayer/ijkavformat/ijkavformat.h"
#include "ijkplayer/version.h"
#include "string.h"

// static const char *kIJKFFRequiredFFmpegVersion =
// "ff2.8--ijk0.4.1.1--dev0.3.3--rc4";

@interface IJKFFMoviePlayerController ()

@property(nonatomic, readonly) NSDictionary* mediaMeta;
@property(nonatomic, readonly) NSDictionary* videoMeta;
@property(nonatomic, readonly) NSDictionary* audioMeta;

@end

@implementation IJKFFMoviePlayerController {
    //    IjkMediaPlayer *_mediaPlayer;     // for subclass to access
    IJKSDLGLView* _glView;
    IJKFFMoviePlayerMessagePool* _msgPool;
    NSString* _urlString;

    NSInteger _videoWidth;
    NSInteger _videoHeight;
    NSInteger _sampleAspectRatioNumerator;
    NSInteger _sampleAspectRatioDenominator;

    BOOL _seeking;
    NSInteger _bufferingTime;
    NSInteger _bufferingPosition;

    //    BOOL _keepScreenOnWhilePlaying;   // for subclass to access
    BOOL _pauseInBackground;
    BOOL _isVideoToolboxOpen;
    BOOL _playingBeforeInterruption;

    IJKNotificationManager* _notificationManager;

    BOOL _shouldShowHudView;
    IJKMPMoviePlaybackState _currentState;

    NSTimer* _hudTimer;
}

@synthesize view = _view;
@synthesize currentPlaybackTime;
@synthesize duration;
@synthesize playableDuration;
@synthesize bufferingProgress = _bufferingProgress;

@synthesize numberOfBytesTransferred = _numberOfBytesTransferred;

@synthesize isPreparedToPlay = _isPreparedToPlay;
@synthesize playbackState = _playbackState;
@synthesize loadState = _loadState;

@synthesize naturalSize = _naturalSize;
@synthesize scalingMode = _scalingMode;
@synthesize shouldAutoplay = _shouldAutoplay;
@synthesize shouldAccurateSeek = _shouldAccurateSeek;
@synthesize shouldVideoDropFrame = _shouldVideoDropFrame;

@synthesize allowsMediaAirPlay = _allowsMediaAirPlay;
@synthesize airPlayMediaActive = _airPlayMediaActive;

@synthesize isDanmakuMediaAirPlay = _isDanmakuMediaAirPlay;

// TODO,这三个变量官方代码重构到IJKFFMonitor里了,酌情cp
@synthesize mediaMeta = _mediaMeta;
@synthesize videoMeta = _videoMeta;
@synthesize audioMeta = _audioMeta;

#define FFP_IO_STAT_STEP (50 * 1024)

// as an example
void IJKFFIOStatDebugCallback(const char* url, int type, int bytes) {
    static int64_t s_ff_io_stat_check_points = 0;
    static int64_t s_ff_io_stat_bytes = 0;
    if (!url) return;

    if (type != IJKMP_IO_STAT_READ) return;

    if (!av_strstart(url, "http:", NULL)) return;

    s_ff_io_stat_bytes += bytes;
    if (s_ff_io_stat_bytes < s_ff_io_stat_check_points ||
        s_ff_io_stat_bytes > s_ff_io_stat_check_points + FFP_IO_STAT_STEP) {
        s_ff_io_stat_check_points = s_ff_io_stat_bytes;
        NSLog(@"io-stat: %s, +%d = %" PRId64 "\n", url, bytes, s_ff_io_stat_bytes);
    }
}

void IJKFFIOStatRegister(void (*cb)(const char* url, int type, int bytes)) {
    ijkmp_io_stat_register(cb);
}

void IJKFFIOStatCompleteDebugCallback(const char* url, int64_t read_bytes, int64_t total_size,
                                      int64_t elpased_time, int64_t total_duration) {
    if (!url) return;

    if (!av_strstart(url, "http:", NULL)) return;

    NSLog(@"io-stat-complete: %s, %" PRId64 "/%" PRId64 ", %" PRId64 "/%" PRId64 "\n", url,
          read_bytes, total_size, elpased_time, total_duration);
}

void IJKFFIOStatCompleteRegister(void (*cb)(const char* url, int64_t read_bytes, int64_t total_size,
                                            int64_t elpased_time, int64_t total_duration)) {
    ijkmp_io_stat_complete_register(cb);
}

- (id)initWithContentURL:(NSURL*)aUrl withOptions:(IJKFFOptions*)options {
    if (aUrl == nil) return nil;

    // Detect if URL is file path and return proper string for it
    NSString* aUrlString = [aUrl isFileURL] ? [aUrl path] : [aUrl absoluteString];

    return [self initWithContentURLString:aUrlString withOptions:options];
}

- (id)initWithContentURLString:(NSString*)aUrlString withOptions:(IJKFFOptions*)options {
    if (aUrlString == nil) return nil;

    [IJKFFMoviePlayerController setupLogCallbackOnce];

    self = [super init];
    if (self) {
        ijkmp_global_init();
        ijkmp_global_set_inject_callback(ijkff_inject_callback);

        [IJKFFMoviePlayerController checkIfFFmpegVersionMatch:NO];

        if (options == nil) options = [IJKFFOptions optionsByDefault];

        // IJKFFIOStatRegister(IJKFFIOStatDebugCallback);
        // IJKFFIOStatCompleteRegister(IJKFFIOStatCompleteDebugCallback);

        // init fields
        _scalingMode = IJKMPMovieScalingModeAspectFit;
        _shouldAutoplay = YES;
        // init media resource
        _urlString = aUrlString;
        _mediaMeta = [[NSDictionary alloc] init];

        // init player
        _mediaPlayer = ijkmp_ios_create(media_player_msg_loop);
        if (_mediaPlayer == nil) return nil;
        _msgPool = [[IJKFFMoviePlayerMessagePool alloc] init];

        ijkmp_set_weak_thiz(_mediaPlayer, (__bridge_retained void*)self);
        ijkmp_set_inject_opaque(_mediaPlayer, (__bridge void*)self);
        ijkmp_set_option_int(_mediaPlayer, IJKMP_OPT_CATEGORY_PLAYER, "start-on-prepared",
                             _shouldAutoplay ? 1 : 0);

        // init video sink
        if (!options.useReuseGLView) {
            _glView = [[IJKSDLGLView alloc] initWithFrame:[[UIScreen mainScreen] bounds]
                                                sessionId:ijkmp_get_session_id(_mediaPlayer)];
            _glView.shouldShowHudView = NO;
            _view = _glView;
            ijkmp_ios_set_glview(_mediaPlayer, _glView);
        }

        self.shouldShowHudView = options.showHudView;

        ijkmp_set_option(_mediaPlayer, IJKMP_OPT_CATEGORY_PLAYER, "overlay-format", "fcc-i420");

        // init audio sink
        IJKAudioKit* audioKit = [IJKAudioKit sharedInstance];
        audioKit.enableMixAudio = options.enableMixAudio;
        [audioKit setupAudioSession];

        [options applyTo:_mediaPlayer];
        _pauseInBackground = NO;

        // init extra
        _keepScreenOnWhilePlaying = YES;
        [self setScreenOn:YES];

        _notificationManager = [[IJKNotificationManager alloc] init];
        [self registerApplicationObservers];

        _playerDebugId = ijkmp_get_session_id(_mediaPlayer);
        _currentState = IJKMPMoviePlaybackStateInited;
    }
    return self;
}

+ (void)setupLogCallbackOnce {
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
#ifdef DEBUG
        [IJKFFMoviePlayerController setLogLevel:k_IJK_LOG_INFO];
        [IJKFFMoviePlayerController setKwaiLogLevel:k_IJK_LOG_DEBUG];
        [IJKFFMoviePlayerController setLogReport:YES];
#else
        [IJKFFMoviePlayerController setLogLevel:k_IJK_LOG_WARN];
        [IJKFFMoviePlayerController setKwaiLogLevel:k_IJK_LOG_INFO];
#endif
    });
}

- (void)setScreenOn:(BOOL)on {
    [IJKMediaModule sharedModule].mediaModuleIdleTimerDisabled = on;
    // [UIApplication sharedApplication].idleTimerDisabled = on;
}

- (void)dealloc {
    //    [self unregisterApplicationObservers];
}

- (void)setShouldAccurateSeek:(BOOL)shouldAccurateSeek {
    if (!_mediaPlayer) return;

    ijkmp_set_option_int(_mediaPlayer, IJKMP_OPT_CATEGORY_PLAYER, "enable-accurate-seek",
                         shouldAccurateSeek ? 1 : 0);
}

- (void)setShouldAutoplay:(BOOL)shouldAutoplay {
    _shouldAutoplay = shouldAutoplay;

    if (!_mediaPlayer) return;

    ijkmp_set_option_int(_mediaPlayer, IJKMP_OPT_CATEGORY_PLAYER, "start-on-prepared",
                         _shouldAutoplay ? 1 : 0);
}

- (BOOL)shouldAutoplay {
    return _shouldAutoplay;
}

- (void)setShouldVideoDropFrame:(BOOL)shouldVideoDropFrame {
    _shouldVideoDropFrame = shouldVideoDropFrame;

    if (!_mediaPlayer) return;

    ijkmp_set_option_int(_mediaPlayer, FFP_OPT_CATEGORY_PLAYER, "framedrop",
                         shouldVideoDropFrame ? 8 : 0);
}

- (BOOL)shouldVideoDropFrame {
    return _shouldVideoDropFrame;
}

- (void)setVideoDataBlock:(void (^)(CVPixelBufferRef, int))videoDataBlock {
    _videoDataBlock = videoDataBlock;
    if (_glView) {
        _glView.videoDataBlock = videoDataBlock;
    }
}
/*
 * 设置音频回调接口.
 * 参数类型：
 * CMSampleBufferRef sampleBuffer: 应用存放解码后的音频数据，PCM格式。
 * int sampleRate
 * int channels
 */
- (void)setAudioDataBlock:(void (^)(CMSampleBufferRef, int, int))audioDataBlock {
    _audioDataBlock = audioDataBlock;
    if (_mediaPlayer != nil) {
        ijkmp_set_audio_data_callback(_mediaPlayer, (__bridge_retained void*)(audioDataBlock));
    }
}

- (void)setLiveEventBlock:(void (^)(char*, int))liveEventBlock {
    _liveEventBlock = liveEventBlock;
    if (_mediaPlayer != nil) {
        ijkmp_set_live_event_callback(_mediaPlayer, CFBridgingRetain(liveEventBlock));
    }
}

- (void)setShouldDisplayInternal:(BOOL)shouldDisplayInternal {
    if (_glView) {
        _glView.shouldDisplayInternal = shouldDisplayInternal;
    }
}

- (void)prepareToPlay {
    if (!_mediaPlayer) return;

    if (_urlString != nil) {
        [self setHudUrl:_urlString];
    }

    [self setScreenOn:_keepScreenOnWhilePlaying];
    ijkmp_set_data_source(_mediaPlayer, [_urlString UTF8String]);
    //    ijkmp_set_option(_mediaPlayer, IJKMP_OPT_CATEGORY_FORMAT, "safe",
    //    "0"); // for concat demuxer
    ijkmp_prepare_async(_mediaPlayer);
    _currentState = IJKMPMoviePlaybackStatePrepared;
}

- (void)setHudUrl:(NSString*)urlString {
    if ([[NSThread currentThread] isMainThread]) {
        NSRange range = [urlString rangeOfString:@"://"];
        if (range.location != NSNotFound) {
            NSString* urlFullScheme = [urlString substringToIndex:range.location];

            NSRange rangeOfLastScheme =
                [urlFullScheme rangeOfString:@":"
                                     options:NSBackwardsSearch
                                       range:NSMakeRange(0, range.location)];
            if (rangeOfLastScheme.location != NSNotFound) {
                //                NSString *urlExtra  = [urlString
                //                substringFromIndex:rangeOfLastScheme.location
                //                + 1]; NSURL *url = [NSURL
                //                URLWithString:urlExtra];
                //                [_glView setHudValue:urlFullScheme
                //                forKey:@"scheme"];
                //                [_glView setHudValue:url.host forKey:@"host"];
                //                [_glView setHudValue:url.path forKey:@"path"];
                return;
            }
        }

        //        NSURL *url = [NSURL URLWithString:urlString];
        //        [_glView setHudValue:url.scheme forKey:@"scheme"];
        //        [_glView setHudValue:url.host   forKey:@"host"];
        //        [_glView setHudValue:url.path   forKey:@"path"];
    } else {
        dispatch_async(dispatch_get_main_queue(), ^{
            [self setHudUrl:urlString];
        });
    }
}

- (void)play {
    if (!_mediaPlayer) return;

    [self setScreenOn:_keepScreenOnWhilePlaying];

    [self startHudTimer];
    ijkmp_start(_mediaPlayer);
    _currentState = IJKMPMoviePlaybackStatePlaying;
}

- (void)pause {
    if (!_mediaPlayer) return;

    [self stopHudTimer];
    ijkmp_pause(_mediaPlayer);
    _currentState = IJKMPMoviePlaybackStatePaused;
}

- (void)step {
    if (!_mediaPlayer) return;

    ijkmp_step_frame(_mediaPlayer);
}

- (void)stop {
    if (!_mediaPlayer) return;

    [self stopHudTimer];
    ijkmp_stop(_mediaPlayer);
    _currentState = IJKMPMoviePlaybackStateStopped;
}

- (BOOL)isPlaying {
    if (!_mediaPlayer) return NO;

    return ijkmp_is_playing(_mediaPlayer);
}

- (void)setPauseInBackground:(BOOL)pause {
    _pauseInBackground = pause;
}

- (BOOL)isVideoToolboxOpen {
    if (!_mediaPlayer) return NO;

    return _isVideoToolboxOpen;
}

inline static int getPlayerOption(IJKFFOptionCategory category) {
    int mp_category = -1;
    switch (category) {
        case kIJKFFOptionCategoryFormat:
            mp_category = IJKMP_OPT_CATEGORY_FORMAT;
            break;
        case kIJKFFOptionCategoryCodec:
            mp_category = IJKMP_OPT_CATEGORY_CODEC;
            break;
        case kIJKFFOptionCategorySws:
            mp_category = IJKMP_OPT_CATEGORY_SWS;
            break;
        case kIJKFFOptionCategoryPlayer:
            mp_category = IJKMP_OPT_CATEGORY_PLAYER;
            break;
        default:
            NSLog(@"unknown option category: %d\n", category);
    }
    return mp_category;
}

- (void)setOptionValue:(NSString*)value
                forKey:(NSString*)key
            ofCategory:(IJKFFOptionCategory)category {
    if (!_mediaPlayer) return;

    ijkmp_set_option(_mediaPlayer, getPlayerOption(category), [key UTF8String], [value UTF8String]);
}

- (void)setOptionIntValue:(int64_t)value
                   forKey:(NSString*)key
               ofCategory:(IJKFFOptionCategory)category {
    if (!_mediaPlayer) return;

    ijkmp_set_option_int(_mediaPlayer, getPlayerOption(category), [key UTF8String], value);
}

- (void)liveAudioOnly:(BOOL)on {
    if (_mediaPlayer) {
        ijkmp_audio_only(_mediaPlayer, on);
    }
}

+ (void)setLogReport:(BOOL)preferLogReport {
    ijkmp_global_set_log_report(preferLogReport ? 1 : 0);
}

+ (void)setLogLevel:(IJKLogLevel)logLevel {
    ijkmp_global_set_log_level(logLevel);
}

+ (void)setKwaiLogLevel:(IJKLogLevel)logLevel {
    ijkmp_global_set_kwailog_level(logLevel);
}

+ (BOOL)checkIfFFmpegVersionMatch:(BOOL)showAlert;
{
    return YES;
    //    const char *actualVersion = av_version_info();
    //    const char *expectVersion = kIJKFFRequiredFFmpegVersion;
    //    if (0 == strcmp(actualVersion, expectVersion)) {
    //        return YES;
    //    } else {
    //        NSString *message = [NSString stringWithFormat:@"actual: %s\n
    //        expect: %s\n", actualVersion, expectVersion];
    //        NSLog(@"\n!!!!!!!!!!\n%@\n!!!!!!!!!!\n", message);
    //        if (showAlert) {
    //            UIAlertView *alertView = [[UIAlertView alloc]
    //            initWithTitle:@"Unexpected FFmpeg version"
    //                                                                message:message
    //                                                               delegate:nil
    //                                                      cancelButtonTitle:@"OK"
    //                                                      otherButtonTitles:nil];
    //            [alertView show];
    //        }
    //        return NO;
    //    }
}

+ (BOOL)checkIfPlayerVersionMatch:(BOOL)showAlert
                            major:(unsigned int)major
                            minor:(unsigned int)minor
                            micro:(unsigned int)micro {
    unsigned int actualVersion = ijkmp_version_int();
    if (actualVersion == AV_VERSION_INT(major, minor, micro)) {
        return YES;
    } else {
        if (showAlert) {
            NSString* message =
                [NSString stringWithFormat:@"actual: %s\n expect: %d.%d.%d\n",
                                           ijkmp_version_ident(), major, minor, micro];
            UIAlertView* alertView =
                [[UIAlertView alloc] initWithTitle:@"Unexpected ijkplayer version"
                                           message:message
                                          delegate:nil
                                 cancelButtonTitle:@"OK"
                                 otherButtonTitles:nil];
            [alertView show];
        }
        return NO;
    }
}

- (void)shutdown {
    if (!_mediaPlayer) return;

    [self stopHudTimer];
    [self unregisterApplicationObservers];

    [self performSelectorInBackground:@selector(shutdownWaitStop:) withObject:self];
}

- (void)shutdownWaitStop:(IJKFFMoviePlayerController*)mySelf {
    if (!_mediaPlayer) return;

    ijkmp_stop(_mediaPlayer);
    ijkmp_shutdown(_mediaPlayer);
    [[IJKAudioKit sharedInstance] releaseAudioSession];

    [self performSelectorOnMainThread:@selector(shutdownClose:) withObject:self waitUntilDone:YES];
}

- (void)shutdownClose:(IJKFFMoviePlayerController*)mySelf {
    if (!_mediaPlayer) return;

    _segmentOpenDelegate = nil;
    _tcpOpenDelegate = nil;
    _httpOpenDelegate = nil;
    _liveOpenDelegate = nil;

    __unused id weakPlayer =
        (__bridge_transfer IJKFFMoviePlayerController*)ijkmp_set_weak_thiz(_mediaPlayer, NULL);

    ijkmp_dec_ref_p(&_mediaPlayer);

    [self didShutdown];
}

- (void)didShutdown {
}

// 这块逻辑和audioSessionInterrupt有联动，若要修改注意相关影响
- (IJKMPMoviePlaybackState)playbackState {
    if (!_mediaPlayer) return NO;

    IJKMPMoviePlaybackState mpState = IJKMPMoviePlaybackStateStopped;
    int state = ijkmp_get_state(_mediaPlayer);
    switch (state) {
        case MP_STATE_STOPPED:
        case MP_STATE_COMPLETED:
        case MP_STATE_ERROR:
        case MP_STATE_END:
            mpState = IJKMPMoviePlaybackStateStopped;
            break;
        case MP_STATE_IDLE:
        case MP_STATE_INITIALIZED:
        case MP_STATE_ASYNC_PREPARING:
        case MP_STATE_PAUSED:
            mpState = IJKMPMoviePlaybackStatePaused;
            break;
        case MP_STATE_PREPARED:
        case MP_STATE_STARTED: {
            if (_seeking)
                mpState = IJKMPMoviePlaybackStateSeekingForward;
            else
                mpState = IJKMPMoviePlaybackStatePlaying;
            break;
        }
    }

    // IJKMPMoviePlaybackStatePlaying,
    // IJKMPMoviePlaybackStatePaused,
    // IJKMPMoviePlaybackStateStopped,
    // IJKMPMoviePlaybackStateInterrupted,
    // IJKMPMoviePlaybackStateSeekingForward,
    // IJKMPMoviePlaybackStateSeekingBackward
    return mpState;
}

- (void)setCurrentPlaybackTime:(NSTimeInterval)aCurrentPlaybackTime {
    if (!_mediaPlayer) return;

    _seeking = YES;
    [[NSNotificationCenter defaultCenter]
        postNotificationName:IJKMPMoviePlayerPlaybackStateDidChangeNotification
                      object:self];

    _bufferingPosition = 0;
    ijkmp_seek_to(_mediaPlayer, aCurrentPlaybackTime * 1000);
}

- (float)probeFps {
    if (!_mediaPlayer) return 0.0f;

    return ijkmp_get_probe_fps(_mediaPlayer);
}

- (NSString*)getVideoCodecInfo {
    if (!_mediaPlayer) return nil;

    char* videoInfo = NULL;

    if (ijkmp_get_video_codec_info(_mediaPlayer, &videoInfo) != 0 || (NULL == videoInfo)) {
        return nil;
    }

    NSString* videoCodecInfo = [NSString stringWithUTF8String:videoInfo];

    return videoCodecInfo;
}

- (NSString*)getAudioCodecInfo {
    if (!_mediaPlayer) return nil;

    char* audioInfo = NULL;

    if (ijkmp_get_audio_codec_info(_mediaPlayer, &audioInfo) != 0 || (NULL == audioInfo)) {
        return nil;
    }

    NSString* audioCodecInfo = [NSString stringWithUTF8String:audioInfo];

    return audioCodecInfo;
}

- (NSTimeInterval)currentPlaybackTime {
    if (!_mediaPlayer) return 0.0f;

    NSTimeInterval ret = ijkmp_get_current_position(_mediaPlayer);
    if (isnan(ret) || isinf(ret)) return -1;

    return ret / 1000;
}

- (NSTimeInterval)duration {
    if (!_mediaPlayer) return 0.0f;

    NSTimeInterval ret = ijkmp_get_duration(_mediaPlayer);
    if (isnan(ret) || isinf(ret)) return -1;

    return ret / 1000;
}

- (NSTimeInterval)playableDuration {
    if (!_mediaPlayer) return 0.0f;

    NSTimeInterval ret = ijkmp_get_playable_duration(_mediaPlayer);
    return ret / 1000;
}

- (CGSize)naturalSize {
    return _naturalSize;
}

- (void)changeNaturalSize {
    [self willChangeValueForKey:@"naturalSize"];
    if (_sampleAspectRatioNumerator > 0 && _sampleAspectRatioDenominator > 0) {
        self->_naturalSize = CGSizeMake(
            1.0f * _videoWidth * _sampleAspectRatioNumerator / _sampleAspectRatioDenominator,
            _videoHeight);
    } else {
        self->_naturalSize = CGSizeMake(_videoWidth, _videoHeight);
    }
    [self didChangeValueForKey:@"naturalSize"];

    if (self->_naturalSize.width > 0 && self->_naturalSize.height > 0) {
        [[NSNotificationCenter defaultCenter]
            postNotificationName:IJKMPMovieNaturalSizeAvailableNotification
                          object:self];
    }
}

- (void)setScalingMode:(IJKMPMovieScalingMode)aScalingMode {
    IJKMPMovieScalingMode newScalingMode = aScalingMode;
    switch (aScalingMode) {
        case IJKMPMovieScalingModeNone:
            [_view setContentMode:UIViewContentModeCenter];
            break;
        case IJKMPMovieScalingModeAspectFit:
            [_view setContentMode:UIViewContentModeScaleAspectFit];
            break;
        case IJKMPMovieScalingModeAspectFill:
            [_view setContentMode:UIViewContentModeScaleAspectFill];
            break;
        case IJKMPMovieScalingModeFill:
            [_view setContentMode:UIViewContentModeScaleToFill];
            break;
        default:
            newScalingMode = _scalingMode;
    }

    _scalingMode = newScalingMode;
}

// deprecated, for MPMoviePlayerController compatiable
- (UIImage*)thumbnailImageAtTime:(NSTimeInterval)playbackTime
                      timeOption:(IJKMPMovieTimeOption)option {
    return nil;
}

- (UIImage*)thumbnailImageAtCurrentTime {
    if ([_view isKindOfClass:[IJKSDLGLView class]]) {
        IJKSDLGLView* glView = (IJKSDLGLView*)_view;
        return [glView snapshot];
    }

    return nil;
}

- (CGFloat)fpsAtOutput {
    return _glView.fps;
}

- (void)refreshKHudViewKflv {
#if ENABLE_HUD_VIEW
    if (_mediaPlayer == nil) return;

    KFlvPlayerStatistic stat = ijkmp_get_kflv_statisitc(_mediaPlayer);

    //    [_glView setHudValue:[NSString stringWithFormat:@"%dkbps",
    //    stat.kflv_stat.flvs[0].total_bandwidth_kbps] forKey:[NSString
    //    stringWithFormat:@"%dx%d 码率", s.kflv_stat.flvs[0].width,
    //    s.kflv_stat.flvs[0].height]];

    if (stat.kflv_stat.flv_nb <= 0) {
        return;
    }

    // 基本信息
    //    [_glView setHudValue:[NSString stringWithFormat:@"%dms",
    //    stat.kflv_stat.segment_dur_ms] forKey:@"GOP均长"];
    // todo 码流 的分辨率和带宽
    // todo 内存还得考虑下 packet_queue里的
    for (int i = 0; i < stat.kflv_stat.flv_nb; i++) {
        FlvInfo flvInfo = stat.kflv_stat.flvs[i];
        //
        [_glView
            setHudValue:[NSString stringWithFormat:@"(%dkbps) %@", flvInfo.total_bandwidth_kbps,
                                                   stat.kflv_stat.cur_decoding_flv_index == i
                                                       ? @",解码中"
                                                       : @""]
                 forKey:[NSString stringWithFormat:@"码流%d", i]];
    }

    // 动态信息
    [_glView setHudValue:[NSString stringWithFormat:@"%3.2fs", (stat.kflv_stat.cached_v_dur_ms +
                                                                stat.kflv_stat.cached_tag_dur_ms) *
                                                                   1.0 / 1000]
                  forKey:@"缓冲总时长"];
    //    [_glView setHudValue:[NSString stringWithFormat:@"%dx%d", stat.render_width,
    //    stat.render_height]
    //                  forKey:@"播放分辨率"];
    //    [_glView setHudValue:[NSString stringWithFormat:@"%d", stat.render_fps]
    //    forKey:@"当前FPS"];
    [_glView setHudValue:[NSString stringWithFormat:@"%dkbps", stat.kflv_stat.bandwidth_current]
                  forKey:@"码控估算带宽"];
    //    [_glView setHudValue:[NSString stringWithFormat:@"%lldkbps",
    //    stat.kflv_stat.download_speed_profile.speed_kbps] forKey:@"下载速度"];
    //    [_glView setHudValue:[NSString stringWithFormat:@"%lldkbps",
    //    stat.kflv_stat.read_packet_speed_profile.speed_kbps]
    //    forKey:@"播放速度"];
#endif
}

- (void)refreshHudView {
#if ENABLE_HUD_VIEW
    if (_mediaPlayer == nil) return;

    int64_t vdec = ijkmp_get_property_int64(_mediaPlayer, FFP_PROP_INT64_VIDEO_DECODER,
                                            FFP_PROPV_DECODER_UNKNOWN);
    float vdps =
        ijkmp_get_property_float(_mediaPlayer, FFP_PROP_FLOAT_VIDEO_DECODE_FRAMES_PER_SECOND, .0f);
    float vfps =
        ijkmp_get_property_float(_mediaPlayer, FFP_PROP_FLOAT_VIDEO_OUTPUT_FRAMES_PER_SECOND, .0f);

    switch (vdec) {
        case FFP_PROPV_DECODER_VIDEOTOOLBOX:
            [_glView setHudValue:@"VideoToolbox" forKey:@"vdec"];
            break;
        case FFP_PROPV_DECODER_AVCODEC:
            [_glView
                setHudValue:[NSString
                                stringWithFormat:@"avcodec %d.%d.%d", LIBAVCODEC_VERSION_MAJOR,
                                                 LIBAVCODEC_VERSION_MINOR, LIBAVCODEC_VERSION_MICRO]
                     forKey:@"vdec"];
            break;
        default:
            [_glView setHudValue:@"N/A" forKey:@"vdec"];
            break;
    }

    [_glView setHudValue:[NSString stringWithFormat:@"%.2f / %.2f", vdps, vfps] forKey:@"fps"];

    int64_t vcacheb = ijkmp_get_property_int64(_mediaPlayer, FFP_PROP_INT64_VIDEO_CACHED_BYTES, 0);
    int64_t acacheb = ijkmp_get_property_int64(_mediaPlayer, FFP_PROP_INT64_AUDIO_CACHED_BYTES, 0);
    int64_t vcached =
        ijkmp_get_property_int64(_mediaPlayer, FFP_PROP_INT64_VIDEO_CACHED_DURATION, 0);
    int64_t acached =
        ijkmp_get_property_int64(_mediaPlayer, FFP_PROP_INT64_AUDIO_CACHED_DURATION, 0);
    int64_t vcachep =
        ijkmp_get_property_int64(_mediaPlayer, FFP_PROP_INT64_VIDEO_CACHED_PACKETS, 0);
    int64_t acachep =
        ijkmp_get_property_int64(_mediaPlayer, FFP_PROP_INT64_AUDIO_CACHED_PACKETS, 0);
    [_glView setHudValue:[NSString stringWithFormat:@"%" PRId64 " ms, %" PRId64 " bytes, %" PRId64
                                                     " packets",
                                                    vcached, vcacheb, vcachep]
                  forKey:@"v-cache"];
    [_glView setHudValue:[NSString stringWithFormat:@"%" PRId64 " ms, %" PRId64 " bytes, %" PRId64
                                                     " packets",
                                                    acached, acacheb, acachep]
                  forKey:@"a-cache"];

    float avdelay = ijkmp_get_property_float(_mediaPlayer, FFP_PROP_FLOAT_AVDELAY, .0f);
    float avdiff = ijkmp_get_property_float(_mediaPlayer, FFP_PROP_FLOAT_AVDIFF, .0f);
    [_glView setHudValue:[NSString stringWithFormat:@"%.3f %.3f", avdelay, -avdiff]
                  forKey:@"delay"];
#endif
}

- (void)startHudTimer {
#if ENABLE_HUD_VIEW
    if (!_shouldShowHudView) return;

    if (_hudTimer != nil) return;

    if ([[NSThread currentThread] isMainThread]) {
        _glView.shouldShowHudView = YES;
        _hudTimer = [NSTimer
            scheduledTimerWithTimeInterval:.5f
                                    target:self
                                  //                                                   selector:@selector(refreshHudView)
                                  selector:@selector(refreshKHudViewKflv)
                                  userInfo:nil
                                   repeats:YES];
    } else {
        dispatch_async(dispatch_get_main_queue(), ^{
            [self startHudTimer];
        });
    }
#endif
}

- (void)stopHudTimer {
#if ENABLE_HUD_VIEW
    if (_hudTimer == nil) return;

    if ([[NSThread currentThread] isMainThread]) {
        _glView.shouldShowHudView = NO;
        [_hudTimer invalidate];
        _hudTimer = nil;
    } else {
        dispatch_async(dispatch_get_main_queue(), ^{
            [self stopHudTimer];
        });
    }
#endif
}

- (void)setShouldShowHudView:(BOOL)shouldShowHudView {
#if ENABLE_HUD_VIEW
    if (shouldShowHudView == _shouldShowHudView) {
        return;
    }
    _shouldShowHudView = shouldShowHudView;
    if (shouldShowHudView)
        [self startHudTimer];
    else
        [self stopHudTimer];
#endif
}

- (BOOL)shouldShowHudView {
#if ENABLE_HUD_VIEW
    return _shouldShowHudView;
#else
    return NO;
#endif
}

- (void)setPlaybackRate:(float)playbackRate {
    if (!_mediaPlayer) return;

    return ijkmp_set_playback_rate(_mediaPlayer, playbackRate, false);
}

- (void)setPlaybackTone:(int)playbackTone {
    if (!_mediaPlayer) return;

    return ijkmp_set_playback_tone(_mediaPlayer, playbackTone);
}

- (float)playbackRate {
    if (!_mediaPlayer) return 0.0f;

    return ijkmp_get_property_float(_mediaPlayer, FFP_PROP_FLOAT_PLAYBACK_RATE, 0.0f);
}

- (int64_t)trafficStatistic {
    if (!_mediaPlayer) return 0;
    return ijkmp_get_property_int64(_mediaPlayer, FFP_PROP_INT64_TRAFFIC_STATISTIC_BYTE_COUNT, 0);
}

inline static void fillMetaInternal(NSMutableDictionary* meta, IjkMediaMeta* rawMeta,
                                    const char* name, NSString* defaultValue) {
    if (!meta || !rawMeta || !name) return;

    NSString* key = [NSString stringWithUTF8String:name];
    const char* value = ijkmeta_get_string_l(rawMeta, name);
    if (value) {
        [meta setObject:[NSString stringWithUTF8String:value] forKey:key];
    } else if (defaultValue) {
        [meta setObject:defaultValue forKey:key];
    } else {
        [meta removeObjectForKey:key];
    }
}

- (void)postEvent:(IJKFFMoviePlayerMessage*)msg {
    if (!msg || !_mediaPlayer) return;

    AVMessage* avmsg = &msg->_msg;
    switch (avmsg->what) {
        case FFP_MSG_FLUSH:
            break;
        case FFP_MSG_ERROR: {
            [[NSNotificationCenter defaultCenter]
                postNotificationName:IJKMPMoviePlayerPlaybackStateDidChangeNotification
                              object:self];

            [[NSNotificationCenter defaultCenter]
                postNotificationName:IJKMPMoviePlayerPlaybackDidFinishNotification
                              object:self
                            userInfo:@{
                                IJKMPMoviePlayerPlaybackDidFinishReasonUserInfoKey :
                                    @(IJKMPMovieFinishReasonPlaybackError),
                                @"error" : @(avmsg->arg1)
                            }];
            break;
        }
        case FFP_MSG_PREPARED: {
            IjkMediaMeta* rawMeta = ijkmp_get_meta_l(_mediaPlayer);
            if (rawMeta) {
                ijkmeta_lock(rawMeta);

                NSMutableDictionary* newMediaMeta = [[NSMutableDictionary alloc] init];

                fillMetaInternal(newMediaMeta, rawMeta, IJKM_KEY_FORMAT, nil);
                fillMetaInternal(newMediaMeta, rawMeta, IJKM_KEY_DURATION_US, nil);
                fillMetaInternal(newMediaMeta, rawMeta, IJKM_KEY_START_US, nil);
                fillMetaInternal(newMediaMeta, rawMeta, IJKM_KEY_BITRATE, nil);

                fillMetaInternal(newMediaMeta, rawMeta, IJKM_KEY_VIDEO_STREAM, nil);
                fillMetaInternal(newMediaMeta, rawMeta, IJKM_KEY_AUDIO_STREAM, nil);
                fillMetaInternal(newMediaMeta, rawMeta, IJKM_KEY_HTTP_CONNECT_TIME, nil);
                fillMetaInternal(newMediaMeta, rawMeta, IJKM_KEY_HTTP_FIRST_DATA_TIME, nil);
                fillMetaInternal(newMediaMeta, rawMeta, IJKM_KEY_HTTP_ANALYZE_DNS, nil);

                int64_t video_stream = ijkmeta_get_int64_l(rawMeta, IJKM_KEY_VIDEO_STREAM, -1);
                int64_t audio_stream = ijkmeta_get_int64_l(rawMeta, IJKM_KEY_AUDIO_STREAM, -1);

                NSMutableArray* streams = [[NSMutableArray alloc] init];

                size_t count = ijkmeta_get_children_count_l(rawMeta);
                for (size_t i = 0; i < count; ++i) {
                    IjkMediaMeta* streamRawMeta = ijkmeta_get_child_l(rawMeta, i);
                    NSMutableDictionary* streamMeta = [[NSMutableDictionary alloc] init];

                    if (streamRawMeta) {
                        fillMetaInternal(streamMeta, streamRawMeta, IJKM_KEY_TYPE,
                                         k_IJKM_VAL_TYPE__UNKNOWN);
                        const char* type = ijkmeta_get_string_l(streamRawMeta, IJKM_KEY_TYPE);
                        if (type) {
                            fillMetaInternal(streamMeta, streamRawMeta, IJKM_KEY_CODEC_NAME, nil);
                            fillMetaInternal(streamMeta, streamRawMeta, IJKM_KEY_CODEC_PROFILE,
                                             nil);
                            fillMetaInternal(streamMeta, streamRawMeta, IJKM_KEY_CODEC_LONG_NAME,
                                             nil);
                            fillMetaInternal(streamMeta, streamRawMeta, IJKM_KEY_BITRATE, nil);

                            if (0 == strcmp(type, IJKM_VAL_TYPE__VIDEO)) {
                                fillMetaInternal(streamMeta, streamRawMeta, IJKM_KEY_WIDTH, nil);
                                fillMetaInternal(streamMeta, streamRawMeta, IJKM_KEY_HEIGHT, nil);
                                fillMetaInternal(streamMeta, streamRawMeta, IJKM_KEY_FPS_NUM, nil);
                                fillMetaInternal(streamMeta, streamRawMeta, IJKM_KEY_FPS_DEN, nil);
                                fillMetaInternal(streamMeta, streamRawMeta, IJKM_KEY_TBR_NUM, nil);
                                fillMetaInternal(streamMeta, streamRawMeta, IJKM_KEY_TBR_DEN, nil);
                                fillMetaInternal(streamMeta, streamRawMeta, IJKM_KEY_SAR_NUM, nil);
                                fillMetaInternal(streamMeta, streamRawMeta, IJKM_KEY_SAR_DEN, nil);

                                if (video_stream == i) {
                                    _videoMeta = streamMeta;

                                    int64_t fps_num =
                                        ijkmeta_get_int64_l(streamRawMeta, IJKM_KEY_FPS_NUM, 0);
                                    int64_t fps_den =
                                        ijkmeta_get_int64_l(streamRawMeta, IJKM_KEY_FPS_DEN, 0);
                                    if (fps_num > 0 && fps_den > 0) {
                                        _fpsInMeta = ((CGFloat)(fps_num)) / fps_den;
                                        NSLog(@"fps in meta %f\n", _fpsInMeta);
                                    }
                                }

                            } else if (0 == strcmp(type, IJKM_VAL_TYPE__AUDIO)) {
                                fillMetaInternal(streamMeta, streamRawMeta, IJKM_KEY_SAMPLE_RATE,
                                                 nil);
                                fillMetaInternal(streamMeta, streamRawMeta, IJKM_KEY_CHANNEL_LAYOUT,
                                                 nil);

                                if (audio_stream == i) {
                                    _audioMeta = streamMeta;
                                }
                            }
                        }
                    }

                    [streams addObject:streamMeta];
                }

                [newMediaMeta setObject:streams forKey:kk_IJKM_KEY_STREAMS];

                ijkmeta_unlock(rawMeta);
                _mediaMeta = newMediaMeta;
            }

            [self startHudTimer];
            _isPreparedToPlay = YES;

            [[NSNotificationCenter defaultCenter]
                postNotificationName:IJKMPMediaPlaybackIsPreparedToPlayDidChangeNotification
                              object:self];

            _loadState = IJKMPMovieLoadStatePlayable | IJKMPMovieLoadStatePlaythroughOK;

            [[NSNotificationCenter defaultCenter]
                postNotificationName:IJKMPMoviePlayerLoadStateDidChangeNotification
                              object:self];

            break;
        }
        case FFP_MSG_COMPLETED: {
            [[NSNotificationCenter defaultCenter]
                postNotificationName:IJKMPMoviePlayerPlaybackStateDidChangeNotification
                              object:self];

            [[NSNotificationCenter defaultCenter]
                postNotificationName:IJKMPMoviePlayerPlaybackDidFinishNotification
                              object:self
                            userInfo:@{
                                IJKMPMoviePlayerPlaybackDidFinishReasonUserInfoKey :
                                    @(IJKMPMovieFinishReasonPlaybackEnded)
                            }];
            break;
        }
        case FFP_MSG_VIDEO_SIZE_CHANGED:
            if (avmsg->arg1 > 0) _videoWidth = avmsg->arg1;
            if (avmsg->arg2 > 0) _videoHeight = avmsg->arg2;
            [self changeNaturalSize];
            break;
        case FFP_MSG_SAR_CHANGED:
            if (avmsg->arg1 > 0) _sampleAspectRatioNumerator = avmsg->arg1;
            if (avmsg->arg2 > 0) _sampleAspectRatioDenominator = avmsg->arg2;
            [self changeNaturalSize];
            break;
        case FFP_MSG_BUFFERING_START: {
            _loadState = IJKMPMovieLoadStateStalled;

            [[NSNotificationCenter defaultCenter]
                postNotificationName:IJKMPMoviePlayerLoadStateDidChangeNotification
                              object:self];
            break;
        }
        case FFP_MSG_BUFFERING_END: {
            _loadState = IJKMPMovieLoadStatePlayable | IJKMPMovieLoadStatePlaythroughOK;

            [[NSNotificationCenter defaultCenter]
                postNotificationName:IJKMPMoviePlayerLoadStateDidChangeNotification
                              object:self];
            [[NSNotificationCenter defaultCenter]
                postNotificationName:IJKMPMoviePlayerPlaybackStateDidChangeNotification
                              object:self];
            break;
        }
        case FFP_MSG_BUFFERING_UPDATE:
            _bufferingPosition = avmsg->arg1;
            _bufferingProgress = avmsg->arg2;
            [[NSNotificationCenter defaultCenter]
                postNotificationName:IJKMPMoviePlayerBufferUpdateNotification
                              object:self
                            userInfo:@{
                                IJKMPMoviePlayerBufferPositionKey : @(avmsg->arg1),
                            }];
            // NSLog(@"FFP_MSG_BUFFERING_UPDATE: %d, %%%d\n", _bufferingPosition,
            // _bufferingProgress);
            break;
        case FFP_MSG_BUFFERING_BYTES_UPDATE:
            // NSLog(@"FFP_MSG_BUFFERING_BYTES_UPDATE: %d\n", avmsg->arg1);
            break;
        case FFP_MSG_BUFFERING_TIME_UPDATE:
            _bufferingTime = avmsg->arg1;
            // NSLog(@"FFP_MSG_BUFFERING_TIME_UPDATE: %d\n", avmsg->arg1);
            break;
        case FFP_MSG_PLAYBACK_STATE_CHANGED:
            [[NSNotificationCenter defaultCenter]
                postNotificationName:IJKMPMoviePlayerPlaybackStateDidChangeNotification
                              object:self];
            break;
        case FFP_MSG_SEEK_COMPLETE:
        case FFP_MSG_ACCURATE_SEEK_COMPLETE: {
            [[NSNotificationCenter defaultCenter]
                postNotificationName:IJKMPMoviePlayerDidSeekCompleteNotification
                              object:self
                            userInfo:@{
                                IJKMPMoviePlayerDidSeekCompleteTargetKey : @(avmsg->arg1),
                                IJKMPMoviePlayerDidSeekCompleteErrorKey : @(avmsg->arg2)
                            }];
            _seeking = NO;
            break;
        }
        case FFP_MSG_VIDEO_DECODER_OPEN: {
            _isVideoToolboxOpen = avmsg->arg1;
            //            NSLog(@"FFP_MSG_VIDEO_DECODER_OPEN: %@\n",
            //            _isVideoToolboxOpen ? @"true" : @"false");
            [[NSNotificationCenter defaultCenter]
                postNotificationName:IJKMPMoviePlayerVideoDecoderOpenNotification
                              object:self];
            break;
        }
        case FFP_MSG_VIDEO_RENDERING_START: {
            [[NSNotificationCenter defaultCenter]
                postNotificationName:IJKMPMoviePlayerFirstVideoFrameRenderedNotification
                              object:self];
            break;
        }
        case FFP_MSG_AUDIO_RENDERING_START: {
            [[NSNotificationCenter defaultCenter]
                postNotificationName:IJKMPMoviePlayerFirstAudioFrameRenderedNotification
                              object:self];
            break;
        }

        case FFP_MSG_RELOADED_VIDEO_RENDERING_START: {
            [[NSNotificationCenter defaultCenter]
                postNotificationName:IJKMPMoviePlayerFirstReloadedVideoFrameRenderedNotification
                              object:self];
            break;
        }

        case FFP_MSG_VIDEO_RENDERING_START_AFTER_SEEK: {
            [[NSNotificationCenter defaultCenter]
                postNotificationName:IJKMPMoviePlayerFirstVideoFrameRenderedAfterSeekNotification
                              object:self];
            break;
        }

        case FFP_MSG_AUDIO_RENDERING_START_AFTER_SEEK: {
            [[NSNotificationCenter defaultCenter]
                postNotificationName:IJKMPMoviePlayerFirstAudioFrameRenderedAfterSeekNotification
                              object:self];
            break;
        }

            //        case FFP_MSG_LINK_LATENCY: {
            //            [[NSNotificationCenter defaultCenter]
            //             postNotificationName:IJKMPMoviePlayerLinkLatencyNotification
            //             object:self];
            //          break;
            //    }

        case FFP_MSG_PLAY_TO_END: {
            [[NSNotificationCenter defaultCenter]
                postNotificationName:IJKMPMoviePlayerPlayToEndNotification
                              object:self];
            break;
        }

        case FFP_MSG_DECODE_ERROR: {
            [[NSNotificationCenter defaultCenter]
                postNotificationName:IJKMPMoviePlayerDecodeErrorNotification
                              object:self
                            userInfo:@{IJKMPMoviePlayerDecodeErrorCode : @(avmsg->arg1)}];
            break;
        }

        case FFP_MSG_LIVE_TYPE_CHANGE: {
            [[NSNotificationCenter defaultCenter]
                postNotificationName:IJKMPMoviePlayerLiveTypeChangeNotification
                              object:self
                            userInfo:@{IJKMPMoviePlayerDidLiveTypeChangeType : @(avmsg->arg1)}];
            break;
        }

        case FFP_MSG_LIVE_VOICE_COMMENT_CHANGE: {
            int64_t vc_time = avmsg->arg1;
            vc_time = vc_time << 32 | avmsg->arg2;
            NSLog(@"voice_com: FFP_MSG_LIVE_VOICE_COMMENT_CHANGE, "
                  @"getKwaiLiveVocieComment(time = %lld)\n",
                  vc_time);
            NSString* vc = [self getKwaiLiveVocieComment:vc_time];
            if (vc != nil) {
                NSMutableDictionary* userInfo = [NSMutableDictionary dictionary];
                NSLog(@"voice_com: FFP_MSG_LIVE_VOICE_COMMENT_CHANGE, vc = %@\n", vc);
                [userInfo setObject:vc forKey:IJKMPMoviePlayerLiveVoiceComment];

                [[NSNotificationCenter defaultCenter]
                    postNotificationName:IJKMPMoviePlayerLiveVoiceCommentChangeNotification
                                  object:self
                                userInfo:userInfo];
            }
            break;
        }
        case FFP_MSG_PRE_LOAD_FINISH: {
            [[NSNotificationCenter defaultCenter] postNotificationName:IJKMPMoviePlayerPreloadFinish
                                                                object:self];
            break;
        }
        default:
            // NSLog(@"unknown FFP_MSG_xxx(%d)\n", avmsg->what);
            break;
    }

    [_msgPool recycle:msg];
}

- (IJKFFMoviePlayerMessage*)obtainMessage {
    return [_msgPool obtain];
}

inline static IJKFFMoviePlayerController* ffplayerRetain(void* arg) {
    return (__bridge_transfer IJKFFMoviePlayerController*)arg;
}

int media_player_msg_loop(void* arg) {
    @autoreleasepool {
        IjkMediaPlayer* mp = (IjkMediaPlayer*)arg;
        __weak IJKFFMoviePlayerController* ffpController =
            ffplayerRetain(ijkmp_set_weak_thiz(mp, NULL));

        while (ffpController) {
            @autoreleasepool {
                IJKFFMoviePlayerMessage* msg = [ffpController obtainMessage];
                if (!msg) break;

                int retval = ijkmp_get_msg(mp, &msg->_msg, 1);
                if (retval < 0) break;

                // block-get should never return 0
                assert(retval > 0);

                [ffpController performSelectorOnMainThread:@selector(postEvent:)
                                                withObject:msg
                                             waitUntilDone:NO];
            }
        }

        // retained in prepare_async, before SDL_CreateThreadEx
        ijkmp_dec_ref_p(&mp);
        return 0;
    }
}

#pragma mark av_format_control_message

static int onInjectUrlOpen(IJKFFMoviePlayerController* mpc, id<IJKMediaUrlOpenDelegate> delegate,
                           int type, void* data, size_t data_size) {
    IJKAVInject_OnUrlOpenData* realData = data;
    assert(realData);
    assert(sizeof(IJKAVInject_OnUrlOpenData) == data_size);
    realData->is_handled = NO;
    realData->is_url_changed = NO;

    if (delegate == nil) return 0;

    NSString* urlString = [NSString stringWithUTF8String:realData->url];
    NSURL* url = [NSURL URLWithString:urlString];
    if ([url.scheme isEqualToString:@"tcp"] || [url.scheme isEqualToString:@"udp"]) {
        if ([url.host ijk_isIpv4]) {
            [mpc->_glView setHudValue:url.host forKey:@"ip"];
        }
    } else {
        [mpc setHudUrl:urlString];
    }

    IJKMediaUrlOpenData* openData =
        [[IJKMediaUrlOpenData alloc] initWithUrl:urlString
                                        openType:(IJKMediaUrlOpenType)type
                                    segmentIndex:realData->segment_index
                                    retryCounter:realData->retry_counter];

    [delegate willOpenUrl:openData];
    if (openData.error < 0) return -1;

    if (openData.isHandled) {
        realData->is_handled = YES;
        if (openData.isUrlChanged && openData.url != nil) {
            realData->is_url_changed = YES;
            const char* newUrlUTF8 = [openData.url UTF8String];
            strlcpy(realData->url, newUrlUTF8, sizeof(realData->url));
            realData->url[sizeof(realData->url) - 1] = 0;
        }
    }

    return 0;
}

// NOTE: could be called from multiple thread
static int ijkff_inject_callback(void* opaque, int message, void* data, size_t data_size) {
    IJKFFMoviePlayerController* mpc = (__bridge IJKFFMoviePlayerController*)opaque;

    switch (message) {
        case IJKAVINJECT_CONCAT_RESOLVE_SEGMENT:
            return onInjectUrlOpen(mpc, mpc.segmentOpenDelegate, message, data, data_size);
        case IJKAVINJECT_ON_TCP_OPEN:
            return onInjectUrlOpen(mpc, mpc.tcpOpenDelegate, message, data, data_size);
        case IJKAVINJECT_ON_HTTP_OPEN:
            return onInjectUrlOpen(mpc, mpc.httpOpenDelegate, message, data, data_size);
        case IJKAVINJECT_ON_LIVE_RETRY:
            return onInjectUrlOpen(mpc, mpc.liveOpenDelegate, message, data, data_size);
        default: {
            return 0;
        }
    }
}

#pragma mark Airplay

- (BOOL)allowsMediaAirPlay {
    if (!self) return NO;
    return _allowsMediaAirPlay;
}

- (void)setAllowsMediaAirPlay:(BOOL)b {
    if (!self) return;
    _allowsMediaAirPlay = b;
}

- (BOOL)airPlayMediaActive {
    if (!self) return NO;
    if (_isDanmakuMediaAirPlay) {
        return YES;
    }
    return NO;
}

- (BOOL)isDanmakuMediaAirPlay {
    return _isDanmakuMediaAirPlay;
}

- (void)setIsDanmakuMediaAirPlay:(BOOL)isDanmakuMediaAirPlay {
    _isDanmakuMediaAirPlay = isDanmakuMediaAirPlay;
    if (_isDanmakuMediaAirPlay) {
        _glView.scaleFactor = 1.0f;
    } else {
        CGFloat scale = [[UIScreen mainScreen] scale];
        if (scale < 0.1f) scale = 1.0f;
        _glView.scaleFactor = scale;
    }
    [[NSNotificationCenter defaultCenter]
        postNotificationName:IJKMPMoviePlayerIsAirPlayVideoActiveDidChangeNotification
                      object:nil
                    userInfo:nil];
}

#pragma mark Option Conventionce

- (void)setFormatOptionValue:(NSString*)value forKey:(NSString*)key {
    [self setOptionValue:value forKey:key ofCategory:kIJKFFOptionCategoryFormat];
}

- (void)setCodecOptionValue:(NSString*)value forKey:(NSString*)key {
    [self setOptionValue:value forKey:key ofCategory:kIJKFFOptionCategoryCodec];
}

- (void)setSwsOptionValue:(NSString*)value forKey:(NSString*)key {
    [self setOptionValue:value forKey:key ofCategory:kIJKFFOptionCategorySws];
}

- (void)setPlayerOptionValue:(NSString*)value forKey:(NSString*)key {
    [self setOptionValue:value forKey:key ofCategory:kIJKFFOptionCategoryPlayer];
}

- (void)setFormatOptionIntValue:(int64_t)value forKey:(NSString*)key {
    [self setOptionIntValue:value forKey:key ofCategory:kIJKFFOptionCategoryFormat];
}

- (void)setCodecOptionIntValue:(int64_t)value forKey:(NSString*)key {
    [self setOptionIntValue:value forKey:key ofCategory:kIJKFFOptionCategoryCodec];
}

- (void)setSwsOptionIntValue:(int64_t)value forKey:(NSString*)key {
    [self setOptionIntValue:value forKey:key ofCategory:kIJKFFOptionCategorySws];
}

- (void)setPlayerOptionIntValue:(int64_t)value forKey:(NSString*)key {
    [self setOptionIntValue:value forKey:key ofCategory:kIJKFFOptionCategoryPlayer];
}

- (void)setMaxBufferSize:(int)maxBufferSize {
    [self setPlayerOptionIntValue:maxBufferSize forKey:@"max-buffer-size"];
}

#pragma mark app state changed

- (void)registerApplicationObservers {
    [_notificationManager addObserver:self
                             selector:@selector(audioSessionInterrupt:)
                                 name:AVAudioSessionInterruptionNotification
                               object:nil];

    [_notificationManager addObserver:self
                             selector:@selector(applicationWillEnterForeground)
                                 name:UIApplicationWillEnterForegroundNotification
                               object:nil];

    [_notificationManager addObserver:self
                             selector:@selector(applicationDidBecomeActive)
                                 name:UIApplicationDidBecomeActiveNotification
                               object:nil];

    [_notificationManager addObserver:self
                             selector:@selector(applicationWillResignActive)
                                 name:UIApplicationWillResignActiveNotification
                               object:nil];

    [_notificationManager addObserver:self
                             selector:@selector(applicationDidEnterBackground)
                                 name:UIApplicationDidEnterBackgroundNotification
                               object:nil];

    [_notificationManager addObserver:self
                             selector:@selector(applicationWillTerminate)
                                 name:UIApplicationWillTerminateNotification
                               object:nil];

    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(onPlayerLoadStateChangedOld:)
                                                 name:IJKMPMoviePlayerLoadStateDidChangeNotification
                                               object:self];
}

- (void)unregisterApplicationObservers {
    [_notificationManager removeAllObservers:self];
    [[NSNotificationCenter defaultCenter]
        removeObserver:self];  // for onPlayerLoadStateChangedOld only
}

- (void)audioSessionInterrupt:(NSNotification*)notification {
    int reason = [[[notification userInfo] valueForKey:AVAudioSessionInterruptionTypeKey] intValue];
    switch (reason) {
        case AVAudioSessionInterruptionTypeBegan: {
            NSLog(@"audioSessionInterrupt: begin\n");
            switch (self.playbackState) {
                case IJKMPMoviePlaybackStatePaused:
                    if (_currentState == IJKMPMoviePlaybackStatePlaying) {
                        _playingBeforeInterruption = YES;
                    } else {
                        _playingBeforeInterruption = NO;
                    }
                    break;
                case IJKMPMoviePlaybackStateStopped:
                    _playingBeforeInterruption = NO;
                    break;
                default:
                    _playingBeforeInterruption = YES;
                    break;
            }
            [self pause];
            [self liveAudioOnly:NO];  // not enter audioonly in pause cases
            [[IJKAudioKit sharedInstance] setActive:NO];
            break;
        }
        case AVAudioSessionInterruptionTypeEnded: {
            NSLog(@"audioSessionInterrupt: end\n");
            [[IJKAudioKit sharedInstance] setActive:YES];
            if (_playingBeforeInterruption) {
                [self play];
            }
            break;
        }
    }
}

- (void)applicationWillEnterForeground {
    NSLog(@"applicationWillEnterForeground: %d",
          (int)[UIApplication sharedApplication].applicationState);
}

- (void)applicationDidBecomeActive {
    NSLog(@"applicationDidBecomeActive: %d",
          (int)[UIApplication sharedApplication].applicationState);
}

- (void)applicationWillResignActive {
#if 0
    if (self.playbackState == IJKMPMoviePlaybackStatePlaying || self.playbackState == IJKMPMoviePlaybackStatePaused) {
        [[UIApplication sharedApplication] beginReceivingRemoteControlEvents];

    } else {
        [[UIApplication sharedApplication] endReceivingRemoteControlEvents];

    }
#endif
    NSLog(@"applicationWillResignActive: %d",
          (int)[UIApplication sharedApplication].applicationState);
    dispatch_async(dispatch_get_main_queue(), ^{
        if (_pauseInBackground) {
            [self pause];
        }
    });
}

- (void)applicationDidEnterBackground {
    NSLog(@"applicationDidEnterBackground: %d",
          (int)[UIApplication sharedApplication].applicationState);
    dispatch_async(dispatch_get_main_queue(), ^{
        if (_pauseInBackground) {
            [self pause];
        }
    });
}

- (void)applicationWillTerminate {
    NSLog(@"applicationWillTerminate: %d", (int)[UIApplication sharedApplication].applicationState);
    dispatch_async(dispatch_get_main_queue(), ^{
        if (_pauseInBackground) {
            [self pause];
        }
    });
}

#pragma kwai code that has to remain here in IJKFFMoviePlayerController

- (void)setRotateDegress:(int)degress {
    if (degress < 0 || degress > 270 || degress % 90 != 0 || degress == _rotateDegress) return;

    if (_glView) {
        [_glView setRotateDegress:degress];
        _rotateDegress = degress;
    }
}

- (void)setMirrorOn:(BOOL)on {
    if (_glView) [_glView setMirror:on];
}

+ (UIView*)newGLView {
    IJKSDLGLView* glView = [[IJKSDLGLView alloc] initWithFrame:[[UIScreen mainScreen] bounds]
                                                     sessionId:-1];
    glView.shouldShowHudView = NO;
    glView.isReusedView = YES;
    return glView;
}

- (void)setGLView:(UIView*)view {
    if ([view isKindOfClass:[IJKSDLGLView class]]) {
        _glView = (IJKSDLGLView*)view;
        _view = _glView;
        ijkmp_ios_set_glview(_mediaPlayer, _glView);
        [_glView resetViewStatus];
    } else {
        assert(0);
    }
}

- (NSDictionary*)getMetadata {
    if (nil == _mediaPlayer) {
        return nil;
    } else {
        IjkMediaMeta* meta = ijkmp_get_meta_l(_mediaPlayer);
        if (nil == meta) {
            return nil;
        }

        NSMutableDictionary* dict = [[NSMutableDictionary alloc] init];

        fillMetaInternal(dict, meta, IJKM_KEY_FORMAT, nil);
        fillMetaInternal(dict, meta, IJKM_KEY_DURATION_US, nil);
        fillMetaInternal(dict, meta, IJKM_KEY_START_US, nil);
        fillMetaInternal(dict, meta, IJKM_KEY_BITRATE, nil);

        // fillMetaInternal(dict, meta, IJKM_KEY_VIDEO_STREAM, nil);
        // fillMetaInternal(dict, meta, IJKM_KEY_AUDIO_STREAM, nil);
        fillMetaInternal(dict, meta, IJKM_KEY_HTTP_CONNECT_TIME, nil);
        fillMetaInternal(dict, meta, IJKM_KEY_HTTP_FIRST_DATA_TIME, nil);
        fillMetaInternal(dict, meta, IJKM_KEY_HTTP_ANALYZE_DNS, nil);
        fillMetaInternal(dict, meta, IJKM_KEY_STREAMID, nil);
        return dict;
    }
}

- (NSInteger)getDnsTime {
    if (NULL == _mediaPlayer) return -1;
    IjkMediaMeta* rawMeta = ijkmp_get_meta_l(_mediaPlayer);
    int64_t value = ijkmeta_get_int64_l(rawMeta, IJKM_KEY_HTTP_ANALYZE_DNS, 0);

    return (NSInteger)value;
}

- (NSInteger)getConnectTime {
    if (NULL == _mediaPlayer) return -1;

    IjkMediaMeta* rawMeta = ijkmp_get_meta_l(_mediaPlayer);
    const char* value = ijkmeta_get_string_l(rawMeta, IJKM_KEY_HTTP_CONNECT_TIME);

    if (NULL == value) {
        return 0;
    }
    NSString* time = [NSString stringWithUTF8String:value];
    return [time integerValue];
}

- (NSInteger)getResponseTime {
    if (NULL == _mediaPlayer) return -1;
    IjkMediaMeta* rawMeta = ijkmp_get_meta_l(_mediaPlayer);
    const char* value = ijkmeta_get_string_l(rawMeta, IJKM_KEY_HTTP_FIRST_DATA_TIME);

    if (NULL == value) {
        return 0;
    }
    NSString* time = [NSString stringWithUTF8String:value];
    return [time integerValue];
}

- (NSInteger)getHttpCode {
    if (NULL == _mediaPlayer) return -1;
    IjkMediaMeta* rawMeta = ijkmp_get_meta_l(_mediaPlayer);
    const char* value = ijkmeta_get_string_l(rawMeta, IJKM_KEY_HTTP_CODE);

    if (NULL == value) {
        return 0;
    }
    NSString* code = [NSString stringWithUTF8String:value];
    return [code integerValue];
}

- (NSString*)getCache {
    if (NULL == _mediaPlayer) return NULL;
    IjkMediaMeta* rawMeta = ijkmp_get_meta_l(_mediaPlayer);
    const char* value = ijkmeta_get_string_l(rawMeta, IJKM_KEY_HTTP_X_CACHE);

    if (NULL == value) {
        return nil;
    }
    NSString* cache = [NSString stringWithUTF8String:value];
    return cache;
}

- (NSString*)getKwaiSign {
    if (NULL == _mediaPlayer) {
        return NULL;
    }
    char kwaisign[CDN_KWAI_SIGN_MAX_LEN] = "";
    ijkmp_get_vod_kwai_sign(_mediaPlayer, kwaisign);
    NSString* sign = [NSString stringWithUTF8String:kwaisign];
    return sign;
}

- (NSString*)getXksCache {
    if (NULL == _mediaPlayer) {
        return NULL;
    }
    char xkscache[CDN_X_KS_CACHE_MAX_LEN] = "";
    ijkmp_get_x_ks_cache(_mediaPlayer, xkscache);
    NSString* x_ks_cache = [NSString stringWithUTF8String:xkscache];
    return x_ks_cache;
}

- (NSString*)getKwaiLiveVocieComment:(int64_t)time {
    if (NULL == _mediaPlayer) {
        return NULL;
    }
    char vc[KWAI_LIVE_VOICE_COMMENT_LEN] = "";
    ijkmp_get_live_voice_comment(_mediaPlayer, vc, time);
    NSString* live_vc = [NSString stringWithUTF8String:vc];
    return live_vc;
}

@end
