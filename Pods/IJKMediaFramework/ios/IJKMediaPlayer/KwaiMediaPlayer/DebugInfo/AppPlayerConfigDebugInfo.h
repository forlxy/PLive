//
//  AppPlayerConfigDebugInfo.h
//  IJKMediaFramework
//
//  Created by MarshallShuai on 2018/12/13.
//  Copyright © 2018 kuaishou. All rights reserved.
//

#import <Foundation/Foundation.h>

@class KwaiFFPlayerController;

@interface AppPlayerConfigDebugInfo : NSObject

- (instancetype)initWith:(KwaiFFPlayerController*)kwaiPlayer NS_DESIGNATED_INITIALIZER;

- (void)refresh;

- (NSString*)getPrettySingleText;

@property(nonatomic) int playerMaxBufDurMs;
@property(nonatomic) int playerStartOnPrepared;

@property(nonatomic) int cacheBufferDataSourceSizeKb;
@property(nonatomic) int cacheSeekReopenTHKb;

@property(nonatomic) NSString* cacheDataSourceType;
@property(nonatomic) int cacheFlags;

@property(nonatomic) int cacheProgressCbIntervalMs;

@property(nonatomic) NSString* cacheHttpType;
@property(nonatomic) NSString* cacheCurlType;
@property(nonatomic) int cacheHttpMaxRetryCnt;

@property(nonatomic) int cacheConnectTimeoutMs;
@property(nonatomic) int cacheReadTimeoutMs;
@property(nonatomic) int cacheCurlBufferSizeKb;
@property(nonatomic) int cacheSocketOrigKb;
@property(nonatomic) int cacheSocketCfgKb;
@property(nonatomic) int cacheSocketActKb;
@property(nonatomic) NSString* cacheHeader;
@property(nonatomic) NSString* cacheUserAgent;

@end
