//
//  ResourceDownloadTask.m
//  IJKMediaFramework
//
//  Created by MarshallShuai on 2019/9/24.
//  Copyright © 2019 kuaishou. All rights reserved.
//

#import "ResourceDownloadTask.h"
#import "Hodor.h"
#import "util/HeaderUtil.h"
#import "KSCache/KSAwesomeCacheCallbackDelegate.h"
#import "AbstractHodorTask+private.h"
#import "hodor_log_switch.h"

#include "data_spec.h"
#include "ac_log.h"
#include "hodor_downloader/hodor_downloader.h"
#include "hodor_downloader/single_file_download_priority_step_task.h"
#include "awesome_cache/ios/ios_awesome_cache_callback.h"

#pragma mark ResourceDownloadInfo
@implementation ResourceDownloadInfo
- (BOOL)isComplete {
    return _totalBytes > 0 && _progressBytes == _totalBytes;
}
@end

#pragma mark ResourceDownloadDelegate
@interface ResourceDownloadDelegate : NSObject <KSAwesomeCacheCallbackDelegate>

@property(nonatomic, strong) ResourceDownloadTaskCallback progressBlock;
@property(nonatomic, strong) ResourceDownloadTaskCallback endBlock;

@end

@implementation ResourceDownloadDelegate {
    ResourceDownloadInfo *_resourceDownloadInfo;
}

- (instancetype)init
{
    self = [super init];
    if (self) {
        _resourceDownloadInfo = [[ResourceDownloadInfo alloc] init];
    }
    return self;
}

- (void)onSessionProgress:(KSAcCallbackInfo *)info {
    if (_progressBlock) {
        _resourceDownloadInfo.expectBytes = info.contentLength;
        _resourceDownloadInfo.progressBytes = info.progressPosition;
        _resourceDownloadInfo.totalBytes = info.totalBytes;
        _progressBlock(_resourceDownloadInfo);
    }
}

- (void)onDownloadFinish:(KSAcCallbackInfo *)info {
    if (_endBlock) {
        _resourceDownloadInfo.progressBytes = info.progressPosition;
        _resourceDownloadInfo.errorCode = info.errorCode;
        _resourceDownloadInfo.downloadedBytes = info.downloadBytes;
        _resourceDownloadInfo.cdnStatJson = info.cdnStatJson;
        _endBlock(_resourceDownloadInfo);
    }
}

@end


#pragma mark ResourceDownloadTask
using namespace kuaishou::cache;

@interface ResourceDownloadTask()

@property (nonatomic, strong)NSURL *url;
@property (nonatomic, strong)NSString *flatHeadersString;
@property (nonatomic, strong)ResourceDownloadDelegate *downloadDelegate;

@end


@implementation ResourceDownloadTask

- (instancetype)initWithURL:(NSURL *)url
                withHeaders:(NSDictionary *)headers
                   cacheKey:(NSString *)cacheKey
              progressBlock:(ResourceDownloadTaskCallback)progressBlock
                   endBlock:(ResourceDownloadTaskCallback)endBlock {
  if (self) {
    _url = url;
    _cacheKey = cacheKey;
    _flatHeadersString = [HeaderUtil parseHeaderMapToFlatString:headers];

    _downloadDelegate = [[ResourceDownloadDelegate alloc] init];
    _downloadDelegate.progressBlock = progressBlock;
    _downloadDelegate.endBlock = endBlock;
  }
  return self;
}

- (void)submit
{
    if ([_cacheKey length] == 0) {
        LOG_ERROR("[ResourceDownloadTask_submit]fail, cacheKey is empty");
        return;
    }
    
    DataSpec spec = DataSpec().WithUri([[_url absoluteString] UTF8String]).WithKey([_cacheKey UTF8String]);
    
    DownloadOpts opts;
    ExtractValueIntoDownloadOpts(self, opts);
    opts.headers = [_flatHeadersString UTF8String];
    opts.tcp_connection_reuse = CacheV2Settings::GetTcpMaxConnects();
    opts.interrupt_cb = HodorDownloader::GetInstance()->GetInterruptCallback();
    if (_minimumProgressInterval > 0) {
        opts.progress_cb_interval_ms = _minimumProgressInterval * 1000;
    }
    
    auto callback = _downloadDelegate ? std::make_shared<IOSAwesomeCacheCallback>(_downloadDelegate) : nullptr;
    auto task = std::make_shared<SingleFileDownloadPriorityStepTask>(spec, opts, callback,
                                                                     super.mainPriority, super.subPriority);
    
    HodorDownloader::GetInstance()->SubmitTask(task);

#if (VERBOSE_HODOR_TASK_CONFIG)
    LOG_DEBUG("[ResourceDownloadTask_submit] mainPriority:%d, subPriority:%,d cache_key_str:%s, url_str:%s",
              super.mainPriority, super.subPriority, [_cacheKey UTF8String], [[_url absoluteString] UTF8String]);
#endif
    
}

- (void)cancel
{
    HodorDownloader::GetInstance()->CancelTaskByKey([_cacheKey UTF8String], (TaskType)[self taskType]);
}

- (KSTaskType)taskType
{
    return Resource;
}
@end
