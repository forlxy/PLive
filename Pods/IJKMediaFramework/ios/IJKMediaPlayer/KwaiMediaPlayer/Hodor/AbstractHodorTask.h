//
//  AbstractHodorTask.h
//  IJKMediaFramework
//
//  Created by MarshallShuai on 2019/11/14.
//  Copyright © 2019 kuaishou. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "IHodorTask.h"
#import "PlayerTempDef.h"

NS_ASSUME_NONNULL_BEGIN

@interface AbstractHodorTask : NSObject <IHodorTask>

@property(nonatomic) int mainPriority;
@property(nonatomic) int subPriority;

// ios暂不支持upStream,先注释
//@property(nonatomic) KwaiCacheUpstreamType upstreamType;
// maxSpeed 预加载的最大下载速度，单位kbps，1k=1024, 默认值为-1，不限速
@property(nonatomic) int maxSpeedKbps;
// 下载请求超时，默认3秒
@property(nonatomic) int connectTimeoutMs;

@end

NS_ASSUME_NONNULL_END
