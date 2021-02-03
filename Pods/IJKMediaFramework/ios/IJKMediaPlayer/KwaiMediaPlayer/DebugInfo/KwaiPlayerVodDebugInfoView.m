//
//  KwaiPlayerVodDebugInfoView.m
//  IJKMediaFramework
//
//  Created by MarshallShuai on 2018/12/13.
//  Copyright © 2018 kuaishou. All rights reserved.
//

#import "KwaiPlayerVodDebugInfoView.h"
#import <KSAwesomeCache.h>
#import <MJExtension/MJExtension.h>
#import <Masonry/Masonry.h>
#import "DebugInfoUtil.h"
#import "KSAwesomeCache.h"
#import "SpeedChart.h"

typedef NS_ENUM(NSInteger, DebugViewStatus) {
    DebugViewStatus_Basic = 0,
    DebugViewStatus_DebuggerInfo,
    DebugViewStatus_NetInfo,
    DebugViewStatus_Hodor,
    DebugViewStatus_ConfigInfo,
    DebugViewStatus_VodAdaptiveInfo
};
static DebugViewStatus sCurrentStatus = DebugViewStatus_Basic;

static const float TOGGLE_BTN_ALPHA_PRESSED = 1.f;
static const float VIEW_BG_ALPHA = 0.7f;

static const int KB = 1024;
static const int MB = 1024 * 1024;

static const NSTimeInterval REPORT_INTERVAL = 0.33;

@interface KwaiPlayerVodDebugInfoView ()

@property(nonatomic) NSTimer* timer;
@property(nonatomic) UIButton* basicBtnToggle;
@property(nonatomic) UIButton* debuggerBtnToggle;
@property(nonatomic) UIButton* netBtnToggle;
@property(nonatomic) UIButton* hodorBtnToggle;
@property(nonatomic) UIButton* configBtnToggle;
@property(nonatomic) UIButton* adaptiveBtnToggle;

@property(nonatomic) KwaiPlayerDebugInfo* lastKwaiDebugInfo;

#pragma mark BasicInfo
@property(nonatomic) UIView* subViewVodBasic;
// section 播放器相关配置
@property(nonatomic) UILabel* lbMediaType;
@property(nonatomic) UILabel* lbInputUrl;
@property(nonatomic) UILabel* lbPreLoad;
// section 视频信息
@property(nonatomic) UILabel* lbDimenFpsKps;
@property(nonatomic) UILabel* lbVideoCodec;
@property(nonatomic) UILabel* lbAudioCodec;
@property(nonatomic) UILabel* lbMetaComment;
// section 首屏
@property(nonatomic) UILabel* lbFirstRender;
@property(nonatomic) UILabel* lbStartPlayBlockStatus;
// section 播放状态
@property(nonatomic) UILabel* lbPlayerStatus;
@property(nonatomic) UILabel* lbBlockInfo;
@property(nonatomic) UILabel* lbDropFrame;
@property(nonatomic) UIProgressView* pbPlayProgress;
@property(nonatomic) UIProgressView* pbPlaySecondaryProgress;
@property(nonatomic) UILabel* lbPositionDuraiton;
@property(nonatomic) UILabel* lbAvQueueStatus;
@property(nonatomic) UILabel* lbAudioPacketInfo;
@property(nonatomic) UILabel* lbFirstScreen;
@property(nonatomic) UILabel* lbFirstScreenDetail;

#pragma mark DebuggerInfo
@property(nonatomic) UIView* subViewDebuggerInfo;
@property(nonatomic) UILabel* lbExtraInfoFromApp;
@property(nonatomic) UILabel* lbAutoTestTags;
@property(nonatomic) UILabel* lbSdkVer;

#pragma mark NetInfo
@property(nonatomic) UIView* subViewNetInfo;
@property(nonatomic) UILabel* lbHostIp;
@property(nonatomic) UIProgressView* pbTotalCacheRatio;
@property(nonatomic) UILabel* lbCacheTotalSpace;
@property(nonatomic) UIProgressView* pbCurrentDownloadProgress;
@property(nonatomic) UILabel* lbCachingInfo;
@property(nonatomic) UILabel* lbPlayingUri;
@property(nonatomic) UILabel* lbRetryInfo;
@property(nonatomic) UILabel* lbDccAlgStatus;
@property(nonatomic) UILabel* lbTvDownloadStatus;
@property(nonatomic) SpeedChart* speedChart;

