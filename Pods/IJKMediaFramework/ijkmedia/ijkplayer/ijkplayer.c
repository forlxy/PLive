/*
 * ijkplayer.c
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

#include "ijkplayer.h"
#include "ijkplayer_internal.h"
#include "version.h"
#include "ijkkwai/kwai_qos.h"
#include "ijkkwai/kwaiplayer_lifecycle.h"

#define MP_SESSION (mp->ffplayer != NULL ? mp->ffplayer->session_id : -1)

#define MP_RET_IF_FAILED(ret) \
    do { \
        int retval = ret; \
        if (retval != 0) return (retval); \
    } while(0)

#define MPST_RET_IF_EQ_INT(real, expected, errcode) \
    do { \
        if ((real) == (expected)) return (errcode); \
    } while(0)

#define MPST_RET_IF_EQ(real, expected) \
    MPST_RET_IF_EQ_INT(real, expected, EIJK_INVALID_STATE)

#define MPST_RET_IF_NOT_IN_PROGRESS(real) \
    MPST_RET_IF_EQ(real, MP_STATE_IDLE);                \
    MPST_RET_IF_EQ(real, MP_STATE_INITIALIZED);         \
    MPST_RET_IF_EQ(real, MP_STATE_ASYNC_PREPARING);     \
    MPST_RET_IF_EQ(real, MP_STATE_COMPLETED);           \
    MPST_RET_IF_EQ(real, MP_STATE_STOPPED);             \
    MPST_RET_IF_EQ(real, MP_STATE_ERROR);               \
    MPST_RET_IF_EQ(real, MP_STATE_END);                 \

inline static void ijkmp_destroy(IjkMediaPlayer* mp) {
    if (!mp)
        return;

    unsigned session_id = MP_SESSION;
    ALOGI("[%d] %s start \n", session_id, __func__);

    // MPTRACE("ijkmp_destroy \n");
    pthread_mutex_lock(&mp->mutex);
    ffp_destroy_p(&mp->ffplayer);
    if (mp->msg_thread) {
        SDL_WaitThread(mp->msg_thread, NULL);
        mp->msg_thread = NULL;
    }
    pthread_mutex_unlock(&mp->mutex);
    pthread_mutex_destroy(&mp->mutex);

    freep((void**)&mp->data_source);
    VodQosDebugInfo_release(&mp->vod_qos_debug_info);
    PlayerConfigDebugInfo_release(&mp->player_config_debug_info);

    KwaiPlayerResultQos_releasep(&mp->player_result_qos);
    mp->player_result_qos = NULL;

    memset(mp, 0, sizeof(IjkMediaPlayer));
    freep((void**)&mp);
    KwaiPlayerLifeCycle_on_player_destroyed();

    ALOGI("[%d] %s finish \n", session_id, __func__);
}

inline static void ijkmp_destroy_p(IjkMediaPlayer** pmp) {
    if (!pmp)
        return;

    ijkmp_destroy(*pmp);
    *pmp = NULL;
}

void ijkmp_global_init() {
    global_session_id = 0;
    ffp_global_init();
    KwaiPlayerLifeCycle_module_init();
}

void ijkmp_global_uninit() {
    ffp_global_uninit();
}

void ijkmp_global_set_log_report(int use_report) {
    ffp_global_set_log_report(use_report);
}

void ijkmp_global_set_log_level(int log_level) {
    ffp_global_set_log_level(log_level);
}

void ijkmp_global_set_inject_callback(ijk_inject_callback cb) {
    ffp_global_set_inject_callback(cb);
}

void ijkmp_global_set_kwailog_level(int log_level) {
    kwai_set_log_level(log_level);
}

const char* ijkmp_version_ident() {
    return LIBIJKPLAYER_IDENT;
}

unsigned int ijkmp_version_int() {
    return LIBIJKPLAYER_VERSION_INT;
}

void ijkmp_io_stat_register(void (*cb)(const char* url, int type, int bytes)) {
    ffp_io_stat_register(cb);
}

void ijkmp_io_stat_complete_register(void (*cb)(const char* url,
                                                int64_t read_bytes, int64_t total_size,
                                                int64_t elpased_time, int64_t total_duration)) {
    ffp_io_stat_complete_register(cb);
}

static inline const char* ijkmp_state_to_str(int state) {
    switch (state) {
        case MP_STATE_IDLE:
            return "MP_STATE_IDLE";
        case MP_STATE_INITIALIZED:
            return "MP_STATE_INITIALIZED";
        case MP_STATE_ASYNC_PREPARING:
            return "MP_STATE_ASYNC_PREPARING";
        case MP_STATE_PREPARED:
            return "MP_STATE_PREPARED";
        case MP_STATE_STARTED:
            return "MP_STATE_STARTED";
        case MP_STATE_PAUSED:
            return "MP_STATE_PAUSED";
        case MP_STATE_COMPLETED:
            return "MP_STATE_COMPLETED";
        case MP_STATE_STOPPED:
            return "MP_STATE_STOPPED";
        case MP_STATE_ERROR:
            return "MP_STATE_ERROR";
        case MP_STATE_END:
            return "MP_STATE_END";
        default:
            return "unknown";
    }
}

void ijkmp_change_state_l(IjkMediaPlayer* mp, int new_state) {
    ALOGI("[%d] %s from %s to %s\n", MP_SESSION, __FUNCTION__, ijkmp_state_to_str(mp->mp_state), ijkmp_state_to_str(new_state));
    mp->mp_state = new_state;
    ffp_notify_msg2(mp->ffplayer, FFP_MSG_PLAYBACK_STATE_CHANGED, new_state);

    // for qos
    if (new_state == MP_STATE_STARTED) {
        KwaiQos_onAppStartPlayer(mp->ffplayer);
    } else if (new_state == MP_STATE_PAUSED || new_state == MP_STATE_STOPPED ||
               new_state == MP_STATE_ERROR || new_state == MP_STATE_END) {
        KwaiQos_onAppPausePlayer(mp->ffplayer);
    }
}

IjkMediaPlayer* ijkmp_create(int (*msg_loop)(void*)) {
    IjkMediaPlayer* mp = (IjkMediaPlayer*) mallocz(sizeof(IjkMediaPlayer));
    if (!mp)
        goto fail;

    mp->ffplayer = ffp_create();
    if (!mp->ffplayer)
        goto fail;

    mp->msg_loop = msg_loop;

    ijkmp_inc_ref(mp);
    pthread_mutex_init(&mp->mutex, NULL);

    mp->player_result_qos = KwaiPlayerResultQos_create();
    VodQosDebugInfo_init(&mp->vod_qos_debug_info);
    PlayerConfigDebugInfo_init(&mp->player_config_debug_info);

    KwaiPlayerLifeCycle_on_player_created();
    return mp;

fail:
    ijkmp_destroy_p(&mp);
    return NULL;
}

void ijkmp_set_inject_opaque(IjkMediaPlayer* mp, void* opaque) {
    assert(mp);

    // MPTRACE("%s(%p)\n", __func__, opaque);
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"
#endif
    ijkmp_set_option_int(mp, IJKMP_OPT_CATEGORY_FORMAT, "ijkinject-opaque", (int64_t)opaque);
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
    // MPTRACE("%s()=void\n", __func__);
}

void ijkmp_set_option(IjkMediaPlayer* mp, int opt_category, const char* name, const char* value) {
    assert(mp);

    // MPTRACE("%s(%s, %s)\n", __func__, name, value);
    pthread_mutex_lock(&mp->mutex);
    ffp_set_option(mp->ffplayer, opt_category, name, value);
    pthread_mutex_unlock(&mp->mutex);
    // MPTRACE("%s()=void\n", __func__);
}

void ijkmp_set_option_int(IjkMediaPlayer* mp, int opt_category, const char* name, int64_t value) {
    assert(mp);

    // MPTRACE("%s(%s, %"PRId64")\n", __func__, name, value);
    pthread_mutex_lock(&mp->mutex);
    ffp_set_option_int(mp->ffplayer, opt_category, name, value);
    pthread_mutex_unlock(&mp->mutex);
    // MPTRACE("%s()=void\n", __func__);
}

int ijkmp_get_video_codec_info(IjkMediaPlayer* mp, char** codec_info) {
    assert(mp);

    // MPTRACE("%s\n", __func__);
    pthread_mutex_lock(&mp->mutex);
    int ret = ffp_get_video_codec_info(mp->ffplayer, codec_info);
    pthread_mutex_unlock(&mp->mutex);
    // MPTRACE("%s()=void\n", __func__);
    return ret;
}

int ijkmp_get_audio_codec_info(IjkMediaPlayer* mp, char** codec_info) {
    assert(mp);

    // MPTRACE("%s\n", __func__);
    pthread_mutex_lock(&mp->mutex);
    int ret = ffp_get_audio_codec_info(mp->ffplayer, codec_info);
    pthread_mutex_unlock(&mp->mutex);
    // MPTRACE("%s()=void\n", __func__);
    return ret;
}

/*
 * rate: 指定播放的速度，小于1会慢速播放，大于1会快速播放。
 * is_sound_touch: 是否使用sound touch实现音频变速播放。
 *                 android: 使用sound touch，
 *                 ios: 不使用sound touch.
 */
