//
//  DebugInfoUtil.h
//  IJKMediaFramework
//
//  Created by MarshallShuai on 2018/12/13.
//  Copyright © 2018 kuaishou. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#define na_str @"N/A"
#define safe_from_c_string(str) (str ? [NSString stringWithUTF8String:str] : na_str)
#define is_na_str(str) [str isEqualToString:na_str]

#ifndef WEAK_OBJ_REF
#define WEAK_OBJ_REF(obj) __weak __typeof__(obj) weak_##obj = obj
#define STRONG_OBJ_REF(obj) __strong __typeof__(obj) strong_##obj = obj
#define WS WEAK_OBJ_REF(self);
#define SS __strong __typeof__(weak_self) self = weak_self;
#endif  // WEAK_OBJ_REF

NS_ASSUME_NONNULL_BEGIN

static const float MARGIN_SMALL = 2;
static const float MARGIN_DEFAULT = 5;
static const float MARGIN_BIG = 15;
static const float GUIDE_LINE_RATIO = 0.3f;

static const NSString* kValLabelKey = @"kValLabelKey";
static const NSString* kProgressViewKey = @"kProgressViewKey";
static const NSString* kSecondaryProgressViewKey = @"kSecondaryProgressViewKey";

@interface DebugInfoUtil : NSObject

+ (UIColor*)UIColor_Orange;
+ (UIColor*)UIColor_Red;
+ (UIColor*)UIColor_Green;
+ (UIColor*)UIColor_Grey;
+ (UIColor*)UIColor_Disabled;

+ (UILabel*)makeTitleLabel;

+ (UILabel*)makeContentLabel;

+ (UIButton*)makeUIButtonAndAddSubView:(nonnull UIView*)parent
                                 title:(NSString*)title
                                   tag:(int)tag
                          bottomOffset:(int)offset
                              selector:(SEL)aSelector;

+ (UIView*)appendRowTitle:(nonnull UIView*)parent
                   anchor:(nullable UIView*)anchor
                     text:(nonnull NSString*)title;

+ (UILabel*)appendRowKeyValueContent:(UIView*)parent anchor:(UIView*)anchor key:(NSString*)key;

+ (NSDictionary*)appendRowKeyValuePrgressViewContent:(UIView*)parent
                                              anchor:(UIView*)anchor
                                                 key:(NSString*)key
                                       makeSecondary:(BOOL)secondary;

+ (UIProgressView*)appendProgressView:(UIView*)parent anchorTextView:(UIView*)anchor;

+ (UILabel*)appendRowSingleContent:(UIView*)parent anchor:(UIView*)anchor;

@end

NS_ASSUME_NONNULL_END
