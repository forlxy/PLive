//
//  MediaPreloadPriorityTask.m
//  IJKMediaFramework
//
//  Created by MarshallShuai on 2019/8/16.
//  Copyright © 2019 kuaishou. All rights reserved.
//

#import "MediaPreloadPriorityTask.h"
#import "util/HeaderUtil.h"
#import "AbstractHodorTask+private.h"
#import "hodor_log_switch.h"

#include <ac_log.h>
#include "utils/macro_util.h"
#include "data_spec.h"
#include "hodor_downloader/hodor_downloader.h"
#include "hodor_downloader/scope_download_priority_step_task.h"
#include "awesome_cache/ios/ios_awesome_cache_callback.h"

USING_HODOR_NAMESPAE


@implementation MediaPreloadPriorityTask {
    NSString *_flatHeadersString;
}

- (instancetype)initWithUrl:(NSString *)url withHeaders:(nullable NSDictionary *)headers cacheKey:(NSString *)cacheKey
{
    self = [super init];
    if (self) {
        _url = url;
        _cacheKey = cacheKey;
        _flatHeadersString = [HeaderUtil parseHeaderMapToFlatString:headers];
        _preloadBytes = 1*1024*1024; // 默认加载1M
        _md5HashCode = [[NSString alloc] init];
        _evictStrategy = KSEvictStrategy_LRU;
        _isNoOp = NO;
    }
    return self;
}

- (void)submit
{
    DataSpec spec = DataSpec().WithUri([_url UTF8String]).WithKey([_cacheKey UTF8String]);
    if (_preloadBytes > 0) {
        spec.WithLength(_preloadBytes);
    }
    DownloadOpts opts;
    ExtractValueIntoDownloadOpts(self, opts);
    opts.headers = [_flatHeadersString UTF8String];
    opts.md5_hash_code = [_md5HashCode UTF8String];
    opts.tcp_connection_reuse = CacheV2Settings::GetTcpMaxConnects();
    // 不受限制的task不应该设置interrupt callback
    if (super.mainPriority != UnLimited) {
        opts.interrupt_cb = HodorDownloader::GetInstance()->GetInterruptCallback();
    }
    
    // sf2010 临时代码，表示这个task是春节视频
    if (_evictStrategy == KSEvictStrategy_NEVER) {
        HodorDownloader::GetInstance()->OnStrategyNeverTaskAdded([_cacheKey UTF8String]);
    }
    
    // sf2010，只有春节视频可能设置 isNoOp
    if (!_isNoOp) {
        auto task = std::make_shared<ScopeDownloadPriorityStepTask>(spec, opts, std::make_shared<IOSAwesomeCacheCallback>(_callbackDelegate),
                                                                    super.mainPriority, super.subPriority);
        HodorDownloader::GetInstance()->SubmitTask(task);
    }
    
#if (VERBOSE_HODOR_TASK_CONFIG)
    LOG_DEBUG("[MediaPreloadPriorityTask_submit] mainPriority:%d, subPriority:%d, connect_timeout_ms:%dms, evictStrategy:%d, cache_key_str:%s, url_str:%s",
              super.mainPriority, super.subPriority, opts.connect_timeout_ms, _evictStrategy, [_cacheKey UTF8String], [_url UTF8String]);
#endif
}

- (void)cancel
{
    HodorDownloader::GetInstance()->CancelTaskByKey([_cacheKey UTF8String], (TaskType)[self taskType]);
}

- (KSTaskType)taskType {
    return Media;
}

@end
