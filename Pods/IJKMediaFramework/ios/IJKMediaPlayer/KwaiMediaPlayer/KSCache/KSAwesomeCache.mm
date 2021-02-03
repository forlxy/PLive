//
//  AwesomeCache.m
//  KSYPlayerCore
//
//  Created by 帅龙成 on 13/12/2017.
//  Copyright © 2017 kuaishou. All rights reserved.
//
#import "KSAwesomeCache.h"
#import "cache_manager.h"
#import "KSCacheTask+private.h"
#import "KSOfflineCacheTask+private.h"
#import <mutex>

#import "v2/cache/cache_def_v2.h"
#if ENABLE_CACHE_V2
#import "v2/cache/cache_v2_settings.h"
#import "v2/cache/cache_content_index_v2.h"
#import "v2/cache/cache_v2_file_manager.h"
#endif

@implementation KSAwesomeCache;

using namespace kuaishou::cache;

+ (void)initWithPath:(NSString*)path maxCacheSize:(long)maxCacheSize {
    auto manager = CacheManager::GetInstance();
    manager->SetOptInt(kMaxCacheBytes, maxCacheSize);
    manager->SetOptStr(kCacheDir, [path UTF8String]);
    
#if ENABLE_CACHE_V2
    // 这里不能判断 CacheManager::GetInstance()->IsCacheV2Enabled()，因为这个时候还拿不到这个信息，无脑初始化没问题
    CacheV2Settings::SetCacheRootDirPath(std::string([path UTF8String]));
#endif
    CacheManager::GetInstance()->Init();
}

+ (void)globalEnableCache:(BOOL)enable {
    CacheManager::GetInstance()->SetEnabled(enable);
}

+ (void)clearCacheDirectory {
#if ENABLE_CACHE_V2
    if (CacheManager::GetInstance()->IsCacheV2Enabled()) {
        CacheV2FileManager::GetMediaDirManager()->PruneWithCacheTotalBytesLimit(0);
    }
#endif
    CacheManager::GetInstance()->ClearCacheDir();
}

+ (BOOL)isFullyCached:(NSString*)key {
#if ENABLE_CACHE_V2
    if (CacheManager::GetInstance()->IsCacheV2Enabled()) {
        auto content = CacheV2FileManager::GetMediaDirManager()->Index()->GetCacheContent("", [key UTF8String]);
        return content->IsFullyCached();
    }
#endif
    
    return CacheManager::GetInstance()->IsFullyCached([key UTF8String]);
}

+ (int64_t)getCachedBytesForKey:(NSString*)key {
#if ENABLE_CACHE_V2
    if (CacheManager::GetInstance()->IsCacheV2Enabled()) {
        auto content = CacheV2FileManager::GetMediaDirManager()->Index()->GetCacheContent("", [key UTF8String]);
        return content->GetCachedBytes();
    }
#endif
    
    return CacheManager::GetInstance()->GetCachedBytesForKey([key UTF8String]);
}

+ (int64_t)getTotalBytesForKey:(NSString*)key {
#if ENABLE_CACHE_V2
    if (CacheManager::GetInstance()->IsCacheV2Enabled()) {
        std::shared_ptr<CacheContentV2WithScope> cache_content = CacheV2FileManager::GetMediaDirManager()->Index()->GetCacheContent("", [key UTF8String]);
        return cache_content->GetContentLength();
    }
#endif
    return CacheManager::GetInstance()->GetTotalBytesForKey([key UTF8String]);
}

+ (int64_t)getCachedBytes {
#if ENABLE_CACHE_V2
    if (CacheManager::GetInstance()->IsCacheV2Enabled()) {
        return CacheManager::GetInstance()->GetCachedBytes() + CacheV2FileManager::GetMediaDirManager()->GetTotalCachedBytes();
    }
#endif
    
    return CacheManager::GetInstance()->GetCachedBytes();
}

