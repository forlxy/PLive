//
//  KwaiPlayerVodBuilder.m
//  IJKMediaFramework
//
//  Created by MarshallShuai on 2019/9/19.
//  Copyright © 2019 kuaishou. All rights reserved.
//

#import "KwaiPlayerVodBuildParams.h"

@implementation KwaiPlayerVodBuildParams

- (instancetype)init {
    self = [super init];
    if (self) {
        _enableCache = YES;
        _ijkFFOptions = [IJKFFOptions optionsByDefault];
    }
    return self;
}

@end
