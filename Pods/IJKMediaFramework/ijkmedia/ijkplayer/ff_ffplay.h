/*
 * ff_ffplay.h
 *
 * Copyright (c) 2003 Fabrice Bellard
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

#ifndef FFPLAY__FF_FFPLAY_H
#define FFPLAY__FF_FFPLAY_H

#include <awesome_cache/include/awesome_cache_callback_c.h>
#include "ff_ffplay_def.h"
#include "ff_fferror.h"
#include "ff_ffmsg.h"
#include "ijkkwai/kwai_player_qos.h"
#include "ijkkwai/cache/ffmpeg_adapter.h"
#include "ijkkwai/qos/kwaiplayer_result_qos.h"
#include "cache_session_listener_c.h"

void      ffp_global_init();
void      ffp_global_uninit();
void      ffp_global_set_log_report(int use_report);
void      ffp_global_set_log_level(int log_level);
void      ffp_global_set_inject_callback(ijk_inject_callback cb);
void      ffp_io_stat_register(void (*cb)(const char* url, int type, int bytes));
void      ffp_io_stat_complete_register(void (*cb)(const char* url,
                                                   int64_t read_bytes, int64_t total_size,
                                                   int64_t elpased_time, int64_t total_duration));

FFPlayer* ffp_create();
void      ffp_destroy(FFPlayer* ffp);
void      ffp_destroy_p(FFPlayer** pffp);
void      ffp_reset(FFPlayer* ffp);

/* set options before ffp_prepare_async_l() */

void      ffp_set_option(FFPlayer* ffp, int opt_category, const char* name, const char* value);
void      ffp_set_option_int(FFPlayer* ffp, int opt_category, const char* name, int64_t value);

int       ffp_get_video_codec_info(FFPlayer* ffp, char** codec_info);
int       ffp_get_audio_codec_info(FFPlayer* ffp, char** codec_info);

/* playback controll */
int       ffp_reprepare_async_l(FFPlayer* ffp, const char* file_name, bool is_flush);
int       ffp_prepare_async_l(FFPlayer* ffp, const char* file_name);
int       ffp_start_from_l(FFPlayer* ffp, long msec);
int       ffp_start_l(FFPlayer* ffp);
int       ffp_pause_l(FFPlayer* ffp);
int       ffp_is_paused_l(FFPlayer* ffp);
int       ffp_step_frame_l(FFPlayer* ffp);
int       ffp_stop_l(FFPlayer* ffp);
int       ffp_read_stop_l(FFPlayer* ffp);
int       ffp_wait_stop_l(FFPlayer* ffp, KwaiPlayerResultQos* result_qos);
int       ffp_reload_audio_l(FFPlayer* ffp);
int       ffp_reload_video_l(FFPlayer* ffp);

/* all in milliseconds */
int       ffp_seek_to_l(FFPlayer* ffp, long msec);
float     ffp_get_probe_fps_l(FFPlayer* ffp);
long      ffp_get_current_position_l(FFPlayer* ffp);
long      ffp_get_duration_l(FFPlayer* ffp);
long      ffp_get_playable_duration_l(FFPlayer* ffp);
void      ffp_set_loop(FFPlayer* ffp, int loop);
int       ffp_get_loop(FFPlayer* ffp);

void      ffp_on_clock_changed(FFPlayer* ffp, Clock* clock);

/* for internal usage */
int       ffp_packet_queue_init(PacketQueue* q);
void      ffp_packet_queue_destroy(PacketQueue* q);
void      ffp_packet_queue_abort(PacketQueue* q);
void      ffp_packet_queue_start(PacketQueue* q);
void      ffp_packet_queue_flush(PacketQueue* q);
int       ffp_packet_queue_get(PacketQueue* q, AVPacket* pkt, int block, int* serial);
int       ffp_packet_queue_get_with_abs_time(FFPlayer* ffp, PacketQueue* q, AVPacket* pkt, int* serial, int* finished, int64_t* abs_time);
int       ffp_packet_queue_get_or_buffering(FFPlayer* ffp, PacketQueue* q, AVPacket* pkt, int* serial, int* finished);
int       ffp_packet_queue_put(PacketQueue* q, AVPacket* pkt);
bool      ffp_is_flush_packet(AVPacket* pkt);

Frame*    ffp_frame_queue_peek_writable(FrameQueue* f);
void      ffp_frame_queue_push(FrameQueue* f);

int       ffp_queue_picture_with_abs_time(FFPlayer* ffp, AVFrame* src_frame, double pts, double duration, int64_t pos, int serial, int64_t abs_time);
int       ffp_queue_picture(FFPlayer* ffp, AVFrame* src_frame, double pts, double duration, int64_t pos, int serial);

int       ffp_get_master_sync_type(VideoState* is);
double    ffp_get_master_clock(VideoState* is);

void      ffp_toggle_buffering_l(FFPlayer* ffp, int start_buffering, int is_block);
void      ffp_toggle_buffering(FFPlayer* ffp, int start_buffering, int is_block);
void      ffp_check_buffering_l(FFPlayer* ffp, bool is_eof);
void      ffp_track_statistic_l(FFPlayer* ffp, AVStream* st, PacketQueue* q, FFTrackCacheStatistic* cache);
void      ffp_audio_statistic_l(FFPlayer* ffp);
void      ffp_video_statistic_l(FFPlayer* ffp);
void      ffp_statistic_l(FFPlayer* ffp);

