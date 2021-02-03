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

static GLboolean yuv420sp_vu_use(IJK_GLES2_Renderer* renderer) {
    ALOGI("use render yuv420sp_vu\n");
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    glUseProgram(renderer->program);            IJK_GLES2_checkError_TRACE("glUseProgram");

    if (0 == renderer->plane_textures[0])
        glGenTextures(2, renderer->plane_textures);

    for (int i = 0; i < 2; ++i) {
        glActiveTexture(GL_TEXTURE0 + i);
        glBindTexture(GL_TEXTURE_2D, renderer->plane_textures[i]);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glUniform1i(renderer->us2_sampler[i], i);
    }

    glUniformMatrix3fv(renderer->um3_color_conversion, 1, GL_FALSE, IJK_GLES2_getColorMatrix(renderer->color_space, renderer->color_range));
    if (renderer->color_space == AVCOL_SPC_SMPTE170M && renderer->color_range == AVCOL_RANGE_JPEG) {
        glUniform1f(renderer->y_offset, 0);
    } else {
        glUniform1f(renderer->y_offset, 16.0f);
    }

    return GL_TRUE;
}

static GLsizei yuv420sp_vu_getBufferWidth(IJK_GLES2_Renderer* renderer, SDL_VoutOverlay* overlay) {
    if (!overlay)
        return 0;

    return overlay->pitches[0] / 1;
}

static GLboolean yuv420sp_vu_uploadTexture(IJK_GLES2_Renderer* renderer, SDL_VoutOverlay* overlay) {
    if (!renderer || !overlay)
        return GL_FALSE;

    const GLsizei widths[2]    = { overlay->pitches[0], overlay->pitches[1] / 2 };
    const GLsizei heights[2]   = { overlay->h,          overlay->h / 2 };
    const GLubyte* pixels[2]   = { overlay->pixels[0],  overlay->pixels[1] };

    switch (overlay->format) {
        case SDL_FCC_NV21:
            break;
        default:
            ALOGE("[yuv420sp_vu] unexpected format %x\n", overlay->format);
            return GL_FALSE;
    }

    glBindTexture(GL_TEXTURE_2D, renderer->plane_textures[0]);
    glTexImage2D(GL_TEXTURE_2D,
                 0,
                 GL_LUMINANCE,
                 widths[0],
                 heights[0],
                 0,
                 GL_LUMINANCE,
                 GL_UNSIGNED_BYTE,
                 pixels[0]);

    glBindTexture(GL_TEXTURE_2D, renderer->plane_textures[1]);
    glTexImage2D(GL_TEXTURE_2D,
                 0,
                 GL_LUMINANCE_ALPHA,
                 widths[1],
                 heights[1],
                 0,
                 GL_LUMINANCE_ALPHA,
                 GL_UNSIGNED_BYTE,
                 pixels[1]);

    return GL_TRUE;
}

IJK_GLES2_Renderer* IJK_GLES2_Renderer_create_yuv420sp_vu() {
    IJK_GLES2_Renderer* renderer = IJK_GLES2_Renderer_create_base(IJK_GLES2_getFragmentShader_yuv420sp_vu());
    if (!renderer)
        goto fail;

    renderer->us2_sampler[0] = glGetUniformLocation(renderer->program, "us2_SamplerX"); IJK_GLES2_checkError_TRACE("glGetUniformLocation(us2_SamplerX)");
    renderer->us2_sampler[1] = glGetUniformLocation(renderer->program, "us2_SamplerY"); IJK_GLES2_checkError_TRACE("glGetUniformLocation(us2_SamplerY)");

    renderer->um3_color_conversion = glGetUniformLocation(renderer->program, "um3_ColorConversion"); IJK_GLES2_checkError_TRACE("glGetUniformLocation(um3_ColorConversionMatrix)");
    renderer->y_offset = glGetUniformLocation(renderer->program, "y_Offset"); IJK_GLES2_checkError_TRACE("glGetUniformLocation(y_Offset)");

    renderer->func_use            = yuv420sp_vu_use;
    renderer->func_getBufferWidth = yuv420sp_vu_getBufferWidth;
    renderer->func_uploadTexture  = yuv420sp_vu_uploadTexture;

    return renderer;
fail:
    IJK_GLES2_Renderer_free(renderer);
    return NULL;
}
