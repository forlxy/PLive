//
//  ResourceDownloadTask.h
//  IJKMediaFramework
//
//  Created by MarshallShuai on 2019/9/24.
//  Copyright © 2019 kuaishou. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "AbstractHodorTask.h"
#import "HodorConfig.h"
#import "IHodorTask.h"

NS_ASSUME_NONNULL_BEGIN

/**
 * 下载回调的信息
 * 底层的下载任务是用nge请求的方式分段下载，目前分片大小是1MB
 * 所以以1MB分片为例， downloadedBytes <= expectBytes <= 1MB
 * ResourceDownloadCallback.onDownloadEnd在每个分片下载完的时候都会回调，想要知道整个文件是否下载完成，可以
 * 调用ResourceDownloadInfo.isComplete来确认
 */
@interface ResourceDownloadInfo : NSObject

// 当前已经缓存了多少字节
@property(nonatomic) int64_t progressBytes;
// 文件总长度是多大
@property(nonatomic) int64_t totalBytes;

// 本次下载预期能下载的字节数（一般等于range请求返回的 content-length)
@property(nonatomic) int64_t expectBytes;
// 本次下载实际下载了多少字节
@property(nonatomic) int64_t downloadedBytes;
// 正常情况是0，出错是非零的负值
@property(nonatomic) int errorCode;

// 详情上报
@property(nonatomic, copy) NSString *cdnStatJson;

- (BOOL)isComplete;

@end

typedef void (^ResourceDownloadTaskCallback)(ResourceDownloadInfo *info);

@interface ResourceDownloadTask : AbstractHodorTask

- (instancetype)initWithURL:(NSURL *)url
                withHeaders:(nullable NSDictionary *)headers
                   cacheKey:(NSString *)cacheKey
              progressBlock:(ResourceDownloadTaskCallback)progressBlock
                   endBlock:(ResourceDownloadTaskCallback)endBlock;

- (void)submit;

- (void)cancel;

- (KSTaskType)taskType;

@property(nonatomic, copy) NSString *cacheKey;

// 默认为Strategy_UseCache
@property(nonatomic) HodorCacheStrategy cacheStrategy;
// 回调进度最小间隔,默认0.1秒
@property(nonatomic) NSTimeInterval minimumProgressInterval;

@end

NS_ASSUME_NONNULL_END
