//
//  SpeedChart.m
//  IJKMediaFramework
//
//  Created by MarshallShuai on 2018/12/16.
//  Copyright © 2018 kuaishou. All rights reserved.
//

#import "SpeedChart.h"
#include "DebugInfoUtil.h"

#define ROW_COLUMN_LABLE_NUM 5
#define SPEED_INCREASE_STEP (ROW_COLUMN_LABLE_NUM * 100)  // 最大速度值以500K位单位扩大

#define colorWithAlpha(r, g, b, a) \
    [UIColor colorWithRed:(r) / 255.0 green:(g) / 255.0 blue:(b) / 255.0 alpha:(a)]

@interface SpeedChart ()

@property(nonatomic) NSTimeInterval intervalSec;
@property(nonatomic) int currentSampleCnt;

@end

@implementation SpeedChart

- (instancetype)initWithInterval:(NSTimeInterval)interval
                    maxSpeedKbps:(int)kbps
                     maxTimeline:(NSTimeInterval)periodSec {
    self = [super init];
    if (self) {
        _intervalSec = interval;

        self.rowMaxValue = periodSec;
        [self resetColNames];

        self.columnMaxValue = kbps;
        [self resetRowNames];

        self.gridRowCount = self.gridColumnCount = 1;
        self.gridLineColor = [DebugInfoUtil UIColor_Orange];

        self.pointValues = [[NSMutableArray alloc] init];
        //    self.rowLabelsWidth = 40;
        self.pointValues = @[].mutableCopy;

        self.delegate = nil;

        self.backgroundColor = [UIColor colorWithWhite:0.f alpha:0.f];
        self.fillLayerBackgroundColor = [UIColor orangeColor];
        self.rowLabelsTitleColor = self.columnLabelsTitleColor = [UIColor whiteColor];
        self.fillLayerBackgroundColor = colorWithAlpha(51, 181, 229, 0.5f);
        self.curveLineColor = colorWithAlpha(51, 181, 229, 0.8f);
        self.curveLineWidth = 1.5;
        self.drawWithAnimation = FALSE;
    }

    return self;
}

// 5行5列
- (void)resetRowNames {
    CGFloat gapKbps = self.columnMaxValue / ROW_COLUMN_LABLE_NUM;
    NSMutableArray* temp = [NSMutableArray new];
    for (int i = 0; i <= ROW_COLUMN_LABLE_NUM; i++) {
        [temp addObject:[NSString
                            stringWithFormat:@"%6.0f kbps", (ROW_COLUMN_LABLE_NUM - i) * gapKbps]];
    }
    self.rowNames = temp;
}

- (void)resetColNames {
    CGFloat gapInterval = self.rowMaxValue / ROW_COLUMN_LABLE_NUM;
    NSMutableArray* temp = [NSMutableArray new];
    for (int i = 0; i <= ROW_COLUMN_LABLE_NUM; i++) {
        [temp addObject:[NSString stringWithFormat:@"%3.1f秒", i * gapInterval]];
    }
    self.columnNames = temp;
}

- (void)appendSpeedSample:(int)speedKbps {
    CGFloat timeRowVal = _currentSampleCnt * _intervalSec;
    CGFloat speedColumnVal = speedKbps;

    if (timeRowVal > self.rowMaxValue) {
        self.rowMaxValue += 2;
        [self resetColNames];
        [self setColumnLabelsContainer];
    }

    if (speedColumnVal > self.columnMaxValue) {
        int maxLevel = (speedColumnVal / SPEED_INCREASE_STEP + 1) * SPEED_INCREASE_STEP;

        self.columnMaxValue = maxLevel;
        [self resetRowNames];
        [self setRowLabelsContainer];
    }

    [self.pointValues addObject:@{
        CurveViewPointValuesRowValueKey : @(timeRowVal),
        CurveViewPointValuesColumnValueKey : @(speedColumnVal)
    }];

    _currentSampleCnt++;

    [super setCurveLine];
}

@end
