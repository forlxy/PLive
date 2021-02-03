/*
 * copyright (c) 2016 Zhang Rui <bbcallen@gmail.com>
 *
 * This file is part of ijkPlayer.
 *
 * ijkPlayer is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * ijkPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with ijkPlayer; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "internal.h"
#import <OpenGLES/EAGL.h>
#import <CoreVideo/CoreVideo.h>
#include "ijksdl_vout_overlay_videotoolbox.h"
#include "ijkkwai/ios/KwaiVideoToolBoxColor.h"

typedef struct IJK_GLES2_Renderer_Opaque {
    CVOpenGLESTextureCacheRef cv_texture_cache;
    CVOpenGLESTextureRef      cv_texture[2];

    CFTypeRef                 color_attachments;    // color space
    OSType                    pixelbuffer_format;
} IJK_GLES2_Renderer_Opaque;

static void apply_new_color_conversion_if_needed(IJK_GLES2_Renderer* renderer, CFTypeRef color_attachments, OSType pixelbuffer_format);
static GLboolean yuv420sp_vtb_use(IJK_GLES2_Renderer* renderer) {
    ALOGI("use render yuv420sp_vtb\n");
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    glUseProgram(renderer->program);            IJK_GLES2_checkError_TRACE("glUseProgram");

    for (int i = 0; i < 2; ++i) {
        glUniform1i(renderer->us2_sampler[i], i);
    }

    CFTypeRef color_space = getColorSpace(renderer->color_space);
    OSType pixelbuffer_format = getPixelBufferFormat(renderer->color_range);
    apply_new_color_conversion_if_needed(renderer, color_space, pixelbuffer_format);
    return GL_TRUE;
}

static GLvoid yuv420sp_vtb_clean_textures(IJK_GLES2_Renderer* renderer) {
    if (!renderer || !renderer->opaque)
        return;

    IJK_GLES2_Renderer_Opaque* opaque = renderer->opaque;

    for (int i = 0; i < 2; ++i) {
        if (opaque->cv_texture[i]) {
            CFRelease(opaque->cv_texture[i]);
            opaque->cv_texture[i] = nil;
        }
    }

    // Periodic texture cache flush every frame
    if (opaque->cv_texture_cache)
        CVOpenGLESTextureCacheFlush(opaque->cv_texture_cache, 0);
}

static GLsizei yuv420sp_vtb_getBufferWidth(IJK_GLES2_Renderer* renderer, SDL_VoutOverlay* overlay) {
    if (!overlay)
        return 0;

    return overlay->pitches[0] / 1;
}

static BOOL is_eqaul_CFString(CFTypeRef s1, CFTypeRef s2) {
    if (s1 == s2) {
        return YES;
    }

    if (s1 != nil && s2 != nil && CFStringCompare(s1, s2, 0) == kCFCompareEqualTo) {
        return YES;
    }

    return NO;
}
static void apply_new_color_conversion_if_needed(IJK_GLES2_Renderer* renderer, CFTypeRef color_attachments, OSType pixelbuffer_format) {
    if (!renderer || !renderer->opaque) {
        return;
    }

    IJK_GLES2_Renderer_Opaque* opaque = renderer->opaque;

    if (is_eqaul_CFString(color_attachments, opaque->color_attachments) && opaque->pixelbuffer_format == pixelbuffer_format) {
        return;
    }

    // default policy: bt601 and video_range
    if (color_attachments == nil) {
        // default is 601
        if (pixelbuffer_format == kCVPixelFormatType_420YpCbCr8BiPlanarFullRange) {
            glUniform1f(renderer->y_offset, 0.0f);
            glUniformMatrix3fv(renderer->um3_color_conversion, 1, GL_FALSE, IJK_GLES2_getColorMatrix_bt601_full_range());
        } else {
            glUniform1f(renderer->y_offset, 16.0f);
            glUniformMatrix3fv(renderer->um3_color_conversion, 1, GL_FALSE, IJK_GLES2_getColorMatrix_bt601());
        }
    } else {
        glUniform1f(renderer->y_offset, 16.0f);
        if (is_eqaul_CFString(color_attachments, kCVImageBufferYCbCrMatrix_ITU_R_709_2)) {
            glUniformMatrix3fv(renderer->um3_color_conversion, 1, GL_FALSE, IJK_GLES2_getColorMatrix_bt709());
        } else {
            if (pixelbuffer_format == kCVPixelFormatType_420YpCbCr8BiPlanarFullRange) {
                glUniform1f(renderer->y_offset, 0.0f);
                glUniformMatrix3fv(renderer->um3_color_conversion, 1, GL_FALSE, IJK_GLES2_getColorMatrix_bt601_full_range());
            } else {
                glUniform1f(renderer->y_offset, 16.0f);
                glUniformMatrix3fv(renderer->um3_color_conversion, 1, GL_FALSE, IJK_GLES2_getColorMatrix_bt601());
            }
        }
    }

    if (opaque->color_attachments != nil) {
        CFRelease(opaque->color_attachments);
        opaque->color_attachments = nil;
    }
    if (color_attachments != nil) {
        opaque->color_attachments = CFRetain(color_attachments);
    }

    opaque->pixelbuffer_format = pixelbuffer_format;

    NSLog(@"apply_new_color_conversion pixelbuffer_format:%d, cv_full_range is:%d, cv_video_range is:%d,  color_attachments:%@",
          opaque->pixelbuffer_format,
          kCVPixelFormatType_420YpCbCr8BiPlanarFullRange,
          kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange,
          opaque->color_attachments);
}
static GLboolean yuv420sp_vtb_uploadTexture(IJK_GLES2_Renderer* renderer, SDL_VoutOverlay* overlay) {
    if (!renderer || !renderer->opaque || !overlay)
        return GL_FALSE;

    if (!overlay->is_private)
        return GL_FALSE;

    switch (overlay->format) {
        case SDL_FCC__VTB:
            break;
        default:
            ALOGE("[yuv420sp_vtb] unexpected format %x\n", overlay->format);
            return GL_FALSE;
    }

    IJK_GLES2_Renderer_Opaque* opaque = renderer->opaque;
    if (!opaque->cv_texture_cache) {
        ALOGE("nil textureCache\n");
        return GL_FALSE;
    }

    CVPixelBufferRef pixel_buffer = SDL_VoutOverlayVideoToolBox_GetCVPixelBufferRef(overlay);
    if (!pixel_buffer) {
        ALOGE("nil pixelBuffer in overlay\n");
        return GL_FALSE;
    }

    CFTypeRef color_attachments = CVBufferGetAttachment(pixel_buffer, kCVImageBufferYCbCrMatrixKey, NULL);
    OSType pixelbuffer_format = CVPixelBufferGetPixelFormatType(pixel_buffer);
    apply_new_color_conversion_if_needed(renderer, color_attachments, pixelbuffer_format);

    yuv420sp_vtb_clean_textures(renderer);

    GLsizei frame_width  = (GLsizei)CVPixelBufferGetWidth(pixel_buffer);
    GLsizei frame_height = (GLsizei)CVPixelBufferGetHeight(pixel_buffer);

    glActiveTexture(GL_TEXTURE0);
    CVOpenGLESTextureCacheCreateTextureFromImage(kCFAllocatorDefault,
                                                 opaque->cv_texture_cache,
                                                 pixel_buffer,
                                                 NULL,
                                                 GL_TEXTURE_2D,
                                                 GL_RED_EXT,
                                                 (GLsizei)frame_width,
                                                 (GLsizei)frame_height,
                                                 GL_RED_EXT,
                                                 GL_UNSIGNED_BYTE,
                                                 0,
                                                 &opaque->cv_texture[0]);
    glBindTexture(CVOpenGLESTextureGetTarget(opaque->cv_texture[0]), CVOpenGLESTextureGetName(opaque->cv_texture[0]));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);


    glActiveTexture(GL_TEXTURE1);
    CVOpenGLESTextureCacheCreateTextureFromImage(kCFAllocatorDefault,
                                                 opaque->cv_texture_cache,
                                                 pixel_buffer,
                                                 NULL,
                                                 GL_TEXTURE_2D,
                                                 GL_RG_EXT,
                                                 (GLsizei)frame_width / 2,
                                                 (GLsizei)frame_height / 2,
                                                 GL_RG_EXT,
                                                 GL_UNSIGNED_BYTE,
                                                 1,
                                                 &opaque->cv_texture[1]);
    glBindTexture(CVOpenGLESTextureGetTarget(opaque->cv_texture[1]), CVOpenGLESTextureGetName(opaque->cv_texture[1]));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    return GL_TRUE;
}

static GLvoid yuv420sp_vtb_destroy(IJK_GLES2_Renderer* renderer) {
    if (!renderer || !renderer->opaque)
        return;

    yuv420sp_vtb_clean_textures(renderer);

    IJK_GLES2_Renderer_Opaque* opaque = renderer->opaque;

    if (opaque->cv_texture_cache) {
        CFRelease(opaque->cv_texture_cache);
        opaque->cv_texture_cache = nil;
    }

    if (opaque->color_attachments != nil) {
        CFRelease(opaque->color_attachments);
        opaque->color_attachments = nil;
    }
    free(renderer->opaque);
    renderer->opaque = nil;

}

IJK_GLES2_Renderer* IJK_GLES2_Renderer_create_yuv420sp_vtb(SDL_VoutOverlay* overlay) {
    CVReturn err = 0;
    EAGLContext* context = [EAGLContext currentContext];

    if (!overlay) {
        ALOGW("invalid overlay, fall back to yuv420sp renderer\n");
        return IJK_GLES2_Renderer_create_yuv420sp();
    }

    if (!overlay) {
        ALOGW("non-private overlay, fall back to yuv420sp renderer\n");
        return IJK_GLES2_Renderer_create_yuv420sp();
    }

    if (!context) {
        ALOGW("nil EAGLContext, fall back to yuv420sp renderer\n");
        return IJK_GLES2_Renderer_create_yuv420sp();
    }

    ALOGI("create render yuv420sp_vtb\n");
    IJK_GLES2_Renderer* renderer = IJK_GLES2_Renderer_create_base(IJK_GLES2_getFragmentShader_yuv420sp());
    if (!renderer)
        goto fail;

    renderer->us2_sampler[0] = glGetUniformLocation(renderer->program, "us2_SamplerX"); IJK_GLES2_checkError_TRACE("glGetUniformLocation(us2_SamplerX)");
    renderer->us2_sampler[1] = glGetUniformLocation(renderer->program, "us2_SamplerY"); IJK_GLES2_checkError_TRACE("glGetUniformLocation(us2_SamplerY)");

    renderer->um3_color_conversion = glGetUniformLocation(renderer->program, "um3_ColorConversion"); IJK_GLES2_checkError_TRACE("glGetUniformLocation(um3_ColorConversionMatrix)");
    renderer->y_offset             = glGetUniformLocation(renderer->program, "y_Offset"); IJK_GLES2_checkError_TRACE("glGetUniformLocation(y_Offset)");

    renderer->func_use            = yuv420sp_vtb_use;
    renderer->func_getBufferWidth = yuv420sp_vtb_getBufferWidth;
    renderer->func_uploadTexture  = yuv420sp_vtb_uploadTexture;
    renderer->func_destroy        = yuv420sp_vtb_destroy;

    renderer->opaque = calloc(1, sizeof(IJK_GLES2_Renderer_Opaque));
    if (!renderer->opaque)
        goto fail;

    err = CVOpenGLESTextureCacheCreate(kCFAllocatorDefault, NULL, context, NULL, &renderer->opaque->cv_texture_cache);
    if (err || renderer->opaque->cv_texture_cache == nil) {
        ALOGE("Error at CVOpenGLESTextureCacheCreate %d\n", err);
        goto fail;
    }

    return renderer;
fail:
    IJK_GLES2_Renderer_free(renderer);
    return NULL;
}
