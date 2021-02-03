//
//  KSHodor.h
//  IJKMediaFramework
//
//  Created by MarshallShuai on 2019/8/16.
//  Copyright © 2019 kuaishou. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "HodorConfig.h"
#import "IHodorTask.h"

NS_ASSUME_NONNULL_BEGIN

/**
 * 本下载器默认面向的都是资源下载的场景
 */
@interface Hodor : NSObject

/**
 *  单例方法
 */
+ (instancetype)sharedInstance;

- (void)initWithConfig:(HodorConfig *)config;

- (void)submitTask:(id<IHodorTask>)task;

/**
 * 暂停任务接口系列，当前执行了一半会被中断
 */
- (void)pauseTaskByKey:(NSString *)key taskType:(KSTaskType)type;
- (void)pauseAllTasksOfPriority:(int)priority;
- (void)pauseAllTasks;

/**
 * 恢复任务的接口系列
 */
- (void)resumeTaskByKey:(NSString *)key taskType:(KSTaskType)type;
- (void)resumeAllTasksOfPriority:(int)priority;
- (void)resumeAllTasks;

/**
 * 取消所有队列里的任务接口系列，当前执行了一半的任务会被中断
 */
- (void)cancelTaskByKey:(NSString *)key taskType:(KSTaskType)type;
- (void)cancelTasksWithPriority:(int)priority;
- (void)cancelAllTasks;

/**
 * 删除指定cacheKey对应的分片
 * @param cacheKey 删除任务的cachekey
 */
- (void)deleteCacheByKey:(NSString *)cacheKey taskType:(KSTaskType)type;

/**
 * 获取资源文件下载后的路径
 */
- (NSString *)cacheResourceFilePathForKey:(NSString *)key;

/**
 * 清理没有记录在案的永久储存的Media视频文件，供春节任务调用
 * @param clearAll true表示全部清除，false表示只清理不在服务器下发列表里的资源
 */
- (void)pruneStrategyNeverCacheContent:(BOOL)clearAll;

@end

NS_ASSUME_NONNULL_END
