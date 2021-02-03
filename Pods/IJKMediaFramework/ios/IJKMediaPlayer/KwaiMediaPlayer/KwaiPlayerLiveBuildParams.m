//
//  KwaiPlayerLiveBuilder.m
//  IJKMediaFramework
//
//  Created by MarshallShuai on 2019/9/20.
//  Copyright © 2019 kuaishou. All rights reserved.
//

#import "KwaiPlayerLiveBuildParams.h"

@implementation KwaiPlayerLiveBuildParams

- (instancetype)init {
    self = [super init];
    if (self) {
        _enableCache = NO;
        _ijkFFOptions = [IJKFFOptions optionsByDefault];
    }
    return self;
}

@end
