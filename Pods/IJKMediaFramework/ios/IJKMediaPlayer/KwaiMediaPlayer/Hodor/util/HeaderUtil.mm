//
//  HeaderUtil.m
//  IJKMediaFramework
//
//  Created by MarshallShuai on 2019/10/28.
//  Copyright © 2019 kuaishou. All rights reserved.
//

#import "HeaderUtil.h"

@implementation HeaderUtil

+ (NSString *)parseHeaderMapToFlatString:(NSDictionary *)headers
{
    NSString* headerString = [[NSString alloc] init];
    if (headers != nil && [headers count] > 0) {
        for (NSString* key in headers) {
            NSString* val = [headers objectForKey:key];
            // 经测headers的格式必须是 "key: value", 不能是
            // key:value，即value前有一个空格
            headerString = [headerString
                            stringByAppendingString:[NSString stringWithFormat:@"%@: %@\r\n", key, val]];
        }
    }
    return headerString;
}

@end
