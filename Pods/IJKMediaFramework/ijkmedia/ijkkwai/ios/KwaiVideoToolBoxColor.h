#ifndef KwaiVideoToolBoxColor_h
#define KwaiVideoToolBoxColor_h

#import <Foundation/Foundation.h>
#include <libavutil/pixfmt.h>

CFTypeRef getColorSpace(enum AVColorSpace color_space);

OSType getPixelBufferFormat(enum AVColorRange colorr_range);


#endif /* KwaiVideoToolBoxColor_h */
