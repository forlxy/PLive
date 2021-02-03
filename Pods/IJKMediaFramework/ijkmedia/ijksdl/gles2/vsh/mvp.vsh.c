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

#include "ijksdl/gles2/internal.h"

static const char g_shader[] = IJK_GLES_STRING(
                                   precision highp float;
                                   //varying   highp vec2 vv2_Texcoord;
                                   varying   highp vec2 vv2_y_Texcoord;
                                   varying   highp vec2 vv2_uv_Texcoord;

                                   attribute highp vec4 av4_Position;
                                   //attribute highp vec2 av2_Texcoord;
                                   attribute highp vec2 av2_y_Texcoord;
                                   attribute highp vec2 av2_uv_Texcoord;
                                   uniform         mat4 um4_ModelViewProjection;

void main() {
    gl_Position  = um4_ModelViewProjection * av4_Position;
    vv2_y_Texcoord  = av2_y_Texcoord.xy;
    vv2_uv_Texcoord = av2_uv_Texcoord.xy;
}
                               );

const char* IJK_GLES2_getVertexShader_default() {
    return g_shader;
}
