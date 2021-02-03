#import "KwaiPlayerLiveDebugInfoView.h"
#import <KSAwesomeCache.h>
#import <MJExtension/MJExtension.h>
#import <Masonry/Masonry.h>
#import "DebugInfoUtil.h"
#import "KSAwesomeCache.h"
#import "SpeedChart.h"

typedef NS_ENUM(NSInteger, DebugViewStatus) {
    DebugViewStatus_Basic = 0,
    DebugViewStatus_DebuggerInfo
};

typedef NS_ENUM(short, VideoCapturerDeviceType) {
    kVideoCapturerDeviceUnknown = 0,
    kVideoCapturerDeviceCamera,
    kVideoCapturerDeviceFrontCamera,
    kVideoCapturerDeviceBackCamera,
    kVideoCapturerDeviceGlass
};

static DebugViewStatus sCurrentStatus = DebugViewStatus_Basic;

static const float TOGGLE_BTN_ALPHA_PRESSED = 1.f;
static const float VIEW_BG_ALPHA = 0.7f;

@interface KwaiPlayerLiveDebugInfoView ()

@property(nonatomic) UIButton* basicBtnToggle;
@property(nonatomic) UIButton* debugBtnToggle;
@property(nonatomic) KwaiPlayerDebugInfo* lastKwaiDebugInfo;

#pragma mark BasicInfo
@property(nonatomic) UIView* subViewBasicInfo;
// section 播放器相关配置
@property(nonatomic) UILabel* lbMediaType;
@property(nonatomic) UILabel* lbDeviceType;
@property(nonatomic) UILabel* lbInputUrl;
// section 视频信息
@property(nonatomic) UILabel* lbDimenFpsKps;
@property(nonatomic) UILabel* lbVideoCodec;
@property(nonatomic) UILabel* lbAudioCodec;
// section 首屏
@property(nonatomic) UILabel* lbFirstRender;
@property(nonatomic) UILabel* lbStartPlayBlockStatus;
// section 播放状态
@property(nonatomic) UILabel* lbPlayerStatus;
@property(nonatomic) UILabel* lbBlockInfo;
@property(nonatomic) UILabel* lbFirstScreenDrop;
@property(nonatomic) UILabel* lbTotalDrop;
@property(nonatomic) UILabel* lbDelay;
@property(nonatomic) UILabel* lbHostInfo;
@property(nonatomic) UILabel* lbVencInit;
@property(nonatomic) UILabel* lbAencInit;
@property(nonatomic) UILabel* lbVencDynamic;
@property(nonatomic) UILabel* lbComment;

#pragma mark DebuggerInfo
@property(nonatomic) UIView* subViewDebuggerInfo;
@property(nonatomic) UILabel* lbExtraInfoFromApp;
@property(nonatomic) UILabel* lbAvQueueStatus;
@property(nonatomic) UILabel* lbTotalDataStatus;
@property(nonatomic) UILabel* lbFirstScreen;
@property(nonatomic) UILabel* lbFirstScreenDetail;
@property(nonatomic) UILabel* lbSpeedupThreshold;

@property(nonatomic, assign) NSTimeInterval lastRefreshTime;
@property(nonatomic, assign) uint64_t lastDataSize;
@property(nonatomic, assign) double lastReadSize;
@end

@implementation KwaiPlayerLiveDebugInfoView

- (instancetype)initWithFrame:(CGRect)frame {
    self = [super initWithFrame:frame];
    if (self) {
        [self _setupSubViews];
    }
    return self;
}

- (UIView*)hitTest:(CGPoint)point withEvent:(UIEvent*)event {
    id view = [super hitTest:point withEvent:event];
    if (view == _basicBtnToggle) {
        return _basicBtnToggle;
    } else if (view == _debugBtnToggle) {
        return _debugBtnToggle;
    } else {
        return nil;
    }
}

