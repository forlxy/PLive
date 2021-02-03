/*
 * ijkplayer.h
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

#ifndef IJKPLAYER_ANDROID__IJKPLAYER_H
#define IJKPLAYER_ANDROID__IJKPLAYER_H

#include <stdbool.h>
#include <awesome_cache/include/awesome_cache_callback_c.h>
#include "ff_ffmsg_queue.h"
#include "ff_ffplay_def.h"
#include "ijkmeta.h"
#include "ijkkwai/kwai_player_qos.h"
#include "cache_session_listener_c.h"
#include "ijkkwai/qos/qos_live_realtime.h"
#include "ijkkwai/qos/qos_live_adaptive_realtime.h"
#include "ijkkwai/qos/vod_qos_debug_info.h"
#include "ijkkwai/qos/live_qos_debug_info.h"
#include "ijkkwai/qos/player_config_debug_info.h"
#include "ijkkwai/qos/kwaiplayer_result_qos.h"

#ifndef MPTRACE
#define MPTRACE ALOGD
#endif

typedef struct IjkMediaPlayer IjkMediaPlayer;
struct FFPlayer;
struct SDL_Vout;

/*-
 MPST_CHECK_NOT_RET(mp->mp_state, MP_STATE_IDLE);
 MPST_CHECK_NOT_RET(mp->mp_state, MP_STATE_INITIALIZED);
 MPST_CHECK_NOT_RET(mp->mp_state, MP_STATE_ASYNC_PREPARING);
 MPST_CHECK_NOT_RET(mp->mp_state, MP_STATE_PREPARED);
 MPST_CHECK_NOT_RET(mp->mp_state, MP_STATE_STARTED);
 MPST_CHECK_NOT_RET(mp->mp_state, MP_STATE_PAUSED);
 MPST_CHECK_NOT_RET(mp->mp_state, MP_STATE_COMPLETED);
 MPST_CHECK_NOT_RET(mp->mp_state, MP_STATE_STOPPED);
 MPST_CHECK_NOT_RET(mp->mp_state, MP_STATE_ERROR);
 MPST_CHECK_NOT_RET(mp->mp_state, MP_STATE_END);
 */

/*-
 * ijkmp_set_data_source()  -> MP_STATE_INITIALIZED
 *
 * ijkmp_reset              -> self
 * ijkmp_release            -> MP_STATE_END
 */
#define MP_STATE_IDLE               0

/*-
 * ijkmp_prepare_async()    -> MP_STATE_ASYNC_PREPARING
 *
 * ijkmp_reset              -> MP_STATE_IDLE
 * ijkmp_release            -> MP_STATE_END
 */
#define MP_STATE_INITIALIZED        1

/*-
 *                   ...    -> MP_STATE_PREPARED
 *                   ...    -> MP_STATE_ERROR
 *
 * ijkmp_reset              -> MP_STATE_IDLE
 * ijkmp_release            -> MP_STATE_END
 */
#define MP_STATE_ASYNC_PREPARING    2

/*-
 * ijkmp_seek_to()          -> self
 * ijkmp_start()            -> MP_STATE_STARTED
 *
 * ijkmp_reset              -> MP_STATE_IDLE
 * ijkmp_release            -> MP_STATE_END
 */
#define MP_STATE_PREPARED           3

/*-
 * ijkmp_seek_to()          -> self
 * ijkmp_start()            -> self
 * ijkmp_pause()            -> MP_STATE_PAUSED
 * ijkmp_stop()             -> MP_STATE_STOPPED
 *                   ...    -> MP_STATE_COMPLETED
 *                   ...    -> MP_STATE_ERROR
 *
 * ijkmp_reset              -> MP_STATE_IDLE
 * ijkmp_release            -> MP_STATE_END
 */
#define MP_STATE_STARTED            4

