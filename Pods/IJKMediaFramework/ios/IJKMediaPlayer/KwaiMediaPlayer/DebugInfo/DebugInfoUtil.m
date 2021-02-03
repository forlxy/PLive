//
//  DebugInfoUtil.m
//  IJKMediaFramework
//
//  Created by MarshallShuai on 2018/12/13.
//  Copyright © 2018 kuaishou. All rights reserved.
//

#import "DebugInfoUtil.h"
#import <Masonry/Masonry.h>

#define TOGGLE_BTN_ALPHA_NORMAL 0.7f
#define TOGGLE_BTN_ALPHA_PRESSED 1.f

#define TITLE_FONT_SZIE 14
#define CONTENT_FONT_SZIE 13

@implementation DebugInfoUtil

+ (UIColor*)UIColor_Orange {
    return [UIColor colorWithRed:1.f green:.5f blue:0 alpha:1];
}

+ (UIColor*)UIColor_Red {
    return [UIColor colorWithRed:0.984 green:0.345 blue:0.345 alpha:1];
}

+ (UIColor*)UIColor_Green {
    return [UIColor colorWithRed:0.42 green:0.827 blue:0.475 alpha:1];
}

+ (UIColor*)UIColor_Grey {
    return [UIColor colorWithRed:0.7 green:0.7 blue:0.7 alpha:1];
}

+ (UIColor*)UIColor_Disabled {
    return [DebugInfoUtil UIColor_Grey];
}

+ (UILabel*)makeTitleLabel {
    UILabel* label = [[UILabel alloc] init];
    label.textColor = [DebugInfoUtil UIColor_Orange];
    label.font = [UIFont systemFontOfSize:TITLE_FONT_SZIE weight:UIFontWeightHeavy];
    return label;
}

+ (UILabel*)makeContentLabel {
    UILabel* label = [[UILabel alloc] init];
    label.textColor = [UIColor colorWithWhite:1.f alpha:1.f];
    label.font = [UIFont systemFontOfSize:CONTENT_FONT_SZIE];
    label.lineBreakMode = NSLineBreakByWordWrapping;
    label.numberOfLines = 0;
    return label;
}

+ (UIButton*)makeUIButtonAndAddSubView:(UIView*)parent
                                 title:(NSString*)title
                                   tag:(int)tag
                          bottomOffset:(int)offset
                              selector:(SEL)aSelector {
    int btnSize = 42;

    UIButton* btnToggle = [UIButton buttonWithType:UIButtonTypeCustom];
    [btnToggle setTitleColor:[UIColor colorWithWhite:1.f alpha:TOGGLE_BTN_ALPHA_NORMAL]
                    forState:UIControlStateNormal];
    [btnToggle setTitleColor:[UIColor colorWithWhite:1.f alpha:TOGGLE_BTN_ALPHA_PRESSED]
                    forState:UIControlStateHighlighted];
    btnToggle.titleLabel.font = [UIFont boldSystemFontOfSize:21];

    btnToggle.layer.borderColor = [UIColor colorWithWhite:1.f alpha:0.5f].CGColor;
    btnToggle.layer.borderWidth = 3;
    btnToggle.layer.cornerRadius = btnSize / 2;

    btnToggle.tag = tag;
    [btnToggle setTitle:title forState:UIControlStateNormal];

    [btnToggle addTarget:parent action:aSelector forControlEvents:UIControlEventTouchUpInside];

    [parent addSubview:btnToggle];

    [btnToggle mas_makeConstraints:^(MASConstraintMaker* make) {
        make.left.equalTo(parent.mas_left).offset(10);
        make.bottom.equalTo(parent.mas_bottom).offset(offset);
        make.size.mas_equalTo(CGSizeMake(btnSize, btnSize));
    }];

    return btnToggle;
}

+ (UIView*)appendRowTitle:(UIView*)parent anchor:(UIView*)anchor text:(NSString*)title {
    UILabel* lbTitle = [DebugInfoUtil makeTitleLabel];
    lbTitle.text = title;
    [parent addSubview:lbTitle];

    [lbTitle mas_makeConstraints:^(MASConstraintMaker* make) {
        make.trailing.equalTo(parent.mas_trailing).multipliedBy(GUIDE_LINE_RATIO);
        make.top.equalTo(anchor ? anchor.mas_bottom : parent.mas_top).offset(MARGIN_DEFAULT);
    }];

    return lbTitle;
}

