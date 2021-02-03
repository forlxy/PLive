//
//  HlsPreloadPriorityTask.h
//  IJKMediaPlayer
//
//  Created by 李金海 on 2019/8/27.
//  Copyright © 2019 kuaishou. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "AbstractHodorTask.h"
#import "IHodorTask.h"
#import "KSAwesomeCacheCallbackDelegate.h"

NS_ASSUME_NONNULL_BEGIN

@interface HlsPreloadPriorityTask : AbstractHodorTask

- (instancetype)initWithManifest:(NSString *)manifest
                 preferBandwidth:(int)bandwidth
                     withHeaders:(nullable NSDictionary *)header;

// preloadBytes 加载的最大长度，默认值为1 MB，如果想全部下载完，则填0即可
@property(nonatomic) int64_t preloadBytes;

@property(nonatomic, readonly) NSString *manifest;

@property(nonatomic, readonly) NSString *headers;

@property(nonatomic, readonly) int preferBandWidth;

@property(nonatomic, strong) id<KSAwesomeCacheCallbackDelegate> awesomeCacheCallbackDelegate;

- (KSTaskType)taskType;
- (void)submit;
- (void)cancel;
- (void)deleteCache;

@end

NS_ASSUME_NONNULL_END