int       ffp_video_thread(FFPlayer* ffp);

void      ffp_set_video_codec_info(FFPlayer* ffp, const char* codec, const char* decoder);
void      ffp_set_audio_codec_info(FFPlayer* ffp, const char* codec, const char* decoder);

void      ffp_set_playback_rate(FFPlayer* ffp, float rate, bool is_sound_touch);
void      ffp_set_playback_tone(FFPlayer* ffp, int tone);
void      ffp_set_live_manifest_switch_mode(FFPlayer* ffp, int mode);
int       ffp_get_video_rotate_degrees(FFPlayer* ffp);
int       ffp_set_stream_selected(FFPlayer* ffp, int stream, int selected);

float     ffp_get_property_float(FFPlayer* ffp, int id, float default_value);
void      ffp_set_property_float(FFPlayer* ffp, int id, float value);
int64_t   ffp_get_property_int64(FFPlayer* ffp, int id, int64_t default_value);
void      ffp_set_property_int64(FFPlayer* ffp, int id, int64_t value);
long      ffp_get_property_long(FFPlayer* ffp, int id, long default_value);
char*     ffp_get_property_string(FFPlayer* ffp, int id);

char*     ffp_get_playing_url(FFPlayer* ffp);
void      ffp_get_speed_change_info(FFPlayer* ffp, SpeedChangeStat* info);
// qos
void      ffp_get_qos_info(FFPlayer* ffp, KsyQosInfo* info);
void      ffp_free_qos_info(FFPlayer* ffp, KsyQosInfo* info);
char*     ffp_get_live_stat_json_str(FFPlayer* ffp); // the returned str needs to free after usage
char*     ffp_get_video_stat_json_str(FFPlayer* ffp); // the returned str needs to free after usage
char*     ffp_get_brief_video_stat_json_str(FFPlayer* ffp); // the returned str needs to free after usage
void      ffp_set_last_try_flag(FFPlayer* ffp, int is_last_try);

// AwesomeCache
void    ffp_setup_cache_session_listener(FFPlayer* ffp, CCacheSessionListener* listener);
void    ffp_setup_awesome_cache_callback(FFPlayer* mp, AwesomeCacheCallback_Opaque callback);
bool    ffp_get_use_cache_l(FFPlayer* ffp);

bool    ffp_is_live_manifest_l(FFPlayer* ffp);

// pre_demux
void ffp_enable_pre_demux_l(FFPlayer* ffp, int pre_demux_ver, int64_t dur_ms);
void ffp_interrupt_pre_demux_l(FFPlayer* ffp);

KFlvPlayerStatistic ffp_get_kflv_statisitc(FFPlayer* ffp);

// must be freed with free();
struct IjkMediaMeta* ffp_get_meta_l(FFPlayer* ffp);

// kwai code start
void      set_buffersize(FFPlayer* ffp, int size);
void      set_timeout(FFPlayer* ffp, int time);
void      set_codecflag(FFPlayer* ffp, int flag);
void      ffp_set_audio_data_callback(FFPlayer* ffp, void* arg);
void      ffp_set_live_event_callback(FFPlayer* ffp, void* callback);
int       ffp_set_mute(FFPlayer* ffp, int mute);
void      set_volume(FFPlayer* ffp, float leftVolume, float rightVolume);
void      ffp_get_screen_shot(FFPlayer* ffp, int stride, void* dst_buffer);
void      ffp_set_config_json(FFPlayer* ffp, const char* config_json);
void      ffp_set_live_low_delay_config_json(FFPlayer* ffp, const char* config_json);
void      ffp_enable_ab_loop_l(FFPlayer* ffp, int64_t a_pts_ms, int64_t b_pts_ms);
void      ffp_enable_buffer_loop_l(FFPlayer* ffp, int buffer_start_percent, int buffer_end_percent, int64_t loop_begin);
bool      ffp_is_hw_l(FFPlayer* ffp);
void      ffp_get_kwai_sign(FFPlayer* ffp, char* sign);
void      ffp_get_x_ks_cache(FFPlayer* ffp, char* x_ks_cache);
void      ffp_get_vod_adaptive_url(FFPlayer* ffp, char* current_url);
void      ffp_get_vod_adaptive_cache_key(FFPlayer* ffp, char* cache_key);
void      ffp_get_vod_adaptive_host_name(FFPlayer* ffp, char* host_name);
void      ffp_get_kwai_live_voice_comment(FFPlayer* ffp, char* voice_comment, int64_t time);
void      ffp_set_hevc_codec_name(FFPlayer* ffp, const char* hevc_codec_name);
void      ffp_set_start_play_buffer_ms(FFplayer* ffp, int buffer_ms, int max_buffer_cost_ms);
bool      ffp_check_can_start_play(FFplayer* ffp);
int32_t   ffp_get_downloaded_percent(FFplayer* ffp);
void      ffp_kwai_collect_dts_info(FFplayer* ffp, AVPacket* pkt, int audio_stream, int video_stream, AVStream* st);


#endif