/*-
 * ijkmp_seek_to()          -> self
 * ijkmp_start()            -> MP_STATE_STARTED
 * ijkmp_pause()            -> self
 * ijkmp_stop()             -> MP_STATE_STOPPED
 *
 * ijkmp_reset              -> MP_STATE_IDLE
 * ijkmp_release            -> MP_STATE_END
 */
#define MP_STATE_PAUSED             5

/*-
 * ijkmp_seek_to()          -> self
 * ijkmp_start()            -> MP_STATE_STARTED (from beginning)
 * ijkmp_pause()            -> self
 * ijkmp_stop()             -> MP_STATE_STOPPED
 *
 * ijkmp_reset              -> MP_STATE_IDLE
 * ijkmp_release            -> MP_STATE_END
 */
#define MP_STATE_COMPLETED          6

/*-
 * ijkmp_stop()             -> self
 * ijkmp_prepare_async()    -> MP_STATE_ASYNC_PREPARING
 *
 * ijkmp_reset              -> MP_STATE_IDLE
 * ijkmp_release            -> MP_STATE_END
 */
#define MP_STATE_STOPPED            7

/*-
 * ijkmp_reset              -> MP_STATE_IDLE
 * ijkmp_release            -> MP_STATE_END
 */
#define MP_STATE_ERROR              8

/*-
 * ijkmp_release            -> self
 */
#define MP_STATE_END                9



#define IJKMP_IO_STAT_READ 1


#define IJKMP_OPT_CATEGORY_FORMAT FFP_OPT_CATEGORY_FORMAT
#define IJKMP_OPT_CATEGORY_CODEC  FFP_OPT_CATEGORY_CODEC
#define IJKMP_OPT_CATEGORY_SWS    FFP_OPT_CATEGORY_SWS
#define IJKMP_OPT_CATEGORY_PLAYER FFP_OPT_CATEGORY_PLAYER


void            ijkmp_global_init();
void            ijkmp_global_uninit();
void            ijkmp_global_set_log_report(int use_report);
void            ijkmp_global_set_log_level(int log_level);   // log_level = AV_LOG_xxx
void            ijkmp_global_set_inject_callback(ijk_inject_callback cb);
void            ijkmp_global_set_kwailog_level(int log_level); // log_level = AV_LOG_xxx
const char*     ijkmp_version_ident();
unsigned int    ijkmp_version_int();
void            ijkmp_io_stat_register(void (*cb)(const char* url, int type, int bytes));
void            ijkmp_io_stat_complete_register(void (*cb)(const char* url,
                                                           int64_t read_bytes, int64_t total_size,
                                                           int64_t elpased_time, int64_t total_duration));

// ref_count is 1 after open
IjkMediaPlayer* ijkmp_create(int (*msg_loop)(void*));
void            ijkmp_set_inject_opaque(IjkMediaPlayer* mp, void* opaque);

void            ijkmp_set_option(IjkMediaPlayer* mp, int opt_category, const char* name, const char* value);
void            ijkmp_set_option_int(IjkMediaPlayer* mp, int opt_category, const char* name, int64_t value);

int             ijkmp_get_video_codec_info(IjkMediaPlayer* mp, char** codec_info);
int             ijkmp_get_audio_codec_info(IjkMediaPlayer* mp, char** codec_info);
void            ijkmp_set_playback_rate(IjkMediaPlayer* mp, float rate, bool is_sound_touch);
void            ijkmp_set_playback_tone(IjkMediaPlayer* mp, int tone);
void            ijkmp_set_live_manifest_switch_mode(IjkMediaPlayer* mp, int mode);
int             ijkmp_set_stream_selected(IjkMediaPlayer* mp, int stream, int selected);

float           ijkmp_get_property_float(IjkMediaPlayer* mp, int id, float default_value);
void            ijkmp_set_property_float(IjkMediaPlayer* mp, int id, float value);
int64_t         ijkmp_get_property_int64(IjkMediaPlayer* mp, int id, int64_t default_value);
void            ijkmp_set_property_int64(IjkMediaPlayer* mp, int id, int64_t value);
char*           ijkmp_get_property_string(IjkMediaPlayer* mp, int id);

