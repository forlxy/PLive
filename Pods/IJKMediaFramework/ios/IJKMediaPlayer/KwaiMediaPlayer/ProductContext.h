//
//  ProductContext.h
//  IJKMediaFramework
//
//  Copyright © 2019 kuaishou. All rights reserved.
//

#ifndef ProductContext_h
#define ProductContext_h

#import <Foundation/Foundation.h>

/**
 * @brief 客户端相关参数，用于上报数据分析
 */
@interface ProductContext : NSObject

/**
 * initWithName()
 * @param productName 播放场景名称（短视频/长视频/小课堂等）
 * @param playIndex 同一个视频第几次播放，用于统计重试次数，第一次为1
 * @param extraMsg 专用信息，json字符串
 */
- (instancetype)initWithName:(NSString*)productName
               withPlayIndex:(int)playIndex
                withExtraMsg:(NSString*)extraMsg;

@property(nonatomic, strong, readonly) NSString* stringJson;

@end

#endif /* ProductContext_h */