void ijkmp_set_playback_rate(IjkMediaPlayer* mp, float rate, bool is_sound_touch) {
    assert(mp);

    // MPTRACE("%s(%f)\n", __func__, rate);
    pthread_mutex_lock(&mp->mutex);
    ALOGI("[%d] %s %f\n", MP_SESSION, __FUNCTION__, rate);
    ffp_set_playback_rate(mp->ffplayer, rate, is_sound_touch);
    pthread_mutex_unlock(&mp->mutex);
}

/*
 * tone: 指定播放的音调，小于0音调变低，大于0音调变高。
 */
void ijkmp_set_playback_tone(IjkMediaPlayer* mp, int tone) {
    assert(mp);

    // MPTRACE("%s(%f)\n", __func__, tune);
    pthread_mutex_lock(&mp->mutex);
    ALOGI("[%d] %s %d\n", MP_SESSION, __FUNCTION__, tone);
    ffp_set_playback_tone(mp->ffplayer, tone);
    pthread_mutex_unlock(&mp->mutex);
}

void ijkmp_set_live_manifest_switch_mode(IjkMediaPlayer* mp, int mode) {
    assert(mp);

    // MPTRACE("%s(%f)\n", __func__, tune);
    pthread_mutex_lock(&mp->mutex);
    ALOGI("[%d] %s %d\n", MP_SESSION, __FUNCTION__, mode);
    ffp_set_live_manifest_switch_mode(mp->ffplayer, mode);
    pthread_mutex_unlock(&mp->mutex);
}

int ijkmp_set_stream_selected(IjkMediaPlayer* mp, int stream, int selected) {
    assert(mp);

    // MPTRACE("%s(%d, %d)\n", __func__, stream, selected);
    pthread_mutex_lock(&mp->mutex);
    ALOGI("[%d] %s stream %d selected %d\n", MP_SESSION, __FUNCTION__, stream, selected);
    int ret = ffp_set_stream_selected(mp->ffplayer, stream, selected);
    pthread_mutex_unlock(&mp->mutex);
    // MPTRACE("%s(%d, %d)=%d\n", __func__, stream, selected, ret);
    return ret;
}

float ijkmp_get_property_float(IjkMediaPlayer* mp, int id, float default_value) {
    assert(mp);

    pthread_mutex_lock(&mp->mutex);
    float ret = ffp_get_property_float(mp->ffplayer, id, default_value);
    pthread_mutex_unlock(&mp->mutex);
    return ret;
}

void ijkmp_set_property_float(IjkMediaPlayer* mp, int id, float value) {
    assert(mp);

    pthread_mutex_lock(&mp->mutex);
    ffp_set_property_float(mp->ffplayer, id, value);
    pthread_mutex_unlock(&mp->mutex);
}

int64_t ijkmp_get_property_int64(IjkMediaPlayer* mp, int id, int64_t default_value) {
    assert(mp);

    pthread_mutex_lock(&mp->mutex);
    int64_t ret = ffp_get_property_int64(mp->ffplayer, id, default_value);
    pthread_mutex_unlock(&mp->mutex);
    return ret;
}

void ijkmp_set_property_int64(IjkMediaPlayer* mp, int id, int64_t value) {
    assert(mp);

    pthread_mutex_lock(&mp->mutex);
    ffp_set_property_int64(mp->ffplayer, id, value);
    pthread_mutex_unlock(&mp->mutex);
}

char* ijkmp_get_property_string(IjkMediaPlayer* mp, int id) {
    assert(mp);

    pthread_mutex_lock(&mp->mutex);
    char* str = ffp_get_property_string(mp->ffplayer, id);
    pthread_mutex_unlock(&mp->mutex);

    return str;
}
IjkMediaMeta* ijkmp_get_meta_l(IjkMediaPlayer* mp) {
    assert(mp);

    // MPTRACE("%s\n", __func__);
    IjkMediaMeta* ret = ffp_get_meta_l(mp->ffplayer);
    // MPTRACE("%s()=void\n", __func__);
    return ret;
}

void ijkmp_shutdown_l(IjkMediaPlayer* mp) {
    assert(mp);

    ALOGI("[%d] %s\n", MP_SESSION, __FUNCTION__);
    // MPTRACE("ijkmp_shutdown_l()\n");
    if (mp->ffplayer) {
        ffp_stop_l(mp->ffplayer);
        ffp_wait_stop_l(mp->ffplayer, mp->player_result_qos);
    }
    // MPTRACE("ijkmp_shutdown_l()=void\n");
}

void ijkmp_shutdown(IjkMediaPlayer* mp) {
    // fix protential ffp_getXXX_l multi-thread NPE bugs
    pthread_mutex_lock(&mp->mutex);
    ijkmp_shutdown_l(mp);
    pthread_mutex_unlock(&mp->mutex);
}

void ijkmp_inc_ref(IjkMediaPlayer* mp) {
    assert(mp);
    __sync_fetch_and_add(&mp->ref_count, 1);
}

int ijkmp_dec_ref(IjkMediaPlayer* mp) {
    if (!mp)
        return 0;

    int ref_count = __sync_sub_and_fetch(&mp->ref_count, 1);
    if (ref_count == 0) {
        // MPTRACE("ijkmp_dec_ref(): ref=0\n");
        // ALOGD("[%d] ijkmp_dec_ref, ref_count == 0", MP_SESSION);
        ijkmp_shutdown(mp);
        ijkmp_destroy_p(&mp);
        return 1;
    }
    return 0;
}

int ijkmp_dec_ref_p(IjkMediaPlayer** pmp) {
    if (!pmp)
        return 0;


    int ret = ijkmp_dec_ref(*pmp);
    *pmp = NULL;
    return ret;
}