@property(nonatomic) UILabel* lbCustomString;
@property(nonatomic) UILabel* lbCacheV2Info;

@property(nonatomic) UIView* subViewHodorInfo;

#pragma mark ConfigInfo
@property(nonatomic) UIView* subViewConfigInfo;
@property(nonatomic) UILabel* lbPlayerDetailConfig;

#pragma mark VodAdaptiveInfo
@property(nonatomic) UIView* subViewVodAdaptiveInfo;
@property(nonatomic) UILabel* lbVodAdaptive;

@end

@implementation KwaiPlayerVodDebugInfoView

- (instancetype)initWithFrame:(CGRect)frame {
    self = [super initWithFrame:frame];
    if (self) {
        [self _setupSubViews];
    }
    return self;
}

- (void)setHodorView:(UIView*)hodorView {
    _subViewHodorInfo = hodorView;
    [self insertSubview:_subViewHodorInfo atIndex:0];
    [self _updateStatusToDebug:sCurrentStatus];
}

- (UIView*)hitTest:(CGPoint)point withEvent:(UIEvent*)event {
    id view = [super hitTest:point withEvent:event];
    if (view == _basicBtnToggle) {
        return _basicBtnToggle;
    } else if (view == _debuggerBtnToggle) {
        return _debuggerBtnToggle;
    } else if (view == _netBtnToggle) {
        return _netBtnToggle;
    } else if (view == _hodorBtnToggle) {
        return _hodorBtnToggle;
    } else if (view == _configBtnToggle) {
        return _configBtnToggle;
    } else if (view == _adaptiveBtnToggle) {
        return _adaptiveBtnToggle;
    } else {
        return nil;
    }
}

#pragma mark setup views
- (void)_setupSubViews {
    [self _setupBasicInfoViews];
    [self _setupDebuggerInfoViews];
    [self _setupNetInfoViews];
    [self _setupConfigInfoViews];
    [self _setupVodAdaptiveViews];
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
    UIView* subRoot = _subViewVodBasic = [self addSubViewRoot];

    UIView* lastAnchor = nil;
    NSDictionary* tmpDict;
    lastAnchor = [DebugInfoUtil appendRowTitle:subRoot anchor:lastAnchor text:@"基本信息"];
    lastAnchor = _lbMediaType = [DebugInfoUtil appendRowKeyValueContent:subRoot
                                                                 anchor:lastAnchor
                                                                    key:@"视频类型"];
    lastAnchor = _lbInputUrl = [DebugInfoUtil appendRowKeyValueContent:subRoot
                                                                anchor:lastAnchor
                                                                   key:@"inputUrl"];
    lastAnchor = _lbExtraInfoFromApp = [DebugInfoUtil appendRowKeyValueContent:subRoot
                                                                        anchor:lastAnchor
                                                                           key:@"App端信息"];
    lastAnchor = _lbSdkVer = [DebugInfoUtil appendRowKeyValueContent:subRoot
                                                              anchor:lastAnchor
                                                                 key:@"SDK Ver"];

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
    lastAnchor = _lbMetaComment = [DebugInfoUtil appendRowKeyValueContent:subRoot
                                                                   anchor:lastAnchor
                                                                      key:@"meta.comment"];

    lastAnchor = [DebugInfoUtil appendRowTitle:subRoot anchor:lastAnchor text:@"播放状态"];
    lastAnchor = _lbPlayerStatus = [DebugInfoUtil appendRowKeyValueContent:subRoot
                                                                    anchor:lastAnchor
                                                                       key:@"播放器状态"];

    tmpDict = [DebugInfoUtil appendRowKeyValuePrgressViewContent:subRoot
                                                          anchor:lastAnchor
                                                             key:@"播放进度"
                                                   makeSecondary:YES];
    lastAnchor = _lbPositionDuraiton = [tmpDict objectForKey:kValLabelKey];
    _pbPlayProgress = [tmpDict objectForKey:kProgressViewKey];
    _pbPlaySecondaryProgress = [tmpDict objectForKey:kSecondaryProgressViewKey];

    lastAnchor = _lbFirstRender = [DebugInfoUtil appendRowKeyValueContent:subRoot
                                                                   anchor:lastAnchor
                                                                      key:@"首屏耗时"];

    lastAnchor = _lbBlockInfo = [DebugInfoUtil appendRowKeyValueContent:subRoot
                                                                 anchor:lastAnchor
                                                                    key:@"卡顿信息"];

    lastAnchor = _lbDropFrame = [DebugInfoUtil appendRowKeyValueContent:subRoot
                                                                 anchor:lastAnchor
                                                                    key:@"丢帧信息"];

    lastAnchor = [DebugInfoUtil appendRowTitle:subRoot anchor:lastAnchor text:@"优化策略"];
    lastAnchor = _lbPreLoad = [DebugInfoUtil appendRowKeyValueContent:subRoot
                                                               anchor:lastAnchor
                                                                  key:@"预加载"];
    lastAnchor = _lbStartPlayBlockStatus = [DebugInfoUtil appendRowKeyValueContent:subRoot
                                                                            anchor:lastAnchor
                                                                               key:@"启播Buffer"];
    lastAnchor = _lbDccAlgStatus = [DebugInfoUtil appendRowKeyValueContent:subRoot
                                                                    anchor:lastAnchor
                                                                       key:@"带宽优化状态"];
}

