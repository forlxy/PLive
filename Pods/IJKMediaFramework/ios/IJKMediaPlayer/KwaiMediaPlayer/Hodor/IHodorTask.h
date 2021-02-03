//
//  IHodorTask.h
//  IJKMediaFramework
//
//  Created by MarshallShuai on 2019/9/24.
//  Copyright © 2019 kuaishou. All rights reserved.
//

#import <Foundation/Foundation.h>

// 下列的值必须和 download_priority_step_task.h中的值对应
typedef NS_ENUM(NSUInteger, KSTaskType) {
    Media = 0,     // Media表示播放器相关的预加载任务
    Resource = 1,  // Resource表示静态资源下载任务，和播放器无关的任务
};

typedef NS_ENUM(NSUInteger, KSEvictStrategy) {
    KSEvictStrategy_LRU = 1,  // 默认值，会被分片淘汰机制删除，也可以被cleanCacheDir触发
    KSEvictStrategy_NEVER = 2,  // 只能通过Hodor.pruneStrategyNeverCacheContent的内部去重接口清除
};

/**
 * task优先级，应用层也可以自己定义priority，Hodor内部处理priority的机制是prioity的值越大，优先级越高
 */
typedef NS_ENUM(NSInteger, KSTaskPriority) {
    UnLimited =
        10000,  // 这个优先级的任务不会收到播放器和网速的影响，是无条件进行的，只能设置给MainPriority
    High = 3000,
    Medium = 2000,
    Low = 1000,
    Unknown = -1,
};

@protocol IHodorTask <NSObject>

- (KSTaskType)taskType;
- (void)submit;
- (void)cancel;

@end