static int ijkmp_set_data_source_l(IjkMediaPlayer* mp, const char* url) {
    assert(mp);
    assert(url);

    // MPST_RET_IF_EQ(mp->mp_state, MP_STATE_IDLE);
    MPST_RET_IF_EQ(mp->mp_state, MP_STATE_INITIALIZED);
    MPST_RET_IF_EQ(mp->mp_state, MP_STATE_ASYNC_PREPARING);
    MPST_RET_IF_EQ(mp->mp_state, MP_STATE_PREPARED);
    MPST_RET_IF_EQ(mp->mp_state, MP_STATE_STARTED);
    MPST_RET_IF_EQ(mp->mp_state, MP_STATE_PAUSED);
    MPST_RET_IF_EQ(mp->mp_state, MP_STATE_COMPLETED);
    MPST_RET_IF_EQ(mp->mp_state, MP_STATE_STOPPED);
    MPST_RET_IF_EQ(mp->mp_state, MP_STATE_ERROR);
    MPST_RET_IF_EQ(mp->mp_state, MP_STATE_END);

    ALOGI("[%d] %s url %s\n", MP_SESSION, __FUNCTION__, url);

    freep((void**)&mp->data_source);
    mp->data_source = strdup(url);
    if (!mp->data_source)
        return EIJK_OUT_OF_MEMORY;

    ijkmp_change_state_l(mp, MP_STATE_INITIALIZED);
    return 0;
}

int ijkmp_set_data_source(IjkMediaPlayer* mp, const char* url) {
    assert(mp);
    assert(url);

    // MPTRACE("ijkmp_set_data_source(url=\"%s\")\n", url);
    pthread_mutex_lock(&mp->mutex);
    int retval = ijkmp_set_data_source_l(mp, url);
    pthread_mutex_unlock(&mp->mutex);
    // MPTRACE("ijkmp_set_data_source(url=\"%s\")=%d\n", url, retval);
    return retval;
}

static int ijkmp_msg_loop(void* arg) {
    IjkMediaPlayer* mp = arg;
    int ret = mp->msg_loop(arg);
    return ret;
}

static int ijkmp_reprepare_async_l(IjkMediaPlayer* mp, bool is_flush) {
    assert(mp);
    ALOGI("[%d] %s is_flush %d\n", MP_SESSION, __FUNCTION__, is_flush);
    int retval = ffp_reprepare_async_l(mp->ffplayer, mp->data_source, is_flush);
    if (retval < 0) {
        ijkmp_change_state_l(mp, MP_STATE_ERROR);
        return retval;
    }
    ijkmp_change_state_l(mp, MP_STATE_ASYNC_PREPARING);

    return 0;

}
static int ijkmp_prepare_async_l(IjkMediaPlayer* mp) {
    assert(mp);

    ALOGI("[%d] %s\n", MP_SESSION, __FUNCTION__);

    MPST_RET_IF_EQ(mp->mp_state, MP_STATE_IDLE);
    // MPST_RET_IF_EQ(mp->mp_state, MP_STATE_INITIALIZED);
    MPST_RET_IF_EQ(mp->mp_state, MP_STATE_ASYNC_PREPARING);
    MPST_RET_IF_EQ(mp->mp_state, MP_STATE_PREPARED);
    MPST_RET_IF_EQ(mp->mp_state, MP_STATE_STARTED);
    MPST_RET_IF_EQ(mp->mp_state, MP_STATE_PAUSED);
    MPST_RET_IF_EQ(mp->mp_state, MP_STATE_COMPLETED);
    // MPST_RET_IF_EQ(mp->mp_state, MP_STATE_STOPPED);
    MPST_RET_IF_EQ(mp->mp_state, MP_STATE_ERROR);
    MPST_RET_IF_EQ(mp->mp_state, MP_STATE_END);

    assert(mp->data_source);

    ijkmp_change_state_l(mp, MP_STATE_ASYNC_PREPARING);

    msg_queue_start(&mp->ffplayer->msg_queue);

    // released in msg_loop
    ijkmp_inc_ref(mp);
    mp->msg_thread = SDL_CreateThreadEx(&mp->_msg_thread, ijkmp_msg_loop, mp, "ff_msg_loop");
    // msg_thread is detached inside msg_loop
    // TODO: 9 release weak_thiz if pthread_create() failed;

    int retval = ffp_prepare_async_l(mp->ffplayer, mp->data_source);
    if (retval < 0) {
        ijkmp_change_state_l(mp, MP_STATE_ERROR);
        return retval;
    }

    return 0;
}

int ijkmp_reprepare_async(IjkMediaPlayer* mp, bool is_flush) {
    assert(mp);

    // MPTRACE("ijkmp_prepare_async()\n");
    pthread_mutex_lock(&mp->mutex);
    int retval = ijkmp_reprepare_async_l(mp, is_flush);
    pthread_mutex_unlock(&mp->mutex);
    // MPTRACE("ijkmp_prepare_async()=%d\n", retval);
    return retval;
}

int ijkmp_prepare_async(IjkMediaPlayer* mp) {
    assert(mp);
    ALOGI("[%d] %s\n", MP_SESSION, __FUNCTION__);

    KwaiQos_onPrepareAsync(&mp->ffplayer->kwai_qos);
    // MPTRACE("ijkmp_prepare_async()\n");
    pthread_mutex_lock(&mp->mutex);
    int retval = ijkmp_prepare_async_l(mp);
    pthread_mutex_unlock(&mp->mutex);
    // MPTRACE("ijkmp_prepare_async()=%d\n", retval);
    return retval;
}

static int ikjmp_chkst_start_l(int mp_state) {
    MPST_RET_IF_EQ(mp_state, MP_STATE_IDLE);
    MPST_RET_IF_EQ(mp_state, MP_STATE_INITIALIZED);
    MPST_RET_IF_EQ(mp_state, MP_STATE_ASYNC_PREPARING);
    // MPST_RET_IF_EQ(mp_state, MP_STATE_PREPARED);
    // MPST_RET_IF_EQ(mp_state, MP_STATE_STARTED);
    // MPST_RET_IF_EQ(mp_state, MP_STATE_PAUSED);
    // MPST_RET_IF_EQ(mp_state, MP_STATE_COMPLETED);
    MPST_RET_IF_EQ(mp_state, MP_STATE_STOPPED);
    MPST_RET_IF_EQ(mp_state, MP_STATE_ERROR);
    MPST_RET_IF_EQ(mp_state, MP_STATE_END);

    return 0;
}

static int ijkmp_start_l(IjkMediaPlayer* mp) {
    assert(mp);

    ALOGI("[%d] %s\n", MP_SESSION, __FUNCTION__);

    MP_RET_IF_FAILED(ikjmp_chkst_start_l(mp->mp_state));
    KwaiQos_onAppStart(&mp->ffplayer->kwai_qos);
    // must interrupt before MP_RET_IF_FAILED(ikjmp_chkst_start_l(mp->mp_state))
    ffp_interrupt_pre_demux_l(mp->ffplayer);

    ffp_remove_msg(mp->ffplayer, FFP_REQ_START);
    ffp_remove_msg(mp->ffplayer, FFP_REQ_PAUSE);
    ffp_notify_msg1(mp->ffplayer, FFP_REQ_START);

    return 0;
}

int ijkmp_start(IjkMediaPlayer* mp) {
    assert(mp);

    // MPTRACE("ijkmp_start()\n");
    pthread_mutex_lock(&mp->mutex);
    int retval = ijkmp_start_l(mp);
    pthread_mutex_unlock(&mp->mutex);
    // MPTRACE("ijkmp_start()=%d\n", retval);
    return retval;
}