- (SpeedChart*)_setupSpeedChard:(UIView*)parent anchor:(UIView*)anchor {
    SpeedChart* chart = [[SpeedChart alloc] initWithInterval:REPORT_INTERVAL
                                                maxSpeedKbps:500
                                                 maxTimeline:1];

    [parent addSubview:chart];
    [chart mas_makeConstraints:^(MASConstraintMaker* make) {
        make.leading.equalTo(parent.mas_trailing).multipliedBy(GUIDE_LINE_RATIO).offset(-85);
        make.trailing.equalTo(parent).offset(-MARGIN_BIG);
        make.top.equalTo(anchor ? anchor.mas_bottom : anchor.mas_top).offset(MARGIN_SMALL);
        make.height.equalTo(chart.mas_width).multipliedBy(0.3);
        make.bottom.lessThanOrEqualTo(parent.mas_bottom);
    }];

    UILabel* lbVal = [DebugInfoUtil makeContentLabel];
    [parent addSubview:lbVal];

    [lbVal mas_makeConstraints:^(MASConstraintMaker* make) {
        make.leading.equalTo(parent.mas_trailing).multipliedBy(GUIDE_LINE_RATIO).offset(MARGIN_BIG);
        make.trailing.offset(-MARGIN_DEFAULT);
        make.top.equalTo(anchor ? anchor.mas_bottom : parent.mas_top).offset(MARGIN_BIG);
    }];
    _lbTvDownloadStatus = lbVal;

    [chart drawCurveView];

    return chart;
}

- (void)_setupDebuggerInfoViews {
    UIView* subRoot = _subViewDebuggerInfo = [self addSubViewRoot];

    UIView* lastAnchor = nil;

    lastAnchor = [DebugInfoUtil appendRowTitle:subRoot anchor:lastAnchor text:@"Debugger"];

    lastAnchor = _lbAutoTestTags = [DebugInfoUtil appendRowKeyValueContent:subRoot
                                                                    anchor:lastAnchor
                                                                       key:@"自动化tag"];
    lastAnchor = _lbAvQueueStatus = [DebugInfoUtil appendRowKeyValueContent:subRoot
                                                                     anchor:lastAnchor
                                                                        key:@"音视频队列"];

    lastAnchor = _lbFirstScreen = [DebugInfoUtil appendRowKeyValueContent:subRoot
                                                                   anchor:lastAnchor
                                                                      key:@"首屏耗时"];

    _lbFirstScreenDetail = [DebugInfoUtil appendRowKeyValueContent:subRoot
                                                            anchor:lastAnchor
                                                               key:@"首屏细分耗时"];
}

