#import "HodorDebugInfoView.h"
#import <KSAwesomeCache.h>
#import <MJExtension/MJExtension.h>
#import <Masonry/Masonry.h>
#import "DebugInfoUtil.h"
#import "KSAwesomeCache.h"
#import "hodor_c.h"

#define HODOR_STATUS_LEN 2048
static const float VIEW_BG_ALPHA = 0.7f;
static const NSTimeInterval REPORT_INTERVAL = 0.33;

@interface HodorDebugInfoView ()

@property(nonatomic) NSTimer* timer;
@property(nonatomic) NSTimeInterval reportInterval;
@property(nonatomic) UILabel* lbHodorStatus;
@property(nonatomic) UIView* tempTest;

@end

@implementation HodorDebugInfoView

- (instancetype)initWithFrame:(CGRect)frame {
    self = [super initWithFrame:frame];
    if (self) {
        [self _setupSubViews];
    }
    return self;
}

- (UIView*)addSubViewRoot {
    UIView* subRoot = [[UIView alloc] init];
    subRoot.userInteractionEnabled = NO;
    subRoot.layer.cornerRadius = 20;
    subRoot.backgroundColor = [UIColor colorWithWhite:0.f alpha:VIEW_BG_ALPHA];
    [self addSubview:subRoot];
    [subRoot mas_makeConstraints:^(MASConstraintMaker* make) {
        make.edges.equalTo(self).insets(
            UIEdgeInsetsMake(MARGIN_DEFAULT, MARGIN_DEFAULT, MARGIN_DEFAULT, MARGIN_DEFAULT));
    }];
    return subRoot;
}

- (void)_setupSubViews {
    UIView* subRoot = [self addSubViewRoot];
    UIView* lastAnchor = nil;
    lastAnchor = [DebugInfoUtil appendRowTitle:subRoot anchor:lastAnchor text:@"Hodor"];
    lastAnchor = _lbHodorStatus = [DebugInfoUtil appendRowKeyValueContent:subRoot
                                                                   anchor:lastAnchor
                                                                      key:@"Hodor队列"];
}

- (void)dealloc {
    [self stopTimer];
}

- (void)stopTimer {
    if (_timer) {
        [_timer invalidate];
        _timer = nil;
    }
}

- (void)startTimer {
    if (_timer) {
        return;
    }

    _reportInterval = REPORT_INTERVAL;
    _timer = [NSTimer scheduledTimerWithTimeInterval:_reportInterval
                                              target:self
                                            selector:@selector(_showDebugInfo)
                                            userInfo:nil
                                             repeats:YES];
}

- (void)_showDebugInfo {
    char hodorStatus[HODOR_STATUS_LEN];
    Hodor_get_status_for_debug_info(hodorStatus, HODOR_STATUS_LEN);
    _lbHodorStatus.text = safe_from_c_string(hodorStatus);
}

@end
