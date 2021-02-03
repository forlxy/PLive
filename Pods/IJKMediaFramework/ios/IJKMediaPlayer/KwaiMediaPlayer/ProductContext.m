//
//  ProductContext.m
//  IJKMediaFramework
//
//  Copyright © 2019 kuaishou. All rights reserved.
//

#import "ProductContext.h"

@implementation ProductContext

- (instancetype)initWithName:(NSString *)productName
               withPlayIndex:(int)playIndex
                withExtraMsg:(NSString *)extraMsg {
    if (self = [super init]) {
        NSDictionary *dict = @{
            @"product" : productName == NULL ? @"N/A" : productName,
            @"play_index" : @(playIndex),
            @"extra_msg" : extraMsg == NULL ? @"{}" : extraMsg
        };
        NSData *jsonData = [NSJSONSerialization dataWithJSONObject:dict
                                                           options:NSJSONWritingPrettyPrinted
                                                             error:NULL];
        _stringJson = [[NSString alloc] initWithData:jsonData encoding:NSUTF8StringEncoding];
    }
    return self;
}

@end
