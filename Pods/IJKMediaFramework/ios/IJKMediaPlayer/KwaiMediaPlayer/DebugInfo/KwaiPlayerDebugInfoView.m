//
//  KwaiPlayerDebugInfoView.m
//  IJKMediaFramework
//
//  Created by MarshallShuai on 2018/12/13.
//  Copyright © 2018 kuaishou. All rights reserved.
//

#import "KwaiPlayerDebugInfoView.h"
#import <KSAwesomeCache.h>
#import <MJExtension/MJExtension.h>
#import <Masonry/Masonry.h>
#import "DebugInfoUtil.h"
#import "KSAwesomeCache.h"
#import "SpeedChart.h"
static const float TOGGLE_BTN_ALPHA_NORMAL = 0.7f;
static const float TOGGLE_BTN_ALPHA_PRESSED = 1.f;
static bool isDebugInfoViewShown = false;

@interface KwaiPlayerDebugInfoView ()

@property(nonatomic) NSTimer* timer;
@property(nonatomic) KwaiPlayerDebugInfo* lastKwaiDebugInfo;
@property(nonatomic) NSTimeInterval reportInterval;
@property(nonatomic) BOOL isShown;

@end

@implementation KwaiPlayerDebugInfoView

- (instancetype)initVodFrame:(CGRect)frame {
    if (self = [super initWithFrame:frame]) {
        _vodDebugInfoView = [[KwaiPlayerVodDebugInfoView alloc] initWithFrame:frame];
        [self addSubview:_vodDebugInfoView];
        [self _setupToggleButton];
        [self _updateDebugViewStatus:isDebugInfoViewShown];
    }

    return self;
}

- (instancetype)initLiveFrame:(CGRect)frame {
    if (self = [super initWithFrame:frame]) {
        _liveDebugInfoView = [[KwaiPlayerLiveDebugInfoView alloc] initWithFrame:frame];
        [self addSubview:_liveDebugInfoView];
        [self _setupToggleButton];
        [self _updateDebugViewStatus:isDebugInfoViewShown];
    }
    return self;
}

- (void)setHodorView:(UIView*)hodorView {
    _hodorDebugInfoView = hodorView;
    [_vodDebugInfoView setHodorView:hodorView];
}

- (void)_setupToggleButton {
    int btnSize = 42;

    _switchModeBtnToggle = [UIButton buttonWithType:UIButtonTypeCustom];
    [_switchModeBtnToggle setTitleColor:[UIColor colorWithWhite:1.f alpha:TOGGLE_BTN_ALPHA_NORMAL]
                               forState:UIControlStateNormal];
    [_switchModeBtnToggle setTitleColor:[UIColor colorWithWhite:1.f alpha:TOGGLE_BTN_ALPHA_PRESSED]
                               forState:UIControlStateHighlighted];
    _switchModeBtnToggle.titleLabel.font = [UIFont boldSystemFontOfSize:21];

    _switchModeBtnToggle.layer.borderColor = [UIColor colorWithWhite:1.f alpha:0.5f].CGColor;
    _switchModeBtnToggle.layer.borderWidth = 3;
    _switchModeBtnToggle.layer.cornerRadius = btnSize / 2;

    [_switchModeBtnToggle addTarget:self
                             action:@selector(_onToggleBtnTouchUpInside:)
                   forControlEvents:UIControlEventTouchUpInside];
    [_switchModeBtnToggle addTarget:self
                             action:@selector(_onToggleBtnTouchDown)
                   forControlEvents:UIControlEventTouchDown];

    UILongPressGestureRecognizer* longPressReg = [[UILongPressGestureRecognizer alloc]
        initWithTarget:self
                action:@selector(_copyDebugInfoToPasteBoard:)];
    [_switchModeBtnToggle addGestureRecognizer:longPressReg];

    [self addSubview:_switchModeBtnToggle];

    [_switchModeBtnToggle mas_makeConstraints:^(MASConstraintMaker* make) {
        make.right.equalTo(self.mas_right).offset(-10);
        make.top.equalTo(self.mas_top).offset(150);
        make.size.mas_equalTo(CGSizeMake(btnSize, btnSize));
    }];
}

