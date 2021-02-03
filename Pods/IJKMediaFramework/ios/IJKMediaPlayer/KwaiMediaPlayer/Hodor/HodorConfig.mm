//
//  HodorConfig.m
//  IJKMediaFramework
//
//  Created by MarshallShuai on 2019/9/24.
//  Copyright © 2019 kuaishou. All rights reserved.
//

#import "HodorConfig.h"
#import "hodor_downloader/hodor_defs.h"

@implementation HodorConfig

- (instancetype)init
{
    self = [super init];
    if (self) {
        _maxConcurrentCount = kHodorThreadWorkCountInit;
    }
    return self;
}

@end
