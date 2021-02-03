#import <Foundation/Foundation.h>
#import "KwaiFFPlayerController.h"

NS_ASSUME_NONNULL_BEGIN

@interface KwaiPlayerLiveDebugInfoView : UIView

@property(nonatomic, nullable, weak) KwaiFFPlayerController* player;
@property(nonatomic, copy) NSString* extraInfoOfApp;

- (void)showDebugInfo:(KwaiPlayerDebugInfo*)info;

@end

NS_ASSUME_NONNULL_END
