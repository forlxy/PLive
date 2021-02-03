#import <CoreVideo/CoreVideo.h>
#include "KwaiVideoToolBoxColor.h"

CFTypeRef getColorSpace(enum AVColorSpace color_space) {
    switch (color_space) {
        case AVCOL_SPC_BT709:
            return kCVImageBufferYCbCrMatrix_ITU_R_709_2;
        case AVCOL_SPC_SMPTE170M:
            return kCVImageBufferYCbCrMatrix_ITU_R_601_4;
        default:
            return kCVImageBufferYCbCrMatrix_ITU_R_601_4;
    }
}

OSType getPixelBufferFormat(enum AVColorRange colorr_range) {
    if (colorr_range == AVCOL_RANGE_JPEG) {
        return kCVPixelFormatType_420YpCbCr8BiPlanarFullRange;
    } else {
        return kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange;
    }
}
