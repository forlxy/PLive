//
//  KSYQosInfo.h
//  IJKMediaPlayer
//
//  Created by 崔崔 on 16/3/14.
//  Copyright © 2016年 bilibili. All rights reserved.
//

#import <Foundation/Foundation.h>

/**
 * Qos信息
 */
@interface KSYQosInfo : NSObject

/**
    audio queue size in bytes
 */
@property(nonatomic, assign) int audioBufferByteLength;
/**
    audio queue time length in ms
 */
@property(nonatomic, assign) int audioBufferTimeLength;
/**
    size of data have arrived at audio queue since playing. unit:byte
 */
@property(nonatomic, assign) int64_t audioTotalDataSize;
/**
    video queue size in bytes
 */
@property(nonatomic, assign) int videoBufferByteLength;
/**
    video queue time length in ms
 */
@property(nonatomic, assign) int videoBufferTimeLength;
/**
    size of data have arrived at video queue since playing. unit:byte
 */
@property(nonatomic, assign) int64_t videoTotalDataSize;
/**
    size of total audio and video data since playing. unit: byte
 */
@property(nonatomic, assign) int64_t totalDataSize;
/**
    audio render delay, calculated with PTS and WallClock. unit: milliseconds
 */
@property(nonatomic, assign) int audioDelay;
/**
    video recv delay, calculated with PTS and WallClock. unit: milliseconds
 */
@property(nonatomic, assign) int videoDelayRecv;
/**
    video beforeDec delay, calculated with PTS and WallClock. unit: milliseconds
 */
@property(nonatomic, assign) int videoDelayBefDec;
/**
    video afterDec delay, calculated with PTS and WallClock. unit: milliseconds
 */
@property(nonatomic, assign) int videoDelayAftDec;
/**
    video render delay, calculated with PTS and WallClock. unit: milliseconds
 */
@property(nonatomic, assign) int videoDelayRender;

/**
    first screen time(total), from starting playing to rendering first video
   frame. unit: milliseconds Total ~= (InputOpen + StreamFind + CodecOpen +
   PktReceive + PreDecode + Decode + Render)
 */
@property(nonatomic, assign) int firstScreenTimeTotal;

/**
    duration of DNS analyzing, part of first screen time
 */
@property(nonatomic, assign) int firstScreenTimeDnsAnalyze;

/**
    duration of HTTP connecting, part of first screen time
 */
@property(nonatomic, assign) int firstScreenTimeHttpConnect;

/**
    duration of opening input stream(including DnsAnalyze && HttpConnect), part
   of first screen time
 */
@property(nonatomic, assign) int firstScreenTimeInputOpen;

/**
    duration of finding best a/v streams, part of first screen time
 */
@property(nonatomic, assign) int firstScreenTimeStreamFind;

/**
    duration of opening codec, part of first screen time
 */
@property(nonatomic, assign) int firstScreenTimeCodecOpen;

/**
 duration of all prepared, part of first screen time
 */
@property(nonatomic, assign) int firstScreenTimeAllPrepared;

/**
 duration of calling start by app, not part of first screen time
 */
@property(nonatomic, assign) int firstScreenTimeWaitForPlay;

/**
    duration of receiving first video packet, part of first screen time
 */
@property(nonatomic, assign) int firstScreenTimePktReceive;

/**
    duration from receiving first pkt to sending it to decoder, part of first
   screen time
 */
@property(nonatomic, assign) int firstScreenTimePreDecode;

/**
    duration of decoding first video frame, part of first screen time
 */
@property(nonatomic, assign) int firstScreenTimeDecode;

/**
    duration of rendering first video frame, part of first screen time
 */
@property(nonatomic, assign) int firstScreenTimeRender;

/**
    duration of dropped media pkts, at starting phase of playing
 */
@property(nonatomic, assign) int firstScreenTimeDroppedDuration;

/**
    total duration of dropped media pkts
 */
@property(nonatomic, assign) int totalDroppedDuration;

/**
    live host info
 */
@property(nonatomic) NSString* hostInfo;

/**
    video encoder initial configs
 */
@property(nonatomic) NSString* vencInit;

/**
    audio encoder initial configs
 */
@property(nonatomic) NSString* aencInit;

/**
    video encoder dynamic params
 */
@property(nonatomic) NSString* vencDynamic;

/**
    comment info in metadata
 */
@property(nonatomic) NSString* comment;

// for AwesomeCache
@property(nonatomic) NSString* currentReadUri;
@property(nonatomic) int64_t totalBytes;
@property(nonatomic) int64_t cachedBytes;
@property(nonatomic) int reopenCntBySeek;

@end
