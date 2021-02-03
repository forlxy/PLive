//
//  KwaiVodManifestConfig.h
//  IJKMediaFramework
//
//  Created by yuxin liu on 2019/8/15.
//  Copyright © 2019 kuaishou. All rights reserved.
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface KwaiVodManifestConfig : NSObject

// 网络类型
@property(nonatomic) int netType;
// 配置参数
@property(nonatomic, nullable) NSString* rateConfig;
// 设备分辨率 宽
@property(nonatomic) int deviceWidth;
// 设备分辨率 高
@property(nonatomic) int deviceHeight;
// 是否为低端机型
@property(nonatomic) int lowDevice;
// 信号强度
@property(nonatomic) int signalStrength;
// 手动切换
@property(nonatomic) int switchCode;

// A1配置参数
@property(nonatomic, nullable) NSString* rateConfigA1;
// 增加算法模式，区别对待A1和非A1情况
@property(nonatomic) int algorithm_mode;

@end

NS_ASSUME_NONNULL_END