static int ikjmp_chkst_pause_l(int mp_state) {
    MPST_RET_IF_EQ(mp_state, MP_STATE_IDLE);
    MPST_RET_IF_EQ(mp_state, MP_STATE_INITIALIZED);
//    MPST_RET_IF_EQ(mp_state, MP_STATE_ASYNC_PREPARING);
    // MPST_RET_IF_EQ(mp_state, MP_STATE_PREPARED);
    // MPST_RET_IF_EQ(mp_state, MP_STATE_STARTED);
    // MPST_RET_IF_EQ(mp_state, MP_STATE_PAUSED);
    // MPST_RET_IF_EQ(mp_state, MP_STATE_COMPLETED);
    MPST_RET_IF_EQ(mp_state, MP_STATE_STOPPED);
    MPST_RET_IF_EQ(mp_state, MP_STATE_ERROR);
    MPST_RET_IF_EQ(mp_state, MP_STATE_END);

    return 0;
}

static int ijkmp_pause_l(IjkMediaPlayer* mp) {
    assert(mp);
    ALOGI("[%d] %s\n", MP_SESSION, __FUNCTION__);
    MP_RET_IF_FAILED(ikjmp_chkst_pause_l(mp->mp_state));

    ffp_remove_msg(mp->ffplayer, FFP_REQ_START);
    ffp_remove_msg(mp->ffplayer, FFP_REQ_PAUSE);
    ffp_notify_msg1(mp->ffplayer, FFP_REQ_PAUSE);

    return 0;
}

int ijkmp_pause(IjkMediaPlayer* mp) {
    assert(mp);

    // MPTRACE("ijkmp_pause()\n");
    pthread_mutex_lock(&mp->mutex);
    int retval = ijkmp_pause_l(mp);
    pthread_mutex_unlock(&mp->mutex);
    // MPTRACE("ijkmp_pause()=%d\n", retval);
    return retval;
}

static int ikjmp_chkst_step_frame_l(int mp_state) {
    MPST_RET_IF_EQ(mp_state, MP_STATE_IDLE);
    MPST_RET_IF_EQ(mp_state, MP_STATE_INITIALIZED);
    MPST_RET_IF_EQ(mp_state, MP_STATE_ASYNC_PREPARING);
    // MPST_RET_IF_EQ(mp_state, MP_STATE_PREPARED);
    MPST_RET_IF_EQ(mp_state, MP_STATE_STARTED);
    // MPST_RET_IF_EQ(mp_state, MP_STATE_PAUSED);
    MPST_RET_IF_EQ(mp_state, MP_STATE_COMPLETED);
    MPST_RET_IF_EQ(mp_state, MP_STATE_STOPPED);
    MPST_RET_IF_EQ(mp_state, MP_STATE_ERROR);
    MPST_RET_IF_EQ(mp_state, MP_STATE_END);

    return 0;
}

static int ijkmp_step_frame_l(IjkMediaPlayer* mp) {
    assert(mp);
    ALOGI("[%d] %s\n", MP_SESSION, __FUNCTION__);
    MP_RET_IF_FAILED(ikjmp_chkst_step_frame_l(mp->mp_state));

    ffp_remove_msg(mp->ffplayer, FFP_REQ_START);
    ffp_remove_msg(mp->ffplayer, FFP_REQ_STEP);
    ffp_notify_msg1(mp->ffplayer, FFP_REQ_STEP);

    return 0;
}

int ijkmp_step_frame(IjkMediaPlayer* mp) {
    assert(mp);

    // MPTRACE("ijkmp_step_frame()\n");
    pthread_mutex_lock(&mp->mutex);
    int retval = ijkmp_step_frame_l(mp);
    pthread_mutex_unlock(&mp->mutex);
    // MPTRACE("ijkmp_step_frame()=%d\n", retval);
    return retval;
}

static int ijkmp_stop_l(IjkMediaPlayer* mp) {
    assert(mp);
    ALOGI("[%d] %s\n", MP_SESSION, __FUNCTION__);
    MPST_RET_IF_EQ(mp->mp_state, MP_STATE_IDLE);
    MPST_RET_IF_EQ(mp->mp_state, MP_STATE_INITIALIZED);
    // MPST_RET_IF_EQ(mp->mp_state, MP_STATE_ASYNC_PREPARING);
    // MPST_RET_IF_EQ(mp->mp_state, MP_STATE_PREPARED);
    // MPST_RET_IF_EQ(mp->mp_state, MP_STATE_STARTED);
    // MPST_RET_IF_EQ(mp->mp_state, MP_STATE_PAUSED);
    // MPST_RET_IF_EQ(mp->mp_state, MP_STATE_COMPLETED);
    // MPST_RET_IF_EQ(mp->mp_state, MP_STATE_STOPPED);
    MPST_RET_IF_EQ(mp->mp_state, MP_STATE_ERROR);
    MPST_RET_IF_EQ(mp->mp_state, MP_STATE_END);

    ffp_remove_msg(mp->ffplayer, FFP_REQ_START);
    ffp_remove_msg(mp->ffplayer, FFP_REQ_PAUSE);

    int retval = ffp_stop_l(mp->ffplayer);
    if (retval < 0) {
        return retval;
    }

    ijkmp_change_state_l(mp, MP_STATE_STOPPED);
    return 0;
}

int ijkmp_stop_reading(IjkMediaPlayer* mp) {
    assert(mp);

    // MPTRACE("ijkmp_stop()\n");
    pthread_mutex_lock(&mp->mutex);
    ALOGI("[%d] %s\n", MP_SESSION, __FUNCTION__);
    int retval = ffp_read_stop_l(mp->ffplayer);
    pthread_mutex_unlock(&mp->mutex);
    ijkmp_change_state_l(mp, MP_STATE_IDLE);
    // MPTRACE("ijkmp_stop()=%d\n", retval);
    return retval;
}

int ijkmp_stop(IjkMediaPlayer* mp) {
    assert(mp);

    // MPTRACE("ijkmp_stop()\n");
    pthread_mutex_lock(&mp->mutex);
    int retval = ijkmp_stop_l(mp);
    pthread_mutex_unlock(&mp->mutex);
    // MPTRACE("ijkmp_stop()=%d\n", retval);
    return retval;
}

static int ijkmp_audio_only_l(IjkMediaPlayer* mp, bool on) {
    assert(mp);

    ALOGI("[%d] %s, audioonly=%s\n", MP_SESSION, __FUNCTION__, on ? "Yes" : "No");
    if (on) {
        ffp_remove_msg(mp->ffplayer, FFP_REQ_LIVE_RELOAD_AUDIO);
        ffp_notify_msg1(mp->ffplayer, FFP_REQ_LIVE_RELOAD_AUDIO);
    } else {
        ffp_remove_msg(mp->ffplayer, FFP_REQ_LIVE_RELOAD_VIDEO);
        ffp_notify_msg1(mp->ffplayer, FFP_REQ_LIVE_RELOAD_VIDEO);
    }
    return 0;
}

int ijkmp_audio_only(IjkMediaPlayer* mp, bool on) {
    assert(mp);

    pthread_mutex_lock(&mp->mutex);
    int retval = ijkmp_audio_only_l(mp, on);
    pthread_mutex_unlock(&mp->mutex);
    return retval;
}

bool ijkmp_is_playing(IjkMediaPlayer* mp) {
    assert(mp);
    if (mp->mp_state == MP_STATE_PREPARED ||
        mp->mp_state == MP_STATE_STARTED) {
        return true;
    }

    return false;
}