- (void)_setupNetInfoViews {
    UIView* subRoot = _subViewNetInfo = [self addSubViewRoot];
    NSDictionary* tmpDict;
    UIView* lastAnchor = nil;
    lastAnchor = [DebugInfoUtil appendRowTitle:subRoot anchor:lastAnchor text:@"网络"];
    lastAnchor = _lbHostIp = [DebugInfoUtil appendRowKeyValueContent:subRoot
                                                              anchor:lastAnchor
                                                                 key:@"Host/IP"];
    tmpDict = [DebugInfoUtil appendRowKeyValuePrgressViewContent:subRoot
                                                          anchor:lastAnchor
                                                             key:@"总空间"
                                                   makeSecondary:NO];
    lastAnchor = _lbCacheTotalSpace = [tmpDict objectForKey:kValLabelKey];
    _pbTotalCacheRatio = [tmpDict objectForKey:kProgressViewKey];

    tmpDict = [DebugInfoUtil appendRowKeyValuePrgressViewContent:subRoot
                                                          anchor:lastAnchor
                                                             key:@"当前已缓存"
                                                   makeSecondary:NO];
    lastAnchor = _lbCachingInfo = [tmpDict objectForKey:kValLabelKey];
    _pbCurrentDownloadProgress = [tmpDict objectForKey:kProgressViewKey];

    lastAnchor = _lbPlayingUri = [DebugInfoUtil appendRowKeyValueContent:subRoot
                                                                  anchor:lastAnchor
                                                                     key:@"PlayingURI"];
    _lbPlayingUri.numberOfLines = 1;  // playingurl 暂时不展开

    lastAnchor = _lbRetryInfo = [DebugInfoUtil appendRowKeyValueContent:subRoot
                                                                 anchor:lastAnchor
                                                                    key:@"RetryInfo"];

    lastAnchor = _speedChart = [self _setupSpeedChard:subRoot anchor:lastAnchor];

    lastAnchor = _lbCustomString = [DebugInfoUtil appendRowKeyValueContent:subRoot
                                                                    anchor:lastAnchor
                                                                       key:@"自定义字段"];

    lastAnchor = _lbCacheV2Info = [DebugInfoUtil appendRowKeyValueContent:subRoot
                                                                   anchor:lastAnchor
                                                                      key:@"CacheV2"];

    // 暂时小一点，因为空间有限，而且hodor队列的内容比较多
    _lbCacheV2Info.font = [UIFont systemFontOfSize:11];
}

- (void)_setupConfigInfoViews {
    UIView* subRoot = _subViewConfigInfo = [self addSubViewRoot];

    UIView* lastAnchor = nil;
    lastAnchor = _lbPlayerDetailConfig = [DebugInfoUtil appendRowKeyValueContent:subRoot
                                                                          anchor:lastAnchor
                                                                             key:@"配置详情"];
}

- (void)_setupVodAdaptiveViews {
    UIView* subRoot = _subViewVodAdaptiveInfo = [self addSubViewRoot];

    UIView* lastAnchor = nil;
    lastAnchor = [DebugInfoUtil appendRowTitle:subRoot anchor:lastAnchor text:@"短视频多码率"];
    lastAnchor = _lbVodAdaptive = [DebugInfoUtil appendRowSingleContent:subRoot anchor:lastAnchor];
}

#pragma mark toggle button
- (void)_setupToggleButton {
    // button
    //基本信息
    _basicBtnToggle =
        [DebugInfoUtil makeUIButtonAndAddSubView:self
                                           title:@"基"
                                             tag:DebugViewStatus_Basic
                                    bottomOffset:-280
                                        selector:@selector(_onToggleBtnTouchUpInside:)];

    _debuggerBtnToggle =
        [DebugInfoUtil makeUIButtonAndAddSubView:self
                                           title:@"调"
                                             tag:DebugViewStatus_DebuggerInfo
                                    bottomOffset:-230
                                        selector:@selector(_onToggleBtnTouchUpInside:)];

    _netBtnToggle = [DebugInfoUtil makeUIButtonAndAddSubView:self
                                                       title:@"网"
                                                         tag:DebugViewStatus_NetInfo
                                                bottomOffset:-180
                                                    selector:@selector(_onToggleBtnTouchUpInside:)];

    _hodorBtnToggle =
        [DebugInfoUtil makeUIButtonAndAddSubView:self
                                           title:@"H"
                                             tag:DebugViewStatus_Hodor
                                    bottomOffset:-130
                                        selector:@selector(_onToggleBtnTouchUpInside:)];

    _configBtnToggle =
        [DebugInfoUtil makeUIButtonAndAddSubView:self
                                           title:@"配"
                                             tag:DebugViewStatus_ConfigInfo
                                    bottomOffset:-80
                                        selector:@selector(_onToggleBtnTouchUpInside:)];

    _adaptiveBtnToggle =
        [DebugInfoUtil makeUIButtonAndAddSubView:self
                                           title:@"多"
                                             tag:DebugViewStatus_VodAdaptiveInfo
                                    bottomOffset:-30
                                        selector:@selector(_onToggleBtnTouchUpInside:)];
}

