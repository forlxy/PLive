//
//  KSOfflineCacheTask.h
//  IJKMediaFramework
//
//  Created by liuyuxin on 2018/5/7.
//  Copyright © 2018年 kuaishou. All rights reserved.
//

#import <Foundation/Foundation.h>

@interface KSOfflineCacheTask : NSObject

- (void)runWithOnSuccessBlock:(void (^)())onSuccess
                  onFailBlock:(void (^)(int))onFail
                onCancelBlock:(void (^)())onCancel
                   onProgress:(void (^)(int64_t position, int64_t totalSize))onProgress
                    onStopped:(void (^)(int64_t downloadedBytes, int64_t transferContumeMs,
                                        NSString* sign))onStopped
                    onStarted:(void (^)(int64_t starPos, int64_t cachedBytes,
                                        int64_t totalSize))onStarted;

- (void)releaseAsync;

- (void)clearBlock;
@end
