//
//  KwaiPlayerDebugInfoView.h
//  IJKMediaFramework
//
//  Created by MarshallShuai on 2018/12/13.
//  Copyright © 2018 kuaishou. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "HodorDebugInfoView.h"
#import "KwaiFFPlayerController.h"
#import "KwaiPlayerLiveDebugInfoView.h"
#import "KwaiPlayerVodDebugInfoView.h"
NS_ASSUME_NONNULL_BEGIN

@interface KwaiPlayerDebugInfoView : UIView
@property(nonatomic, readonly) KwaiPlayerVodDebugInfoView* vodDebugInfoView;
@property(nonatomic, readonly) KwaiPlayerLiveDebugInfoView* liveDebugInfoView;
@property(nonatomic, readonly) HodorDebugInfoView* hodorDebugInfoView;
@property(nonatomic) void (^showToastBlock)(NSString* message);
@property(nonatomic, nullable, weak) KwaiFFPlayerController* player;
@property(nonatomic, copy) NSString* extraInfoOfApp;
@property(nonatomic) UIButton* switchModeBtnToggle;
- (void)setHodorView:(UIView*)hodorView;
- (instancetype)initVodFrame:(CGRect)frame;
- (instancetype)initLiveFrame:(CGRect)frame;
- (void)startHodorMonitor;
- (void)stopHodorMonitor;
@end

NS_ASSUME_NONNULL_END