- (void)_onToggleBtnTouchUpInside:(UIButton*)btn {
    sCurrentStatus = btn.tag;
    [self _updateStatusToDebug:sCurrentStatus];
}

- (void)_updateStatusToDebug:(DebugViewStatus)status {
    switch (status) {
        case DebugViewStatus_Basic:
            _subViewVodBasic.hidden = NO;
            _subViewDebuggerInfo.hidden = YES;
            _subViewNetInfo.hidden = YES;
            _subViewHodorInfo.hidden = YES;
            _subViewConfigInfo.hidden = YES;
            _subViewVodAdaptiveInfo.hidden = YES;
            break;

        case DebugViewStatus_DebuggerInfo:
            _subViewVodBasic.hidden = YES;
            _subViewDebuggerInfo.hidden = NO;
            _subViewNetInfo.hidden = YES;
            _subViewHodorInfo.hidden = YES;
            _subViewConfigInfo.hidden = YES;
            _subViewVodAdaptiveInfo.hidden = YES;
            break;
        case DebugViewStatus_NetInfo:
            _subViewVodBasic.hidden = YES;
            _subViewDebuggerInfo.hidden = YES;
            _subViewNetInfo.hidden = NO;
            _subViewHodorInfo.hidden = YES;
            _subViewConfigInfo.hidden = YES;
            _subViewVodAdaptiveInfo.hidden = YES;
            break;
        case DebugViewStatus_Hodor:
            _subViewVodBasic.hidden = YES;
            _subViewDebuggerInfo.hidden = YES;
            _subViewNetInfo.hidden = YES;
            _subViewHodorInfo.hidden = NO;
            _subViewConfigInfo.hidden = YES;
            _subViewVodAdaptiveInfo.hidden = YES;
            break;
        case DebugViewStatus_ConfigInfo:
            _subViewVodBasic.hidden = YES;
            _subViewDebuggerInfo.hidden = YES;
            _subViewNetInfo.hidden = YES;
            _subViewHodorInfo.hidden = YES;
            _subViewConfigInfo.hidden = NO;
            _subViewVodAdaptiveInfo.hidden = YES;
            break;
        case DebugViewStatus_VodAdaptiveInfo:
            _subViewVodBasic.hidden = YES;
            _subViewDebuggerInfo.hidden = YES;
            _subViewNetInfo.hidden = YES;
            _subViewHodorInfo.hidden = YES;
            _subViewConfigInfo.hidden = YES;
            _subViewVodAdaptiveInfo.hidden = NO;
            break;
        default:
            NSLog(@"%s InnerErorr, status:%d", __func__, (int)status);
            break;
    }

    _basicBtnToggle.layer.borderColor =
        [UIColor colorWithWhite:1.f
                          alpha:(status == DebugViewStatus_Basic) ? TOGGLE_BTN_ALPHA_PRESSED : 0.5f]
            .CGColor;
    _debuggerBtnToggle.layer.borderColor =
        [UIColor colorWithWhite:1.f
                          alpha:(status == DebugViewStatus_DebuggerInfo) ? TOGGLE_BTN_ALPHA_PRESSED
                                                                         : 0.5f]
            .CGColor;
    _netBtnToggle.layer.borderColor =
        [UIColor
            colorWithWhite:1.f
                     alpha:(status == DebugViewStatus_NetInfo) ? TOGGLE_BTN_ALPHA_PRESSED : 0.5f]
            .CGColor;
    _hodorBtnToggle.layer.borderColor =
        [UIColor colorWithWhite:1.f
                          alpha:(status == DebugViewStatus_Hodor) ? TOGGLE_BTN_ALPHA_PRESSED : 0.5f]
            .CGColor;
    _configBtnToggle.layer.borderColor =
        [UIColor
            colorWithWhite:1.f
                     alpha:(status == DebugViewStatus_ConfigInfo) ? TOGGLE_BTN_ALPHA_PRESSED : 0.5f]
            .CGColor;
    _adaptiveBtnToggle.layer.borderColor =
        [UIColor
            colorWithWhite:1.f
                     alpha:(status == DebugViewStatus_VodAdaptiveInfo) ? TOGGLE_BTN_ALPHA_PRESSED
                                                                       : 0.5f]
            .CGColor;
}