+ (NSDictionary*)appendRowKeyValuePrgressViewContent:(UIView*)parent
                                              anchor:(UIView*)anchor
                                                 key:(NSString*)key
                                       makeSecondary:(BOOL)secondary {
    UILabel* lbKey = [DebugInfoUtil makeContentLabel];
    lbKey.text = key;
    [parent addSubview:lbKey];
    [lbKey mas_makeConstraints:^(MASConstraintMaker* make) {
        make.trailing.equalTo(parent.mas_trailing).multipliedBy(GUIDE_LINE_RATIO);
        make.top.equalTo(anchor ? anchor.mas_bottom : parent.mas_top).offset(MARGIN_SMALL);
    }];

    UIProgressView* pvSecondary;
    if (secondary) {
        pvSecondary = [[UIProgressView alloc] init];
        pvSecondary.trackTintColor = [UIColor colorWithWhite:0.f alpha:0.f];
        pvSecondary.tintColor = [[DebugInfoUtil UIColor_Grey] colorWithAlphaComponent:0.5];
        [pvSecondary.subviews enumerateObjectsUsingBlock:^(__kindof UIView* _Nonnull obj,
                                                           NSUInteger idx, BOOL* _Nonnull stop) {
            obj.layer.cornerRadius = 5;
            obj.clipsToBounds = YES;
        }];
        [parent addSubview:pvSecondary];
        [pvSecondary mas_makeConstraints:^(MASConstraintMaker* make) {
            make.leading.equalTo(parent.mas_trailing)
                .multipliedBy(GUIDE_LINE_RATIO)
                .offset(MARGIN_DEFAULT);
            make.trailing.equalTo(parent.mas_trailing).multipliedBy(0.8);
            make.top.equalTo(anchor ? anchor.mas_bottom : parent.mas_top).offset(MARGIN_SMALL);
            make.height.mas_equalTo(TITLE_FONT_SZIE);
        }];
    }

    UIProgressView* pv = [[UIProgressView alloc] init];
    pv.trackTintColor = [UIColor colorWithWhite:1.f alpha:0.2];
    pv.tintColor = [[DebugInfoUtil UIColor_Orange] colorWithAlphaComponent:0.5];
    [pv.subviews enumerateObjectsUsingBlock:^(__kindof UIView* _Nonnull obj, NSUInteger idx,
                                              BOOL* _Nonnull stop) {
        obj.layer.cornerRadius = 5;
        obj.clipsToBounds = YES;
    }];
    [parent addSubview:pv];
    [pv mas_makeConstraints:^(MASConstraintMaker* make) {
        make.leading.equalTo(parent.mas_trailing)
            .multipliedBy(GUIDE_LINE_RATIO)
            .offset(MARGIN_DEFAULT);
        make.trailing.equalTo(parent.mas_trailing).multipliedBy(0.8);
        make.top.equalTo(anchor ? anchor.mas_bottom : parent.mas_top).offset(MARGIN_SMALL);
        make.height.mas_equalTo(TITLE_FONT_SZIE);
    }];

    UILabel* lbVal = [DebugInfoUtil makeContentLabel];
    [parent addSubview:lbVal];
    [lbVal mas_makeConstraints:^(MASConstraintMaker* make) {
        make.leading.equalTo(parent.mas_trailing)
            .multipliedBy(GUIDE_LINE_RATIO)
            .offset(MARGIN_DEFAULT + 2);  // 比一般的KeyValue的textView更多偏移一些
        make.trailing.offset(-MARGIN_DEFAULT);
        make.top.equalTo(anchor ? anchor.mas_bottom : parent.mas_top).offset(MARGIN_SMALL);
        make.bottom.greaterThanOrEqualTo(lbKey.mas_bottom);
    }];

    if (secondary) {
        return
            @{kValLabelKey : lbVal, kProgressViewKey : pv, kSecondaryProgressViewKey : pvSecondary};
    } else {
        return @{kValLabelKey : lbVal, kProgressViewKey : pv};
    }
}

+ (UILabel*)appendRowKeyValueContent:(UIView*)parent anchor:(UIView*)anchor key:(NSString*)key {
    UILabel* lbKey = [DebugInfoUtil makeContentLabel];
    lbKey.text = key;
    [parent addSubview:lbKey];

    UILabel* lbVal = [DebugInfoUtil makeContentLabel];
    [parent addSubview:lbVal];

    [lbKey mas_makeConstraints:^(MASConstraintMaker* make) {
        make.trailing.equalTo(parent.mas_trailing).multipliedBy(GUIDE_LINE_RATIO);
        make.top.equalTo(anchor ? anchor.mas_bottom : parent.mas_top).offset(MARGIN_SMALL);
    }];

    [lbVal mas_makeConstraints:^(MASConstraintMaker* make) {
        make.leading.equalTo(parent.mas_trailing)
            .multipliedBy(GUIDE_LINE_RATIO)
            .offset(MARGIN_DEFAULT);
        make.trailing.offset(-MARGIN_DEFAULT);
        make.top.equalTo(anchor ? anchor.mas_bottom : parent.mas_top).offset(MARGIN_SMALL);
        make.bottom.greaterThanOrEqualTo(lbKey.mas_bottom);
    }];

    return lbVal;
}

+ (UIProgressView*)appendProgressView:(UIView*)parent anchorTextView:(UIView*)anchor {
    UIProgressView* pv = [[UIProgressView alloc] init];
    pv.trackTintColor = [UIColor colorWithWhite:1.f alpha:0.2];
    pv.tintColor = [[DebugInfoUtil UIColor_Orange] colorWithAlphaComponent:0.5];
    [pv.subviews enumerateObjectsUsingBlock:^(__kindof UIView* _Nonnull obj, NSUInteger idx,
                                              BOOL* _Nonnull stop) {
        obj.layer.cornerRadius = 5;
        obj.clipsToBounds = YES;
    }];
    [parent addSubview:pv];

    [pv mas_makeConstraints:^(MASConstraintMaker* make) {
        make.leading.equalTo(parent.mas_trailing)
            .multipliedBy(GUIDE_LINE_RATIO)
            .offset(MARGIN_DEFAULT);
        make.trailing.equalTo(parent.mas_trailing).multipliedBy(0.8);
        make.top.equalTo(anchor.mas_top);
        make.bottom.equalTo(anchor.mas_bottom);
    }];

    return pv;
}

+ (UILabel*)appendRowSingleContent:(UIView*)parent anchor:(UIView*)anchor {
    UILabel* lbTitle = [DebugInfoUtil makeContentLabel];
    [parent addSubview:lbTitle];

    [lbTitle mas_makeConstraints:^(MASConstraintMaker* make) {
        make.leading.equalTo(parent.mas_leading).offset(MARGIN_BIG);
        make.trailing.equalTo(parent.mas_trailing).offset(-MARGIN_BIG);
        make.top.equalTo(anchor ? anchor.mas_bottom : parent.mas_top).offset(MARGIN_SMALL);
    }];

    return lbTitle;
}

@end

#undef TITLE_FONT_SZIE
#undef CONTENT_FONT_SZIE
