#import <Foundation/Foundation.h>

@interface KSCacheTask : NSObject

- (void)runWithOnSuccessBlock:(void (^)())onSuccess
                  onFailBlock:(void (^)(int))onFail
                onCancelBlock:(void (^)())onCancel
              onProgressBlock:(void (^)(int64_t downloadedBytes, int64_t totalBytes))onProgress;

- (void)cancel;
- (void)releaseAsync;

@end
