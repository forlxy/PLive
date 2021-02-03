//
//  KSOfflineCacheTask.m
//  IJKMediaFramework
//
//  Created by liuyuxin on 2018/5/7.
//  Copyright © 2018年 kuaishou. All rights reserved.
//
#import "KSOfflineCacheTask+private.h"
#import "task.h"
#import "task_listener.h"
#import "cache_errors.h"

using namespace kuaishou::cache;

@interface KSOfflineCacheTask()
@property(nonatomic, copy) void(^onSuccessfulBlock)();
@property(nonatomic, copy) void(^onFailureBlock)(int reason);
@property(nonatomic, copy) void(^onCancelledBlock)();
@property(nonatomic, copy) void(^onTaskProgress)(int64_t position, int64_t totalsize);
@property(nonatomic, copy) void(^onTaskStopped)(int64_t download_bytes, int64_t transfer_consume_ms, NSString* sign);
@property(nonatomic, copy) void(^onTaskStarted)(int64_t start_pos, int64_t cached_bytes, int64_t totalsize);
@end

namespace {
class OfflineCacheListenerProxy : public TaskListener {
  public:
    OfflineCacheListenerProxy(KSOfflineCacheTask* task) : _task(task) {}
    ~OfflineCacheListenerProxy() {}
    virtual void OnTaskSuccessful() override {
        if (_task.onSuccessfulBlock) {
            _task.onSuccessfulBlock();
        }
        [_task releaseAsync];
        [_task clearBlock];
    }

    virtual void OnTaskCancelled() override {
        if (_task.onCancelledBlock) {
            _task.onCancelledBlock();
        }
        [_task clearBlock];
    }

    virtual void OnTaskFailed(int32_t fail_reason) override {
        if (_task.onFailureBlock) {
            _task.onFailureBlock(fail_reason);
        }
        [_task releaseAsync];
        [_task clearBlock];
    }

    virtual void onTaskProgress(int64_t position, int64_t totalsize) override {
        if (_task.onTaskProgress) {
            _task.onTaskProgress(position, totalsize);
        }
    }

    virtual void onTaskStopped(int64_t download_bytes, int64_t transfer_consume_ms, const char* sign) override {
        if (_task.onTaskStopped) {
            NSString* sign_ = [NSString stringWithUTF8String:sign];
            _task.onTaskStopped(download_bytes, transfer_consume_ms, sign_);
        }
    }

    virtual void onTaskStarted(int64_t start_pos, int64_t cached_bytes, int64_t totalsize) override {
        if (_task.onTaskStarted) {
            _task.onTaskStarted(start_pos, cached_bytes, totalsize);
        }
    }
  private:
    KSOfflineCacheTask* _task;
};
}

@implementation KSOfflineCacheTask {
    std::unique_ptr<Task> _nativeTask;
    std::unique_ptr<OfflineCacheListenerProxy> _nativeListener;
    NSLock* _nativeLock;
}

- (instancetype)init {
    self = [super init];
    if (self) {
        _nativeListener = std::unique_ptr<OfflineCacheListenerProxy>(new OfflineCacheListenerProxy(self));
        _nativeLock = [[NSLock alloc] init];
    }
    return self;
}

- (void)dealloc {}

- (void)runWithOnSuccessBlock:(void(^)())onSuccess
                  onFailBlock:(void(^)(int))onFail
                onCancelBlock:(void(^)())onCancel
                   onProgress:(void(^)(int64_t, int64_t))onProgress
                    onStopped:(void(^)(int64_t, int64_t, NSString* sign))onStopped
                    onStarted:(void(^)(int64_t, int64_t, int64_t))onStarted {
    _onSuccessfulBlock = onSuccess;
    _onFailureBlock = onFail;
    _onCancelledBlock = onCancel;
    _onTaskProgress = onProgress;
    _onTaskStopped = onStopped;
    _onTaskStarted = onStarted;

    if (!_nativeTask) {
        if (_onFailureBlock) {
            _onFailureBlock(kTaskFailReasonCreateTaskFail);
        }
    } else {
        _nativeTask->Run();
    }
}

- (TaskListener*)getNativeTaskListener {
    return _nativeListener.get();
}

- (void)cancel {
    [_nativeLock lock];
    if (_nativeTask) {
        _nativeTask->Cancel();
        _nativeTask.reset();
        _nativeListener.reset();
    }
    [_nativeLock unlock];
}

- (void)clearBlock {
    _onSuccessfulBlock = nil;
    _onFailureBlock = nil;
    _onCancelledBlock = nil;
    _onTaskProgress = nil;
    _onTaskStopped = nil;
    _onTaskStarted = nil;
}

- (void)releaseAsync {
    NSThread* thread = [[NSThread alloc] initWithTarget:self selector:@selector(cancel) object:nil];
    [thread start];
}

- (void)setNativeTask:(std::unique_ptr<Task>)nativeTask {
    _nativeTask = std::move(nativeTask);
}

@end
