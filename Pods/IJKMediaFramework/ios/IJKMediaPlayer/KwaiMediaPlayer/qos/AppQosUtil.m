//
//  AppQosUtil.m
//  IJKMediaFramework
//
//  Created by 帅龙成 on 26/02/2018.
//  Copyright © 2018 kuaishou. All rights reserved.
//

#import "AppQosUtil.h"
#import <Foundation/Foundation.h>
#import "cpu_resource.h"
#import "ff_ffplay_def.h"

@implementation AppQosUtil

+ (int64_t)getTimestamp {
    return [[NSDate date] timeIntervalSince1970] * 1000;
}

+ (int32_t)getCpuUsagePercent {
    float usage = 0.0f;
    getProcessCpuUsage(&usage);
    return usage * 100;
}

@end
