//
//  AcCallbackInfo.m
//  IJKMediaFramework
//
//  Created by MarshallShuai on 2019/2/1.
//  Copyright © 2019 kuaishou. All rights reserved.
//

#import "KSAcCallbackInfo.h"

@implementation KSAcCallbackInfo

- (BOOL)isFullyCached {
    return _totalBytes > 0 && _progressPosition == _totalBytes;
}

@end
