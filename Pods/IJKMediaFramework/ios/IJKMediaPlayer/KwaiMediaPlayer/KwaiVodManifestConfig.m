//
//  KwaiVodManifestConfig.m
//  IJKMediaFramework
//
//  Created by yuxin liu on 2019/8/15.
//  Copyright © 2019 kuaishou. All rights reserved.
//

#import "KwaiVodManifestConfig.h"

@implementation KwaiVodManifestConfig

- (id)init {
    self = [super init];
    if (self != nil) {
        _switchCode = 0;
        _signalStrength = 0;
        _lowDevice = 0;
        _rateConfig = nil;
        _rateConfigA1 = nil;
        _algorithm_mode = 0;
    }
    return self;
}

@end
