#import "KSCacheTask+private.h"
#import "task.h"
#import "task_listener.h"
#import "cache_errors.h"
using namespace kuaishou::cache;

@interface KSCacheTask()
@property(nonatomic, copy) void(^onSuccessfulBlock)();
@property(nonatomic, copy) void(^onFailureBlock)(int reason);
@property(nonatomic, copy) void(^onCancelledBlock)();
@property(nonatomic, copy) void(^onProgressBlock)(int64_t downloadedBytes, int64_t totalBytes);
@end

namespace {
class TaskListenerProxy : public TaskListener {
  public:
    TaskListenerProxy(KSCacheTask* task) : _task(task) {}
    virtual void OnTaskSuccessful() override {
        if (_task.onSuccessfulBlock) {
            _task.onSuccessfulBlock();
        }
        [_task releaseAsync];
    }

    virtual void OnTaskCancelled() override {
        if (_task.onCancelledBlock) {
            _task.onCancelledBlock();
        }
    }

    virtual void OnTaskFailed(int32_t fail_reason) override {
        if (_task.onFailureBlock) {
            _task.onFailureBlock(fail_reason);
        }
        [_task releaseAsync];
    }

    virtual void onTaskProgress(int64_t position, int64_t totalsize) override {
        if (_task.onProgressBlock) {
            _task.onProgressBlock(position, totalsize);
        }
    }
  private:
    KSCacheTask* _task;
};
}


@implementation KSCacheTask {
    std::unique_ptr<Task> _nativeTask;
    std::unique_ptr<TaskListenerProxy> _nativeListener;
    NSLock * _nativeLock;
}

- (instancetype)init {
    if (self = [super init]) {
        _nativeListener = std::unique_ptr<TaskListenerProxy>(new TaskListenerProxy(self));
        _nativeLock = [[NSLock alloc] init];
    }
    return self;
}

- (void)dealloc {}

- (void)runWithOnSuccessBlock:(void(^)())onSuccess
                  onFailBlock:(void(^)(int))onFail
                onCancelBlock:(void(^)())onCancel
              onProgressBlock:(void (^)(int64_t downloadedBytes, int64_t totalBytes))onProgress {
    _onSuccessfulBlock = onSuccess;
    _onFailureBlock = onFail;
    _onCancelledBlock = onCancel;
    _onProgressBlock = onProgress;
    if (!_nativeTask) {
        if (_onFailureBlock) {
            _onFailureBlock(kTaskFailReasonCreateTaskFail);
        }
    } else {
        _nativeTask->Run();
    }
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

- (void)releaseAsync {
    NSThread* thread = [[NSThread alloc] initWithTarget:self selector:@selector(cancel) object:nil];
    [thread start];
}

- (void)setNativeTask:(std::unique_ptr<Task>)nativeTask {
    _nativeTask = std::move(nativeTask);
}

- (TaskListener*)getNativeTaskListener {
    return _nativeListener.get();
}

@end