+ (int)getCachedPercentForKey:(NSString*)key {
    if (!key) {
        return 0;
    }
    int64_t cachedBytes = [self getCachedBytesForKey:key];
    int64_t totalBytes = [self getTotalBytesForKey:key];
    if (totalBytes > 0) {
        return (int)(100 * cachedBytes / totalBytes);
    } else {
        return 0;
    }
}

+ (int64_t)getCachedBytesLimit {
#if ENABLE_CACHE_V2
    if (CacheManager::GetInstance()->IsCacheV2Enabled()) {
        return CacheManager::GetInstance()->GetOptInt(kMaxCacheBytes) + CacheV2FileManager::GetMediaDirManager()->GetCacheBytesLimit();
    }
#endif
    return CacheManager::GetInstance()->GetOptInt(kMaxCacheBytes);
}
+ (void) enableCacheV2:(BOOL)enable {
#if ENABLE_CACHE_V2
    CacheManager::GetInstance()->SetCacheV2Enabled(enable);
#endif
}

+ (void)setCacheV2ScopeKb:(int)scope_kb {
#if ENABLE_CACHE_V2
    CacheV2Settings::SetScopeMaxSize(scope_kb * KB);
#endif
}

+ (KSCacheTask*)newExportCachedFileTaskWithUri:(NSString*)uri key:(NSString*)key
                                          host:(NSString*)host toPath:(NSString*)path {
    KSCacheTask* task = [[KSCacheTask alloc] init];
    DataSourceOpts opts;
    opts.type = kDataSourceTypeDefault;
    opts.download_opts.priority = kPriorityHigh;
    std::string cpp_host = host == nil ? "" : [host UTF8String];
    if (!cpp_host.empty()) {
        opts.download_opts.headers = "Host: " + cpp_host;
    }
    auto cpp_task = CacheManager::GetInstance()->
                    CreateExportCachedFileTask(uri == nil ? "" : [uri UTF8String],
                                               key == nil ? "" : [key UTF8String], opts,
                                               path == nil ? "" : [path UTF8String], [task getNativeTaskListener]);
    [task setNativeTask:std::move(cpp_task)];
    return task;
}

+ (KSOfflineCacheTask*)newOfflineCachedFileTaskWithUri:(NSString*)uri key:(NSString*)key
                                                  host:(NSString*)host {
    KSOfflineCacheTask* task = [[KSOfflineCacheTask alloc] init];
    DataSourceOpts opts;
    opts.type = kDataSourceTypeDefault;
    opts.download_opts.priority = kPriorityHigh;
    opts.enable_vod_adaptive = 0;
    opts.progress_cb_interval_ms = 50;
    std::string cpp_host = host == nil ? "" : [host UTF8String];
    if (!cpp_host.empty()) {
        opts.download_opts.headers = "Host: " + cpp_host;
    }

    auto cpp_task = CacheManager::GetInstance()->
                    CreateOfflineCachedFileTask(uri == nil ? "" : [uri UTF8String],
                                                key == nil ? "" : [key UTF8String],
                                                opts,
                                                [task getNativeTaskListener]);
    [task setNativeTask:std::move(cpp_task)];
    return task;
}

+ (BOOL)importToCache:(NSString*)filePath key:(NSString*)key {
#if ENABLE_CACHE_V2
    if (CacheManager::GetInstance()->IsCacheV2Enabled()) {
        return CacheManager::GetInstance()->ImportFileToCacheV2([filePath UTF8String], [key UTF8String]);
    }
#endif
    
    return CacheManager::GetInstance()->ImportFileToCache([filePath UTF8String], [key UTF8String]);
}

+ (void)setHttpProxyAddress:(NSString *)ipAndPort {
    std::string proxyAddr = "";
    if (ipAndPort != nil) {
        proxyAddr = [ipAndPort UTF8String];
    }
    CacheManager::GetInstance()->SetOptStr(kHttpProxyAddress, proxyAddr);
}
@end
