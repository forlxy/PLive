//
//  SpeedChart.h
//  IJKMediaFramework
//
//  Created by MarshallShuai on 2018/12/16.
//  Copyright © 2018 kuaishou. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "CurveView.h"

NS_ASSUME_NONNULL_BEGIN

@interface SpeedChart : CurveView

- (instancetype)initWithInterval:(NSTimeInterval)interval
                    maxSpeedKbps:(int)kbps
                     maxTimeline:(NSTimeInterval)period;

- (void)appendSpeedSample:(int)speedKbps;

@end

NS_ASSUME_NONNULL_END