- (void)_updateDebugViewStatus:(bool)isShown {
    if (isShown) {
        [_switchModeBtnToggle setTitle:@"关" forState:UIControlStateNormal];
        if (_player.isLive) {
            _liveDebugInfoView.hidden = NO;
            _vodDebugInfoView.hidden = YES;
        } else {
            _vodDebugInfoView.hidden = NO;
            _liveDebugInfoView.hidden = YES;
        }
    } else {
        [_switchModeBtnToggle setTitle:@"开" forState:UIControlStateNormal];
        _liveDebugInfoView.hidden = YES;
        _vodDebugInfoView.hidden = YES;
    }
}

- (void)_onToggleBtnTouchUpInside:(UIButton*)btn {
    isDebugInfoViewShown = !isDebugInfoViewShown;
    [self _updateDebugViewStatus:isDebugInfoViewShown];
}

- (void)_onToggleBtnTouchDown {
    _switchModeBtnToggle.layer.borderColor =
        [UIColor colorWithWhite:1.f alpha:TOGGLE_BTN_ALPHA_PRESSED].CGColor;
}

- (void)showToast:(NSString*)message {
    if (_showToastBlock) {
        _showToastBlock(message);
    }
}

- (void)_copyDebugInfoToPasteBoard:(UILongPressGestureRecognizer*)gesture {
    if (gesture.state == UIGestureRecognizerStateBegan) {
        if (_lastKwaiDebugInfo != nil) {
            UIPasteboard* pb = [UIPasteboard generalPasteboard];
            NSString* debugJson = [_lastKwaiDebugInfo mj_JSONString];
            [pb setString:[debugJson stringByReplacingOccurrencesOfString:@"\\" withString:@""]];

            [self showToast:@"复制debugInfo成功，快发给接锅侠们定位Bug吧~"];
        } else {
            [self showToast:@"DebugInfo信息为空，复制失败"];
        }

        _switchModeBtnToggle.layer.borderColor =
            [UIColor colorWithWhite:1.f alpha:TOGGLE_BTN_ALPHA_NORMAL].CGColor;
    }
}

- (UIView*)hitTest:(CGPoint)point withEvent:(UIEvent*)event {
    UIView* test = nil;
    if (_player.isLive) {
        test = [_liveDebugInfoView hitTest:point withEvent:event];
    } else {
        test = [_vodDebugInfoView hitTest:point withEvent:event];
    }
    if (test == nil) {
        id view = [super hitTest:point withEvent:event];
        if (view == _switchModeBtnToggle) {
            return _switchModeBtnToggle;
        }
        return nil;
    }
    return test;
}

- (void)dealloc {
    [self _stopTimer];
    [self stopHodorMonitor];
}

- (void)_stopTimer {
    if (_timer) {
        [_timer invalidate];
        _timer = nil;
    }
}

- (void)_startTimer {
    if (_player == nil) {
        NSLog(@"Player is null, startTimer fail");
        return;
    }

    [self _stopTimer];

    _timer = [NSTimer scheduledTimerWithTimeInterval:_reportInterval
                                              target:self
                                            selector:@selector(_showDebugInfo)
                                            userInfo:nil
                                             repeats:YES];
}

- (void)setPlayer:(KwaiFFPlayerController*)player {
    [self _stopTimer];

    _player = player;

    if (_player.isLive) {
        _reportInterval = 1.0;
    } else {
        _reportInterval = 0.33;
    }

    if (_player) {
        [self _startTimer];
    }
}

- (void)_showDebugInfo {
    KwaiPlayerDebugInfo* info = _player.kwaiPlayerDebugInfo;
    // player is weak ref, so info may be nil here
    if (!info) {
        return;
    }
    if (_player.isLive) {
        [_liveDebugInfoView showDebugInfo:info];
        [self stopHodorMonitor];
    } else {
        [_vodDebugInfoView showDebugInfo:info];
    }

    _lastKwaiDebugInfo = info;
}

- (void)startHodorMonitor {
    _liveDebugInfoView.hidden = NO;
    [self _updateDebugViewStatus:isDebugInfoViewShown];
    [_hodorDebugInfoView startTimer];
}

- (void)stopHodorMonitor {
    [_hodorDebugInfoView stopTimer];
}
@end
