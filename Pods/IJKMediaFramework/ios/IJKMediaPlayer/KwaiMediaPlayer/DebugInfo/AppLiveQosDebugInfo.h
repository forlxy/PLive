//
//  AppLiveQosDebugInfo.h
//  IJKMediaFramework
//
//  Created by wangtao03 on 2018/4/26.
//  Copyright © 2018年 kuaishou. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

@class KwaiFFPlayerController;

// Live debug info
@interface AppLiveQosDebugInfo : NSObject

- (instancetype)init NS_UNAVAILABLE;

- (instancetype)initWith:(KwaiFFPlayerController*)kwaiPlayer NS_DESIGNATED_INITIALIZER;

- (void)refresh;

@property(nonatomic) NSString* serverAddress;
@property(nonatomic) NSString* naturalSizeString;
@property(nonatomic) double readSize;
// audio queue size in bytes
@property(nonatomic) int audioBufferByteLength;
// video queue size in bytes
@property(nonatomic) int videoBufferByteLength;
// size of data have arrived at audio queue since playing
@property(nonatomic) int64_t audioTotalDataSize;
// size of data have arrived at video queue since playing
@property(nonatomic) int64_t videoTotalDataSize;
// size of total audio and video data since playing
@property(nonatomic) int64_t totalDataSize;
// buffer loading in ms
@property(nonatomic) int64_t blockDuration;
// first video frame received in ms
@property(nonatomic) int64_t firstFrameReceived;
// speed up threshold
@property(nonatomic) int speedupThresholdMs;
// whether live adaptive
@property(nonatomic) BOOL isLiveManifest;
// collect device type
@property(nonatomic) int sourceDeviceType;
// live adaptive current playing bitrate
@property(nonatomic) int kflvPlayingBitrate;
// live adaptive current bandwidth
@property(nonatomic) int kflvBandwidthCurrent;
@property(nonatomic) int kflvBandwidthFragment;
// live adaptive current buffer and threshold
@property(nonatomic) int kflvCurrentBufferMs;
@property(nonatomic) int kflvEstimateBufferMs;
@property(nonatomic) int kflvPredictedBufferMs;
@property(nonatomic) int kflvSpeedupThresholdMs;

// audio queue time length in ms
@property(nonatomic) int audioBufferTimeLength;
// video queue time length in ms
@property(nonatomic) int videoBufferTimeLength;
// first screen time(total), in milliseconds, include:
// InputOpen + StreamFind + CodecOpen + PktReceive + PreDecode + Decode + Render
@property(nonatomic) int firstScreenTimeTotal;
// duration of calling start by app, not part of first screen time
@property(nonatomic) int firstScreenTimeWaitForPlay;
// duration of opening input stream(including DnsAnalyze && HttpConnect), part
// of first screen time
@property(nonatomic) int firstScreenTimeInputOpen;
// duration of DNS analyzing, part of first screen time
@property(nonatomic) int firstScreenTimeDnsAnalyze;
// duration of HTTP connecting, part of first screen time
@property(nonatomic) int firstScreenTimeHttpConnect;
// duration of finding best a/v streams, part of first screen time
@property(nonatomic) int firstScreenTimeStreamFind;
// duration of opening codec, part of first screen time
@property(nonatomic) int firstScreenTimeCodecOpen;
// duration of receiving first video packet, part of first screen time
@property(nonatomic) int firstScreenTimePktReceive;
// duration from receiving first pkt to sending it to decoder, part of first
// screen time
@property(nonatomic) int firstScreenTimePreDecode;
// duration of decoding first video frame, part of first screen time
@property(nonatomic) int firstScreenTimeDecode;
// duration of rendering first video frame, part of first screen time
@property(nonatomic) int firstScreenTimeRender;
// duration of dropped media pkts, at starting phase of playing
@property(nonatomic) int firstScreenTimeDroppedDuration;
// total duration of dropped media pkts
@property(nonatomic) int totalDroppedDuration;
// buffer loading count
@property(nonatomic) int blockCnt;
// frame rate of reading packets
@property(nonatomic) float videoReadFramesPerSecond;
// frame rate of decoding packets
@property(nonatomic) float videoDecodeFramesPerSecond;
// frame rate of displaying frames
@property(nonatomic) float videoDisplayFramesPerSecond;

// audio render delay, calculated with PTS and WallClock. unit: milliseconds
@property(nonatomic) int audioDelay;
// video recv delay, calculated with PTS and WallClock. unit: milliseconds
@property(nonatomic) int videoDelayRecv;
// video beforeDec delay, calculated with PTS and WallClock. unit: milliseconds
@property(nonatomic) int videoDelayBefDec;
// video afterDec delay, calculated with PTS and WallClock. unit: milliseconds
@property(nonatomic) int videoDelayAftDec;
// video render delay, calculated with PTS and WallClock. unit: milliseconds
@property(nonatomic) int videoDelayRender;
// play url
@property(nonatomic) NSString* playUrl;
// host
@property(nonatomic) NSString* host;
// first screen step cost
@property(nonatomic) NSString* firstScreenStepCostInfo;
// video decoder info
@property(nonatomic) NSString* videoDecoder;
// audio decoder info
@property(nonatomic) NSString* audioDecoder;

// live host info
@property(nonatomic) NSString* hostInfo;
// video encoder initial configs
@property(nonatomic) NSString* vencInit;
// audio encoder initial configs
@property(nonatomic) NSString* aencInit;
// video encoder dynamic params
@property(nonatomic) NSString* vencDynamic;
// comment info in metadata
@property(nonatomic) NSString* comment;

@end
