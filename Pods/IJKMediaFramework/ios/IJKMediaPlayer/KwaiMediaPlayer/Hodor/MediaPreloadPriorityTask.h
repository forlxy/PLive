//
//  MediaPreloadPriorityTask.h
//  IJKMediaFramework
//
//  Created by MarshallShuai on 2019/8/16.
//  Copyright © 2019 kuaishou. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "AbstractHodorTask.h"
#import "IHodorTask.h"
#import "KSAwesomeCacheCallbackDelegate.h"

NS_ASSUME_NONNULL_BEGIN

@interface MediaPreloadPriorityTask : AbstractHodorTask

- (instancetype)initWithUrl:(NSString *)url
                withHeaders:(nullable NSDictionary *)headers
                   cacheKey:(NSString *)cacheKey;

// preloadBytes 加载的最大长度，默认值为1 MB，如果想全部下载完，则填0即可
@property(nonatomic) int64_t preloadBytes;

//设置预加载文件md5值，预加载完成进行校验，十六进制表示,32位字符串，格式"%2x"
@property(nonatomic, copy) NSString *md5HashCode;
@property(nonatomic, copy, readonly) NSString *url;
@property(nonatomic, copy, readonly) NSString *cacheKey;
@property(nonatomic, strong) id<KSAwesomeCacheCallbackDelegate> callbackDelegate;
@property(nonatomic) KSEvictStrategy evictStrategy;

/**
 * 春节相关任务，为了让这个任务不被清除，但是同时也不会触发下载。
 * true表示这个任务只提交，但不做真正下载，false表示会下载，默认为false
 */
@property(nonatomic) BOOL isNoOp;

- (void)submit;
- (void)cancel;

@end

NS_ASSUME_NONNULL_END
