/*
 * IJKFFMoviePlayerController.h
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

#import <AVFoundation/AVFoundation.h>
#import "IJKFFOptions.h"
#import "IJKMediaPlayback.h"
// media meta
#define k_IJKM_KEY_FORMAT @"format"
#define k_IJKM_KEY_DURATION_US @"duration_us"
#define k_IJKM_KEY_START_US @"start_us"
#define k_IJKM_KEY_BITRATE @"bitrate"

// stream meta
#define k_IJKM_KEY_TYPE @"type"
#define k_IJKM_VAL_TYPE__VIDEO @"video"
#define k_IJKM_VAL_TYPE__AUDIO @"audio"
#define k_IJKM_VAL_TYPE__UNKNOWN @"unknown"

#define k_IJKM_KEY_CODEC_NAME @"codec_name"
#define k_IJKM_KEY_CODEC_PROFILE @"codec_profile"
#define k_IJKM_KEY_CODEC_LONG_NAME @"codec_long_name"

// stream: video
#define k_IJKM_KEY_WIDTH @"width"
#define k_IJKM_KEY_HEIGHT @"height"
#define k_IJKM_KEY_FPS_NUM @"fps_num"
#define k_IJKM_KEY_FPS_DEN @"fps_den"
#define k_IJKM_KEY_TBR_NUM @"tbr_num"
#define k_IJKM_KEY_TBR_DEN @"tbr_den"
#define k_IJKM_KEY_SAR_NUM @"sar_num"
#define k_IJKM_KEY_SAR_DEN @"sar_den"
// stream: audio
#define k_IJKM_KEY_SAMPLE_RATE @"sample_rate"
#define k_IJKM_KEY_CHANNEL_LAYOUT @"channel_layout"

#define kk_IJKM_KEY_STREAMS @"streams"

typedef enum IJKLogLevel {
    k_IJK_LOG_UNKNOWN = 0,
    k_IJK_LOG_DEFAULT = 1,
    k_IJK_LOG_VERBOSE = 2,
    k_IJK_LOG_DEBUG = 3,
    k_IJK_LOG_INFO = 4,
    k_IJK_LOG_WARN = 5,
    k_IJK_LOG_ERROR = 6,
    k_IJK_LOG_FATAL = 7,
    k_IJK_LOG_SILENT = 8,
} IJKLogLevel;

@interface IJKFFMoviePlayerController : NSObject <IJKMediaPlayback>

- (id)initWithContentURL:(NSURL*)aUrl withOptions:(IJKFFOptions*)options;

- (id)initWithContentURLString:(NSString*)aUrlString withOptions:(IJKFFOptions*)options;

- (void)prepareToPlay;
- (void)play;
- (void)pause;
- (void)step;
- (void)stop;
- (BOOL)isPlaying;
- (int64_t)trafficStatistic;

- (void)setPauseInBackground:(BOOL)pause;
- (BOOL)isVideoToolboxOpen;

- (void)setPlaybackTone:(int)playbackTone;

// 获取CDN kwai sign
- (NSString*)getKwaiSign;

// 获取CDN X-Ks-Cache
- (NSString*)getXksCache;

// Only for Kwai backend Live playback
// 1. cannot be used standalone
// 2. could only be used during live video playback
// 3. audioonly and previous video has the same prefix
// 4. parameter "boolean on" defination:
// on: true, audio only
// on: false, recover video playback
- (void)liveAudioOnly:(BOOL)on;

+ (void)setLogReport:(BOOL)preferLogReport;
+ (void)setLogLevel:(IJKLogLevel)logLevel;
+ (void)setKwaiLogLevel:(IJKLogLevel)logLevel;
+ (BOOL)checkIfFFmpegVersionMatch:(BOOL)showAlert;
+ (BOOL)checkIfPlayerVersionMatch:(BOOL)showAlert
                            major:(unsigned int)major
                            minor:(unsigned int)minor
                            micro:(unsigned int)micro;

@property(nonatomic, readonly) CGFloat fpsInMeta;
@property(nonatomic, readonly) CGFloat fpsAtOutput;
@property(nonatomic) BOOL shouldShowHudView;

- (void)setOptionValue:(NSString*)value
                forKey:(NSString*)key
            ofCategory:(IJKFFOptionCategory)category;

- (void)setOptionIntValue:(int64_t)value
                   forKey:(NSString*)key
               ofCategory:(IJKFFOptionCategory)category;

- (void)setFormatOptionValue:(NSString*)value forKey:(NSString*)key;
- (void)setCodecOptionValue:(NSString*)value forKey:(NSString*)key;
- (void)setSwsOptionValue:(NSString*)value forKey:(NSString*)key;
- (void)setPlayerOptionValue:(NSString*)value forKey:(NSString*)key;

- (void)setFormatOptionIntValue:(int64_t)value forKey:(NSString*)key;
- (void)setCodecOptionIntValue:(int64_t)value forKey:(NSString*)key;
- (void)setSwsOptionIntValue:(int64_t)value forKey:(NSString*)key;
- (void)setPlayerOptionIntValue:(int64_t)value forKey:(NSString*)key;

@property(nonatomic, retain) id<IJKMediaUrlOpenDelegate> segmentOpenDelegate;
@property(nonatomic, retain) id<IJKMediaUrlOpenDelegate> tcpOpenDelegate;
@property(nonatomic, retain) id<IJKMediaUrlOpenDelegate> httpOpenDelegate;
@property(nonatomic, retain) id<IJKMediaUrlOpenDelegate> liveOpenDelegate;

- (void)didShutdown;

// ============ for kwai start ============
@property(nonatomic, readonly)
    struct IjkMediaPlayer* mediaPlayer;  // for subclass to access and should be readonly
@property(nonatomic, readonly) int playerDebugId;

// reload logic need it ,if we don't need reload, then we can put those two back
// to private instance variable
@property(nonatomic, readonly)
    BOOL keepScreenOnWhilePlaying;  // for subclass to access and should be readonly
- (void)setScreenOn:(BOOL)on;
- (void)setMirrorOn:(BOOL)on;

@property(nonatomic)
    BOOL shouldDisplayInternal;  // 用来控制是否内部自己显示，还是把画面输出，我们iOS的直播业务需要
@property(nonatomic, copy) void (^videoDataBlock)(CVPixelBufferRef pixelBuffer, int rotation);
@property(nonatomic, copy) void (^audioDataBlock)
    (CMSampleBufferRef sampleBuffer, int sampleRate, int channels);
@property(nonatomic) int rotateDegress;
@property(nonatomic) BOOL shouldVideoDropFrame;

// Live event pass-through callback
@property(nonatomic, copy) void (^liveEventBlock)(char* content, int length);

+ (UIView*)newGLView;
- (void)setGLView:(UIView*)view;

#pragma mark IjkMeta related
- (NSDictionary*)getMetadata;
- (NSInteger)getDnsTime;
- (NSInteger)getConnectTime;
- (NSInteger)getResponseTime;
- (NSInteger)getHttpCode;
- (NSString*)getCache;
- (NSString*)getVideoCodecInfo;
- (NSString*)getAudioCodecInfo;
// ============ for kwai end ============
@end

#define IJK_FF_IO_TYPE_READ (1)
void IJKFFIOStatDebugCallback(const char* url, int type, int bytes);
void IJKFFIOStatRegister(void (*cb)(const char* url, int type, int bytes));

void IJKFFIOStatCompleteDebugCallback(const char* url, int64_t read_bytes, int64_t total_size,
                                      int64_t elpased_time, int64_t total_duration);
void IJKFFIOStatCompleteRegister(void (*cb)(const char* url, int64_t read_bytes, int64_t total_size,
                                            int64_t elpased_time, int64_t total_duration));
