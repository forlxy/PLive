//
//  KwaiPlayerDebugInfo.m
//  IJKMediaFramework
//
//  Created by MarshallShuai on 2018/12/13.
//  Copyright © 2018 kuaishou. All rights reserved.
//

#import "KwaiPlayerDebugInfo.h"
#import "KwaiFFPlayerController.h"

@interface KwaiPlayerDebugInfo ()

@property(nonatomic) BOOL isLive;

@end

@implementation KwaiPlayerDebugInfo

- (instancetype)init {
    @throw [NSException exceptionWithName:NSInternalInconsistencyException
                                   reason:@"-init is not a valid initializer "
                                          @"for the class KwaiPlayerDebugInfo"
                                 userInfo:nil];
    return nil;
}

- (instancetype)initWith:(KwaiFFPlayerController*)kwaiPlayer {
    self = [super init];
    if (self) {
        _isLive = kwaiPlayer.isLive;
        _appLiveDebugInfo = [[AppLiveQosDebugInfo alloc] initWith:kwaiPlayer];
        _appVodDebugInfo = [[AppVodQosDebugInfo alloc] initWith:kwaiPlayer];
        _appPlayerDebugInfo = [[AppPlayerConfigDebugInfo alloc] initWith:kwaiPlayer];
    }
    return self;
}

- (void)refresh {
    [_appPlayerDebugInfo refresh];

    if (_isLive) {
        [_appLiveDebugInfo refresh];
    } else {
        [_appVodDebugInfo refresh];
    }
}

@end
