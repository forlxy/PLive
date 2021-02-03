//
//  AppQosUtil.h
//  IJKMediaPlayer
//
//  Created by 帅龙成 on 26/02/2018.
//  Copyright © 2018 kuaishou. All rights reserved.
//

#ifndef AppQosUtil_h
#define AppQosUtil_h

#import <Foundation/Foundation.h>

@interface AppQosUtil : NSObject

+ (int64_t)getTimestamp;

+ (int32_t)getCpuUsagePercent;
@end

#endif /* AppQosUtil_h */
