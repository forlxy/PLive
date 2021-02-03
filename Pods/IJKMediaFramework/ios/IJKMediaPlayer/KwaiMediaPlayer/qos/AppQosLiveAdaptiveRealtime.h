//
//  AppQosLiveAdaptiveRealtime.m
//  IJKMediaFramework
//
//  Created by wangtao03 on 2018/3/6.
//  Copyright © 2018年 kuaishou. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "KwaiFFPlayerController.h"
#import "PlayerTempDef.h"
#import "ijkplayer.h"

@interface AppQosLiveAdaptiveRealtime : NSObject

@property(nonatomic) NSString* host;

- (instancetype)init NS_UNAVAILABLE;

- (instancetype)initWith:(KwaiFFPlayerController*)kwaiPlayer NS_DESIGNATED_INITIALIZER;

- (void)startReport:(KSYPlyQosStatBlock)qosStatBlock
    reportIntervalMs:(int64_t)interval
    enableAdditional:(BOOL)enable;
- (void)stopReport;

@end