static int ikjmp_chkst_seek_l(int mp_state) {
    MPST_RET_IF_EQ(mp_state, MP_STATE_IDLE);
    MPST_RET_IF_EQ(mp_state, MP_STATE_INITIALIZED);
    MPST_RET_IF_EQ(mp_state, MP_STATE_ASYNC_PREPARING);
    // MPST_RET_IF_EQ(mp_state, MP_STATE_PREPARED);
    // MPST_RET_IF_EQ(mp_state, MP_STATE_STARTED);
    // MPST_RET_IF_EQ(mp_state, MP_STATE_PAUSED);
    // MPST_RET_IF_EQ(mp_state, MP_STATE_COMPLETED);
    MPST_RET_IF_EQ(mp_state, MP_STATE_STOPPED);
    MPST_RET_IF_EQ(mp_state, MP_STATE_ERROR);
    MPST_RET_IF_EQ(mp_state, MP_STATE_END);

    return 0;
}

int ijkmp_seek_to_l(IjkMediaPlayer* mp, long msec) {
    assert(mp);
    ALOGI("[%d] %s %ld\n", MP_SESSION, __FUNCTION__, msec);
    MP_RET_IF_FAILED(ikjmp_chkst_seek_l(mp->mp_state));

    ffp_remove_msg(mp->ffplayer, FFP_REQ_SEEK);
    ffp_notify_msg2(mp->ffplayer, FFP_REQ_SEEK, (int)msec);

    ClockTracker_update_is_seeking(&mp->ffplayer->clock_tracker, true, msec);
    // TODO: 9 64-bit long?

    return 0;
}

int ijkmp_seek_to(IjkMediaPlayer* mp, long msec) {
    assert(mp);

    // MPTRACE("ijkmp_seek_to(%ld)\n", msec);
    pthread_mutex_lock(&mp->mutex);
    int retval = ijkmp_seek_to_l(mp, msec);
    pthread_mutex_unlock(&mp->mutex);
    // MPTRACE("ijkmp_seek_to(%ld)=%d\n", msec, retval);

    return retval;
}

int ijkmp_get_state(IjkMediaPlayer* mp) {
    return mp->mp_state;
}


static float ijkmp_get_probe_fps_l(IjkMediaPlayer* mp) {
    return ffp_get_probe_fps_l(mp->ffplayer);
}


float ijkmp_get_probe_fps(IjkMediaPlayer* mp) {
    if (!mp)
        return 0.0;

    pthread_mutex_lock(&mp->mutex);
    float retval;
    retval = ijkmp_get_probe_fps_l(mp);
    pthread_mutex_unlock(&mp->mutex);
    return retval;
}

//static long ijkmp_get_current_position_l(IjkMediaPlayer* mp) {
//    if (mp->seek_req)
//        return mp->seek_msec;
//    return ffp_get_current_position_l(mp->ffplayer);
//}

long ijkmp_get_current_position(IjkMediaPlayer* mp) {
    assert(mp);
//
//    pthread_mutex_lock(&mp->mutex);
//    long retval;
//    if (mp->seek_req)
//        retval = mp->seek_msec;
//    else
//        retval = ijkmp_get_current_position_l(mp);
//    pthread_mutex_unlock(&mp->mutex);
//    return retval;

    return mp->ffplayer ? ClockTracker_get_current_position_ms(&mp->ffplayer->clock_tracker) : 0;
}

static long ijkmp_get_duration_l(IjkMediaPlayer* mp) {
    return ffp_get_duration_l(mp->ffplayer);
}

long ijkmp_get_duration(IjkMediaPlayer* mp) {
    assert(mp);


    pthread_mutex_lock(&mp->mutex);
    long retval = ijkmp_get_duration_l(mp);
    pthread_mutex_unlock(&mp->mutex);
    return retval;
}

static long ijkmp_get_playable_duration_l(IjkMediaPlayer* mp) {
    return ffp_get_playable_duration_l(mp->ffplayer);
}

long ijkmp_get_playable_duration(IjkMediaPlayer* mp) {
    assert(mp);

    pthread_mutex_lock(&mp->mutex);
    long retval = ijkmp_get_playable_duration_l(mp);
    pthread_mutex_unlock(&mp->mutex);
    return retval;
}

void ijkmp_set_loop(IjkMediaPlayer* mp, int loop) {
    assert(mp);

    pthread_mutex_lock(&mp->mutex);
    ALOGI("[%d] %s loop %d\n", MP_SESSION, __FUNCTION__, loop);
    ffp_set_loop(mp->ffplayer, loop);
    pthread_mutex_unlock(&mp->mutex);
}

int ijkmp_get_loop(IjkMediaPlayer* mp) {
    assert(mp);

    pthread_mutex_lock(&mp->mutex);
    int loop = ffp_get_loop(mp->ffplayer);
    pthread_mutex_unlock(&mp->mutex);
    return loop;
}

bool ijkmp_is_hw(IjkMediaPlayer* mp) {
    assert(mp);

    pthread_mutex_lock(&mp->mutex);
    bool is_hw = ffp_is_hw_l(mp->ffplayer);
    pthread_mutex_unlock(&mp->mutex);
    return is_hw;
}

void* ijkmp_get_weak_thiz(IjkMediaPlayer* mp) {
    return mp->weak_thiz;
}

CCacheSessionListener* ijkmp_get_cache_session_listener(IjkMediaPlayer* mp) {
    return mp->cache_session_listener;
}

void* ijkmp_set_weak_thiz(IjkMediaPlayer* mp, void* weak_thiz) {
    void* prev_weak_thiz = mp->weak_thiz;

    mp->weak_thiz = weak_thiz;

    return prev_weak_thiz;
}

