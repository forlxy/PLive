//
//  HeaderUtil.h
//  IJKMediaFramework
//
//  Created by MarshallShuai on 2019/10/28.
//  Copyright © 2019 kuaishou. All rights reserved.
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface HeaderUtil : NSObject

+ (NSString *)parseHeaderMapToFlatString:(NSDictionary *)headers;

@end

NS_ASSUME_NONNULL_END
