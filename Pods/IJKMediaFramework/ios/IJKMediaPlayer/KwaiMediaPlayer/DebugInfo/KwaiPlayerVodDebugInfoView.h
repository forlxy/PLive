//
//  KwaiPlayerVodDebugInfoView.h
//  IJKMediaFramework
//
//  Created by MarshallShuai on 2018/12/13.
//  Copyright © 2018 kuaishou. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "KwaiFFPlayerController.h"

NS_ASSUME_NONNULL_BEGIN

@interface KwaiPlayerVodDebugInfoView : UIView

@property(nonatomic, nullable, weak) KwaiFFPlayerController* player;
@property(nonatomic, copy) NSString* extraInfoOfApp;
@property(nonatomic, copy) NSString* retryInfoOfApp;
- (void)setHodorView:(UIView*)hodorView;
- (void)showDebugInfo:(KwaiPlayerDebugInfo*)info;

@end

NS_ASSUME_NONNULL_END
