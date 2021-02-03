//
//  AwesomeCache.h
//  IJKMediaPlayer
//
//  Created by 帅龙成 on 13/12/2017.
//  Copyright © 2017 kuaishou. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "KSCacheTask.h"
#import "KSOfflineCacheTask.h"

@interface KSAwesomeCache : NSObject

+ (void)initWithPath:(NSString*)path maxCacheSize:(long)maxCacheSize;
+ (void)globalEnableCache:(BOOL)enable;
+ (void)clearCacheDirectory;
+ (BOOL)isFullyCached:(NSString*)key;
+ (int64_t)getCachedBytesForKey:(NSString*)key;
+ (int64_t)getTotalBytesForKey:(NSString*)key;
+ (int64_t)getCachedBytes;
+ (int64_t)getCachedBytesLimit;
//获取缓存大小占总文件大小的百分比
+ (int)getCachedPercentForKey:(NSString*)key;
/**
 * CacheV1 向CacheV2过渡的临时使能接口
 */
+ (void)enableCacheV2:(BOOL)enable;
/**
 * 设置CacheV2s的最大scope size
 */
+ (void)setCacheV2ScopeKb:(int)scope_kb;
+ (KSCacheTask*)newExportCachedFileTaskWithUri:(NSString*)uri
                                           key:(NSString*)key
                                          host:(NSString*)host
                                        toPath:(NSString*)path;
+ (KSOfflineCacheTask*)newOfflineCachedFileTaskWithUri:(NSString*)uri
                                                   key:(NSString*)key
                                                  host:(NSString*)host;
+ (BOOL)importToCache:(NSString*)filePath key:(NSString*)key;
/**
 * 注意，此接口仅供测试使用，为了能让charles可以抓到libCurl的请求包，
 * ipAndPort的格式为: ip:port，例如 "192.168.1.3:9000:
 */
+ (void)setHttpProxyAddress:(NSString*)ipAndPort;

@end
