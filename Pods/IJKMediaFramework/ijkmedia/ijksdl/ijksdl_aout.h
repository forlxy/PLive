/*****************************************************************************
 * ijksdl_aout.h
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

#ifndef IJKSDL__IJKSDL_AOUT_H
#define IJKSDL__IJKSDL_AOUT_H

#include "ijksdl_audio.h"
#include "ijksdl_class.h"
#include "ijksdl_mutex.h"

typedef struct SDL_Aout_Opaque SDL_Aout_Opaque;
typedef struct SDL_Aout SDL_Aout;
struct SDL_Aout {
    struct {
        int flush_cnt_1;
        int flush_cnt_2;
        int flush_cnt_3;
        int flush_cnt_4;

        int play_cnt_1;
        int play_cnt_2;

        int pause_cnt;

        int set_speed_cnt;

        int set_volume_cnt_1;
        int set_volume_cnt_2;

        int silent_buf_cnt;
        int silent_buf_total_bytes;

        int audio_track_write_error_count;
    } qos;

    SDL_mutex* mutex;
    double     minimal_latency_seconds;

    SDL_Class*       opaque_class;
    SDL_Aout_Opaque* opaque;

    void (*free_l)(SDL_Aout* vout);
    int (*open_audio)(SDL_Aout* aout, const SDL_AudioSpec* desired, SDL_AudioSpec* obtained);
    void (*pause_audio)(SDL_Aout* aout, int pause_on);
    void (*flush_audio)(SDL_Aout* aout);
    void (*set_volume)(SDL_Aout* aout, float left, float right);
    void (*close_audio)(SDL_Aout* aout);
    void (*mute_audio)(SDL_Aout* aout, int mute);
    double (*func_get_latency_seconds)(SDL_Aout* aout);
    void (*func_set_default_latency_seconds)(SDL_Aout* aout, double latency);

    // Kwai Logic both for Andrid/iOS
    void (*clear_audio)(SDL_Aout* aout, bool is_loop_seek);

    // optional
    void (*func_set_playback_rate)(SDL_Aout* aout, float playbackRate);
    void (*func_set_playback_volume)(SDL_Aout* aout, float playbackVolume);
    int (*func_get_audio_persecond_callbacks)(SDL_Aout* aout);

    void (*live_event_callback)(SDL_Aout* aout, char* content, int length);

    struct {
        // common logic
        int (*func_get_audio_session_id)(SDL_Aout* aout); // todo gles需要补充实现

        struct {
            void (*force_stop)(SDL_Aout* aout);
            int (*get_buf_size)(SDL_Aout* aout);
            void (*process_pcm)(SDL_Aout* aout, uint8_t* pcm, int* size, int samplerate, int channels, int fmt);
            void* userdata;
        } audiotrack;

        struct {
        } opengles;
    } android;

    struct {
        // live event(in aac) callback
        void* live_event_cb;
    } ios;

    // Android only Kwai logigc

    // iOS only Kwai Logic，后续重构放到 上面的 ios struct里
    void* extra;
};


int SDL_AoutOpenAudio(SDL_Aout* aout, const SDL_AudioSpec* desired, SDL_AudioSpec* obtained);
void SDL_AoutPauseAudio(SDL_Aout* aout, int pause_on);
void SDL_AoutFlushAudio(SDL_Aout* aout);
void SDL_AoutClearAudio(SDL_Aout* aout, bool is_loop_seek);
void SDL_AoutSetStereoVolume(SDL_Aout* aout, float left_volume, float right_volume);
void SDL_AoutCloseAudio(SDL_Aout* aout);
void SDL_AoutMuteAudio(SDL_Aout* aout, int mute);
void SDL_AoutFree(SDL_Aout* aout);
void SDL_AoutFreeP(SDL_Aout** paout);

double SDL_AoutGetLatencySeconds(SDL_Aout* aout);
void   SDL_AoutSetDefaultLatencySeconds(SDL_Aout* aout, double latency);

void SDL_Aout_Qos_onSilentBuffer(SDL_Aout* aout, int buffer_bytes);

// optional
void   SDL_AoutSetPlaybackRate(SDL_Aout* aout, float playbackRate);

// android only
int    SDL_AoutGetAudioSessionId(SDL_Aout* aout);
void   SDL_AoutForceStop(SDL_Aout* aout); // 为了防止Android的AudioTrack write anr，这里提供一个强制pause的，iOS不一定需要，写在SDL不一定是最好的，我懂
int    SDL_GetBufSize(SDL_Aout* aout);
void   SDL_AoutProcessPCM(SDL_Aout* aout, uint8_t* pcm, int* size, int samplerate, int channels, int fmt);

// live event in aac processor
void   SDL_AoutProcessAacLiveEvent(SDL_Aout* aout, char* content, int length);


#endif
