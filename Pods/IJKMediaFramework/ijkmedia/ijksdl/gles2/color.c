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

// BT.709, which is the standard for HDTV.
static const GLfloat g_bt709[] = {
    1.164,  1.164,  1.164,
    0.0,   -0.213,  2.112,
    1.793, -0.533,  0.0,
};
const GLfloat* IJK_GLES2_getColorMatrix_bt709() {
    return g_bt709;
}

static const GLfloat g_bt601[] = {
    1.164,  1.164, 1.164,
    0.0,   -0.392, 2.017,
    1.596, -0.813, 0.0,
};

const GLfloat* IJK_GLES2_getColorMatrix_bt601() {
    return g_bt601;
}

static const GLfloat g_bt601_full_range[] = {
    1.0, 1.0, 1.0,
    0.0, -0.343, 1.765,
    1.4, -0.711, 0.0,
};

const GLfloat* IJK_GLES2_getColorMatrix_bt601_full_range() {
    return g_bt601_full_range;
}

const GLfloat* IJK_GLES2_getColorMatrix(enum AVColorSpace color_space, enum AVColorRange color_range) {
    if (color_space == AVCOL_SPC_BT709) {
        return g_bt709;
    } else {
        // assume is 601
        if (color_range == AVCOL_RANGE_JPEG) {
            // full range
            return g_bt601_full_range;
        } else {
            // video range
            return g_bt601;
        }
    }
}