#pragma mark setup views
- (void)_setupSubViews {
    [self _setupBasicInfoViews];
    [self _setupDebuggerInfoViews];
    [self _setupToggleButton];
    [self _updateStatusToDebug:sCurrentStatus];
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

- (void)_setupBasicInfoViews {
    UIView* subRoot = _subViewBasicInfo = [self addSubViewRoot];
    UIView* lastAnchor = nil;

    lastAnchor = [DebugInfoUtil appendRowTitle:subRoot anchor:lastAnchor text:@"基本信息"];
    lastAnchor = _lbMediaType = [DebugInfoUtil appendRowKeyValueContent:subRoot
                                                                 anchor:lastAnchor
                                                                    key:@"视频类型"];
    lastAnchor = _lbDeviceType = [DebugInfoUtil appendRowKeyValueContent:subRoot
                                                                  anchor:lastAnchor
                                                                     key:@"直播设备类型"];
    lastAnchor = _lbInputUrl = [DebugInfoUtil appendRowKeyValueContent:subRoot
                                                                anchor:lastAnchor
                                                                   key:@"inputUrl"];

    lastAnchor = [DebugInfoUtil appendRowTitle:subRoot anchor:lastAnchor text:@"MetaData"];
    lastAnchor = _lbDimenFpsKps = [DebugInfoUtil appendRowKeyValueContent:subRoot
                                                                   anchor:lastAnchor
                                                                      key:@"basic"];
    lastAnchor = _lbVideoCodec = [DebugInfoUtil appendRowKeyValueContent:subRoot
                                                                  anchor:lastAnchor
                                                                     key:@"VideoCodec"];
    lastAnchor = _lbAudioCodec = [DebugInfoUtil appendRowKeyValueContent:subRoot
                                                                  anchor:lastAnchor
                                                                     key:@"AudioCodec"];

    lastAnchor = [DebugInfoUtil appendRowTitle:subRoot anchor:lastAnchor text:@"播放状态"];
    lastAnchor = _lbFirstRender = [DebugInfoUtil appendRowKeyValueContent:subRoot
                                                                   anchor:lastAnchor
                                                                      key:@"首屏耗时"];
    lastAnchor = _lbBlockInfo = [DebugInfoUtil appendRowKeyValueContent:subRoot
                                                                 anchor:lastAnchor
                                                                    key:@"卡顿信息"];
    lastAnchor = _lbFirstScreenDrop = [DebugInfoUtil appendRowKeyValueContent:subRoot
                                                                       anchor:lastAnchor
                                                                          key:@"首屏追赶时长"];
    lastAnchor = _lbTotalDrop = [DebugInfoUtil appendRowKeyValueContent:subRoot
                                                                 anchor:lastAnchor
                                                                    key:@"追赶总时长"];
    lastAnchor = _lbDelay = [DebugInfoUtil appendRowKeyValueContent:subRoot
                                                             anchor:lastAnchor
                                                                key:@"端到端延迟"];

    lastAnchor = [DebugInfoUtil appendRowTitle:subRoot anchor:lastAnchor text:@"其他参数"];
    lastAnchor = _lbHostInfo = [DebugInfoUtil appendRowKeyValueContent:subRoot
                                                                anchor:lastAnchor
                                                                   key:@"主播信息"];
    lastAnchor = _lbVencInit = [DebugInfoUtil appendRowKeyValueContent:subRoot
                                                                anchor:lastAnchor
                                                                   key:@"视频初始参数"];
    lastAnchor = _lbAencInit = [DebugInfoUtil appendRowKeyValueContent:subRoot
                                                                anchor:lastAnchor
                                                                   key:@"音频初始参数"];
    lastAnchor = _lbVencDynamic = [DebugInfoUtil appendRowKeyValueContent:subRoot
                                                                   anchor:lastAnchor
                                                                      key:@"视频动态参数"];
    lastAnchor = _lbComment = [DebugInfoUtil appendRowKeyValueContent:subRoot
                                                               anchor:lastAnchor
                                                                  key:@"comment"];
}

- (void)_setupDebuggerInfoViews {
    UIView* subRoot = _subViewDebuggerInfo = [self addSubViewRoot];

    UIView* lastAnchor = nil;
    lastAnchor = [DebugInfoUtil appendRowTitle:subRoot anchor:lastAnchor text:@"其他信息"];

    lastAnchor = _lbExtraInfoFromApp = [DebugInfoUtil appendRowKeyValueContent:subRoot
                                                                        anchor:lastAnchor
                                                                           key:@"App端信息"];
    lastAnchor = _lbAvQueueStatus = [DebugInfoUtil appendRowKeyValueContent:subRoot
                                                                     anchor:lastAnchor
                                                                        key:@"音视频队列"];
    lastAnchor = _lbTotalDataStatus = [DebugInfoUtil appendRowKeyValueContent:subRoot
                                                                       anchor:lastAnchor
                                                                          key:@"下载总数据"];
    lastAnchor = _lbFirstScreen = [DebugInfoUtil appendRowKeyValueContent:subRoot
                                                                   anchor:lastAnchor
                                                                      key:@"首屏耗时"];
    lastAnchor = _lbFirstScreenDetail = [DebugInfoUtil appendRowKeyValueContent:subRoot
                                                                         anchor:lastAnchor
                                                                            key:@"首屏细分耗时"];
    lastAnchor = _lbSpeedupThreshold = [DebugInfoUtil appendRowKeyValueContent:subRoot
                                                                        anchor:lastAnchor
                                                                           key:@"追帧门限"];
}

#pragma mark toggle button
- (void)_setupToggleButton {
    // button
    _basicBtnToggle =
        [DebugInfoUtil makeUIButtonAndAddSubView:self
                                           title:@"基"
                                             tag:DebugViewStatus_Basic
                                    bottomOffset:-130
                                        selector:@selector(_onToggleBtnTouchUpInside:)];

    _debugBtnToggle =
        [DebugInfoUtil makeUIButtonAndAddSubView:self
                                           title:@"调"
                                             tag:DebugViewStatus_DebuggerInfo
                                    bottomOffset:-80
                                        selector:@selector(_onToggleBtnTouchUpInside:)];
}

- (void)_onToggleBtnTouchUpInside:(UIButton*)btn {
    sCurrentStatus = btn.tag;
    [self _updateStatusToDebug:sCurrentStatus];
}

- (void)_updateStatusToDebug:(DebugViewStatus)status {
    switch (status) {
        case DebugViewStatus_Basic:
            _subViewBasicInfo.hidden = NO;
            _subViewDebuggerInfo.hidden = YES;
            break;

        case DebugViewStatus_DebuggerInfo:
            _subViewBasicInfo.hidden = YES;
            _subViewDebuggerInfo.hidden = NO;
            break;

        default:
            NSLog(@"%s InnerErorr, status:%d", __func__, (int)status);
            break;
    }
    _basicBtnToggle.layer.borderColor =
        [UIColor colorWithWhite:1.f
                          alpha:(status == DebugViewStatus_Basic) ? TOGGLE_BTN_ALPHA_PRESSED : 0.5f]
            .CGColor;
    _debugBtnToggle.layer.borderColor =
        [UIColor colorWithWhite:1.f
                          alpha:(status == DebugViewStatus_DebuggerInfo) ? TOGGLE_BTN_ALPHA_PRESSED
                                                                         : 0.5f]
            .CGColor;
}

#pragma mark render
- (void)renderLiveBasicInfo:(KwaiPlayerDebugInfo*)info {
    AppLiveQosDebugInfo* liveInfo = info.appLiveDebugInfo;
    // section 基本信息
    if (liveInfo.isLiveManifest) {
        _lbMediaType.text = [NSString stringWithFormat:@"直播多码率"];
    } else {
        _lbMediaType.text = [NSString stringWithFormat:@"普通直播"];
    }
    _lbDeviceType.text = [self _getDeviceType:liveInfo];
    _lbInputUrl.text = [NSString stringWithFormat:@"%@", liveInfo.playUrl];
    _lbVideoCodec.text = liveInfo.videoDecoder;
    _lbAudioCodec.text = liveInfo.audioDecoder;

    _lbDimenFpsKps.text = [NSString
        stringWithFormat:@"分辨率 : %@\n实时帧率 : %.2f/%.2f/%.2f\n%@", liveInfo.naturalSizeString,
                         liveInfo.videoReadFramesPerSecond, liveInfo.videoDecodeFramesPerSecond,
                         liveInfo.videoDisplayFramesPerSecond, [self _getSpeedInfo:liveInfo]];

    // section 播放状态
    //首屏
    _lbFirstRender.text = [NSString stringWithFormat:@"%dms", liveInfo.firstScreenTimeTotal];
    //卡顿
    _lbBlockInfo.text = [NSString
        stringWithFormat:@"%d | %.2fs", liveInfo.blockCnt, liveInfo.blockDuration * 1.0 / 1000];
    //首屏追赶时长
    _lbFirstScreenDrop.text =
        [NSString stringWithFormat:@"%dms", liveInfo.firstScreenTimeDroppedDuration];
    //追赶总时长
    _lbTotalDrop.text = [NSString stringWithFormat:@"%dms", liveInfo.totalDroppedDuration];
    //端到端延迟
    _lbDelay.text = [NSString stringWithFormat:@"端到端延迟: Audio=%d, Video=[%d,%d,%d,%d]",
                                               liveInfo.audioDelay, liveInfo.videoDelayRecv,
                                               liveInfo.videoDelayBefDec, liveInfo.videoDelayAftDec,
                                               liveInfo.videoDelayRender];

    _lbHostInfo.text = [NSString stringWithFormat:@"%@", liveInfo.hostInfo];
    _lbVencInit.text = [NSString stringWithFormat:@"%@", liveInfo.vencInit];
    _lbAencInit.text = [NSString stringWithFormat:@"%@", liveInfo.aencInit];
    _lbVencDynamic.text = [NSString stringWithFormat:@"%@", liveInfo.vencDynamic];
    _lbComment.text = [NSString stringWithFormat:@"%@", liveInfo.comment];
}

- (void)renderLiveDebuggerInfo:(KwaiPlayerDebugInfo*)info {
    AppLiveQosDebugInfo* liveInfo = info.appLiveDebugInfo;

    _lbExtraInfoFromApp.text = _extraInfoOfApp == nil ? @"无" : _extraInfoOfApp;
    _lbAvQueueStatus.text = [self _getPacketBufferInfo:liveInfo];
    _lbTotalDataStatus.text = [NSString
        stringWithFormat:@"audio : %.2fK | video : %.2fK\ntotal : %.2fK",
                         liveInfo.audioTotalDataSize / 1024.0f,
                         liveInfo.videoTotalDataSize / 1024.0f, liveInfo.totalDataSize / 1024.0f];
    _lbFirstScreen.text = [NSString stringWithFormat:@"%dms", liveInfo.firstScreenTimeTotal];
    _lbFirstScreenDetail.text = [NSString stringWithFormat:@"%@", liveInfo.firstScreenStepCostInfo];
    _lbSpeedupThreshold.text = [NSString stringWithFormat:@"%dms", liveInfo.speedupThresholdMs];
}

- (void)showDebugInfo:(KwaiPlayerDebugInfo*)info {
    [self renderLiveBasicInfo:info];
    [self renderLiveDebuggerInfo:info];

    _lastKwaiDebugInfo = info;
}

- (NSString*)_getSpeedInfo:(AppLiveQosDebugInfo*)liveInfo {
    NSString* speedInfo = nil;
    CGFloat downloadSpeed = 0;
    CGFloat bps = 0;

    NSTimeInterval currentTime = CACurrentMediaTime();
    if (self.lastRefreshTime) {
        if (liveInfo.totalDataSize < self.lastDataSize) {
            downloadSpeed = 0;
        } else {
            downloadSpeed = (liveInfo.totalDataSize - self.lastDataSize) * 8 / 1000 /
                            (currentTime - self.lastRefreshTime);
        }
        bps = (liveInfo.readSize - self.lastReadSize) * 1000 * 8 /
              (currentTime - self.lastRefreshTime);
    }

    self.lastDataSize = liveInfo.totalDataSize;
    self.lastReadSize = liveInfo.readSize;
    self.lastRefreshTime = currentTime;

    if (liveInfo.isLiveManifest) {
        speedInfo = [NSString stringWithFormat:@"实时网速 : %.1f/%.1fkbps\n码率 : %.2fkbps",
                                               (float)liveInfo.kflvBandwidthCurrent,
                                               (float)liveInfo.kflvBandwidthFragment, bps];
    } else {
        speedInfo =
            [NSString stringWithFormat:@"实时网速 : %.1fkbps\n码率 : %.2fkbps", downloadSpeed, bps];
    }
    return speedInfo;
}

- (NSString*)_getDeviceType:(AppLiveQosDebugInfo*)liveInfo {
    NSString* deviceType = nil;

    switch (liveInfo.sourceDeviceType) {
        case kVideoCapturerDeviceUnknown:
            deviceType = @"Unknown";
            break;
        case kVideoCapturerDeviceCamera:
            deviceType = @"Camera";
            break;
        case kVideoCapturerDeviceFrontCamera:
            deviceType = @"FrontCamera";
            break;
        case kVideoCapturerDeviceBackCamera:
            deviceType = @"BackCamera";
            break;
        case kVideoCapturerDeviceGlass:
            deviceType = @"Glass";
            break;
        default:
            deviceType = @"";
    }

    return deviceType;
}

- (NSString*)_getPacketBufferInfo:(AppLiveQosDebugInfo*)liveInfo {
    NSString* packetBufferInfo = nil;
    if (liveInfo.isLiveManifest) {
        packetBufferInfo =
            [NSString stringWithFormat:@"%s : %.2fs | %.2fK\n%s : %.2f/%.2f/%.2fs | %.2fKB",
                                       "a-packet", (float)liveInfo.audioBufferTimeLength / 1000.0f,
                                       (float)liveInfo.audioBufferByteLength / 1000.0f, "v-packet",
                                       (float)liveInfo.kflvCurrentBufferMs / 1000.0f,
                                       (float)liveInfo.kflvEstimateBufferMs / 1000.0f,
                                       (float)liveInfo.kflvPredictedBufferMs / 1000.0f,
                                       (float)liveInfo.videoBufferByteLength / 1000.0f];
    } else {
        packetBufferInfo =
            [NSString stringWithFormat:@"%s : %.2fs | %.2fK\n%s : %.2fs | %.2fK", "a-packet",
                                       (float)liveInfo.audioBufferTimeLength / 1000.0f,
                                       (float)liveInfo.audioBufferByteLength / 1000.0f, "v-packet",
                                       (float)liveInfo.videoBufferTimeLength / 1000.0f,
                                       (float)liveInfo.videoBufferByteLength / 1000.0f];
    }
    return packetBufferInfo;
}
@end
