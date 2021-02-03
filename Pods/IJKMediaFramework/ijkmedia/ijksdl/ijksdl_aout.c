/*****************************************************************************
 * ijksdl_aout.c
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

#include "ijksdl_aout.h"
#include "ijksdl_log.h"
#include "ijksdl.h"
#include <stdlib.h>

int SDL_AoutOpenAudio(SDL_Aout* aout, const SDL_AudioSpec* desired, SDL_AudioSpec* obtained) {
    if (aout && desired && aout->open_audio)
        return aout->open_audio(aout, desired, obtained);

    return -1;
}

void SDL_AoutPauseAudio(SDL_Aout* aout, int pause_on) {
    if (aout && aout->pause_audio)
        aout->pause_audio(aout, pause_on);
}

void SDL_AoutMuteAudio(SDL_Aout* aout, int mute) {
    if (aout && aout->mute_audio)
        aout->mute_audio(aout, mute);
}

void SDL_AoutFlushAudio(SDL_Aout* aout) {
    if (aout && aout->flush_audio)
        aout->flush_audio(aout);
}

void SDL_AoutClearAudio(SDL_Aout* aout, bool is_loop_seek) {
    if (aout && aout->clear_audio)
        aout->clear_audio(aout, is_loop_seek);
}

void SDL_AoutSetStereoVolume(SDL_Aout* aout, float left_volume, float right_volume) {
    if (aout && aout->set_volume)
        aout->set_volume(aout, left_volume, right_volume);
}

void SDL_AoutCloseAudio(SDL_Aout* aout) {
    if (aout && aout->close_audio)
        return aout->close_audio(aout);
}

void SDL_AoutFree(SDL_Aout* aout) {
    if (!aout)
        return;

    if (aout->free_l)
        aout->free_l(aout);
    else
        free(aout);
}

void SDL_AoutFreeP(SDL_Aout** paout) {
    if (!paout)
        return;

    SDL_AoutFree(*paout);
    *paout = NULL;
}

double SDL_AoutGetLatencySeconds(SDL_Aout* aout) {
    if (!aout)
        return 0;

    if (aout->func_get_latency_seconds)
        return aout->func_get_latency_seconds(aout);

    return aout->minimal_latency_seconds;
}

void SDL_AoutSetDefaultLatencySeconds(SDL_Aout* aout, double latency) {
    if (aout) {
        if (aout->func_set_default_latency_seconds)
            aout->func_set_default_latency_seconds(aout, latency);
        ALOGD("[%s], latency:%fms", __func__, latency * 1000);
        aout->minimal_latency_seconds = latency;
    }
}

void SDL_AoutSetPlaybackRate(SDL_Aout* aout, float playbackRate) {
    if (aout) {
        if (aout->func_set_playback_rate)
            aout->func_set_playback_rate(aout, playbackRate);
    }
}

int SDL_AoutGetAudioSessionId(SDL_Aout* aout) {
    if (aout) {
        if (aout->android.func_get_audio_session_id) {
            return aout->android.func_get_audio_session_id(aout);
        }
    }
    return 0;
}

void SDL_AoutForceStop(SDL_Aout* aout) {
    if (aout && aout->android.audiotrack.force_stop)
        aout->android.audiotrack.force_stop(aout);
}


void SDL_Aout_Qos_onSilentBuffer(SDL_Aout* aout, int buffer_bytes) {
    if (aout) {
        aout->qos.silent_buf_cnt++;
        aout->qos.silent_buf_total_bytes += buffer_bytes;
    }
}

void SDL_AoutProcessPCM(SDL_Aout* aout, uint8_t* pcm, int* size, int samplerate, int channels, int fmt) {
    if (aout && aout->android.audiotrack.process_pcm) {
        aout->android.audiotrack.process_pcm(aout, pcm, size, samplerate, channels, fmt);
    }
}

void SDL_AoutProcessAacLiveEvent(SDL_Aout* aout, char* content, int length) {
    if (aout && aout->live_event_callback) {
        aout->live_event_callback(aout, content, length);
    }
}

int SDL_GetBufSize(SDL_Aout* aout) {
    if (aout) {
        return aout->android.audiotrack.get_buf_size(aout);
    }
    return -1;
}

