//
//  HlsPreloadPriorityTask.m
//  IJKMediaFramework
//
//  Created by 李金海 on 2019/8/27.
//  Copyright © 2019 kuaishou. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "HlsPreloadPriorityTask.h"
#import "ios_awesome_cache_callback.h"
#import "util/HeaderUtil.h"
#import "AbstractHodorTask+private.h"

#include <ac_log.h>
#include "utils/macro_util.h"
#include "data_spec.h"
#include "hodor_downloader/hodor_downloader.h"
#include "hodor_downloader/hls_segment_download_priority_task.h"


USING_HODOR_NAMESPAE

@implementation HlsPreloadPriorityTask {
    std::shared_ptr<HlsSegmentDownloadPriorityTask> downloadTask;
    std::shared_ptr<AwesomeCacheCallback> awesomeCacheCallbackPtr;
}

- (instancetype)initWithManifest:(NSString *)manifest preferBandwidth:(int)bandwidth withHeaders:(NSDictionary*)header;
{
    self = [super init];
    if (self) {
        _preloadBytes = 1*1024*1024; // 默认加载1M
        _preferBandWidth = bandwidth;
        _manifest = manifest;
        _headers = [HeaderUtil parseHeaderMapToFlatString:header];
        downloadTask = nullptr;
    }
    return self;
}

- (void)submit
{
    DownloadOpts opts;
    ExtractValueIntoDownloadOpts(self, opts);
    opts.headers = [_headers UTF8String];
    // 不受限制的task不应该设置interrupt callback
    if (super.mainPriority != UnLimited) {
        opts.interrupt_cb = HodorDownloader::GetInstance()->GetInterruptCallback();
    }
    downloadTask = std::make_shared<HlsSegmentDownloadPriorityTask>([_manifest UTF8String],
                                                                    _preferBandWidth, _preloadBytes, opts, awesomeCacheCallbackPtr,
                                                                    super.mainPriority, super.subPriority);
    HodorDownloader::GetInstance()->SubmitTask(downloadTask);
    NSLog(@"[HlsSegmentDownloadPriorityTask_submit] mainPriority:%d,  subPriority:%d, preloadBytes:%lld",
          super.mainPriority, super.subPriority, _preloadBytes);
}

- (void)cancel
{
    if (downloadTask != nullptr) {
        HodorDownloader::GetInstance()->CancelTask(downloadTask);
    }
}

- (void)deleteCache
{
    if (downloadTask != nullptr) {
        downloadTask->DeleteCache();
    }
}

- (KSTaskType)taskType
{
    return Media;
}

- (void)setAwesomeCacheCallbackDelegate:
(id<KSAwesomeCacheCallbackDelegate>)awesomeCacheCallbackDelegate {
    awesomeCacheCallbackPtr.reset(new IOSAwesomeCacheCallback(awesomeCacheCallbackDelegate));
}
@end
