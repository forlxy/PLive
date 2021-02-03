//
//  NSString+SizeToFit.m
//

#import "NSString+SizeToFit.h"

@implementation NSString (SizeToFit)
- (CGSize)sizeWithfont:(UIFont*)font maxSize:(CGSize)maxSize {
    NSDictionary* attrs = @{NSFontAttributeName : font};
    return [self boundingRectWithSize:maxSize
                              options:NSStringDrawingUsesLineFragmentOrigin
                           attributes:attrs
                              context:nil]
        .size;
}

- (CGSize)sizeWithAttrs:(NSDictionary*)attrs maxSize:(CGSize)maxSize {
    return [self boundingRectWithSize:maxSize
                              options:NSStringDrawingUsesLineFragmentOrigin
                           attributes:attrs
                              context:nil]
        .size;
}

@end