// must be freed with free();
IjkMediaMeta*   ijkmp_get_meta_l(IjkMediaPlayer* mp);

// preferred to be called explicity, can be called multiple times
// NOTE: ijkmp_shutdown may block thread
void            ijkmp_shutdown(IjkMediaPlayer* mp);

void            ijkmp_inc_ref(IjkMediaPlayer* mp);

// call close at last release, also free memory
// NOTE: ijkmp_dec_ref may block thread
// return: 1 if mp is destroyed
int             ijkmp_dec_ref(IjkMediaPlayer* mp);
int             ijkmp_dec_ref_p(IjkMediaPlayer** pmp);

int             ijkmp_set_data_source(IjkMediaPlayer* mp, const char* url);
int             ijkmp_prepare_async(IjkMediaPlayer* mp);
int             ijkmp_reprepare_async(IjkMediaPlayer* mp, bool is_flush);
int             ijkmp_start(IjkMediaPlayer* mp);
int             ijkmp_pause(IjkMediaPlayer* mp);
int             ijkmp_step_frame(IjkMediaPlayer* mp);
int             ijkmp_stop(IjkMediaPlayer* mp);
int             ijkmp_stop_reading(IjkMediaPlayer* mp);
int             ijkmp_audio_only(IjkMediaPlayer* mp, bool on);
int             ijkmp_seek_to(IjkMediaPlayer* mp, long msec);
int             ijkmp_get_state(IjkMediaPlayer* mp);
bool            ijkmp_is_playing(IjkMediaPlayer* mp);
float           ijkmp_get_probe_fps(IjkMediaPlayer* mp);
long            ijkmp_get_current_position(IjkMediaPlayer* mp);
long            ijkmp_get_duration(IjkMediaPlayer* mp);
long            ijkmp_get_playable_duration(IjkMediaPlayer* mp);
void            ijkmp_set_loop(IjkMediaPlayer* mp, int loop);
int             ijkmp_get_loop(IjkMediaPlayer* mp);
bool            ijkmp_is_hw(IjkMediaPlayer* mp);

void*           ijkmp_get_weak_thiz(IjkMediaPlayer* mp);
void*           ijkmp_set_weak_thiz(IjkMediaPlayer* mp, void* weak_thiz);

void            ijkmp_setup_cache_session_listener(IjkMediaPlayer* mp,
                                                   CCacheSessionListener* listener);
CCacheSessionListener* ijkmp_get_cache_session_listener(IjkMediaPlayer* mp);
void            ijkmp_setup_awesome_cache_callback(IjkMediaPlayer* mp,
                                                   AwesomeCacheCallback_Opaque callback);

/* return < 0 if aborted, 0 if no packet and > 0 if packet.  */
int             ijkmp_get_msg(IjkMediaPlayer* mp, AVMessage* msg, int block);

// ksyun
KFlvPlayerStatistic ijkmp_get_kflv_statisitc(IjkMediaPlayer* mp);
int             ijkmp_get_qos_info(IjkMediaPlayer* mp, KsyQosInfo* info);
int             ijkmp_free_qos_info(IjkMediaPlayer* mp, KsyQosInfo* info);
char*           ijkmp_get_live_stat_json_str(IjkMediaPlayer* mp); // the returned str needs to free after usage
char*           ijkmp_get_video_stat_json_str(IjkMediaPlayer* mp); // the returned str needs to free after usage
char*           ijkmp_get_brief_video_stat_json_str(IjkMediaPlayer* mp); // the returned str needs to free after usage
void            ijkmp_set_buffersize(IjkMediaPlayer* mp, int size);
void            ijkmp_set_connectiontimeout(IjkMediaPlayer* mp, int size);
void            ijkmp_set_readtimeout(IjkMediaPlayer* mp, int size);
int             ijkmp_set_mute(IjkMediaPlayer* mp, int mute);
void            ijkmp_set_codecflag(IjkMediaPlayer* mp,  int flag);
void            ijkmp_set_audio_data_callback(IjkMediaPlayer* mp, void* arg);
void            ijkmp_set_live_event_callback(IjkMediaPlayer* mp, void* arg);
void            ijkmp_set_volume(IjkMediaPlayer* mp, float leftVolume, float rightVolume);

