//
//  KSHodor.m
//  IJKMediaFramework
//
//  Created by MarshallShuai on 2019/8/16.
//  Copyright © 2019 kuaishou. All rights reserved.
//

#import "Hodor.h"
#import "hodor_downloader/hodor_downloader.h"
#import "v2/cache/cache_v2_file_manager.h"
#import "ac_log.h"

using namespace kuaishou::cache;

@implementation Hodor

+ (instancetype)sharedInstance
{
    static Hodor* sHodor = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        sHodor = [[Hodor alloc] init];
    });
    return sHodor;
}


- (void)initWithConfig:(HodorConfig *)config
{
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        HodorDownloader::GetInstance()->SetThreadWorkerCount(config.maxConcurrentCount);
    });
}

- (void)submitTask:(id<IHodorTask>)task
{
    [task submit];
}

- (void)pauseTaskByKey:(NSString *)key taskType:(KSTaskType)type;
{
    HodorDownloader::GetInstance()->PauseTaskByKey([key UTF8String], (TaskType)type);
}

- (void)pauseAllTasksOfPriority:(int)priority
{
    HodorDownloader::GetInstance()->PauseAllTasksOfMainPriority(priority);
}

- (void)pauseAllTasks
{
    HodorDownloader::GetInstance()->PauseAllTasks();
}

- (void)resumeTaskByKey:(NSString *)key taskType:(KSTaskType)type;
{
    HodorDownloader::GetInstance()->ResumeTaskByKey([key UTF8String], (TaskType)type);
}

- (void)resumeAllTasksOfPriority:(int)priority
{
    HodorDownloader::GetInstance()->ResumeAllTasksOfMainPriority(priority);
}

- (void)resumeAllTasks
{
    HodorDownloader::GetInstance()->ResumeAllTasks();
}

- (void)cancelTaskByKey:(NSString *)key taskType:(KSTaskType)type;
{
    HodorDownloader::GetInstance()->CancelTaskByKey([key UTF8String], (TaskType)type);
}

- (void)cancelAllTasksOfPriority:(int)priority
{
    HodorDownloader::GetInstance()->CancelAllTasksOfMainPriority(priority);
}

- (void)cancelAllTasks
{
    HodorDownloader::GetInstance()->CancelAllTasks();
}

- (void)cancelTaskOfPriority:(int)priority
{
    HodorDownloader::GetInstance()->CancelAllTasksOfMainPriority(priority);
}

- (void)deleteCacheByKey:(NSString*)cacheKey taskType:(KSTaskType)type
{
    HodorDownloader::GetInstance()->DeleteCacheByKey([cacheKey UTF8String], (TaskType)type);
}

- (NSString *)cacheResourceFilePathForKey:(NSString *)key
{
    if (key.length == 0) {
        LOG_ERROR("[Hodor::cacheResourceFilePathForKey] key is empty, invalid");
        return @"";
    }
    auto cache_content = CacheV2FileManager::GetResourceDirManager()->Index()->GetCacheContent("", [key UTF8String]);
    return [NSString stringWithCString:cache_content->GetCacheContentValidFilePath().c_str() encoding:NSUTF8StringEncoding] ;
}

- (void)pruneStrategyNeverCacheContent : (BOOL)clearAll
{
    HodorDownloader::GetInstance()->PruneStrategyNeverCacheContent(clearAll);
}

@end
