//
//  HodorConfig.h
//  IJKMediaFramework
//
//  Created by MarshallShuai on 2019/9/24.
//  Copyright © 2019 kuaishou. All rights reserved.
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

// 控制缓存的一个策略
typedef enum : NSUInteger {
    Strategy_UseCache = 1,  // 请求下载时，如果已经缓存了，则返回缓存的结果，默认为此模式
    Strategy_ClearBeforeDownload = 2,  // 每次下载请求前强制
} HodorCacheStrategy;

static const int DEFAULT_CONNECT_TIMEOUT_MS = 3000;

@interface HodorConfig : NSObject

// 最大并发数，默认为1，最大可以设置为10
@property(nonatomic) int maxConcurrentCount;

@end

NS_ASSUME_NONNULL_END
