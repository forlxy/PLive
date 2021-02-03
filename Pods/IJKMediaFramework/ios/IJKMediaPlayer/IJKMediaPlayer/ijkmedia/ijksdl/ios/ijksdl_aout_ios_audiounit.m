/*
 * ijksdl_aout_ios_audiounit.m
 *
 * Copyright (c) 2013 Zhang Rui <bbcallen@gmail.com>
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

#include "ijksdl_aout_ios_audiounit.h"

#include <assert.h>
#include <stdbool.h>
#import "IJKSDLAudioQueueController.h"
#import "IJKSDLAudioUnitController.h"
#include "ijksdl/ijksdl_aout_internal.h"
#include "ijksdl/ijksdl_inc_internal.h"
#include "ijksdl/ijksdl_thread.h"

#define SDL_IOS_AUDIO_MAX_CALLBACKS_PER_SEC 15

// Kwai Live Event callback
typedef void (^KwaiLiveEventCallback)(char* content, int length);

struct SDL_Aout_Opaque {
    IJKSDLAudioQueueController* aoutController;
    KwaiLiveEventCallback liveEventCb;
};

static int aout_open_audio(SDL_Aout* aout, const SDL_AudioSpec* desired, SDL_AudioSpec* obtained) {
    assert(desired);
    SDLTRACE("aout_open_audio()\n");
    SDL_Aout_Opaque* opaque = aout->opaque;

    opaque->aoutController = [[IJKSDLAudioQueueController alloc] initWithAudioSpec:desired];
    if (!opaque->aoutController) {
        ALOGE("aout_open_audio_n: failed to new AudioTrcak()\n");
        return -1;
    }

    opaque->liveEventCb = aout->ios.live_event_cb;

    [opaque->aoutController setAudioDataBlock:aout->extra];

    if (obtained) *obtained = opaque->aoutController.spec;

    return 0;
}

static void aout_pause_audio(SDL_Aout* aout, int pause_on) {
    SDLTRACE("aout_pause_audio(%d)\n", pause_on);
    SDL_Aout_Opaque* opaque = aout->opaque;

    if (pause_on) {
        [opaque->aoutController pause];
    } else {
        [opaque->aoutController play];
    }
}

/** KingSoft's code begin **/
static void aout_mute_audio(SDL_Aout* aout, int mute) {
    SDL_Aout_Opaque* opaque = aout->opaque;
    opaque->aoutController.mute = (mute == 0 ? FALSE : TRUE);
}
/** KingSoft's code end **/

static void aout_flush_audio(SDL_Aout* aout) {
    SDLTRACE("aout_flush_audio()\n");
    SDL_Aout_Opaque* opaque = aout->opaque;

    [opaque->aoutController flush];
}

/*
 *is_loop_seek: 是否是循环播放。ios端是否循环播放的处理是一样的。
 */
static void aout_clear_audio(SDL_Aout* aout, bool is_loop_seek) {
    SDLTRACE("aout_clear_audio()\n");
    SDL_Aout_Opaque* opaque = aout->opaque;

    [opaque->aoutController clear];
}

static void aout_close_audio(SDL_Aout* aout) {
    SDLTRACE("aout_close_audio()\n");
    SDL_Aout_Opaque* opaque = aout->opaque;

    [opaque->aoutController close];
}

static void aout_set_playback_rate(SDL_Aout* aout, float playbackRate) {
    SDLTRACE("aout_close_audio()\n");
    SDL_Aout_Opaque* opaque = aout->opaque;

    [opaque->aoutController setPlaybackRate:playbackRate];
}

static void aout_set_volume(SDL_Aout* aout, float left_volume, float right_volume) {
    SDLTRACE("aout_set_volume()volume: %f\n", left_volume);
    SDL_Aout_Opaque* opaque = aout->opaque;

    [opaque->aoutController setPlaybackVolume:left_volume];
}

static void aout_set_playback_volume(SDL_Aout* aout, float volume) {
    SDLTRACE("aout_set_volume()\n");
    SDL_Aout_Opaque* opaque = aout->opaque;

    [opaque->aoutController setPlaybackVolume:volume];
}

static double auout_get_latency_seconds(SDL_Aout* aout) {
    SDL_Aout_Opaque* opaque = aout->opaque;
    return [opaque->aoutController get_latency_seconds];
}

static int aout_get_persecond_callbacks(SDL_Aout* aout) {
    return SDL_IOS_AUDIO_MAX_CALLBACKS_PER_SEC;
}

static void aout_live_event_callbacks(SDL_Aout* aout, char* content, int length) {
    SDL_Aout_Opaque* opaque = aout->opaque;
    if (opaque && opaque->liveEventCb) {
        opaque->liveEventCb(content, length);
    }
}

static void aout_free_l(SDL_Aout* aout) {
    if (!aout) return;

    aout_close_audio(aout);

    SDL_Aout_Opaque* opaque = aout->opaque;
    if (opaque) {
        if (opaque->aoutController) {
            [opaque->aoutController.audioDataBlock release];
            opaque->aoutController.audioDataBlock = nil;
        }
        [opaque->aoutController release];
        opaque->aoutController = nil;

        if (opaque->liveEventCb) {
            CFRelease(opaque->liveEventCb);
            opaque->liveEventCb = nil;
        }
    }

    SDL_Aout_FreeInternal(aout);
}

SDL_Aout* SDL_AoutIos_CreateForAudioUnit(void* extra) {
    SDL_Aout* aout = SDL_Aout_CreateInternal(sizeof(SDL_Aout_Opaque));
    if (!aout) return NULL;

    // SDL_Aout_Opaque *opaque = aout->opaque;

    aout->free_l = aout_free_l;
    aout->open_audio = aout_open_audio;
    aout->pause_audio = aout_pause_audio;
    aout->flush_audio = aout_flush_audio;
    /** KingSoft's code begin **/
    aout->mute_audio = aout_mute_audio;
    /** KingSoft's code end **/
    aout->close_audio = aout_close_audio;
    aout->set_volume = aout_set_volume;

    aout->func_set_playback_rate = aout_set_playback_rate;
    aout->func_set_playback_volume = aout_set_playback_volume;
    aout->func_get_latency_seconds = auout_get_latency_seconds;
    aout->func_get_audio_persecond_callbacks = aout_get_persecond_callbacks;

    aout->live_event_callback = aout_live_event_callbacks;

    // kwai
    aout->clear_audio = aout_clear_audio;
    aout->extra = extra;
    return aout;
}
