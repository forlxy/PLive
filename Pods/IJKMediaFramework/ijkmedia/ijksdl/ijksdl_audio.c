/*****************************************************************************
 * ijksdl_audio.c
 *****************************************************************************
 *
 * copyright (c) 2013 Zhang Rui <bbcallen@gmail.com>
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

#include "ijksdl_audio.h"


void SDL_CalculateAudioSpec(SDL_AudioSpec* spec) {
    switch (spec->format) {
        case AUDIO_U8:
            spec->silence = 0x80;
            break;
        default:
            spec->silence = 0x00;
            break;
    }
    spec->size = SDL_AUDIO_BITSIZE(spec->format) / 8;
    spec->size *= spec->channels;
    spec->size *= spec->samples;
}

void SDL_MixAudio(Uint8*       dst,
                  const Uint8* src,
                  Uint32       len,
                  float        volume[2]) {
    // do nothing;
    if (AUDIO_S16SYS  == AUDIO_S16LSB) {
        {
            Sint16 src_left1, src_left2, src_right1, src_right2;
            int dst_sample_left, dst_sample_right;
            const int max_amplitude = 32767;
            const int min_amplitude = -32768;

            len /= 4;
            while (len--) {
                src_left1 = ((src[1]) << 8 | src[0]);
                src_left1  *= volume[AUDIO_VOLUME_LEFT];

                src_right1 = ((src[3]) << 8 | src[2]);
                src_right1 *= volume[AUDIO_VOLUME_RIGHT];

                src_left2 = ((dst[1]) << 8 | dst[0]);
                src_right2 = ((dst[3]) << 8 | dst[2]);
                src += 4;

                dst_sample_left = src_left1 + src_left2;
                if (dst_sample_left > max_amplitude) {
                    dst_sample_left = max_amplitude;
                } else if (dst_sample_left < min_amplitude) {
                    dst_sample_left = min_amplitude;
                }
                dst[0] = dst_sample_left & 0xFF;
                dst_sample_left >>= 8;
                dst[1] = dst_sample_left & 0xFF;

                dst_sample_right = src_right1 + src_right2;
                if (dst_sample_right > max_amplitude) {
                    dst_sample_right = max_amplitude;
                } else if (dst_sample_right < min_amplitude) {
                    dst_sample_right = min_amplitude;
                }
                dst[2] = dst_sample_right & 0xFF;
                dst_sample_right >>= 8;
                dst[3] = dst_sample_right & 0xFF;
                dst += 4;
            }
        }

    } else {
        //TODO
    }
}
