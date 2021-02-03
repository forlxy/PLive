#import "KSCacheTask.h"
#import "task.h"
#import "task_listener.h"

using namespace kuaishou::cache;

@interface KSCacheTask ()

- (void)setNativeTask:(std::unique_ptr<Task>)nativeTask;

- (TaskListener*)getNativeTaskListener;

@end