#pragma mark render
- (void)renderVodBasicInfo:(KwaiPlayerDebugInfo*)info {
    AppVodQosDebugInfo* vodInfo = info.appVodDebugInfo;
    // section 播放器相关配置
    _lbMediaType.text =
        [NSString stringWithFormat:@"短视频 | 转码类型 : %@", vodInfo.transcodeType];

    _lbInputUrl.text = vodInfo.fileName;
    _lbSdkVer.text = [KwaiFFPlayerController getVersion];
    // 视频信息
    _lbDimenFpsKps.text =
        [NSString stringWithFormat:@"%dx%d | fps:%4.1f | 总码率:%ld kbps", vodInfo.metaWidth,
                                   vodInfo.metaHeight, vodInfo.metaFps, vodInfo.bitrate / KB];
    _lbVideoCodec.text = vodInfo.metaVideoDecoderInfo;
    _lbAudioCodec.text = vodInfo.metaAudioDecoderInfo;
    _lbMetaComment.text = vodInfo.metaComment;

    // section 播放状态
    // todo add decodeFps | renderFps
    // todo add loadState (exccpt for playState)
    _lbPlayerStatus.text =
        [NSString stringWithFormat:@"%@ | Loop:%@ | %@", vodInfo.currentState,
                                   vodInfo.ffpLoopCnt == 0
                                       ? @"无限"
                                       : [NSString stringWithFormat:@"%d", vodInfo.ffpLoopCnt],
                                   vodInfo.fullErrorMsg];

    float playProgress = vodInfo.metaDurationMs <= 0
                             ? 0
                             : (1.f * vodInfo.currentPositionMs / vodInfo.metaDurationMs);
    float bufferProgress =
        vodInfo.metaDurationMs <= 0 ? 0 : 1.f * vodInfo.playableDurationMs / vodInfo.metaDurationMs;
    [_pbPlayProgress setProgress:playProgress];
    [_pbPlaySecondaryProgress setProgress:bufferProgress];

    _lbFirstRender.text = [NSString stringWithFormat:@"%lldms", vodInfo.firstScreenWithoutAppCost];

    _lbBlockInfo.text = vodInfo.blockStatus;
    _lbDropFrame.text = vodInfo.dropFrame;

    if (vodInfo.usePreLoad) {
        _lbPreLoad.text = [NSString
            stringWithFormat:@"%dms | %@ | 运行中Player个数:%d", vodInfo.preLoadMs,
                             vodInfo.preLoadFinish ? @"完成" : @"未完成", vodInfo.alivePlayerCnt];
        _lbPreLoad.textColor =
            (vodInfo.preLoadFinish ? [DebugInfoUtil UIColor_Green] : [DebugInfoUtil UIColor_Red]);
    } else {
        _lbPreLoad.text =
            [NSString stringWithFormat:@"未开启 | 运行中Player个数:%d", vodInfo.alivePlayerCnt];
        _lbPreLoad.textColor = [DebugInfoUtil UIColor_Disabled];
    }

    _lbStartPlayBlockStatus.text = vodInfo.startPlayBlockStatus;
    _lbStartPlayBlockStatus.textColor = vodInfo.startPlayBlockUsed
                                            ? [DebugInfoUtil UIColor_Green]
                                            : [DebugInfoUtil UIColor_Disabled];

    if (vodInfo.dccAlgConfigEnabled) {
        _lbDccAlgStatus.textColor =
            vodInfo.dccAlgUsed ? [DebugInfoUtil UIColor_Green] : [DebugInfoUtil UIColor_Red];
    } else {
        _lbDccAlgStatus.textColor = [DebugInfoUtil UIColor_Disabled];
    }
    _lbDccAlgStatus.text = vodInfo.dccAlgStatus;
}