int ijkmp_get_msg(IjkMediaPlayer* mp, AVMessage* msg, int block) {
    assert(mp);

    while (1) {
        int continue_wait_next_msg = 0;
        int retval = msg_queue_get(&mp->ffplayer->msg_queue, msg, block);
        if (retval <= 0)
            return retval;

        if (msg->what != FFP_MSG_BUFFERING_UPDATE) {
            ALOGI("[%d][%s] %s(%d), %s:%d, %s:%d\n", MP_SESSION, __FUNCTION__,
                  ffp_msg_to_str(msg->what), msg->what,
                  ffp_msg_arg1_to_str(msg->what), msg->arg1,
                  ffp_msg_arg2_to_str(msg->what), msg->arg2);
        }

        switch (msg->what) {
            case FFP_MSG_PREPARED:
                // MPTRACE("ijkmp_get_msg: FFP_MSG_PREPARED\n");
                pthread_mutex_lock(&mp->mutex);
                if (mp->mp_state == MP_STATE_ASYNC_PREPARING) {
                    ijkmp_change_state_l(mp, MP_STATE_PREPARED);
                } else {
                    // FIXME: 1: onError() ?
                    ALOGD("[%d] FFP_MSG_PREPARED: expecting mp_state==MP_STATE_ASYNC_PREPARING\n", MP_SESSION);
                }
                if (!mp->ffplayer->start_on_prepared) {
                    ijkmp_change_state_l(mp, MP_STATE_PAUSED);
                }
                pthread_mutex_unlock(&mp->mutex);
                break;

            case FFP_MSG_COMPLETED:
                // MPTRACE("ijkmp_get_msg: FFP_MSG_COMPLETED\n");
                pthread_mutex_lock(&mp->mutex);
                mp->restart = 1;
                mp->restart_from_beginning = 1;
                ijkmp_change_state_l(mp, MP_STATE_COMPLETED);
                pthread_mutex_unlock(&mp->mutex);
                break;

            case FFP_MSG_SEEK_COMPLETE:
                // MPTRACE("ijkmp_get_msg: FFP_MSG_SEEK_COMPLETE\n");
                pthread_mutex_lock(&mp->mutex);

                // 只有外部调用ijkmp_seek_to_l才对外通知（即mp->seek_req == 1)
                // 如果开启了精准seek，则只报accuratre seek的消息
                if (!mp->seek_req || (mp->ffplayer && mp->ffplayer->enable_accurate_seek)) {
                    continue_wait_next_msg = 1;
                } else {
                    // 如果开启了精准seek，则让精准seek来负责重置这两个值
                    mp->seek_req = 0;
                    mp->seek_msec = 0;
                }
                pthread_mutex_unlock(&mp->mutex);
                break;

            case FFP_MSG_ACCURATE_SEEK_COMPLETE:
                pthread_mutex_lock(&mp->mutex);
                // 只有外部调用ijkmp_seek_to_l才对外通知（即mp->seek_req == 1)
                if (!mp->seek_req) {
                    continue_wait_next_msg = 1;
                }
                mp->seek_req = 0;
                mp->seek_msec = 0;
                pthread_mutex_unlock(&mp->mutex);
                break;

            case FFP_MSG_AUDIO_RENDERING_START_AFTER_SEEK:
            case FFP_MSG_AUDIO_RENDERING_START:
                if (mp->ffplayer && (AV_SYNC_AUDIO_MASTER == ffp_get_master_sync_type(mp->ffplayer->is)))
                    ClockTracker_update_is_seeking(&mp->ffplayer->clock_tracker, false, 0);
                break;

            case FFP_MSG_VIDEO_RENDERING_START_AFTER_SEEK:
            case FFP_MSG_VIDEO_RENDERING_START:
                if (mp->ffplayer && (AV_SYNC_VIDEO_MASTER == ffp_get_master_sync_type(mp->ffplayer->is)))
                    ClockTracker_update_is_seeking(&mp->ffplayer->clock_tracker, false, 0);
                break;

            case FFP_REQ_START:
                // MPTRACE("ijkmp_get_msg: FFP_REQ_START\n");
                continue_wait_next_msg = 1;
                pthread_mutex_lock(&mp->mutex);
                if (0 == ikjmp_chkst_start_l(mp->mp_state)) {
                    // FIXME: 8 check seekable
                    if (mp->restart) {
                        if (mp->restart_from_beginning) {
                            ALOGI("[%d] ijkmp_get_msg: FFP_REQ_START: restart from beginning\n", MP_SESSION);
                            retval = ffp_start_from_l(mp->ffplayer, 0);
                            if (retval == 0) {
                                ijkmp_change_state_l(mp, MP_STATE_STARTED);
                            }
                        } else {
                            ALOGI("[%d] ijkmp_get_msg: FFP_REQ_START: restart from seek pos\n", MP_SESSION);
                            retval = ffp_start_l(mp->ffplayer);
                            if (retval == 0) {
                                ijkmp_change_state_l(mp, MP_STATE_STARTED);
                            }
                        }
                        mp->restart = 0;
                        mp->restart_from_beginning = 0;
                    } else {
                        ALOGI("[%d] ijkmp_get_msg: FFP_REQ_START: start on fly\n", MP_SESSION);
                        retval = ffp_start_l(mp->ffplayer);
                        if (retval == 0) {
                            ijkmp_change_state_l(mp, MP_STATE_STARTED);
                        }
                    }
                }
                pthread_mutex_unlock(&mp->mutex);
                break;

            case FFP_REQ_PAUSE:
                // MPTRACE("ijkmp_get_msg: FFP_REQ_PAUSE\n");
                continue_wait_next_msg = 1;
                pthread_mutex_lock(&mp->mutex);
                if (0 == ikjmp_chkst_pause_l(mp->mp_state)) {
                    int pause_ret = ffp_pause_l(mp->ffplayer);
                    if (pause_ret == 0) {
                        ijkmp_change_state_l(mp, MP_STATE_PAUSED);
                    }
                }
                pthread_mutex_unlock(&mp->mutex);
                break;

            case FFP_REQ_SEEK:
                // MPTRACE("ijkmp_get_msg: FFP_REQ_SEEK\n");
                continue_wait_next_msg = 1;

                pthread_mutex_lock(&mp->mutex);
                mp->seek_req = 1;
                mp->seek_msec = msg->arg1;
                if (0 == ikjmp_chkst_seek_l(mp->mp_state)) {
                    mp->restart_from_beginning = 0;
                    if (0 == ffp_seek_to_l(mp->ffplayer, msg->arg1)) {
                        ALOGI("[%d] ijkmp_get_msg: FFP_REQ_SEEK: seek to %d\n", MP_SESSION, (int)msg->arg1);
                    }
                }
                pthread_mutex_unlock(&mp->mutex);
                break;

            case FFP_REQ_STEP:
                // MPTRACE("ijkmp_get_msg: FFP_REQ_STEP\n");
                continue_wait_next_msg = 1;

                pthread_mutex_lock(&mp->mutex);
                if (0 == ikjmp_chkst_step_frame_l(mp->mp_state)) {
                    ffp_step_frame_l(mp->ffplayer);
                }
                pthread_mutex_unlock(&mp->mutex);
                break;
            case FFP_REQ_LIVE_RELOAD_AUDIO:
                continue_wait_next_msg = 1;

                pthread_mutex_lock(&mp->mutex);
                ALOGI("[%d] %s, calling ffp_reload_audio_l\n", MP_SESSION, __FUNCTION__);
                ffp_reload_audio_l(mp->ffplayer);
                pthread_mutex_unlock(&mp->mutex);
                break;
            case FFP_REQ_LIVE_RELOAD_VIDEO:
                continue_wait_next_msg = 1;

                pthread_mutex_lock(&mp->mutex);
                ALOGI("[%d] %s, calling ffp_reload_video_l\n", MP_SESSION, __FUNCTION__);
                ffp_reload_video_l(mp->ffplayer);
                pthread_mutex_unlock(&mp->mutex);
                break;

            default:
                break;
        }

        if (continue_wait_next_msg)
            continue;

        return retval;
    }

    return -1;
}


void ijkmp_set_buffersize(IjkMediaPlayer* mp, int size) {
    assert(mp);

    pthread_mutex_lock(&mp->mutex);
    set_buffersize(mp->ffplayer, size);
    pthread_mutex_unlock(&mp->mutex);
}

void ijkmp_set_readtimeout(IjkMediaPlayer* mp, int size) {
    assert(mp);

    pthread_mutex_lock(&mp->mutex);
    set_timeout(mp->ffplayer, size);
    pthread_mutex_unlock(&mp->mutex);
}

void ijkmp_set_connectiontimeout(IjkMediaPlayer* mp, int size) {
    // 这个值查过源码，单位是ms，默认值是5000ms
    ijkmp_set_option_int(mp, IJKMP_OPT_CATEGORY_FORMAT, "open_timeout", (int64_t)size * 1000);
}


void ijkmp_set_volume(IjkMediaPlayer* mp, float leftVolume, float rightVolume) {
    assert(mp);

    pthread_mutex_lock(&mp->mutex);
    ALOGI("[%d] %s volume %f,%f\n", MP_SESSION, __FUNCTION__, leftVolume, rightVolume);
    set_volume(mp->ffplayer, leftVolume, rightVolume);
    pthread_mutex_unlock(&mp->mutex);

}

