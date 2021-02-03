//
//  KwaiPlayerDebugInfo.h
//  IJKMediaFramework
//
//  Created by MarshallShuai on 2018/12/13.
//  Copyright © 2018 kuaishou. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "AppLiveQosDebugInfo.h"
#import "AppPlayerConfigDebugInfo.h"
#import "AppVodQosDebugInfo.h"

NS_ASSUME_NONNULL_BEGIN

@class KwaiFFPlayerController;

@interface KwaiPlayerDebugInfo : NSObject

- (instancetype)initWith:(KwaiFFPlayerController*)kwaiPlayer;

- (void)refresh;

@property(nonatomic, readonly) AppPlayerConfigDebugInfo* appPlayerDebugInfo;

@property(nonatomic, readonly) AppLiveQosDebugInfo* appLiveDebugInfo;

@property(nonatomic, readonly) AppVodQosDebugInfo* appVodDebugInfo;

@end

NS_ASSUME_NONNULL_END
