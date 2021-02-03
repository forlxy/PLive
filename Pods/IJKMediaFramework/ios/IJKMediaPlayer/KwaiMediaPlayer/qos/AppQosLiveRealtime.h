//
//  AppQosLiveRealtime.m
//  IJKMediaFramework
//
//  Created by 帅龙成 on 26/02/2018.
//  Copyright © 2018 kuaishou. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "KwaiFFPlayerController.h"
#import "PlayerTempDef.h"
#import "ijkplayer.h"

@interface AppQosLiveRealtime : NSObject

- (instancetype)initWith:(KwaiFFPlayerController*)kwaiPlayer NS_DESIGNATED_INITIALIZER;

- (void)startReport:(KSYPlyQosStatBlock)qosStatBlock reportIntervalMs:(int64_t)interval;
- (void)stopReport;
@end