void            ijkmp_set_wall_clock(IjkMediaPlayer* mp, int64_t wall_clock_epoch_ms);
void            ijkmp_set_max_wall_clock_offset(IjkMediaPlayer* mp, int64_t max_wall_clock_offset_ms);
void            ijkmp_set_config_json(IjkMediaPlayer* mp, const char* config_json);
void            ijkmp_set_live_low_delay_config_json(IjkMediaPlayer* mp, const char* config_json);
void            ijkmp_set_hevc_codec_name(IjkMediaPlayer* mp, const char* hevc_codec_name);


bool            ijkmp_is_cache_enabled(IjkMediaPlayer* mp);
bool            ijkmp_is_live_manifest(IjkMediaPlayer* mp);

char*           ijkmp_get_qos_live_realtime_json_str(IjkMediaPlayer* mp, int first, int last,
                                                     int64_t start_time, int64_t duration,
                                                     int64_t collectInterval);
void            ijkmp_set_live_app_qos_info(IjkMediaPlayer* mp, const char* app_qos_info);
void            ijkmp_get_qos_live_adaptive_realtime(IjkMediaPlayer* mp, QosLiveAdaptiveRealtime* qos);
void            ijkmp_get_vod_qos_debug_info(IjkMediaPlayer* mp, VodQosDebugInfo* qos);
void            ijkmp_get_player_config_debug_info(IjkMediaPlayer* mp, PlayerConfigDebugInfo* di);
void            ijkmp_get_live_qos_debug_info(IjkMediaPlayer* mp, LiveQosDebugInfo* qos);
void            ijkmp_get_vod_kwai_sign(IjkMediaPlayer* mp, char* sign);
void            ijkmp_get_x_ks_cache(IjkMediaPlayer* mp, char* x_ks_cache);
void            ijkmp_get_vod_adaptive_url(IjkMediaPlayer* mp, char* current_url);
void            ijkmp_get_vod_adaptive_cache_key(IjkMediaPlayer* mp, char* cache_key);
void            ijkmp_get_vod_adaptive_host_name(IjkMediaPlayer* mp, char* host_name);
void            ijkmp_set_last_try_flag(IjkMediaPlayer* mp, int is_last_try);

void            ijkmp_get_live_voice_comment(IjkMediaPlayer* mp, char* vc, int64_t time);

// pre_demux ,only for vod, not for live
void            ijkmp_enable_pre_demux(IjkMediaPlayer* mp, int pre_demux_ver, int64_t duration_ms);

// wont work for audio-only media
void            ijkmp_enable_ab_loop(IjkMediaPlayer* mp, int64_t a_pts_ms, int64_t b_pts_ms);
void            ijkmp_enable_loop_on_block(IjkMediaPlayer* mp, int buffer_start_percent,
                                           int buffer_end_percent, int64_t loop_begin);
void            ijkmp_abort_native_cache_io(IjkMediaPlayer* mp);

int             ijkmp_get_session_id(IjkMediaPlayer* mp);

void            ijkmp_set_start_play_block_ms(IjkMediaPlayer* mp, int block_buffer_ms, int max_buffer_cost_ms);
bool            ijkmp_check_can_start_play(IjkMediaPlayer* mp);
int32_t         ijkmp_get_downloaded_percent(IjkMediaPlayer* mp);
KwaiPlayerResultQos* ijkmp_get_result_qos(IjkMediaPlayer* mp);
#endif