void ijkmp_set_codecflag(IjkMediaPlayer* mp,  int flag) {
    assert(mp);

    pthread_mutex_lock(&mp->mutex);
    set_codecflag(mp->ffplayer, flag);
    pthread_mutex_unlock(&mp->mutex);
}

int ijkmp_get_qos_info(IjkMediaPlayer* mp, KsyQosInfo* info) {
    assert(mp);

    //MPST_RET_IF_NOT_IN_PROGRESS(mp->mp_state);

    pthread_mutex_lock(&mp->mutex);
    ffp_get_qos_info(mp->ffplayer, info);
    pthread_mutex_unlock(&mp->mutex);

    return 0;
}

KFlvPlayerStatistic ijkmp_get_kflv_statisitc(IjkMediaPlayer* mp) {
    assert(mp);

    pthread_mutex_lock(&mp->mutex);
    KFlvPlayerStatistic ret = ffp_get_kflv_statisitc(mp->ffplayer);
    pthread_mutex_unlock(&mp->mutex);
    return ret;
}

int ijkmp_free_qos_info(IjkMediaPlayer* mp, KsyQosInfo* info) {
    assert(mp);

    MPST_RET_IF_NOT_IN_PROGRESS(mp->mp_state);

    pthread_mutex_lock(&mp->mutex);
    ffp_free_qos_info(mp->ffplayer, info);
    pthread_mutex_unlock(&mp->mutex);

    return 0;
}

char* ijkmp_get_live_stat_json_str(IjkMediaPlayer* mp) {
    assert(mp);

    pthread_mutex_lock(&mp->mutex);
    char* stat_json_str = ffp_get_live_stat_json_str(mp->ffplayer);
    pthread_mutex_unlock(&mp->mutex);

    return stat_json_str;
}

char* ijkmp_get_video_stat_json_str(IjkMediaPlayer* mp) {
    assert(mp);

    pthread_mutex_lock(&mp->mutex);
    char* stat_json_str = ffp_get_video_stat_json_str(mp->ffplayer);
    pthread_mutex_unlock(&mp->mutex);

    return stat_json_str;
}

char* ijkmp_get_brief_video_stat_json_str(IjkMediaPlayer* mp) {
    assert(mp);

    pthread_mutex_lock(&mp->mutex);
    char* stat_json_str = ffp_get_brief_video_stat_json_str(mp->ffplayer);
    pthread_mutex_unlock(&mp->mutex);

    return stat_json_str;
}

int ijkmp_set_mute(IjkMediaPlayer* mp, int mute) {
    int ret = -1;
    assert(mp);

    pthread_mutex_lock(&mp->mutex);
    ret = ffp_set_mute(mp->ffplayer, mute);
    pthread_mutex_unlock(&mp->mutex);
    return ret;
}

void ijkmp_set_audio_data_callback(IjkMediaPlayer* mp, void* arg) {
    assert(mp);

    pthread_mutex_lock(&mp->mutex);
    ffp_set_audio_data_callback(mp->ffplayer, arg);
    pthread_mutex_unlock(&mp->mutex);
}

void ijkmp_set_live_event_callback(IjkMediaPlayer* mp, void* arg) {
    assert(mp);

    pthread_mutex_lock(&mp->mutex);
    ffp_set_live_event_callback(mp->ffplayer, arg);
    pthread_mutex_unlock(&mp->mutex);
}

void ijkmp_set_wall_clock(IjkMediaPlayer* mp, int64_t wall_clock_epoch_ms) {
    assert(mp);

    pthread_mutex_lock(&mp->mutex);
    if (mp->ffplayer) {
        mp->ffplayer->wall_clock_offset = av_gettime() / 1000 - wall_clock_epoch_ms;
        mp->ffplayer->wall_clock_updated = true;
    }
    pthread_mutex_unlock(&mp->mutex);
}

void ijkmp_set_max_wall_clock_offset(IjkMediaPlayer* mp, int64_t max_wall_clock_offset_ms) {
    assert(mp);

    pthread_mutex_lock(&mp->mutex);
    if (mp->ffplayer) {
        LiveAbsTimeControl_set(&mp->ffplayer->live_abs_time_control, max_wall_clock_offset_ms);
    }
    pthread_mutex_unlock(&mp->mutex);
}


void ijkmp_set_config_json(IjkMediaPlayer* mp, const char* config_json) {
    assert(mp);

    pthread_mutex_lock(&mp->mutex);
    ffp_set_config_json(mp->ffplayer, config_json);
    pthread_mutex_unlock(&mp->mutex);
}

void ijkmp_set_live_low_delay_config_json(IjkMediaPlayer* mp, const char* config_json) {
    assert(mp);

    pthread_mutex_lock(&mp->mutex);
    ffp_set_live_low_delay_config_json(mp->ffplayer, config_json);
    pthread_mutex_unlock(&mp->mutex);
}

void ijkmp_set_hevc_codec_name(IjkMediaPlayer* mp, const char* hevc_codec_name) {
    assert(mp);

    pthread_mutex_lock(&mp->mutex);
    ffp_set_hevc_codec_name(mp->ffplayer, hevc_codec_name);
    pthread_mutex_unlock(&mp->mutex);
}



void ijkmp_setup_cache_session_listener(IjkMediaPlayer* mp, CCacheSessionListener* listener) {
    assert(mp);

    mp->cache_session_listener = listener;

    pthread_mutex_lock(&mp->mutex);
    ffp_setup_cache_session_listener(mp->ffplayer, listener);
    pthread_mutex_unlock(&mp->mutex);
}

void ijkmp_setup_awesome_cache_callback(IjkMediaPlayer* mp,
                                        AwesomeCacheCallback_Opaque callback) {

    pthread_mutex_lock(&mp->mutex);
    ffp_setup_awesome_cache_callback(mp->ffplayer, callback);
    pthread_mutex_unlock(&mp->mutex);
}
static long ijkmp_is_cache_enabled_l(IjkMediaPlayer* mp) {
    return ffp_get_use_cache_l(mp->ffplayer);
}

static long ijkmp_is_live_manifest_l(IjkMediaPlayer* mp) {
    return ffp_is_live_manifest_l(mp->ffplayer);
}


bool ijkmp_is_cache_enabled(IjkMediaPlayer* mp) {
    assert(mp);

    pthread_mutex_lock(&mp->mutex);
    bool retval = ijkmp_is_cache_enabled_l(mp);
    pthread_mutex_unlock(&mp->mutex);
    return retval;
}

bool ijkmp_is_live_manifest(IjkMediaPlayer* mp) {
    assert(mp);

    pthread_mutex_lock(&mp->mutex);
    bool retval = ijkmp_is_live_manifest_l(mp);
    pthread_mutex_unlock(&mp->mutex);
    return retval;
}

char* ijkmp_get_qos_live_realtime_json_str(IjkMediaPlayer* mp, int first, int last,
                                           int64_t start_time, int64_t duration,
                                           int64_t collectInterval) {
    assert(mp);

    pthread_mutex_lock(&mp->mutex);
    char* json_str = QosLiveRealtime_collect(mp->ffplayer, first, last,
                                             start_time, duration, collectInterval);
    pthread_mutex_unlock(&mp->mutex);

    return json_str;
}

void ijkmp_set_live_app_qos_info(IjkMediaPlayer* mp, const char* app_qos_info) {
    assert(mp);

    pthread_mutex_lock(&mp->mutex);
    QosLiveRealtime_set_app_qos_info(mp->ffplayer, app_qos_info);
    pthread_mutex_unlock(&mp->mutex);
}