- (void)renderDebuggerInfo:(KwaiPlayerDebugInfo*)info {
    AppVodQosDebugInfo* vodInfo = info.appVodDebugInfo;
    _lbPositionDuraiton.text =
        [NSString stringWithFormat:@"%3.2fs/%3.2fs", vodInfo.currentPositionMs / 1000.f,
                                   vodInfo.metaDurationMs / 1000.f];
    _lbAutoTestTags.text = vodInfo.autoTestTags;
    _lbAvQueueStatus.text = vodInfo.avQueueStatus;
    _lbFirstScreen.text = [NSString stringWithFormat:@"%lldms", vodInfo.firstScreenWithoutAppCost];
    _lbFirstScreenDetail.text = vodInfo.firstScreenStepCostInfo;

    _lbVodAdaptive.text = info.appVodDebugInfo.vodAdaptiveInfo;
    _lbExtraInfoFromApp.text = _extraInfoOfApp == nil ? @"无" : _extraInfoOfApp;
    _lbRetryInfo.text = _retryInfoOfApp == nil ? @"无" : _retryInfoOfApp;
}

- (void)renderNetInfo:(KwaiPlayerDebugInfo*)info {
    AppVodQosDebugInfo* vodInfo = info.appVodDebugInfo;

    _lbHostIp.text = [NSString stringWithFormat:@"HttpDns:%@ | %@ | %@",
                                                is_na_str(vodInfo.host) ? @"未使用" : @"使用",
                                                vodInfo.domain, vodInfo.serverIp];

    // section nativeCache
    static int64_t sNativeCacheLimitBytes = 0;
    if (sNativeCacheLimitBytes <= 0) {
        sNativeCacheLimitBytes = [KSAwesomeCache getCachedBytesLimit];
    }
    int64_t totalCachedBytes = [KSAwesomeCache getCachedBytes];
    float totalCacheRatio =
        sNativeCacheLimitBytes <= 0 ? 0 : 1.f * totalCachedBytes / sNativeCacheLimitBytes;
    [_pbTotalCacheRatio setProgress:totalCacheRatio];
    _lbCacheTotalSpace.text =
        [NSString stringWithFormat:@"%.2fMB/%lldMB", 1.0f * totalCachedBytes / MB,
                                   sNativeCacheLimitBytes / MB];

    float downloadRatio = vodInfo.cacheTotalBytes <= 0
                              ? 0
                              : 1.f * vodInfo.cacheDownloadedBytes / vodInfo.cacheTotalBytes;
    [_pbCurrentDownloadProgress setProgress:downloadRatio];
    _lbCachingInfo.text =
        [NSString stringWithFormat:@"%4.2fMB/%4.2fMB", 1.f * vodInfo.cacheDownloadedBytes / MB,
                                   1.f * vodInfo.cacheTotalBytes / MB];

    if (vodInfo.cacheEnabled) {
        _lbTvDownloadStatus.hidden = NO;
        _lbCachingInfo.hidden = NO;

        if (vodInfo.cacheErrorCode != 0) {
            _lbTvDownloadStatus.textColor = [DebugInfoUtil UIColor_Red];
        } else if (vodInfo.cacheIsReadingCachedFile) {
            _lbTvDownloadStatus.textColor = [DebugInfoUtil UIColor_Green];
        } else {
            _lbTvDownloadStatus.textColor = [DebugInfoUtil UIColor_Orange];
            [_speedChart appendSpeedSample:vodInfo.downloadCurrentSpeedKbps];
        }
        _lbTvDownloadStatus.text = vodInfo.prettyDownloadSpeedInfo;
        _lbPlayingUri.text = vodInfo.cacheCurrentReadingUri;
    }
}

- (void)renderVodConfigInfo:(KwaiPlayerDebugInfo*)info {
    _lbPlayerDetailConfig.text = [info.appPlayerDebugInfo getPrettySingleText];
    _lbCustomString.text = info.appVodDebugInfo.customString;
    _lbCacheV2Info.text = info.appVodDebugInfo.cacheV2Info;
}

- (void)renderVodAdaptiveInfo:(KwaiPlayerDebugInfo*)info {
    _lbVodAdaptive.text = info.appVodDebugInfo.vodAdaptiveInfo;
}

- (void)showDebugInfo:(KwaiPlayerDebugInfo*)info {
    [self renderVodBasicInfo:info];
    [self renderDebuggerInfo:info];
    [self renderNetInfo:info];
    [self renderVodConfigInfo:info];
    [self renderVodAdaptiveInfo:info];

    _lastKwaiDebugInfo = info;
}

@end
