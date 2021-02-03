//
//  KSOfflineCacheTask+private.h
//  IJKMediaPlayer
//
//  Created by liuyuxin on 2018/5/8.
//  Copyright © 2018年 kuaishou. All rights reserved.
//
#import "KSOfflineCacheTask.h"
#import "task.h"
#import "task_listener.h"

using namespace kuaishou::cache;

@interface KSOfflineCacheTask ()

- (void)setNativeTask:(std::unique_ptr<Task>)nativeTask;

- (TaskListener*)getNativeTaskListener;

@end