void ijkmp_get_vod_qos_debug_info(IjkMediaPlayer* mp, VodQosDebugInfo* qos) {
    pthread_mutex_lock(&mp->mutex);
    VodQosDebugInfo_collect(qos, mp);
    pthread_mutex_unlock(&mp->mutex);
}

void ijkmp_get_player_config_debug_info(IjkMediaPlayer* mp, PlayerConfigDebugInfo* di) {
    pthread_mutex_lock(&mp->mutex);
    PlayerConfigDebugInfo_collect(di, mp);
    pthread_mutex_unlock(&mp->mutex);
}

void ijkmp_get_vod_kwai_sign(IjkMediaPlayer* mp, char* sign) {
    pthread_mutex_lock(&mp->mutex);
    ffp_get_kwai_sign(mp->ffplayer, sign);
    pthread_mutex_unlock(&mp->mutex);
}

void ijkmp_get_x_ks_cache(IjkMediaPlayer* mp, char* x_ks_cache) {
    pthread_mutex_lock(&mp->mutex);
    ffp_get_x_ks_cache(mp->ffplayer, x_ks_cache);
    pthread_mutex_unlock(&mp->mutex);
}

void ijkmp_get_vod_adaptive_url(IjkMediaPlayer* mp, char* current_url) {
    pthread_mutex_lock(&mp->mutex);
    ffp_get_vod_adaptive_url(mp->ffplayer, current_url);
    pthread_mutex_unlock(&mp->mutex);
}

void ijkmp_get_vod_adaptive_cache_key(IjkMediaPlayer* mp, char* cache_key) {
    pthread_mutex_lock(&mp->mutex);
    ffp_get_vod_adaptive_cache_key(mp->ffplayer, cache_key);
    pthread_mutex_unlock(&mp->mutex);
}

void ijkmp_get_vod_adaptive_host_name(IjkMediaPlayer* mp, char* host_name) {
    pthread_mutex_lock(&mp->mutex);
    ffp_get_vod_adaptive_host_name(mp->ffplayer, host_name);
    pthread_mutex_unlock(&mp->mutex);
}

void ijkmp_get_live_voice_comment(IjkMediaPlayer* mp, char* vc, int64_t time) {
    pthread_mutex_lock(&mp->mutex);
    ffp_get_kwai_live_voice_comment(mp->ffplayer, vc, time);
    pthread_mutex_unlock(&mp->mutex);
}

void ijkmp_get_live_qos_debug_info(IjkMediaPlayer* mp, LiveQosDebugInfo* qos) {
    pthread_mutex_lock(&mp->mutex);
    LiveQosDebugInfo_collect(qos, mp);
    pthread_mutex_unlock(&mp->mutex);
}

static void ijkmp_get_qos_live_adaptive_realtime_l(IjkMediaPlayer* mp, QosLiveAdaptiveRealtime* qos) {
    QosLiveAdaptiveRealtime_collect(qos, mp->ffplayer);
}

void ijkmp_get_qos_live_adaptive_realtime(IjkMediaPlayer* mp, QosLiveAdaptiveRealtime* qos) {
    pthread_mutex_lock(&mp->mutex);
    ijkmp_get_qos_live_adaptive_realtime_l(mp, qos);
    pthread_mutex_unlock(&mp->mutex);
}

void ijkmp_enable_pre_demux(IjkMediaPlayer* mp, int pre_demux_ver, int64_t duration_ms) {
    assert(mp);

    pthread_mutex_lock(&mp->mutex);
    ALOGI("[%d] %s duration_ms %lld\n", MP_SESSION, __FUNCTION__, duration_ms);
    ffp_enable_pre_demux_l(mp->ffplayer, pre_demux_ver, duration_ms);
    pthread_mutex_unlock(&mp->mutex);
}

void ijkmp_set_last_try_flag(IjkMediaPlayer* mp, int is_last_try) {
    assert(mp);

    pthread_mutex_lock(&mp->mutex);
    ALOGI("[%d] %s is_last_try %d\n", MP_SESSION, __FUNCTION__, is_last_try);
    ffp_set_last_try_flag(mp->ffplayer, is_last_try);
    pthread_mutex_unlock(&mp->mutex);
}

void ijkmp_enable_ab_loop(IjkMediaPlayer* mp, int64_t a_pts_ms, int64_t b_pts_ms) {
    assert(mp);

    pthread_mutex_lock(&mp->mutex);
    ALOGI("[%d] %s a_pts_ms %lld, b_pts_ms:%lld\n", MP_SESSION, __FUNCTION__, a_pts_ms, b_pts_ms);
    ffp_enable_ab_loop_l(mp->ffplayer, a_pts_ms, b_pts_ms);
    pthread_mutex_unlock(&mp->mutex);
}

void ijkmp_enable_loop_on_block(IjkMediaPlayer* mp, int buffer_start_percent,
                                int buffer_end_percent, int64_t loop_begin) {
    assert(mp);

    pthread_mutex_lock(&mp->mutex);
    ALOGI("[%d] %s buffer_start_percent：%d buffer_end_percent:%d loop_begin:%lld\n",
          MP_SESSION, __FUNCTION__, buffer_start_percent, buffer_end_percent, loop_begin);
    ffp_enable_buffer_loop_l(mp->ffplayer, buffer_start_percent, buffer_end_percent, loop_begin);
    pthread_mutex_unlock(&mp->mutex);
}

void ijkmp_abort_native_cache_io(IjkMediaPlayer* mp) {
    assert(mp);

    ALOGI("[%d] %s\n", MP_SESSION, __FUNCTION__);
    // 此接口暂留，以后还需用到，但是不是目前这样实现
}

int ijkmp_get_session_id(IjkMediaPlayer* mp) {
    assert(mp);

    int session_id = -1;
    pthread_mutex_lock(&mp->mutex);
    if (mp->ffplayer) {
        session_id = mp->ffplayer->session_id;
    }
    pthread_mutex_unlock(&mp->mutex);
    return session_id;
}

void ijkmp_set_start_play_block_ms(IjkMediaPlayer* mp, int block_buffer_ms, int max_buffer_cost_ms) {
    assert(mp);

    pthread_mutex_lock(&mp->mutex);
    ALOGI("[%d] %s block_buffer_ms %d \n", MP_SESSION, __FUNCTION__, block_buffer_ms);
    ffp_set_start_play_buffer_ms(mp->ffplayer, block_buffer_ms, max_buffer_cost_ms);
    pthread_mutex_unlock(&mp->mutex);
}

bool ijkmp_check_can_start_play(IjkMediaPlayer* mp) {
    assert(mp);
    bool retval = false;
    pthread_mutex_lock(&mp->mutex);
    retval = ffp_check_can_start_play(mp->ffplayer);
    pthread_mutex_unlock(&mp->mutex);
    ALOGI("[%d][%s] retval: %d\n", MP_SESSION, __FUNCTION__, retval);
    return retval;
}

int32_t ijkmp_get_downloaded_percent(IjkMediaPlayer* mp) {
    assert(mp);
    int32_t retval = 0;
    pthread_mutex_lock(&mp->mutex);
    retval = ffp_get_downloaded_percent(mp->ffplayer);
    pthread_mutex_unlock(&mp->mutex);
    ALOGI("[%d][%s] retval: %d\n", MP_SESSION, __FUNCTION__, retval);
    return retval;
}

KwaiPlayerResultQos* ijkmp_get_result_qos(IjkMediaPlayer* mp) {
    assert(mp);
    return mp->player_result_qos;
}
