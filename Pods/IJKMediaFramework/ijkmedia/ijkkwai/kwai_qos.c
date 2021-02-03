//
// Created by 帅龙成 on 15/09/2017.
//

#include <stdlib.h>
#include <pthread.h>
#include <regex.h>
#include <ijksdl/ijksdl_misc.h>
#include <ijksdl/ijksdl_log.h>
#include <ijksdl/ijksdl_timer.h>
#include <string.h>
#include <libavkwai/cJSON.h>
#include <libavutil/timestamp.h>

#include "kwai_qos.h"

#include "c_resource_monitor.h"
#include "ijkplayer/ff_ffplay_def.h"
#include "ijkplayer/ff_ffplay.h"
#include "str_encrypt.h"
#include "kwai_player_version_gennerated.h"
#include "kwai_error_code_manager.h"
#include "ijkkwai/cache/ffmpeg_adapter.h"
#include "ijkkwai/cache/cache_statistic.h"
#include <awesome_cache/include/awesome_cache_c.h>
#include <awesome_cache/include/awesome_cache_runtime_info_c.h>
#include <awesome_cache/include/dcc_algorithm_c.h>
#include <ijkmedia/ijkkwai/kwaiplayer_lifecycle.h>
#include "c_abr_engine.h"
#include "ijkplayer/ffplay_modules/ff_ffplay_internal.h"

#if defined(__ANDROID__)
#include "ijkkwai/prof/android/cpu_memory_profiler.h"
#endif

#define CHECK_NULL_RETURN(p) if(!p) return;
#define CHECK_NULL_RETURN_ZERO(p) if(!p) return 0;

// 只统计开播后一定秒数内的丢帧情况
#define KSY_LIVE_CALC_TIME_OF_FST_DROP_DURATION_MS (10000)

//统计开播后一段时间内的卡顿情况
#define KWAI_CALC_TIME_OF_FST_BLOCK_MS (2000)

// 获取系统性能的最小时间间隔
#define KWAI_MIN_SYSTEM_PERFORMANCE_STAT_INTERVAL  (2000)

#define MAX_LEN_SWITCH_REASON   128
#define MAX_LEN_BANDWIDTH_COMPUTER_PROCESS   128
#define MAX_LEN_MAX_RESOLUTION  256
#define MAX_LEN_NET_TYPE        32

#define MAX_BLOCK_INFO_SIZE 10
#define MAX_BLOCK_STAT_SIZE 20
inline static int64_t get_current_time_ms() {
    return av_gettime_relative() / 1000;
}

inline static int64_t get_duration_ms(int64_t dur, int stream_index, AVFormatContext* ic) {
    if (stream_index < 0 || !ic || !ic->streams || !ic->streams[stream_index]) {
        return -1;
    }

    return av_rescale_q(dur, ic->streams[stream_index]->time_base, (AVRational) {1, 1000});
}

/* Get domain from http url
 * example: url: http://ws.pull.yximgs.com/gifshow/FiFyvT0VSS8.flv?wsTime=59097c41&wsSecret=18bd06c726a75a829cd45d88eca31abd
 * domain: ws.pull.yximgs.com
 */
inline static char* get_http_domain(FFplayer* ffp, const char* filename) {
    char* filepath = filename;
    if (ffp->input_data_type == INPUT_DATA_TYPE_INDEX_CONTENT) {
        filepath = ffp->index_content.pre_path;
    }
    if (!filepath) {
        return NULL;
    }
    const char* p_start, *p_end;
    char* domain = NULL;
    if ((p_start = strstr(filepath, "//"))) {
        p_start += 2;
        if ((p_end = strchr(p_start, '/'))) {
            size_t len = p_end - p_start + 1;
            domain = (char*)av_malloc(len);
            memcpy(domain, p_start, len - 1);
            domain[len - 1] = '\0';
        }
    }
    return domain;
}

/* Get stream id from http url
 * example: url: http://ws.pull.yximgs.com/gifshow/FiFyvT0VSS8.flv?wsTime=59097c41&wsSecret=18bd06c726a75a829cd45d88eca31abd
 * stream_id: FiFyvT0VSS8
 */
inline static char* get_http_stream_id(const char* filename) {
    if (!filename) {
        return NULL;
    }
    const char* p1, *p2, *p_start, *p_end;
    char* streamId = NULL;
    if ((p1 = strstr(filename, "//"))) {
        p1 += 2;
        if ((p2 = strchr(p1, '/'))) {
            p2++;
            if ((p_start = strchr(p2, '/'))) {
                p_start++;
                if ((p_end = strstr(p_start, ".flv"))) {
                    size_t len = p_end - p_start + 1;
                    streamId = (char*)av_malloc(len);
                    memcpy(streamId, p_start, len - 1);
                    streamId[len - 1] = '\0';
                }
            }
        }
    }
    return streamId;
}

void
KwaiQos_init(KwaiQos* qos) {

    memset(qos, 0, sizeof(KwaiQos));
    qos->basic.sdk_version = KWAI_PLAYER_VERSION;

    qos->runtime_cost.cost_first_screen = -1;
    qos->runtime_cost.cost_first_sample = -1;
    qos->runtime_cost.cost_start_play_block = -1;
    qos->runtime_cost.cost_total_first_screen = -1;
    qos->runtime_cost.cost_http_connect = -1;
    qos->runtime_cost.cost_http_first_data = -1;
    qos->runtime_cost.cost_dns_analyze = -1;
    qos->runtime_cost.cost_prepare_ms = -1;
    qos->runtime_cost.cost_app_start_play = -1;
    qos->runtime_cost.step_first_video_pkt_received = -1;
    qos->runtime_cost.step_av_input_open = -1;
    qos->runtime_cost.step_av_find_stream_info = -1;
    qos->runtime_cost.step_pre_demux_including_waiting = -1;
    qos->runtime_cost.step_open_decoder = -1;
    qos->runtime_cost.step_all_prepared = -1;
    qos->runtime_cost.cost_wait_for_playing = -1;
    qos->runtime_cost.step_decode_first_frame = -1;
    qos->runtime_cost.step_first_framed_rendered = -1;
    qos->runtime_cost.step_pre_decode_first_video_pkt = -1;
    qos->runtime_cost.step_first_audio_pkt_received = -1;
    qos->runtime_cost.step_decode_first_audio_frame = -1;
    qos->runtime_cost.step_pre_decode_first_audio_pkt = -1;
    qos->runtime_cost.step_first_audio_framed_rendered = -1;

    qos->runtime_stat.begining_dropped_duration = -1;
    qos->runtime_stat.v_hw_dec = false;
    qos->runtime_stat.pix_format = AV_PIX_FMT_NONE;

    qos->player_config.use_awesome_cache = false;
    qos->player_config.pre_load_finish = false;
    qos->player_config.use_pre_load = false;
    qos->player_config.input_data_type = -1;
    qos->player_config.overlay_format = -1;
    qos->player_config.enable_segment_cache = 0;

    qos->media_metadata.fps = -1;
    qos->media_metadata.duration = -1;
    qos->media_metadata.audio_duration = -1;
    qos->media_metadata.channels = -1;
    qos->media_metadata.sample_rate = -1;
    qos->media_metadata.width = -1;
    qos->media_metadata.height = -1;
    qos->media_metadata.bitrate = -1;
    qos->media_metadata.audio_bit_rate = -1;
    qos->media_metadata.comment = NULL;
    qos->media_metadata.audio_profile = NULL;
    qos->media_metadata.transcoder_group = NULL;
    qos->media_metadata.color_space = AVCOL_SPC_NB;

    qos->runtime_stat.is_blocking = false;

    TimeRecorder_init(&qos->runtime_stat.block_duration);
    TimeRecorder_init(&qos->runtime_stat.app_played_duration);
    TimeRecorder_init(&qos->runtime_stat.alive_duration);
    TimeRecorder_init(&qos->runtime_stat.actual_played_duration);
    TimeRecorder_init(&qos->runtime_stat.pause_at_first_screen_duration);

    TimeRecorder_init(&qos->seek_stat.seek_duration);
    TimeRecorder_init(&qos->seek_stat.first_frame_after_seek);
    TimeRecorder_init(&qos->seek_stat.first_packet_after_seek);

    qos->runtime_stat.max_av_diff = 0;
    qos->runtime_stat.min_av_diff = 0;

    qos->seek_stat.seek_cnt = 0;
    qos->seek_stat.seek_first_frame_cnt = 0;
    qos->seek_stat.seek_first_packet_cnt = 0;

    QosLiveRealtime_init(&qos->qos_live_realtime, &qos->live_adaptive);

    qos->vod_adaptive.switch_reason = NULL;
    qos->vod_adaptive.bandwidth_computer_process = NULL;
    qos->vod_adaptive.representations_str = NULL;
    qos->vod_adaptive.net_type = NULL;
    qos->vod_adaptive.cur_url = NULL;
    qos->vod_adaptive.cur_host = NULL;
    qos->vod_adaptive.cur_key = NULL;
    qos->vod_adaptive.cur_quality_show = NULL;
    qos->vod_adaptive.mutex = SDL_CreateMutex();

    qos->dict_mutex = SDL_CreateMutex();
    qos->real_time_block_info_mutex = SDL_CreateMutex();
    qos->sum_block_info_mutex = SDL_CreateMutex();
    qos->qos_live_realtime.app_qos_json_mutex = SDL_CreateMutex();

    qos->hw_decode.video_tool_box.enable = false;
}

void
KwaiQos_close(KwaiQos* qos) {
    av_freep(&qos->media_metadata.video_codec_info);
    av_freep(&qos->media_metadata.audio_codec_info);
    av_freep(&qos->player_config.filename);
    av_freep(&qos->player_config.server_ip);
    av_freep(&qos->player_config.host);
    av_freep(&qos->player_config.domain);
    av_freep(&qos->player_config.stream_id);
    av_freep(&qos->player_config.product_context);
    av_freep(&qos->media_metadata.comment);
    av_freep(&qos->media_metadata.transcoder_ver);
    av_freep(&qos->media_metadata.transcoder_group);
    av_freep(&qos->media_metadata.stream_info);
    av_freep(&qos->media_metadata.audio_profile);
    av_freep(&qos->media_metadata.input_fomat);
    av_freep(&qos->vod_adaptive.switch_reason);
    av_freep(&qos->vod_adaptive.detail_switch_reason);
    av_freep(&qos->vod_adaptive.bandwidth_computer_process);
    av_freep(&qos->vod_adaptive.representations_str);
    av_freep(&qos->vod_adaptive.net_type);
    av_freep(&qos->vod_adaptive.cur_url);
    av_freep(&qos->vod_adaptive.cur_host);
    av_freep(&qos->vod_adaptive.cur_key);
    av_freep(&qos->vod_adaptive.cur_quality_show);
    av_freep(&qos->ac_cache.p2sp_sdk_details);
    av_freep(&qos->audio_str);
    if (qos->sum_block_info) {
        cJSON_Delete(qos->sum_block_info);
        qos->sum_block_info = NULL;
    }
    if (qos->real_time_block_info) {
        cJSON_Delete(qos->real_time_block_info);
        qos->real_time_block_info = NULL;
    }

    if (qos->qos_live_realtime.app_qos_json) {
        cJSON_Delete(qos->qos_live_realtime.app_qos_json);
        qos->qos_live_realtime.app_qos_json = NULL;
    }

    av_dict_free(&qos->ic_metadata);
    av_dict_free(&qos->video_st_metadata);

    SDL_DestroyMutexP(&qos->vod_adaptive.mutex);
    SDL_DestroyMutexP(&qos->dict_mutex);
    SDL_DestroyMutexP(&qos->real_time_block_info_mutex);
    SDL_DestroyMutexP(&qos->sum_block_info_mutex);
    SDL_DestroyMutexP(&qos->qos_live_realtime.app_qos_json_mutex);

    freep((void**)&qos->ac_cache.stats_json_str);
}

void KwaiQos_getTranscoderVersionFromComment(KwaiQos* qos, const char* comment) {
    char* ptr = NULL, *pre = NULL;
    const char* key = "tv=";

    if (!comment) {
        return;
    }

    int len = (int)strlen(key);
    ptr = strstr(comment, key);

    //move ptr to skip key.
    while (ptr && len) {
        ptr++;
        len--;
    }

    pre = ptr;
    while (ptr && *ptr != ']') {
        ptr++;
    }
    if (ptr && *ptr == ']') {
        *ptr = '\0';
        qos->media_metadata.transcoder_ver = strdup(pre);
        *ptr = ']';
    }

    return;
}

void KwaiQos_copyAvformatContextMetadata(KwaiQos* qos, AVFormatContext* ic) {
    if (!qos || !ic) {
        return;
    }
    SDL_LockMutex(qos->dict_mutex);
    if (ic->metadata && !qos->ic_metadata) {
        av_dict_copy(&qos->ic_metadata, ic->metadata, 0);
    }
    SDL_UnlockMutex(qos->dict_mutex);
}

void KwaiQos_copyVideoStreamMetadata(KwaiQos* qos, AVStream* video_stream) {
    if (!qos || !video_stream) {
        return;
    }
    SDL_LockMutex(qos->dict_mutex);
    if (video_stream->metadata && !qos->video_st_metadata) {
        av_dict_copy(&qos->video_st_metadata, video_stream->metadata, 0);
    }
    SDL_UnlockMutex(qos->dict_mutex);
}

float KwaiQos_getAppAverageFps(KwaiQos* qos) {
    int64_t app_play_duration_ms = KwaiQos_getAppPlayTotalDurationMs(qos);

    if (app_play_duration_ms == 0) {
        return 0;
    } else {
        // 由于pause是在 ijkplayer_msg_loop的线程里做的，
        // 为了能兼容调用方在pause后立马同步调用此接口得到准确的结果，所以需要判断是否需要立马结算一次
        float ret = qos->runtime_stat.render_frame_count * 1000.0f / app_play_duration_ms;
        return ret;
    }
}


void KwaiQos_onPrepareAsync(KwaiQos* qos) {
    qos->ts_start_prepare_async = get_current_time_ms();
}

void KwaiQos_onAppStart(KwaiQos* qos) {
    if (qos->runtime_cost.cost_app_start_play <= 0) {
        qos->ts_app_start = get_current_time_ms();
        qos->runtime_cost.cost_app_start_play = qos->ts_app_start - qos->ts_start_prepare_async;
    }
}

void KwaiQos_onInputOpened(KwaiQos* qos) {
    qos->ts_av_input_opened = get_current_time_ms();
    qos->runtime_cost.step_av_input_open = qos->ts_av_input_opened - qos->ts_start_prepare_async;
}

void KwaiQos_onAllPrepared(KwaiQos* qos) {
    qos->ts_all_prepared = get_current_time_ms();
    qos->runtime_cost.step_all_prepared = qos->ts_all_prepared - qos->ts_open_decoder;
    qos->runtime_cost.cost_prepare_ms = qos->ts_all_prepared - qos->ts_start_prepare_async - qos->runtime_cost.step_pre_demux_including_waiting;
}

void KwaiQos_onVideoPacketReceived(KwaiQos* qos) {
    if (qos->ts_first_video_pkt_received <= 0) {
        qos->ts_first_video_pkt_received = get_current_time_ms();
        qos->runtime_cost.step_first_video_pkt_received =
            qos->ts_first_video_pkt_received - qos->ts_av_find_stream_info;
    }
}

void KwaiQos_onAudioPacketReceived(KwaiQos* qos) {
    if (qos->ts_first_audio_pkt_received <= 0) {
        qos->ts_first_audio_pkt_received = get_current_time_ms();
        qos->runtime_cost.step_first_audio_pkt_received =
            qos->ts_first_audio_pkt_received - qos->ts_av_find_stream_info;
    }
}

void KwaiQos_onVideoFrameBeforeDecode(KwaiQos* qos) {
    qos->runtime_stat.v_read_frame_count++;
    if (qos->ts_before_decode_first_frame <= 0) {
        qos->ts_before_decode_first_frame = get_current_time_ms();
        if (qos->ts_open_decoder > 0) {
            qos->runtime_cost.step_pre_decode_first_video_pkt =
                qos->ts_before_decode_first_frame - qos->ts_open_decoder;
        } else {
            qos->runtime_cost.step_pre_decode_first_video_pkt = 0;
        }
    }
}

void KwaiQos_onVideoFrameDecoded(KwaiQos* qos) {
    qos->runtime_stat.v_decode_frame_count++;
    if (qos->ts_after_decode_first_frame <= 0) {
        qos->ts_after_decode_first_frame = get_current_time_ms();
        qos->runtime_cost.step_decode_first_frame =
            qos->ts_after_decode_first_frame - qos->ts_before_decode_first_frame;
    }
}

void KwaiQos_onVideoRenderFirstFrameFilled(KwaiQos* qos) {
    if (qos->ts_first_frame_filled <= 0) {
        qos->ts_first_frame_filled = get_current_time_ms();
    }
}

void KwaiQos_onFrameRendered(KwaiQos* qos, double duration, int start_on_prepared) {
    qos->runtime_stat.render_frame_count++;
    qos->runtime_stat.v_played_duration += duration;

    if (qos->ts_frist_frame_rendered == 0) {
        qos->ts_frist_frame_rendered = get_current_time_ms();

        qos->runtime_cost.step_first_framed_rendered =
            qos->ts_after_decode_first_frame > 0 ?
            qos->ts_frist_frame_rendered - qos->ts_after_decode_first_frame :
            qos->ts_frist_frame_rendered - qos->ts_before_decode_first_frame; // for hardware decoding

        qos->runtime_cost.cost_pause_at_first_screen = KwaiQos_getPauseDurationAtFirstScreen(qos);

        qos->runtime_cost.cost_total_first_screen = qos->ts_frist_frame_rendered - qos->ts_start_prepare_async;
        if (start_on_prepared) {
            // 自动开播的情况下，首屏只需要减去开播前app暂停时长
            qos->runtime_cost.cost_first_screen = qos->runtime_cost.cost_total_first_screen -
                                                  qos->runtime_cost.cost_pause_at_first_screen;
        } else {
            // pre_load/pre_decode
            qos->runtime_cost.cost_first_screen = qos->runtime_cost.cost_total_first_screen -
                                                  qos->runtime_cost.cost_pure_pre_demux -
                                                  qos->runtime_cost.cost_wait_for_playing -
                                                  qos->runtime_cost.cost_pause_at_first_screen;
        }

        // 上面的一系列加减，可能会减出负值，最后做一重纠正，避免-2，-3这种case报上去
        if (qos->runtime_cost.cost_first_screen < 0) {
            qos->runtime_cost.cost_first_screen = 0;
        }
    }
}

void KwaiQos_onSamplePlayed(KwaiQos* qos, double duration, int start_on_prepared) {
    qos->runtime_stat.render_sample_count++;
    qos->runtime_stat.a_played_duration += duration;

    if (qos->runtime_stat.render_sample_count == 2) {
        qos->ts_frist_sample_rendered = get_current_time_ms();

        qos->runtime_cost.cost_pause_at_first_screen = KwaiQos_getPauseDurationAtFirstScreen(qos);
        qos->runtime_cost.step_first_audio_framed_rendered =
            qos->ts_after_decode_first_audio_frame > 0 ?
            qos->ts_frist_sample_rendered - qos->ts_after_decode_first_audio_frame :
            qos->ts_frist_sample_rendered - qos->ts_before_decode_first_audio_frame; // for hardware decoding

        qos->runtime_cost.cost_total_first_sample = qos->ts_frist_sample_rendered - qos->ts_start_prepare_async;
        if (start_on_prepared) {
            // 自动开播的情况下，首屏只需要减去开播前app暂停时长
            qos->runtime_cost.cost_first_sample = qos->runtime_cost.cost_total_first_sample -
                                                  qos->runtime_cost.cost_pause_at_first_screen;
        } else {
            // pre_load/pre_decode
            qos->runtime_cost.cost_first_sample = qos->runtime_cost.cost_total_first_sample -
                                                  qos->runtime_cost.cost_pure_pre_demux -
                                                  qos->runtime_cost.cost_wait_for_playing -
                                                  qos->runtime_cost.cost_pause_at_first_screen;
        }
        // 上面的一系列加减，可能会减出负值，最后做一重纠正，避免-2，-3这种case报上去
        if (qos->runtime_cost.cost_first_sample < 0) {
            qos->runtime_cost.cost_first_sample = 0;
        }
    }
}

void KwaiQos_onAudioFrameBeforeDecode(KwaiQos* qos, int64_t value) {
    qos->runtime_stat.a_read_frame_dur_ms += value;
    if (qos->ts_before_decode_first_audio_frame <= 0) {
        qos->ts_before_decode_first_audio_frame = get_current_time_ms();
        if (qos->ts_open_decoder > 0) {
            qos->runtime_cost.step_pre_decode_first_audio_pkt =
                qos->ts_before_decode_first_audio_frame - qos->ts_open_decoder;
        } else {
            qos->runtime_cost.step_pre_decode_first_audio_pkt = 0;
        }
    }

}

void KwaiQos_onAudioFrameDecoded(KwaiQos* qos, int64_t value) {
    qos->runtime_stat.a_decode_frame_dur_ms += value;
    if (qos->ts_after_decode_first_audio_frame <= 0) {
        qos->ts_after_decode_first_audio_frame = get_current_time_ms();
        qos->runtime_cost.step_decode_first_audio_frame =
            qos->ts_after_decode_first_audio_frame - qos->ts_before_decode_first_audio_frame;
    }
}

void KwaiQos_onAudioDecodeErr(KwaiQos* qos, int64_t value) {
    qos->runtime_stat.a_decode_err_dur_ms += value;
}


void KwaiQos_onSilenceSamplePlayed(KwaiQos* qos) {
    qos->runtime_stat.silence_sample_count++;
}

void KwaiQos_onReadThreadForLoopStart(KwaiQos* qos) {
    qos->read_thread_for_loop_started = true;
}

void KwaiQos_onReadyToRender(KwaiQos* qos) {
    if (qos->runtime_cost.cost_first_render_ready <= 0) {
        qos->ts_first_render_ready = get_current_time_ms();
        qos->runtime_cost.cost_first_render_ready = qos->ts_first_render_ready - qos->ts_start_prepare_async;
        ALOGI("[KwaiQos_onReadyToRender] qos->runtime_cost.cost_first_render_ready:%lldms", qos->runtime_cost.cost_first_render_ready);
    } else {
        ALOGW("[KwaiQos_onReadyToRender] should not be called more than once");
    }
}

void KwaiQos_onStreamInfoFound(KwaiQos* qos) {
    qos->ts_av_find_stream_info = av_gettime_relative() / 1000;
    qos->runtime_cost.step_av_find_stream_info =
        qos->ts_av_find_stream_info - qos->ts_av_input_opened;
}

void KwaiQos_onPreDemuxFinish(KwaiQos* qos, FFPlayer* ffp) {
    qos->ts_predmux_finish = get_current_time_ms();
    qos->runtime_cost.step_pre_demux_including_waiting = ffp->pre_demux ? (qos->ts_predmux_finish - qos->ts_av_find_stream_info) : 0;
    qos->runtime_cost.cost_pure_pre_demux = ffp->pre_demux ? ffp->pre_demux->pre_load_cost_ms : 0;
    qos->player_config.pre_load_finish = ffp->pre_demux ? ffp->pre_demux->complete : 0;
}

void
KwaiQos_onDecoderOpened(KwaiQos* qos) {
    qos->ts_open_decoder = get_current_time_ms();
    qos->runtime_cost.step_open_decoder = qos->ts_open_decoder - qos->ts_predmux_finish;
}

void KwaiQos_onBufferingStart(FFPlayer* ffp, int is_block) {
    KwaiQos* qos = &ffp->kwai_qos;
    if (!is_block)
        return;
    TimeRecoder_start(&qos->runtime_stat.block_duration);
    if (!qos->runtime_stat.is_blocking)
        qos->runtime_stat.block_cnt++;
    qos->runtime_stat.is_blocking = true;
    KwaiQos_setBlockInfo(ffp);
}

void KwaiQos_onBufferingEnd(FFPlayer* ffp, int is_block) {
    KwaiQos* qos = &ffp->kwai_qos;
    if (!is_block)
        return;
    TimeRecorder_end(&qos->runtime_stat.block_duration);
    qos->runtime_stat.is_blocking = false;
    KwaiQos_setBlockInfo(ffp);
}

int64_t KwaiQos_getBufferTotalDurationMs(KwaiQos* qos) {
    return TimeRecoder_get_total_duration_ms(&qos->runtime_stat.block_duration);
}

void KwaiQos_onSeekStart(KwaiQos* qos) {
    TimeRecoder_start(&qos->seek_stat.seek_duration);
    qos->seek_stat.seek_cnt++;
}

void KwaiQos_onSeekEnd(KwaiQos* qos) {
    TimeRecorder_end(&qos->seek_stat.seek_duration);
}

void KwaiQos_onFirstFrameAfterSeekStart(KwaiQos* qos) {
    TimeRecoder_start(&qos->seek_stat.first_frame_after_seek);
}

void KwaiQos_onFirstFrameAfterSeekEnd(KwaiQos* qos) {
    TimeRecorder_end(&qos->seek_stat.first_frame_after_seek);
    qos->seek_stat.seek_first_frame_cnt++;
}

void KwaiQos_onFirstPacketAfterSeekStart(KwaiQos* qos) {
    TimeRecoder_start(&qos->seek_stat.first_packet_after_seek);
}

void KwaiQos_onFirstPacketAfterSeekEnd(KwaiQos* qos) {
    TimeRecorder_end(&qos->seek_stat.first_packet_after_seek);
    qos->seek_stat.seek_first_packet_cnt++;
}


int64_t KwaiQos_getSeekAvgDurationMs(KwaiQos* qos) {
    if (qos->seek_stat.seek_cnt) {
        return TimeRecoder_get_total_duration_ms(&qos->seek_stat.seek_duration) / qos->seek_stat.seek_cnt;
    } else {
        return -1;
    }
}

int64_t KwaiQos_getFirstFrameAvgDurationAfterSeekMs(KwaiQos* qos) {
    if (qos->seek_stat.seek_first_frame_cnt > 0)
        return TimeRecoder_get_total_duration_ms(&qos->seek_stat.first_frame_after_seek) / (qos->seek_stat.seek_first_frame_cnt);
    else
        return -1;
}

int64_t KwaiQos_getFirstPacketAvgDurationAfterSeekMs(KwaiQos* qos) {
    if (qos->seek_stat.seek_first_packet_cnt > 0)
        return TimeRecoder_get_total_duration_ms(&qos->seek_stat.first_packet_after_seek) / (qos->seek_stat.seek_first_packet_cnt);
    else
        return -1;
}

void KwaiQos_setAudioFirstDts(KwaiQos* qos, int64_t value) {
    qos->media_metadata.a_first_pkg_dts = value;
}

void KwaiQos_setVideoFirstDts(KwaiQos* qos, int64_t value) {
    qos->media_metadata.v_first_pkg_dts = value;
}

void KwaiQos_onStartAlivePlayer(KwaiQos* qos) {
    TimeRecoder_start(&qos->runtime_stat.alive_duration);
}

void KwaiQos_onStopAlivePlayer(KwaiQos* qos) {
    TimeRecorder_end(&qos->runtime_stat.alive_duration);
}

int64_t KwaiQos_getAlivePlayerTotalDurationMs(KwaiQos* qos) {
    return TimeRecoder_get_total_duration_ms(&qos->runtime_stat.alive_duration);
}

void KwaiQos_onAppStartPlayer(FFPlayer* ffp) {
    KwaiQos* qos = &ffp->kwai_qos;
    TimeRecoder_start(&qos->runtime_stat.app_played_duration);
    if (qos->runtime_stat.is_blocking) {
        KwaiQos_onBufferingStart(ffp, 1);
    }
}

void KwaiQos_onAppPausePlayer(FFPlayer* ffp) {
    KwaiQos* qos = &ffp->kwai_qos;
    TimeRecorder_end(&qos->runtime_stat.app_played_duration);
    if (qos->runtime_stat.is_blocking) {
        KwaiQos_onBufferingEnd(ffp, 1);
        qos->runtime_stat.is_blocking = true;
    }
}

int64_t KwaiQos_getAppPlayTotalDurationMs(KwaiQos* qos) {
    return TimeRecoder_get_total_duration_ms(&qos->runtime_stat.app_played_duration);
}

void KwaiQos_onStartPlayer(KwaiQos* qos) {
    TimeRecoder_start(&qos->runtime_stat.actual_played_duration);
}

void KwaiQos_onPausePlayer(KwaiQos* qos) {
    TimeRecorder_end(&qos->runtime_stat.actual_played_duration);
}

int64_t KwaiQos_getActualPlayedTotalDurationMs(KwaiQos* qos) {
    return TimeRecoder_get_total_duration_ms(&qos->runtime_stat.actual_played_duration);
}

void KwaiQos_onAppCallStart(KwaiQos* qos, FFPlayer* ffp) {
    // 这段代码是统计开播前暂停的
    if (qos->read_thread_for_loop_started
        && (qos->runtime_stat.render_frame_count <= 0 && qos->runtime_stat.render_sample_count <= 0)
        && qos->runtime_stat.is_pause_at_first_screen) {
        TimeRecorder_end(&qos->runtime_stat.pause_at_first_screen_duration);
        qos->runtime_stat.is_pause_at_first_screen = false;
    }

    // 统计wait for play cost的
    if (qos->runtime_cost.cost_wait_for_playing < 0) {
        int64_t cur_ms = get_current_time_ms();
        if (ffp->start_on_prepared) {
            // do nothing on start_on_prepare stituation
        } else {
            if (ffp_is_pre_demux_enabled(ffp)) {
                if (qos->ts_predmux_finish > 0) {
                    // 说明predemux结束了，等了app一段时间
                    qos->runtime_cost.cost_wait_for_playing = cur_ms - ffp->pre_demux->ts_end_ms;
                    ALOGD("[%s] qos->runtime_cost.cost_wait_for_playing:%lld", __func__, qos->runtime_cost.cost_wait_for_playing);
                } else {
                    qos->runtime_cost.cost_wait_for_playing = 0;
                }
            } else {
                // pre_decode situation
                if (qos->ts_first_render_ready > 0 && qos->ts_first_frame_filled > 0) {
                    qos->runtime_cost.cost_wait_for_playing =
                        cur_ms - FFMAX(qos->ts_first_render_ready, qos->ts_first_frame_filled);
                } else {
                    qos->runtime_cost.cost_wait_for_playing = 0;
                }
            }
        }
    }
}

void KwaiQos_onAppCallPause(KwaiQos* qos) {
    if (qos->read_thread_for_loop_started
        && (qos->runtime_stat.render_frame_count <= 0 && qos->runtime_stat.render_sample_count <= 0)
        && !qos->runtime_stat.is_pause_at_first_screen) {
        TimeRecoder_start(&qos->runtime_stat.pause_at_first_screen_duration);
        qos->runtime_stat.is_pause_at_first_screen = true;
    }
}

int64_t KwaiQos_getPauseDurationAtFirstScreen(KwaiQos* qos) {
    return qos->runtime_stat.pause_at_first_screen_duration.sum_ms;
}

void KwaiQos_onPlayToEnd(KwaiQos* qos) {
    qos->runtime_stat.loop_cnt++;
}

int64_t KwaiQos_getActualPlayTotalDurationMs(KwaiQos* qos) {
    double played_duration = qos->runtime_stat.v_played_duration > 0 ? qos->runtime_stat.v_played_duration : qos->runtime_stat.a_played_duration;
    return (int64_t)(1000 * played_duration);
}

int64_t KwaiQos_getActualAudioPlayTotalDurationMs(KwaiQos* qos) {
    return (int64_t)(1000 * qos->runtime_stat.a_played_duration);
}

int64_t KwaiQos_getFirstScreenCostMs(KwaiQos* qos) {
    return qos->runtime_cost.cost_first_screen >= 0 ?
           qos->runtime_cost.cost_first_screen : qos->runtime_cost.cost_first_sample;
}

int64_t KwaiQos_getTotalFirstScreenCostMs(KwaiQos* qos) {
    return qos->runtime_cost.cost_total_first_screen > 0 ?
           qos->runtime_cost.cost_total_first_screen : qos->runtime_cost.cost_total_first_sample;
}

void KwaiQos_setDomain(KwaiQos* qos, const char* domain) {
    if (domain != NULL) {
        if (qos->player_config.domain != NULL) {
            av_freep(&qos->player_config.domain);
        }
        qos->player_config.domain = av_strdup(domain);
    }
}

char* KwaiQos_getDomain(KwaiQos* qos) {
    return qos->player_config.domain;
}

void KwaiQos_setStreamId(KwaiQos* qos, const char* stream_id) {
    if (stream_id != NULL) {
        if (qos->player_config.stream_id != NULL) {
            av_freep(&qos->player_config.stream_id);
        }
        qos->player_config.stream_id = av_strdup(stream_id);
    }
}

char* KwaiQos_getStreamId(KwaiQos* qos) {
    return qos->player_config.stream_id;
}

void KwaiQos_setLastTryFlag(KwaiQos* qos, int is_last_try) {
    qos->player_config.is_last_try = is_last_try;
}

void KwaiQos_setDnsAnalyzeCostMs(KwaiQos* qos, int value) {
    qos->runtime_cost.cost_dns_analyze = value;
}

void KwaiQos_updateConnectInfo(KwaiQos* qos, AwesomeCacheRuntimeInfo* info) {
    qos->runtime_cost.cost_http_connect = info->connect_infos[0].http_connect_ms;
    qos->runtime_cost.cost_dns_analyze = info->connect_infos[0].http_dns_analyze_ms;
    qos->runtime_cost.cost_http_first_data = info->connect_infos[0].http_first_data_ms;

    for (int i = 0; i < CONNECT_INFO_COUNT; i++) {
        qos->runtime_cost.connect_infos[i].cost_http_connect = info->connect_infos[i].http_connect_ms;
        qos->runtime_cost.connect_infos[i].cost_dns_analyze = info->connect_infos[i].http_connect_ms;
        qos->runtime_cost.connect_infos[i].cost_dns_analyze = info->connect_infos[i].http_dns_analyze_ms;
        qos->runtime_cost.connect_infos[i].cost_http_first_data = info->connect_infos[i].http_first_data_ms;
        if (i > 0) {
            qos->runtime_cost.connect_infos[i].first_data_interval = info->connect_infos[i].first_data_ts - info->connect_infos[i - 1].first_data_ts;
        } else {
            qos->runtime_cost.connect_infos[i].first_data_interval = 0;
        }
    }

}

void KwaiQos_setHttpConnectCostMs(KwaiQos* qos, int value) {
    qos->runtime_cost.cost_http_connect = value;
}

void KwaiQos_setHttpFirstDataCostMs(KwaiQos* qos, int value) {
    qos->runtime_cost.cost_http_first_data = value;
}

void KwaiQos_setSessionUUID(KwaiQos* qos, const char* uuid) {
    snprintf(qos->runtime_stat.session_uuid, SESSION_UUID_BUFFER_LEN, "%s", uuid);
}

void KwaiQos_onFFPlayerOpenInputOver(KwaiQos* qos, int setup_err,
                                     bool cache_global_enabled,
                                     bool cache_used,
                                     bool url_is_cache_whitelist) {
    qos->runtime_stat.open_input_error = setup_err;
    qos->runtime_stat.cache_global_enabled = cache_global_enabled;
    qos->runtime_stat.cache_used = cache_used;
    qos->runtime_stat.url_in_cache_whitelist = url_is_cache_whitelist;
}


void KwaiQos_setAwesomeCacheIsFullyCachedOnOpen(KwaiQos* qos, bool cached) {
    qos->ac_cache.is_fully_cached_on_open = cached;
}

void KwaiQos_setAwesomeCacheIsFullyCachedOnLoop(KwaiQos* qos, bool cached) {
    if (cached) {
        qos->ac_cache.is_fully_cached_cnt_on_loop++;
    } else {
        qos->ac_cache.is_not_fully_cached_cnt_on_loop++;
    }
}

void KwaiQos_setBlockInfoStartPeriod(KwaiQos* qos) {
    if (!qos->is_block_start_period_set && KwaiQos_getActualPlayTotalDurationMs(qos) >= KWAI_CALC_TIME_OF_FST_BLOCK_MS) {
        qos->is_block_start_period_set = true;
        qos->runtime_stat.block_cnt_start_period = qos->runtime_stat.block_cnt;
        qos->runtime_stat.block_duration_start_period = KwaiQos_getBufferTotalDurationMs(qos);
    }
}

void KwaiQos_setBlockInfoStartPeriodIfNeed(KwaiQos* qos) {
    if (KwaiQos_getActualPlayTotalDurationMs(qos) <= KWAI_CALC_TIME_OF_FST_BLOCK_MS) {
        qos->runtime_stat.block_cnt_start_period = qos->runtime_stat.block_cnt;
        qos->runtime_stat.block_duration_start_period = KwaiQos_getBufferTotalDurationMs(qos);
    }
}

void KwaiQos_setAudioStr(KwaiQos* qos, char* audio_str) {
    if (qos->audio_str) {
        av_freep(&qos->audio_str);
    }
    qos->audio_str = audio_str ? av_strdup(audio_str) : NULL;
}

void KwaiQos_setEnableAudioGain(KwaiQos* qos, int enable_audio_gain) {
    qos->enable_audio_gain = enable_audio_gain;
}

void KwaiQos_setEnableModifyBlock(KwaiQos* qos, int enable_modify_block) {
    qos->enable_modify_block = enable_modify_block;
}

void KwaiQos_setAudioProcessCost(KwaiQos* qos, int64_t cost) {
    if (cost > qos->audio_process_cost) {
        qos->audio_process_cost = cost;
    }
}

void KwaiQos_collectAudioTrackInfo(KwaiQos* qos, FFPlayer* ffp) {
    if (ffp->aout) {
        qos->audio_track_write_error_count = ffp->aout->qos.audio_track_write_error_count;
    }
}


void KwaiQos_onSystemPerformance(KwaiQos* qos) {
    int64_t now = get_current_time_ms();
    if (now - qos->system_performance.last_sample_ts_ms < KWAI_MIN_SYSTEM_PERFORMANCE_STAT_INTERVAL) {
        return;
    }
//#ifdef __APPLE__
    qos->system_performance.sample_cnt++;
    qos->system_performance.last_sample_ts_ms = now;

    qos->system_performance.last_process_cpu = get_process_cpu_usage();
    qos->system_performance.process_cpu_pct += qos->system_performance.last_process_cpu;

    qos->system_performance.last_process_memory_size_kb = get_process_memory_size_kb();
    qos->system_performance.process_memory_size_kb += qos->system_performance.last_process_memory_size_kb;

    qos->system_performance.process_cpu_cnt = get_process_cpu_num();

    qos->system_performance.last_system_cpu = get_system_cpu_usage();
//#elif defined(__ANDROID__)
//    qos->system_performance.sample_cnt++;
//    qos->system_performance.last_sample_ts_ms = now;
//
//    profiler_key_result*  prof_ret = update_and_get_current_key_result();
//
//    qos->system_performance.last_process_cpu = (uint32_t) prof_ret->this_process_cpu_percent;
//    qos->system_performance.process_cpu_pct += qos->system_performance.last_process_cpu;
//
//    qos->system_performance.last_process_memory_size_kb = (uint32_t)(prof_ret->rss_mb * 1024);
//    qos->system_performance.process_memory_size_kb += qos->system_performance.last_process_memory_size_kb;
//
//    qos->system_performance.process_cpu_cnt = (uint32_t) prof_ret->cpu_alive_cnt;
//    qos->system_performance.device_cpu_cnt_total = (uint32_t) prof_ret->cpu_total_cnt;
//
//
//    int64_t diff = get_current_time_ms() - now;
//    qos->system_performance.total_prof_cost_ms += (diff >= 0 ? diff : 0);
//
//    // 这段日志别删，调试用
////    ALOGI("[%s] cpu:%u/%.1f, cpu_cnt:%u/%d(total)/%d(alive), memory_kb:%u/%ld(rss)/%ld(vss)", __func__,
////          qos->system_performance.last_process_cpu, prof_ret->this_process_cpu_percent,
////          qos->system_performance.process_cpu_cnt, prof_ret->cpu_total_cnt, prof_ret->cpu_alive_cnt,
////          qos->system_performance.last_process_memory_size_kb, prof_ret->rss_mb, prof_ret->vss_mb);
//#endif
}

void KwaiQos_onDisplayError(KwaiQos* qos, int32_t error) {
    switch (error) {
        case -22:
            qos->runtime_stat.v_error_native_windows_lock++;
            break;
        default:
            if (error < -1) {
                // 这里只统计小于-1的error信息。原因如下：
                // 1. 正常退出的时候，SDL_VoutDisplayYUVOverlay也会返回-1，而实际情况是没有出错，
                //    所以统计-1进来会导致统计结果不准确。
                // 2. 具体的错误原因一般都是小于-1的值，这里记录下来，以便根据统计的错误类型，增加上面的case选项。
                qos->runtime_stat.v_error_unknown = error;
            }
            break;
    }
}

void KwaiQos_setResolution(KwaiQos* qos, int width, int height) {
    qos->media_metadata.width = width;
    qos->media_metadata.height = height;
}

void KwaiQos_setMaxAvDiff(KwaiQos* qos, int max_av_diff) {
    qos->runtime_stat.max_av_diff = max_av_diff;
}

void KwaiQos_setMinAvDiff(KwaiQos* qos, int min_av_diff) {
    qos->runtime_stat.min_av_diff = min_av_diff;
}

void KwaiQos_onError(KwaiQos* qos, int error) {
    qos->runtime_stat.last_error = error;
}

void KwaiQos_onSoftDecodeErr(KwaiQos* qos, int value) {
    qos->runtime_stat.v_sw_dec_err_cnt = value;
}

void KwaiQos_onHevcParameterSetLenChange(KwaiQos* qos, uint8_t* ps) {
    qos->runtime_stat.v_hevc_paramete_set_change_cnt++;
    if (!ps) {
        qos->runtime_stat.v_hevc_paramete_set_update_fail_cnt++;
    }
}

#if defined(__APPLE__)
void KwaiQos_onToolBoxDecodeErr(KwaiQos* qos) {
    qos->runtime_stat.v_tool_box_err_cnt++;
}
#elif defined(__ANDROID__)
void KwaiQos_onMediaCodecDequeueInputBufferErr(KwaiQos* qos, int err) {
    qos->runtime_stat.v_mediacodec_input_err_cnt++;
    qos->runtime_stat.v_mediacodec_input_err_code = err;
}

void KwaiQos_onMediaCodecDequeueOutputBufferErr(KwaiQos* qos, int err) {
    if (err == MEDIACODEC_OUTPUT_ERROR_TRY_AGAIN_LATER) {
        qos->runtime_stat.v_mediacodec_output_try_again_err_cnt++;
    } else if (err == MEDIACODEC_OUTPUT_ERROR_BUFFERS_CHANGED) {
        qos->runtime_stat.v_mediacodec_output_buffer_changed_err_cnt++;
    } else if (err == MEDIACODEC_OUTPUT_ERROR_UNKNOWN) {
        qos->runtime_stat.v_mediacodec_output_unknown_err_cnt++;
    } else {
        qos->runtime_stat.v_mediacodec_output_err_cnt++;
        qos->runtime_stat.v_mediacodec_output_err_code = err;
    }
}

void KwaiQos_onMediacodecType(KwaiQos* qos, FFPlayer* ffp) {
    if (!ffp || !qos) {
        return;
    }
    if (ffp->mediacodec_all_videos || (ffp->mediacodec_hevc && ffp->mediacodec_avc)) {
        snprintf(qos->runtime_stat.v_mediacodec_config_type, MEDIACODEC_CONFIG_TYPE_MAX_LEN,
                 "%s",  "h264h265");
    } else if (ffp->mediacodec_avc) {
        snprintf(qos->runtime_stat.v_mediacodec_config_type, MEDIACODEC_CONFIG_TYPE_MAX_LEN,
                 "%s",  "h264");
    } else if (ffp->mediacodec_hevc) {
        snprintf(qos->runtime_stat.v_mediacodec_config_type, MEDIACODEC_CONFIG_TYPE_MAX_LEN,
                 "%s",  "h265");
    }

    qos->runtime_stat.v_mediacodec_codec_max_cnt = ffp->mediacodec_max_cnt;
}
#endif

void KwaiQos_onBitrate(KwaiQos* qos, int value) {
    qos->media_metadata.bitrate = value;
}

void KwaiQos_setAudioBitRate(KwaiQos* qos, int value) {
    qos->media_metadata.audio_bit_rate = value;
}

void KwaiQos_setAudioProfile(KwaiQos* qos, const char* profile) {
    if (qos->media_metadata.audio_profile) {
        av_freep(&qos->media_metadata.audio_profile);
    }
    qos->media_metadata.audio_profile = av_strdup(profile);
}

void KwaiQos_setVideoFramePixelInfo(KwaiQos* qos, enum AVColorSpace color_space, enum AVPixelFormat pixel_fmt) {
    qos->media_metadata.color_space = color_space;
    qos->runtime_stat.pix_format = pixel_fmt;
}

void KwaiQos_setOverlayOutputFormat(KwaiQos* qos, uint32_t format) {
    qos->player_config.overlay_format = format;
}

void KwaiQos_onHardwareDec(KwaiQos* qos, bool value) {
    qos->runtime_stat.v_hw_dec = value;
}

void KwaiQos_onDecodedDroppedFrame(KwaiQos* qos, int value) {
    qos->runtime_stat.v_decoded_dropped_frame = value;
}

void KwaiQos_onRenderDroppedFrame(KwaiQos* qos, int value) {
    qos->runtime_stat.v_render_dropped_frame = value;
}

void KwaiQos_onAudioPtsJumpForward(KwaiQos* qos, int64_t time_gap) {
    qos->runtime_stat.audio_pts_jump_forward_cnt++;
    qos->runtime_stat.audio_pts_jump_forward_duration += time_gap;
}

void KwaiQos_onAudioPtsJumpBackward(KwaiQos* qos, int64_t time_gap) {
    qos->runtime_stat.audio_pts_jump_backward_index++;
    qos->runtime_stat.audio_pts_jump_backward_time_gap = time_gap;
}

void KwaiQos_onVideoTimestampRollback(KwaiQos* qos, int64_t time_gap) {
    qos->runtime_stat.video_ts_rollback_cnt++;
    qos->runtime_stat.video_ts_rollback_duration += time_gap;
}

void KwaiQos_setVideoCodecInfo(KwaiQos* qos, const char* info) {
    if (qos->media_metadata.video_codec_info) {
        av_freep(&qos->media_metadata.video_codec_info);
    }
    qos->media_metadata.video_codec_info = av_strdup(info);
}

void KwaiQos_setAudioCodecInfo(KwaiQos* qos, const char* info) {
    if (qos->media_metadata.audio_codec_info) {
        av_freep(&qos->media_metadata.audio_codec_info);
    }
    qos->media_metadata.audio_codec_info = av_strdup(info);
}

void KwaiQos_setStreamInfo(KwaiQos* qos, int audio_stream_count, int video_stream_count) {
    if (qos->media_metadata.stream_info) {
        av_freep(&qos->media_metadata.stream_info);
    }
    if (audio_stream_count <= 0 && video_stream_count <= 0) {
        return;
    }
    //对应流信息 表示为 Audio、Video、AudioVideo
    qos->media_metadata.stream_info = av_asprintf("%s%s", audio_stream_count > 0 ? "Audio" : "", video_stream_count > 0 ? "Video" : "");
}

void KwaiQos_setInputFormat(KwaiQos* qos, const char* input_format) {
    if (qos->media_metadata.input_fomat) {
        av_freep(&qos->media_metadata.input_fomat);
    }
    qos->media_metadata.input_fomat = input_format ? av_strdup(input_format) : NULL;
}

void KwaiQos_setAudioDeviceLatencyMs(KwaiQos* qos, int value) {
    qos->runtime_stat.audio_device_latency = value;
}

void KwaiQos_setAudioDeviceAppliedLatencyMs(KwaiQos* qos, int value) {
    qos->runtime_stat.audio_device_applied_latency = value;
}

void KwaiQos_setOpenInputReadBytes(KwaiQos* qos, int64_t bytes) {
    qos->data_read.data_after_open_input = bytes;
}
void KwaiQos_setFindStreamInfoReadBytes(KwaiQos* qos, int64_t bytes) {
    qos->data_read.data_after_stream_info = bytes;
}

void KwaiQos_setAudioPktReadBytes(KwaiQos* qos, int64_t bytes) {
    if (qos->data_read.data_fst_audio_pkt <= 0) {
        qos->data_read.data_fst_audio_pkt = bytes;
    }
}

void KwaiQos_setVideoPktReadBytes(KwaiQos* qos, int64_t bytes) {
    if (qos->data_read.data_fst_video_pkt <= 0) {
        qos->data_read.data_fst_video_pkt = bytes;
    }
}

static void parse_transcode_type_for_vod_adaptive(KwaiQos* qos, char* filename) {
    if (!filename || strlen(filename) <= 0) {
        return;
    }
    const char* pattern = "._.*_.";
    regex_t reg;

    int reg_c = regcomp(&reg, pattern, REG_EXTENDED);
    if (reg_c) {
        // error
        ALOGW("[%s] fail to regcomp", __func__);
    } else {
        regmatch_t pmatch[1];
        int ret = regexec(&reg, filename, 1, pmatch, 0);
        if (!ret) {
            // match!
            int64_t match_len = pmatch[0].rm_eo - pmatch[0].rm_so - 1;
            if (match_len > 1 && match_len < TRANSCODE_TYPE_MAX_LEN) {
                snprintf(qos->vod_adaptive.transcode_type, match_len,
                         "%s", filename + pmatch->rm_so + 1);
            } else {
                ALOGW("[%s] match_len [%lld] <= 1 || >= %d, fail to find transcode type with regex for url:%s\n",
                      __func__, match_len, TRANSCODE_TYPE_MAX_LEN, filename);
            }

        } else if (ret == REG_NOMATCH) {
            ALOGW("[%s] fail to find transcode type with regex for url:%s\n", __func__, filename);
        }
    }
    regfree(&reg);
}

/*未被选中的多码率信息以json数组的方式上报*/
static void KwaiQos_createVodAdaptiveRepresentations(KwaiQos* qos, VodPlayList* playlist, int index) {
    SDL_LockMutex(qos->vod_adaptive.mutex);
    if (qos->vod_adaptive.representations) {
        cJSON_Delete(qos->vod_adaptive.representations);
        qos->vod_adaptive.representations = NULL;
    }
    qos->vod_adaptive.representations = cJSON_CreateArray();

    for (int i = 0; i < playlist->rep_count; i++) {
        if (i != index) {
            cJSON* representation = cJSON_CreateObject();
            cJSON_AddItemToArray(qos->vod_adaptive.representations, representation);
            cJSON_AddNumberToObject(representation, "width", playlist->rep[i].video_resolution.width);
            cJSON_AddNumberToObject(representation, "height", playlist->rep[i].video_resolution.height);
            cJSON_AddNumberToObject(representation, "quality", playlist->rep[i].quality);
            cJSON_AddNumberToObject(representation, "avg_bitrate", playlist->rep[i].avg_bitrate_kbps);
            cJSON_AddNumberToObject(representation, "max_bitrate", playlist->rep[i].max_bitrate_kbps);
            memset(qos->vod_adaptive.transcode_type, 0, TRANSCODE_TYPE_MAX_LEN + 1);
            parse_transcode_type_for_vod_adaptive(qos, playlist->rep[i].url);
            cJSON_AddStringToObject(representation, "transcode_type", qos->vod_adaptive.transcode_type);
        }
    }
    SDL_UnlockMutex(qos->vod_adaptive.mutex);
}

static char* KwaiQos_getVodAdaptiveRepresentations(KwaiQos* qos) {
    char* unselected_source_str = NULL;

    SDL_LockMutex(qos->vod_adaptive.mutex);
    if (qos->vod_adaptive.representations) {
        unselected_source_str = cJSON_PrintUnformatted(qos->vod_adaptive.representations);
        cJSON_Delete(qos->vod_adaptive.representations);
        qos->vod_adaptive.representations = NULL;
    }
    SDL_UnlockMutex(qos->vod_adaptive.mutex);

    return unselected_source_str;
}

void KwaiQos_onHwDecodeErrCode(KwaiQos* qos, int err_code) {
    qos->hw_decode.err_code = err_code;
}

void KwaiQos_onHwDecodeResetSession(KwaiQos* qos) {
    qos->hw_decode.reset_session_cnt++;
}

#if defined(__APPLE__)
// video_tool_box
void KwaiQos_onVideoToolBoxMode(KwaiQos* qos, int mode) {
    qos->hw_decode.video_tool_box.enable = true;
    qos->hw_decode.video_tool_box.mode = mode;
}

void KwaiQos_onVideoToolBoxPktCntOnErr(KwaiQos* qos, int count) {
    qos->hw_decode.video_tool_box.pkt_cnt_on_err = count;
}

void KwaiQos_onVideoToolBoxQueueIsFull(KwaiQos* qos) {
    qos->hw_decode.video_tool_box.queue_is_full_cnt++;
}

void KwaiQos_onVideoToolBoxResolutionChange(KwaiQos* qos) {
    qos->hw_decode.video_tool_box.resolution_change++;
}
#endif

void KwaiQos_onSetLiveManifestSwitchMode(KwaiQos* qos, int mode) {
    qos->live_adaptive.cur_switch_mode = mode;
    if (qos->live_adaptive.cur_switch_mode == LIVE_MANIFEST_AUTO)
        qos->live_adaptive.auto_mode_set = true;
    else {
        qos->live_adaptive.mannual_mode_set = true;
    }
}

void KwaiQos_onVodAdaptive(KwaiQos* qos, VodPlayList* playlist, int index,
                           char* vod_resolution, VodRateAdaptConfig* rate_config,
                           VodRateAdaptConfigA1* rate_config_a1) {
    qos->vod_adaptive.is_vod_adaptive = 1;

    //debug info and video state event report
    qos->vod_adaptive.cached = playlist->cached;
    KwaiQos_createVodAdaptiveRepresentations(qos, playlist, index);
    qos->vod_adaptive.max_bitrate_kbps = playlist->rep[index].max_bitrate_kbps;
    qos->vod_adaptive.avg_bitrate_kbps = playlist->rep[index].avg_bitrate_kbps;
    qos->vod_adaptive.width = playlist->rep[index].video_resolution.width;
    qos->vod_adaptive.height = playlist->rep[index].video_resolution.height;
    qos->vod_adaptive.quality = playlist->rep[index].quality;
    qos->vod_adaptive.device_width = playlist->device_resolution.width;
    qos->vod_adaptive.device_height = playlist->device_resolution.height;
    qos->vod_adaptive.low_device = playlist->low_device;
    qos->vod_adaptive.switch_code = playlist->switch_code;
    qos->vod_adaptive.algorithm_mode = playlist->algorithm_mode;
    qos->vod_adaptive.idle_last_request_ms = c_abr_get_idle_last_request_time();
    qos->vod_adaptive.short_throughput_kbps = c_abr_get_short_throughput_kbps(playlist->algorithm_mode);
    qos->vod_adaptive.long_throughput_kbps = c_abr_get_long_throughput_kbps(playlist->algorithm_mode);

    char net_type[MAX_LEN_NET_TYPE + 1] = {0};
    switch (playlist->net_type) {
        case WIFI:
            strncpy(net_type, "WIFI", MAX_LEN_NET_TYPE);
            break;
        case FOUR_G:
            strncpy(net_type, "FOUR_G", MAX_LEN_NET_TYPE);
            break;
        case THREE_G:
            strncpy(net_type, "THREE_G", MAX_LEN_NET_TYPE);
            break;
        case TWO_G:
            strncpy(net_type, "TWO_G", MAX_LEN_NET_TYPE);
            break;
        case UNKNOW:
        default:
            strncpy(net_type, "UNKNOW", MAX_LEN_NET_TYPE);
            break;
    }
    if (qos->vod_adaptive.net_type) {
        av_freep(&qos->vod_adaptive.net_type);
    }
    qos->vod_adaptive.net_type = av_strdup(net_type);

    char switch_reason[MAX_LEN_SWITCH_REASON + 1] = {0};
    char bandwidth_computer_process[MAX_LEN_BANDWIDTH_COMPUTER_PROCESS + 1] = {0};
    c_abr_get_switch_reason(switch_reason, MAX_LEN_SWITCH_REASON);
    char* ptr = strchr(switch_reason, ';');
    if (ptr) {
        *ptr = 0;
        ptr++;
        if (ptr) {
            strncpy(bandwidth_computer_process, ptr, MAX_LEN_BANDWIDTH_COMPUTER_PROCESS);
        }
    }
    if (qos->vod_adaptive.switch_reason) {
        av_freep(&qos->vod_adaptive.switch_reason);
    }
    qos->vod_adaptive.switch_reason = av_strdup(switch_reason);

    // only for debug info
    if (qos->vod_adaptive.detail_switch_reason) {
        av_freep(&qos->vod_adaptive.detail_switch_reason);
    }
    qos->vod_adaptive.detail_switch_reason = av_strdup(c_abr_get_detail_switch_reason());

    if (qos->vod_adaptive.cur_url) {
        av_freep(&qos->vod_adaptive.cur_url);
    }
    qos->vod_adaptive.cur_url = av_strdup(playlist->rep[index].url);

    if (qos->vod_adaptive.cur_host) {
        av_freep(&qos->vod_adaptive.cur_host);
    }
    qos->vod_adaptive.cur_host = av_strdup(playlist->rep[index].host);

    if (qos->vod_adaptive.cur_key) {
        av_freep(&qos->vod_adaptive.cur_key);
    }
    qos->vod_adaptive.cur_key = av_strdup(playlist->rep[index].key);

    if (qos->vod_adaptive.cur_quality_show) {
        av_freep(&qos->vod_adaptive.cur_quality_show);
    }
    qos->vod_adaptive.cur_quality_show = av_strdup(playlist->rep[index].quality_show);

    if (qos->vod_adaptive.bandwidth_computer_process) {
        av_freep(&qos->vod_adaptive.bandwidth_computer_process);
    }
    qos->vod_adaptive.bandwidth_computer_process = av_strdup(bandwidth_computer_process);

    if (qos->vod_adaptive.representations_str) {
        av_freep(&qos->vod_adaptive.representations_str);
    }
    qos->vod_adaptive.representations_str = KwaiQos_getVodAdaptiveRepresentations(qos);

    //多码率初始化参数
    qos->vod_adaptive.rate_addapt_type = rate_config->rate_addapt_type;
    qos->vod_adaptive.bandwidth_estimation_type = rate_config->bandwidth_estimation_type;
    qos->vod_adaptive.absolute_low_res_low_device = rate_config->absolute_low_res_low_device;
    qos->vod_adaptive.adapt_under_4G = rate_config->adapt_under_4G;
    qos->vod_adaptive.adapt_under_wifi = rate_config->adapt_under_wifi;
    qos->vod_adaptive.adapt_under_other_net = rate_config->adapt_under_other_net;
    qos->vod_adaptive.absolute_low_rate_4G = rate_config->absolute_low_rate_4G;
    qos->vod_adaptive.absolute_low_rate_wifi = rate_config->absolute_low_rate_wifi;
    qos->vod_adaptive.absolute_low_res_4G = rate_config->absolute_low_res_4G;
    qos->vod_adaptive.absolute_low_res_wifi = rate_config->absolute_low_res_wifi;
    qos->vod_adaptive.short_keep_interval = rate_config->short_keep_interval;
    qos->vod_adaptive.long_keep_interval = rate_config->long_keep_interval;
    qos->vod_adaptive.bitrate_init_level = rate_config->bitrate_init_level;
    qos->vod_adaptive.default_weight = rate_config->default_weight;
    qos->vod_adaptive.block_affected_interval = rate_config->block_affected_interval;
    qos->vod_adaptive.wifi_amend = rate_config->wifi_amend;
    qos->vod_adaptive.fourG_amend = rate_config->fourG_amend;
    qos->vod_adaptive.resolution_amend = rate_config->resolution_amend;
    qos->vod_adaptive.device_width_threshold = rate_config->device_width_threshold;
    qos->vod_adaptive.device_hight_threshold = rate_config->device_hight_threshold;
    qos->vod_adaptive.priority_policy = rate_config->priority_policy;

    if (playlist->algorithm_mode) {
        qos->vod_adaptive.bitrate_init_level = rate_config_a1->bitrate_init_level;
        qos->vod_adaptive.long_keep_interval = rate_config_a1->long_keep_interval;
        qos->vod_adaptive.short_keep_interval = rate_config_a1->short_keep_interval;
        qos->vod_adaptive.max_resolution = rate_config_a1->max_resolution;
    }
}


/**
 * 扩展这个函数的时候要注意：必须保证不能有block可能性
 */
void KwaiQos_collectAwesomeCacheInfoOnce(KwaiQos* qos, FFPlayer* ffp) {
    if (ffp->cache_actually_used) {
        if (!qos->ac_cache_collected) {
            AwesomeCacheRuntimeInfo* ac_rt_info = &ffp->cache_stat.ac_runtime_info;
            FfmpegAdapterQos* ffmpeg_adapter_qos = &ffp->cache_stat.ffmpeg_adapter_qos;

            // config
            snprintf(qos->ac_cache.cfg_cache_key, CACHE_KEY_MAX_LEN, "%s",
                     ffp->cache_key ? ffp->cache_key : "N/A");
            qos->ac_cache.data_source_type = ac_rt_info->cache_applied_config.data_source_type;
            qos->ac_cache.upstream_type = ac_rt_info->cache_applied_config.upstream_type;
            qos->ac_cache.buffered_type = ac_rt_info->cache_applied_config.buffered_type;

            qos->ac_cache.fs_error_code = ac_rt_info->sink.fs_error_code;

            // for general
            qos->ac_cache.reopen_cnt = ac_rt_info->buffer_ds.reopen_cnt_by_seek;
            qos->ac_cache.ignore_cache_on_error = ac_rt_info->cache_ds.ignore_cache_on_error;
            qos->ac_cache.total_bytes = (int)ac_rt_info->cache_ds.total_bytes;
            qos->ac_cache.cached_bytes = (int)(ac_rt_info->cache_ds.cached_bytes + ac_rt_info->sink.bytes_not_commited);

            // for ffmpeg adatper
            qos->ac_cache.adapter_error = ffmpeg_adapter_qos->adapter_error;
            qos->ac_cache.read_cost_ms = ffmpeg_adapter_qos->read_cost_ms;
            qos->ac_cache.seek_size_cnt = ffmpeg_adapter_qos->seek_size_cnt;
            qos->ac_cache.seek_set_cnt = ffmpeg_adapter_qos->seek_set_cnt;
            qos->ac_cache.seek_cur_cnt = ffmpeg_adapter_qos->seek_cur_cnt;
            qos->ac_cache.seek_end_cnt = ffmpeg_adapter_qos->seek_end_cnt;

            // for BufferedDataSource
            qos->ac_cache.buffered_datasource_size_kb = ac_rt_info->buffer_ds.buffered_datasource_size_kb;
            qos->ac_cache.buffered_datasource_seek_threshold_kb = ac_rt_info->buffer_ds.buffered_datasource_seek_threshold_kb;


            // for AsyncV2
            qos->ac_cache.curl_ret = ac_rt_info->download_task.curl_ret;
            qos->ac_cache.http_response_code = ac_rt_info->download_task.http_response_code;
            qos->ac_cache.need_report_header = ac_rt_info->download_task.need_report_header;
            snprintf(qos->ac_cache.invalid_header, INVALID_RESPONSE_HEADER, "%s", ac_rt_info->download_task.invalid_header);
            qos->ac_cache.downloaded_bytes = ac_rt_info->download_task.downloaded_bytes;
            qos->ac_cache.recv_valid_bytes = ac_rt_info->download_task.recv_valid_bytes;
            qos->ac_cache.resume_file_fail_cnt = ac_rt_info->cache_v2_info.resume_file_fail_cnt;
            qos->ac_cache.flush_file_fail_cnt = ac_rt_info->cache_v2_info.flush_file_fail_cnt;
            qos->ac_cache.cached_bytes_on_play_start = ac_rt_info->cache_v2_info.cached_bytes_on_play_start;

            // for CacheDataSource
            qos->runtime_stat.setup_cache_error = ac_rt_info->cache_ds.first_open_error; // 历史原因，暂时仍保留放在runtime_stat里
            qos->ac_cache.cache_read_source_cnt = ac_rt_info->cache_ds.cache_read_source_cnt;
            qos->ac_cache.cache_write_source_cnt = ac_rt_info->cache_ds.cache_write_source_cnt;
            qos->ac_cache.cache_upstream_source_cnt = ac_rt_info->cache_ds.cache_upstream_source_cnt;
            qos->ac_cache.byte_range_size = ac_rt_info->cache_ds.byte_range_size;
            qos->ac_cache.first_byte_range_length = ac_rt_info->cache_ds.first_byte_range_length;
            qos->ac_cache.download_exit_reason = ac_rt_info->cache_ds.download_exit_reason;
            qos->ac_cache.read_from_upstream = ac_rt_info->cache_ds.read_from_upstream;
            qos->ac_cache.read_position = ac_rt_info->cache_ds.read_position;
            qos->ac_cache.bytes_remaining = ac_rt_info->cache_ds.bytes_remaining;
            qos->ac_cache.pre_download_cnt = ac_rt_info->cache_ds.pre_download_cnt;

            // for HttpDataSource
            qos->ac_cache.http_retried_cnt = ac_rt_info->http_ds.http_retried_cnt;

            // for DownloadTask
            qos->ac_cache.con_timeout_ms = ac_rt_info->download_task.con_timeout_ms;
            qos->ac_cache.read_timeout_ms = ac_rt_info->download_task.read_timeout_ms;
            qos->ac_cache.download_total_cost_ms = ac_rt_info->download_task.download_total_cost_ms;
            qos->ac_cache.download_feed_input_cost_ms = ac_rt_info->download_task.feed_data_consume_ms_;
            qos->ac_cache.stop_reason = ac_rt_info->download_task.stop_reason;
            // qos->ac_cache.drop_data_cnt = ac_rt_info->download_task.download_total_drop_cnt; // fixme 暂时不支持
            qos->ac_cache.drop_data_bytes = ac_rt_info->download_task.download_total_drop_bytes;
            qos->ac_cache.curl_buffer_size_kb = ac_rt_info->download_task.curl_buffer_size_kb;
            qos->ac_cache.curl_buffer_max_used_kb = ac_rt_info->download_task.curl_buffer_max_used_kb;
            qos->ac_cache.curl_byte_range_error = ac_rt_info->download_task.curl_byte_range_error;

            qos->ac_cache.sock_orig_size_kb = ac_rt_info->download_task.sock_orig_size_kb;
            qos->ac_cache.sock_cfg_size_kb = ac_rt_info->download_task.sock_cfg_size_kb;
            qos->ac_cache.sock_act_size_kb = ac_rt_info->download_task.sock_act_size_kb;

            qos->ac_cache.os_errno = ac_rt_info->download_task.os_errno;

            // for vod p2sp
            qos->ac_cache.p2sp_enabled = ac_rt_info->vod_p2sp.enabled;
            qos->ac_cache.p2sp_cdn_bytes = ac_rt_info->vod_p2sp.cdn_bytes;
            qos->ac_cache.p2sp_bytes_used = ac_rt_info->vod_p2sp.p2sp_bytes_used;
            qos->ac_cache.p2sp_bytes_repeated = ac_rt_info->vod_p2sp.p2sp_bytes_repeated;
            qos->ac_cache.p2sp_bytes_received = ac_rt_info->vod_p2sp.p2sp_bytes_received;
            qos->ac_cache.p2sp_bytes_requested = ac_rt_info->vod_p2sp.p2sp_bytes_requested;
            qos->ac_cache.p2sp_start = ac_rt_info->vod_p2sp.p2sp_start;
            qos->ac_cache.p2sp_error_code = ac_rt_info->vod_p2sp.p2sp_error_code;
            qos->ac_cache.p2sp_first_byte_duration = ac_rt_info->vod_p2sp.p2sp_first_byte_duration;
            qos->ac_cache.p2sp_first_byte_offset = ac_rt_info->vod_p2sp.p2sp_first_byte_offset;
            qos->ac_cache.p2sp_sdk_details = av_strdup(ac_rt_info->vod_p2sp.sdk_details);

            strncpy(qos->ac_cache.http_version, ac_rt_info->download_task.http_version, HTTP_VERSION_MAX_LEN);

            if (qos->runtime_stat.last_error != 0
                || qos->ac_cache.adapter_error != 0
                || qos->runtime_stat.open_input_error != 0) {
                //  FIXME 暂时不开启这块逻辑，目前DataSource_StatsJsonString的实现是线程不安全的，满足不了不block的需求，后续重构
                // 只在出错的时候上报，非耗时操作
//            qos->ac_cache.stats_json_str = AwesomeCache_AVIOContext_get_DataSource_StatsJsonString(ffp->cache_avio_context);
            }

            qos->ac_cache_collected = true;

            //for vod adaptive
            qos->vod_adaptive.consumed_download_ms = ac_rt_info->vod_adaptive.consumed_download_time_ms;
            qos->vod_adaptive.actual_video_size_byte = ac_rt_info->vod_adaptive.actual_video_size_byte;
            if (qos->vod_adaptive.consumed_download_ms != 0) {
                qos->vod_adaptive.average_download_rate_kbps = (uint32_t)((qos->vod_adaptive.actual_video_size_byte * 8000) / (qos->vod_adaptive.consumed_download_ms * 1024));
            }
            qos->vod_adaptive.real_time_throughput_kbps = ac_rt_info->vod_adaptive.real_time_throughput_kbps;
        }
    }
}

void KwaiQos_collectRealTimeStatInfoIfNeeded(FFPlayer* ffp) {
    if (!ffp) {
        return;
    }
    KwaiQos* qos = &ffp->kwai_qos;

    qos->runtime_stat.a_cache_duration = ffp->stat.audio_cache.duration;
    qos->runtime_stat.a_cahce_bytes    = ffp->stat.audio_cache.bytes;
    qos->runtime_stat.a_cache_pakets   = ffp->stat.audio_cache.packets;
    qos->runtime_stat.v_cache_duration = ffp->stat.video_cache.duration;
    qos->runtime_stat.v_cache_bytes    = ffp->stat.video_cache.bytes;
    qos->runtime_stat.v_cache_packets  = ffp->stat.video_cache.packets;
    qos->runtime_stat.max_video_dts_diff_ms = ffp->stat.max_video_dts_diff_ms;
    qos->runtime_stat.max_audio_dts_diff_ms = ffp->stat.max_audio_dts_diff_ms;
    qos->runtime_stat.speed_changed_cnt = ffp->stat.speed_changed_cnt;

    qos->runtime_cost.cost_start_play_block = ffp->kwai_packet_buffer_checker.self_life_cycle_cost_ms;

}

void KwaiQos_onDropPacket(KwaiQos* qos, AVPacket* pkt, AVFormatContext* ic, int audio_stream,
                          int video_stream, unsigned session_id) {
    if (!ic || !pkt) {
        return;
    }

    if (pkt->stream_index == audio_stream) {
        int64_t dur = get_duration_ms(pkt->duration, audio_stream, ic);
        ALOGE("[%u] [audio]pkt_in_play_range is 0, pkt->stream_index:%d, size:%d, pkt_pts:%s, pkt_dur:%lldms \n",
              session_id, pkt->stream_index, pkt->size,
              av_ts2timestr(pkt->pts, &ic->streams[audio_stream]->time_base), dur);

        qos->runtime_stat.total_dropped_duration += dur;
        if (get_current_time_ms() - qos->ts_start_prepare_async <=
            KSY_LIVE_CALC_TIME_OF_FST_DROP_DURATION_MS) {
            qos->runtime_stat.begining_dropped_duration = qos->runtime_stat.total_dropped_duration;
        }

    } else if (pkt->stream_index == video_stream) {
        int64_t dur = get_duration_ms(pkt->duration, video_stream, ic);
        ALOGE("[%u] [video]pkt_in_play_range is 0, pkt->stream_index:%d, size:%d, pkt_pts:%s, pkt_dur:%lldms \n",
              session_id, pkt->stream_index, pkt->size,
              av_ts2timestr(pkt->pts, &ic->streams[video_stream]->time_base), dur);
    } else {
        ALOGE("[%u] [unknown]pkt_in_play_range is 0, pkt->stream_index:%d, size:%d\n",
              session_id, pkt->stream_index, pkt->size);
    }

}

static char* KwaiQos_getTransCoderGroup(KwaiQos* qos) {
    if (!qos) {
        return NULL;
    }

    if (qos->ic_metadata) {
        AVDictionaryEntry* entry = av_dict_get(qos->ic_metadata, "tsc_group", NULL, 0);
        if (entry && entry->value) {
            return av_strdup(entry->value);
        }
    }

    return NULL;
}

static char* KwaiQos_getComment(KwaiQos* qos) {
    if (!qos) {
        return NULL;
    }
    char* local_comment = NULL;
    if (qos->ic_metadata) {
        int max_len = 0, write, offset = 0;

        AVDictionaryEntry* comment = av_dict_get(qos->ic_metadata, "comment", NULL, 0);
        if (comment) {
            max_len += strlen(comment->value);
        }
        AVDictionaryEntry* dscp = av_dict_get(qos->ic_metadata, "dscp", NULL, 0);
        if (dscp) {
            max_len += strlen(dscp->value);
        }
        if (qos->video_st_metadata) {
            AVDictionaryEntry* videoHdl = av_dict_get(qos->video_st_metadata, "handler_name", NULL,
                                                      0);
            if (videoHdl) {
                max_len += strlen(videoHdl->value) + 2;
            }
        }

        if (max_len) {
            max_len += 1;
            local_comment = mallocz(max_len);
            if (comment) {
                write = snprintf(local_comment + offset, max_len, "%s", comment->value);
                max_len = max_len - write;
                offset += write;
            }
            if (dscp) {
                write = snprintf(local_comment + offset, max_len, "%s", dscp->value);
                max_len = max_len - write;
                offset += write;
            }
            if (qos->video_st_metadata) {
                AVDictionaryEntry* videoHdl = av_dict_get(qos->video_st_metadata, "handler_name",
                                                          NULL, 0);
                if (videoHdl) {
                    write = snprintf(local_comment + offset, max_len, "[%s]", videoHdl->value);
                    max_len = max_len - write;
                    offset += write;
                }
            }
        }
    }
    return local_comment;
}


void KwaiQos_setTranscodeType(KwaiQos* qos, const char* transcode_type) {
    memcpy(qos->player_config.trancode_type, transcode_type, TRANSCODE_TYPE_MAX_LEN);
}

void KwaiQos_collectPlayerStaticConfig(FFPlayer* ffp, const char* filename) {
    if (!ffp) {
        return;
    }
    KwaiQos* qos = &ffp->kwai_qos;

    qos->player_config.filename = filename ? av_strdup(filename) : NULL;
    if (ffp->islive) {
        qos->player_config.stream_id = get_http_stream_id(filename);
    }
    qos->player_config.host = av_strdup(ffp->host);
    qos->player_config.domain = ffp->host ? av_strdup(ffp->host) : get_http_domain(ffp, filename);
    AVDictionaryEntry* product_context = av_dict_get(ffp->format_opts, "product-context", NULL, 0);
    qos->player_config.product_context = product_context ? av_strdup(product_context->value) : NULL;

    qos->player_config.use_awesome_cache = ffp->expect_use_cache;
    qos->player_config.enable_segment_cache = ffp->enable_segment_cache;
    qos->player_config.input_data_type = ffp->input_data_type;
    qos->player_config.use_pre_load = (!ffp->islive && ffp->pre_demux != NULL);
    if (qos->player_config.use_pre_load) {
        qos->player_config.pre_load_duraion_ms = (int) ffp->pre_demux->pre_read_duration_ms;
    }
    qos->player_config.tag1 = ffp->tag1;

    qos->player_config.seek_at_start_ms = ffp->seek_at_start;
    qos->player_config.max_buffer_size = ffp->dcc.max_buffer_size;
    qos->player_config.start_on_prepared = ffp->start_on_prepared;
    qos->player_config.enable_accurate_seek = ffp->enable_accurate_seek;
    qos->player_config.enable_seek_forward_offset = ffp->enable_seek_forward_offset;
    qos->player_config.islive = ffp->islive == 1;
    qos->player_config.app_start_time = ffp->app_start_time;

    qos->player_config.max_buffer_dur_ms = ffp->dcc.max_buffer_dur_ms;
    qos->player_config.max_buffer_strategy = ffp->dcc.max_buf_dur_strategy;
    qos->player_config.max_buffer_dur_bsp_ms = ffp->dcc.max_buffer_dur_bsp_ms;
    qos->player_config.last_high_water_mark_in_ms = ffp->dcc.last_high_water_mark_in_ms;
    qos->player_config.prefer_bandwidth = ffp->prefer_bandwidth;
}


void KwaiQos_collectStartPlayBlock(KwaiQos* qos, KwaiPacketQueueBufferChecker* checker) {
    qos->runtime_stat.start_play_block_used = checker->used_strategy != kStartPlayCheckeDisableNone;
    if (qos->runtime_stat.start_play_block_used) {
        qos->runtime_stat.start_play_block_th = checker->used_strategy == kStrategyStartPlayBlockByTimeMs
                                                ? checker->buffer_threshold_ms : checker->buffer_threshold_bytes;
        qos->runtime_stat.start_play_max_cost_ms = checker->self_max_life_cycle_ms;
    }
}

void KwaiQos_collectDccAlg(KwaiQos* qos, DccAlgorithm* alg) {
    qos->exp_dcc.cfg_mbth_10 = alg->config_mark_bitrate_th_10;
    qos->exp_dcc.cfg_pre_read_ms = alg->config_dcc_pre_read_ms;
    qos->exp_dcc.is_used = alg->qos_used;
    qos->exp_dcc.pre_read_ms_used = alg->qos_dcc_pre_read_ms_used;
    qos->exp_dcc.actual_mb_ratio = alg->qos_dcc_actual_mb_ratio;
    qos->exp_dcc.cmp_mark_kbps = alg->cmp_mark_kbps;
}

void KwaiQos_collectPlayerMetaInfo(FFPlayer* ffp) {
    if (!ffp || !ffp->is) {
        return;
    }
    KwaiQos* qos = &ffp->kwai_qos;
    VideoState* is = ffp->is;

    qos->media_metadata.fps = is->probe_fps;

    if (is->ic) {
        qos->media_metadata.duration = fftime_to_milliseconds(is->ic->duration);
        qos->media_metadata.bitrate = is->ic->bit_rate;
        if (is->auddec.avctx) {
            qos->media_metadata.audio_bit_rate =  is->auddec.avctx->bit_rate;
            KwaiQos_setAudioProfile(qos, av_get_profile_name(is->auddec.avctx->codec, is->auddec.avctx->profile));
        }
        if (is->viddec.avctx) {
            qos->media_metadata.video_bit_rate =  is->viddec.avctx->bit_rate;
        }
    }
    qos->media_metadata.audio_duration = fftime_to_milliseconds(is->audio_duration);
    qos->media_metadata.channels = is->audio_filter_src.channels;
    qos->media_metadata.sample_rate = is->audio_filter_src.freq;

    if (!qos->media_metadata.comment) {
        qos->media_metadata.comment = KwaiQos_getComment(qos);
    }

    if (!qos->media_metadata.transcoder_group) {
        qos->media_metadata.transcoder_group = KwaiQos_getTransCoderGroup(qos);
    }
}

void KwaiQos_collectPlayerNetInfo(FFPlayer* ffp) {
    if (!ffp || !ffp->is) {
        return;
    }
    KwaiQos* qos = &ffp->kwai_qos;
    VideoState* is = ffp->is;

    qos->player_config.server_ip = av_strdup(is->server_ip);
}

//更新实时上报卡顿信息
static void KwaiQos_updateRealTimeBlockInfo(FFPlayer* ffp, int64_t time) {
    KwaiQos* qos = &ffp->kwai_qos;
    SDL_LockMutex(qos->real_time_block_info_mutex);
    if (!qos->real_time_block_info)
        qos->real_time_block_info = cJSON_CreateArray();

    int block_info_size = cJSON_GetArraySize(qos->real_time_block_info);

    if (qos->runtime_stat.is_blocking) {
        if (block_info_size < MAX_BLOCK_INFO_SIZE) {
            cJSON* block_detail_info = cJSON_CreateObject();
            cJSON_AddItemToArray(qos->real_time_block_info, block_detail_info);
            cJSON_AddNumberToObject(block_detail_info, "index", qos->runtime_stat.block_cnt);
            cJSON_AddNumberToObject(block_detail_info, "start_time", time);
        }
    } else {
        bool match_flag = false;
        if (block_info_size > 0) {
            {
                cJSON* block_detail_info = cJSON_GetArrayItem(qos->real_time_block_info,
                                                              block_info_size - 1);
                if (block_detail_info) {
                    cJSON* item = cJSON_GetObjectItem(block_detail_info, "index");
                    if (item->valueint == qos->runtime_stat.block_cnt) {
                        cJSON_AddNumberToObject(block_detail_info, "end_time", time);
                        match_flag = true;
                    }
                }
            }
        }
        if (!match_flag && block_info_size < MAX_BLOCK_INFO_SIZE) {
            cJSON* block_detail_info = cJSON_CreateObject();
            cJSON_AddItemToArray(qos->real_time_block_info, block_detail_info);
            cJSON_AddNumberToObject(block_detail_info, "index", qos->runtime_stat.block_cnt);
            cJSON_AddNumberToObject(block_detail_info, "end_time", time);
        }
    }
    SDL_UnlockMutex(qos->real_time_block_info_mutex);
}

/**
 * 更新汇总上报卡顿信息, 卡顿信息以json数组上报，对应格式
 *  [
 *    {"index":1,"start_time":1562896957987,"a_pkt":0,"a_pkt_dur":0,"v_pkt":0,"v_pkt_dur":0,"pos":2766,"r_ic_size":131072
 *      ,"dl_size_s":0,"last_error_s":0,"end_time":1562896971247,"dl_size_e":164298, "last_error_e":0},
 *    {"index":2,"start_time":1562896974545,"a_pkt":0,"a_pkt_dur":0,"v_pkt":2,"v_pkt_dur":100,"pos":6183,"r_ic_size":262144
 *      ,"dl_size_s":180846,"last_error_s":0,"end_time":1562896977418,"dl_size_e":180846, "last_error_e":0}
 *  ]
 * index:卡顿次数，start_time：卡顿开始时间，a_pkt：卡顿时audio pkt数目， a_pkt_dur：audio pkt时长，
 * pos:卡顿位置，r_ic_size：io数据量，dl_size_s：curl下载数据量，last_error_s：开始时错误码，
 * end_time:结束时间，dl_size_e：curl下载数据量，last_error_e:结束时错误码
 */
static void KwaiQos_updateSumBlockInfo(FFPlayer* ffp, int64_t time) {
    KwaiQos* qos = &ffp->kwai_qos;
    SDL_LockMutex(qos->sum_block_info_mutex);
    {
        if (!qos->sum_block_info)
            qos->sum_block_info = cJSON_CreateArray();

        int block_stat_size = cJSON_GetArraySize(qos->sum_block_info);

        if (block_stat_size < MAX_BLOCK_STAT_SIZE) {
            if (qos->runtime_stat.is_blocking) {
                cJSON* block_detail_info = cJSON_CreateObject();
                cJSON_AddItemToArray(qos->sum_block_info, block_detail_info);
                cJSON_AddNumberToObject(block_detail_info, "index", qos->runtime_stat.block_cnt);
                cJSON_AddNumberToObject(block_detail_info, "start_time", time);
                cJSON_AddNumberToObject(block_detail_info, "a_pkt",
                                        qos->runtime_stat.a_cache_pakets);
                cJSON_AddNumberToObject(block_detail_info, "a_pkt_dur",
                                        qos->runtime_stat.a_cache_duration);
                cJSON_AddNumberToObject(block_detail_info, "pos",
                                        ffp_get_current_position_l(ffp));
                cJSON_AddNumberToObject(block_detail_info, "r_ic_size",
                                        ffp->is->bytes_read);
                cJSON_AddNumberToObject(block_detail_info, "dl_size_s",
                                        ffp->cache_stat.ac_runtime_info.http_ds.download_bytes);
                cJSON_AddNumberToObject(block_detail_info, "last_error_s",
                                        qos->runtime_stat.last_error);
            } else {
                if (block_stat_size > 0) {
                    cJSON* block_detail_info = cJSON_GetArrayItem(qos->sum_block_info,
                                                                  block_stat_size - 1);
                    if (block_detail_info) {
                        cJSON* item = cJSON_GetObjectItem(block_detail_info, "index");
                        cJSON* end_time_item = cJSON_GetObjectItem(block_detail_info, "end_time");
                        if (item && !end_time_item && item->valueint == qos->runtime_stat.block_cnt) {
                            cJSON_AddNumberToObject(block_detail_info, "end_time", time);
                            cJSON_AddNumberToObject(block_detail_info, "dl_size_e",
                                                    ffp->cache_stat.ac_runtime_info.http_ds.download_bytes);
                            cJSON_AddNumberToObject(block_detail_info, "last_error_e",
                                                    qos->runtime_stat.last_error);
                        }
                    }
                }
            }
        }
    }
    SDL_UnlockMutex(qos->sum_block_info_mutex);
}

void KwaiQos_setBlockInfo(FFPlayer* ffp) {
    if (!ffp || !ffp->is) {
        return;
    }
    KwaiQos* qos = &ffp->kwai_qos;
    int64_t time = av_gettime() / 1000;

    ALOGI("[%u][BlockRecord] is_blocking:%d, index:%d, time:%lld, a_pkt:%lld, a_pkt_dur:%lld, pos:%ld, "
          "r_ic_size:%lld, dl_size_s:%lld, error:%d, kwai_error:%d",
          ffp->session_id, qos->runtime_stat.is_blocking, qos->runtime_stat.block_cnt, time,
          qos->runtime_stat.a_cache_pakets, qos->runtime_stat.a_cache_duration,
          ffp_get_current_position_l(ffp), ffp->is->bytes_read,
          ffp->cache_stat.ac_runtime_info.http_ds.download_bytes, ffp->error, ffp->kwai_error_code);

    KwaiQos_updateRealTimeBlockInfo(ffp, time);
    KwaiQos_updateSumBlockInfo(ffp, time);
}

char* KwaiQos_getRealTimeBlockInfo(KwaiQos* qos) {
    char* block_str = NULL;

    SDL_LockMutex(qos->real_time_block_info_mutex);
    if (qos->real_time_block_info) {
        block_str = cJSON_PrintUnformatted(qos->real_time_block_info);
        cJSON_Delete(qos->real_time_block_info);
        qos->real_time_block_info = NULL;
    }

    SDL_UnlockMutex(qos->real_time_block_info_mutex);

    return block_str;
}

char* KwaiQos_getSumBlockInfo(KwaiQos* qos) {
    char* block_str = NULL;

    SDL_LockMutex(qos->sum_block_info_mutex);
    if (qos->sum_block_info) {
        block_str = cJSON_PrintUnformatted(qos->sum_block_info);
    }

    SDL_UnlockMutex(qos->sum_block_info_mutex);

    return block_str;
}

static const char* KwaiQos_getColorSpaceString(enum AVColorSpace color_space) {
    switch (color_space) {
        case AVCOL_SPC_RGB:
            return "AVCOL_SPC_RGB";
        case AVCOL_SPC_BT709:
            return "AVCOL_SPC_BT709";
        case AVCOL_SPC_UNSPECIFIED:
            return "AVCOL_SPC_UNSPECIFIED";
        case AVCOL_SPC_RESERVED:
            return "AVCOL_SPC_RESERVED";
        case AVCOL_SPC_FCC:
            return "AVCOL_SPC_FCC";
        case AVCOL_SPC_BT470BG:
            return "AVCOL_SPC_BT470BG";
        case AVCOL_SPC_SMPTE170M:
            return "AVCOL_SPC_SMPTE170M";
        case AVCOL_SPC_SMPTE240M:
            return "AVCOL_SPC_SMPTE240M";
        case AVCOL_SPC_YCOCG:
            return "AVCOL_SPC_YCOCG";
        case AVCOL_SPC_BT2020_NCL:
            return "AVCOL_SPC_BT2020_NCL";
        case AVCOL_SPC_BT2020_CL:
            return "AVCOL_SPC_BT2020_CL";
        default:
            return "AVCOL_SPC_NB";
    }
}

static const char* KwaiQos_getPixelFormatString(enum AVPixelFormat pix_format) {
    switch (pix_format) {
        case AV_PIX_FMT_YUV420P:
            return "AV_PIX_FMT_YUV420P";
        case AV_PIX_FMT_NV12:
            return "AV_PIX_FMT_NV12";
        case AV_PIX_FMT_NV21:
            return "AV_PIX_FMT_NV21";
        default:
            return "N/A";
    }
}

const char* KwaiQos_getOverlayOutputFormatString(uint32_t format) {
    switch (format) {
        case SDL_FCC_YV12:
            return "SDL_FCC_YV12";
        case SDL_FCC_I420:
            return "SDL_FCC_I420";
        case SDL_FCC_I444P10LE:
            return "SDL_FCC_I444P10LE";
        case SDL_FCC_RV32:
            return "SDL_FCC_RV32";
        case SDL_FCC_RV24:
            return "SDL_FCC_RV24";
        case SDL_FCC_RV16:
            return "SDL_FCC_RV16";
        case SDL_FCC__GLES2:
            return "SDL_FCC__GLES2";
        case SDL_FCC_NV21:
            return "SDL_FCC_NV21";
        case SDL_FCC__AMC:
            return "SDL_FCC__AMC";
        case SDL_FCC__VTB:
            return "SDL_FCC__VTB";
        default:
            return "N/A";
    }
}

int KwaiQos_getLiveManifestSwitchFlag(KwaiQos* qos) {
    int live_manifest_switch_flag;
    if (qos->live_adaptive.cur_switch_mode != qos->live_adaptive.last_switch_mode) {
        if (qos->live_adaptive.auto_mode_set) {
            live_manifest_switch_flag = LIVE_MANIFEST_SWITCH_FLAG_AUTOMANUAL;
        } else {
            if (qos->live_adaptive.last_switch_mode == LIVE_MANIFEST_AUTO) {
                live_manifest_switch_flag = LIVE_MANIFEST_SWITCH_FLAG_AUTOMANUAL;
            } else {
                live_manifest_switch_flag = LIVE_MANIFEST_SWITCH_FLAG_MANUAL;
            }
        }
        qos->live_adaptive.last_switch_mode = qos->live_adaptive.cur_switch_mode;
    } else {
        if (qos->live_adaptive.auto_mode_set || qos->live_adaptive.mannual_mode_set) {
            if (qos->live_adaptive.auto_mode_set && qos->live_adaptive.mannual_mode_set) {
                live_manifest_switch_flag = LIVE_MANIFEST_SWITCH_FLAG_AUTOMANUAL;
            } else if (qos->live_adaptive.auto_mode_set) {
                live_manifest_switch_flag = LIVE_MANIFEST_SWITCH_FLAG_AUTO;
            } else {
                live_manifest_switch_flag = LIVE_MANIFEST_SWITCH_FLAG_MANUAL;
            }
        } else {
            if (qos->live_adaptive.cur_switch_mode == LIVE_MANIFEST_AUTO) {
                live_manifest_switch_flag = LIVE_MANIFEST_SWITCH_FLAG_AUTO;
            } else {
                live_manifest_switch_flag = LIVE_MANIFEST_SWITCH_FLAG_MANUAL;
            }
        }
    }

    qos->live_adaptive.auto_mode_set = false;
    qos->live_adaptive.mannual_mode_set = false;

    return live_manifest_switch_flag;
}

// https://wiki.corp.kuaishou.com/pages/viewpage.action?pageId=31036821
char* KwaiQos_getVideoStatJson(KwaiQos* qos) {
    KwaiQos_setBlockInfoStartPeriodIfNeed(qos);

    cJSON* videoinfo = cJSON_CreateObject();
    {
        //basic
        cJSON_AddStringToObject(videoinfo, "ver", qos->basic.sdk_version != NULL
                                ? qos->basic.sdk_version : "N/A");

        //config
        cJSON* config = cJSON_CreateObject();
        cJSON_AddItemToObject(videoinfo, "config", config);
        {
            cJSON_AddStringToObject(config, "url",
                                    qos->player_config.filename ? qos->player_config.filename : "N/A");
            cJSON_AddStringToObject(config, "host",
                                    qos->player_config.host ? qos->player_config.host : "N/A");
            cJSON_AddStringToObject(config, "domain",
                                    qos->player_config.domain ? qos->player_config.domain : "N/A");
            cJSON_AddStringToObject(config, "server_ip",
                                    qos->player_config.server_ip ? qos->player_config.server_ip : "N/A");
            cJSON_AddBoolToObject(config, "pre_load", qos->player_config.use_pre_load);
            cJSON_AddNumberToObject(config, "pre_load_ms", qos->player_config.pre_load_duraion_ms);

            cJSON_AddBoolToObject(config, "pre_load_finish", qos->player_config.pre_load_finish);
            cJSON_AddBoolToObject(config, "cache", qos->player_config.use_awesome_cache);
            cJSON_AddBoolToObject(config, "segment_cache", qos->player_config.enable_segment_cache);
            // 这块后续可以做成条件上报
//            char* cache_dir = ac_dir_path_dup();
//            cJSON_AddStringToObject(config, "cache_dir", cache_dir ? cache_dir :  "N/A");
//            ac_free_strp(&cache_dir);
            cJSON_AddStringToObject(config, "product_context", qos->player_config.product_context ? qos->player_config.product_context : "N/A");

            cJSON_AddNumberToObject(config, "input_type", qos->player_config.input_data_type);
            cJSON_AddNumberToObject(config, "seek_at_start", qos->player_config.seek_at_start_ms);
            cJSON_AddNumberToObject(config, "max_buffer_dur", qos->player_config.max_buffer_dur_ms);
            cJSON_AddNumberToObject(config, "max_buffer_strategy", qos->player_config.max_buffer_strategy);
            cJSON_AddNumberToObject(config, "max_buffer_dur_bsp", qos->player_config.max_buffer_dur_bsp_ms);
            cJSON_AddNumberToObject(config, "max_buffer_size", qos->player_config.max_buffer_size);
            cJSON_AddNumberToObject(config, "last_high_water_mark", qos->player_config.last_high_water_mark_in_ms);
            cJSON_AddBoolToObject(config, "start_on_prepared", qos->player_config.start_on_prepared);
            cJSON_AddBoolToObject(config, "enable_accurate_seek", qos->player_config.enable_accurate_seek);
            cJSON_AddBoolToObject(config, "enable_seek_forward", qos->player_config.enable_seek_forward_offset);
            cJSON_AddBoolToObject(config, "islive", qos->player_config.islive);
            cJSON_AddBoolToObject(config, "is_last_try", qos->player_config.is_last_try);
            cJSON_AddNumberToObject(config, "tag1", qos->player_config.tag1);
            cJSON_AddNumberToObject(config, "prefer_bw", qos->player_config.prefer_bandwidth);
            cJSON_AddNumberToObject(config, "app_start_time", qos->player_config.app_start_time);

            cJSON_AddStringToObject(config, "overlay_format", KwaiQos_getOverlayOutputFormatString(qos->player_config.overlay_format));
        }

        cJSON* data_read = cJSON_CreateObject();
        cJSON_AddItemToObject(videoinfo, "data_read", data_read);
        {
            cJSON_AddNumberToObject(data_read, "open_input", qos->data_read.data_after_open_input);
            cJSON_AddNumberToObject(data_read, "find_stream_info", qos->data_read.data_after_stream_info);
            cJSON_AddNumberToObject(data_read, "fst_a_pkt", qos->data_read.data_fst_audio_pkt);
            cJSON_AddNumberToObject(data_read, "fst_v_pkt", qos->data_read.data_fst_video_pkt);
        }

        //meta
        cJSON* meta = cJSON_CreateObject();
        cJSON_AddItemToObject(videoinfo, "meta", meta);
        {
            cJSON_AddNumberToObject(meta, "fps", qos->media_metadata.fps);
            cJSON_AddNumberToObject(meta, "dur", qos->media_metadata.duration);
            cJSON_AddNumberToObject(meta, "a_dur", qos->media_metadata.audio_duration);
            cJSON_AddStringToObject(meta, "comment", qos->media_metadata.comment != NULL
                                    ? qos->media_metadata.comment : "N/A");
            cJSON_AddNumberToObject(meta, "channels", qos->media_metadata.channels);
            cJSON_AddNumberToObject(meta, "sample_rate", qos->media_metadata.sample_rate);
            cJSON_AddStringToObject(meta, "codec_a",
                                    qos->media_metadata.audio_codec_info != NULL
                                    ? qos->media_metadata.audio_codec_info : "N/A");
            cJSON_AddStringToObject(meta, "codec_v",
                                    qos->media_metadata.video_codec_info != NULL
                                    ? qos->media_metadata.video_codec_info : "N/A");
            cJSON_AddNumberToObject(meta, "width", qos->media_metadata.width);
            cJSON_AddNumberToObject(meta, "height", qos->media_metadata.height);
            cJSON_AddNumberToObject(meta, "bitrate", qos->media_metadata.bitrate);
            cJSON_AddNumberToObject(meta, "a_first_pkg_dts", qos->media_metadata.a_first_pkg_dts);
            cJSON_AddNumberToObject(meta, "v_first_pkg_dts", qos->media_metadata.v_first_pkg_dts);
            cJSON_AddStringToObject(meta, "transcoder_ver",
                                    qos->media_metadata.transcoder_ver != NULL
                                    ? qos->media_metadata.transcoder_ver : "N/A");
            cJSON_AddStringToObject(meta, "stream_info", qos->media_metadata.stream_info != NULL
                                    ? qos->media_metadata.stream_info : "N/A");
            cJSON_AddNumberToObject(meta, "a_bitrate", qos->media_metadata.audio_bit_rate);
            cJSON_AddStringToObject(meta, "a_profile", qos->media_metadata.audio_profile != NULL
                                    ? qos->media_metadata.audio_profile : "N/A");
            cJSON_AddStringToObject(meta, "input_format", qos->media_metadata.input_fomat != NULL
                                    ? qos->media_metadata.input_fomat : "N/A");
            cJSON_AddStringToObject(meta, "color_space", KwaiQos_getColorSpaceString(qos->media_metadata.color_space));
        }

        //runtime stat
        cJSON* runtime_stat = cJSON_CreateObject();
        cJSON_AddItemToObject(videoinfo, "rt_stat", runtime_stat);
        {
            cJSON_AddNumberToObject(runtime_stat, "alive_cnt", KwaiPlayerLifeCycle_get_current_alive_cnt_unsafe());
            cJSON_AddStringToObject(runtime_stat, "session_uuid", qos->runtime_stat.session_uuid);
            cJSON_AddNumberToObject(runtime_stat, "last_error", qos->runtime_stat.last_error);
            cJSON_AddStringToObject(runtime_stat, "err_msg",
                                    kwai_error_code_to_string(qos->runtime_stat.last_error));
            // -- nativeCache
            // 历史原因， open_input_error代表的才是 ffp_setup_open_AwesomeCache_AVIOContext 返回的error
            // 而setup_cache_error 是nativeCache内部第一次open datasource的错误码
            cJSON_AddNumberToObject(runtime_stat, "open_input_error",
                                    qos->runtime_stat.open_input_error);
            cJSON_AddNumberToObject(runtime_stat, "setup_cache_error",
                                    qos->runtime_stat.setup_cache_error);
            cJSON_AddNumberToObject(runtime_stat, "cache_global_enabled",
                                    qos->runtime_stat.cache_global_enabled);
            cJSON_AddNumberToObject(runtime_stat, "cache_used",
                                    qos->runtime_stat.cache_used);
            cJSON_AddNumberToObject(runtime_stat, "url_cache_wl",
                                    qos->runtime_stat.url_in_cache_whitelist);
            // -- 起播buffer
            cJSON_AddBoolToObject(runtime_stat, "spb_used",
                                  qos->runtime_stat.start_play_block_used);
            cJSON_AddNumberToObject(runtime_stat, "spb_th",
                                    qos->runtime_stat.start_play_block_th);
            cJSON_AddNumberToObject(runtime_stat, "spb_max_ms",
                                    qos->runtime_stat.start_play_max_cost_ms);

            // -- many other status
            cJSON_AddNumberToObject(runtime_stat, "played_dur",
                                    KwaiQos_getAppPlayTotalDurationMs(qos));
            cJSON_AddNumberToObject(runtime_stat, "actual_played_dur",
                                    KwaiQos_getActualPlayedTotalDurationMs(qos));
            cJSON_AddNumberToObject(runtime_stat, "a_actual_played_dur",
                                    KwaiQos_getActualAudioPlayTotalDurationMs(qos));
            cJSON_AddNumberToObject(runtime_stat, "alive_dur",
                                    KwaiQos_getAlivePlayerTotalDurationMs(qos));
            cJSON_AddNumberToObject(runtime_stat, "block_cnt", qos->runtime_stat.block_cnt);
            cJSON_AddNumberToObject(runtime_stat, "block_dur", KwaiQos_getBufferTotalDurationMs(qos));
            cJSON_AddStringToObject(runtime_stat, "audio_str",
                                    qos->audio_str ? qos->audio_str : "N/A");
            cJSON_AddNumberToObject(runtime_stat, "use_a_gain", qos->enable_audio_gain);
            cJSON_AddNumberToObject(runtime_stat, "enable_modify_block", qos->enable_modify_block);
            cJSON_AddNumberToObject(runtime_stat, "a_process_cost", qos->audio_process_cost);
            cJSON_AddNumberToObject(runtime_stat, "a_at_write_err_cnt", qos->audio_track_write_error_count);
            cJSON_AddNumberToObject(runtime_stat, "block_cnt_start_period", qos->runtime_stat.block_cnt_start_period);
            cJSON_AddNumberToObject(runtime_stat, "block_dur_start_period", qos->runtime_stat.block_duration_start_period);
            {
                char* tmp_str = KwaiQos_getSumBlockInfo(qos);
                cJSON_AddStringToObject(runtime_stat, "block_info", tmp_str ? tmp_str : "[]");
                av_freep(&tmp_str);
            }
            cJSON_AddNumberToObject(runtime_stat, "dropped_dur",
                                    qos->runtime_stat.total_dropped_duration);
            cJSON_AddNumberToObject(runtime_stat, "v_read_cnt",
                                    qos->runtime_stat.v_read_frame_count);
            cJSON_AddNumberToObject(runtime_stat, "v_dec_cnt",
                                    qos->runtime_stat.v_decode_frame_count);
            cJSON_AddNumberToObject(runtime_stat, "v_render_cnt",
                                    qos->runtime_stat.render_frame_count);
            cJSON_AddNumberToObject(runtime_stat, "v_decoded_dropped_cnt",
                                    qos->runtime_stat.v_decoded_dropped_frame);
            cJSON_AddNumberToObject(runtime_stat, "v_render_dropped_cnt",
                                    qos->runtime_stat.v_render_dropped_frame);
            cJSON_AddNumberToObject(runtime_stat, "a_read_dur",
                                    qos->runtime_stat.a_read_frame_dur_ms);
            cJSON_AddNumberToObject(runtime_stat, "a_dec_dur",
                                    qos->runtime_stat.a_decode_frame_dur_ms);
            cJSON_AddNumberToObject(runtime_stat, "a_dec_err_dur",
                                    qos->runtime_stat.a_decode_err_dur_ms);
            cJSON_AddNumberToObject(runtime_stat, "a_render_cnt",
                                    qos->runtime_stat.render_sample_count);
            cJSON_AddNumberToObject(runtime_stat, "a_silence_cnt",
                                    qos->runtime_stat.silence_sample_count);
            if (!(qos->runtime_stat.v_hw_dec)) {
                cJSON_AddNumberToObject(runtime_stat, "sw_dec_err",
                                        qos->runtime_stat.v_sw_dec_err_cnt);
            }

            cJSON_AddStringToObject(runtime_stat, "pixel_format", KwaiQos_getPixelFormatString(qos->runtime_stat.pix_format));
            cJSON_AddNumberToObject(runtime_stat, "loop_cnt", qos->runtime_stat.loop_cnt);


            cJSON_AddNumberToObject(runtime_stat, "avg_fps", KwaiQos_getAppAverageFps(qos));
            cJSON_AddBoolToObject(runtime_stat, "v_hw_dec", qos->runtime_stat.v_hw_dec);

            cJSON_AddNumberToObject(runtime_stat, "a_device_latency", qos->runtime_stat.audio_device_latency);
            cJSON_AddNumberToObject(runtime_stat, "a_device_applied_latency", qos->runtime_stat.audio_device_applied_latency);

            cJSON_AddNumberToObject(runtime_stat, "v_err_native_windows_lock", qos->runtime_stat.v_error_native_windows_lock);
            cJSON_AddNumberToObject(runtime_stat, "v_err_unknown", qos->runtime_stat.v_error_unknown);

            cJSON_AddNumberToObject(runtime_stat, "max_av_diff", qos->runtime_stat.max_av_diff);
            cJSON_AddNumberToObject(runtime_stat, "min_av_diff", qos->runtime_stat.min_av_diff);

            cJSON_AddNumberToObject(runtime_stat, "a_cache_duration", qos->runtime_stat.a_cache_duration);
            cJSON_AddNumberToObject(runtime_stat, "a_cache_bytes", qos->runtime_stat.a_cahce_bytes);
            cJSON_AddNumberToObject(runtime_stat, "a_cache_packets", qos->runtime_stat.a_cache_pakets);
            cJSON_AddNumberToObject(runtime_stat, "v_cache_duration", qos->runtime_stat.v_cache_duration);
            cJSON_AddNumberToObject(runtime_stat, "v_cache_bytes", qos->runtime_stat.v_cache_bytes);
            cJSON_AddNumberToObject(runtime_stat, "v_cache_packets", qos->runtime_stat.v_cache_packets);
            cJSON_AddNumberToObject(runtime_stat, "a_max_dts_diff", qos->runtime_stat.max_audio_dts_diff_ms);
            cJSON_AddNumberToObject(runtime_stat, "v_max_dts_diff", qos->runtime_stat.max_video_dts_diff_ms);
            cJSON_AddNumberToObject(runtime_stat, "speed_chg_cnt", qos->runtime_stat.speed_changed_cnt);
        }

        //runtime stat
        cJSON* seek_stat = cJSON_CreateObject();
        cJSON_AddItemToObject(videoinfo, "seek_stat", seek_stat);
        {
            cJSON_AddNumberToObject(seek_stat, "seek_cnt", qos->seek_stat.seek_cnt);
            cJSON_AddNumberToObject(seek_stat, "avg_dur", KwaiQos_getSeekAvgDurationMs(qos));
            cJSON_AddNumberToObject(seek_stat, "fst_f_cnt", qos->seek_stat.seek_first_frame_cnt);
            cJSON_AddNumberToObject(seek_stat, "avg_fst_f", KwaiQos_getFirstFrameAvgDurationAfterSeekMs(qos));
            cJSON_AddNumberToObject(seek_stat, "fst_p_cnt", qos->seek_stat.seek_first_packet_cnt);
            cJSON_AddNumberToObject(seek_stat, "avg_fst_p",
                                    KwaiQos_getFirstPacketAvgDurationAfterSeekMs(qos));
        }


        //runtime cost
        cJSON* runtime_cost = cJSON_CreateObject();
        cJSON_AddBoolToObject(videoinfo, "ac_cache_collected", qos->ac_cache_collected);
        cJSON_AddItemToObject(videoinfo, "rt_cost", runtime_cost);
        {
            cJSON_AddNumberToObject(runtime_cost, "http_connect",
                                    qos->runtime_cost.cost_http_connect);
            cJSON_AddNumberToObject(runtime_cost, "http_first_data",
                                    qos->runtime_cost.cost_http_first_data);
            cJSON_AddNumberToObject(runtime_cost, "dns_analyze",
                                    qos->runtime_cost.cost_dns_analyze);

            cJSON_AddNumberToObject(runtime_cost, "prepare",
                                    qos->runtime_cost.cost_prepare_ms);
            cJSON_AddNumberToObject(runtime_cost, "first_screen",
                                    KwaiQos_getFirstScreenCostMs(qos));
            cJSON_AddNumberToObject(runtime_cost, "total_first_screen",
                                    KwaiQos_getTotalFirstScreenCostMs(qos));
            cJSON_AddNumberToObject(runtime_cost, "render_ready",
                                    qos->runtime_cost.cost_first_render_ready);
            cJSON_AddNumberToObject(runtime_cost, "start_play_block",
                                    qos->runtime_cost.cost_start_play_block);
            cJSON_AddNumberToObject(runtime_cost, "fst_app_pause",
                                    qos->runtime_cost.cost_pause_at_first_screen);
            cJSON_AddNumberToObject(runtime_cost, "app_start_play",
                                    qos->runtime_cost.cost_app_start_play);

            if (qos->ac_cache.data_source_type == kDataSourceTypeSegment || qos->player_config.enable_segment_cache) {
                cJSON* extra_connect_info = cJSON_CreateObject();
                cJSON_AddItemToObject(runtime_cost, "extra_connect_info", extra_connect_info);
                {
                    cJSON_AddNumberToObject(extra_connect_info, "http_connect",
                                            qos->runtime_cost.connect_infos[EXTRA_CONNECT_INFO_INDEX].cost_http_connect);
                    cJSON_AddNumberToObject(extra_connect_info, "http_first_data",
                                            qos->runtime_cost.connect_infos[EXTRA_CONNECT_INFO_INDEX].cost_http_first_data);
                    cJSON_AddNumberToObject(extra_connect_info, "dns_analyze",
                                            qos->runtime_cost.connect_infos[EXTRA_CONNECT_INFO_INDEX].cost_dns_analyze);
                    cJSON_AddNumberToObject(extra_connect_info, "first_data_interval",
                                            qos->runtime_cost.connect_infos[EXTRA_CONNECT_INFO_INDEX].first_data_interval);
                }
            }

            //steps
            cJSON* step = cJSON_CreateObject();
            cJSON_AddItemToObject(runtime_cost, "step", step);
            {
                cJSON_AddNumberToObject(step, "input_open",
                                        qos->runtime_cost.step_av_input_open);
                cJSON_AddNumberToObject(step, "find_stream_info",
                                        qos->runtime_cost.step_av_find_stream_info);
                cJSON_AddNumberToObject(step, "pre_demuxed",
                                        qos->runtime_cost.cost_pure_pre_demux);
                cJSON_AddNumberToObject(step, "dec_opened",
                                        qos->runtime_cost.step_open_decoder);
                cJSON_AddNumberToObject(step, "all_prepared",
                                        qos->runtime_cost.step_all_prepared);
                cJSON_AddNumberToObject(step, "wait_for_play",
                                        qos->runtime_cost.cost_wait_for_playing);
                cJSON_AddNumberToObject(step, "fst_v_pkt_recv",
                                        qos->runtime_cost.step_first_video_pkt_received);
                cJSON_AddNumberToObject(step, "fst_v_pkt_pre_dec",
                                        qos->runtime_cost.step_pre_decode_first_video_pkt);
                cJSON_AddNumberToObject(step, "fst_v_pkt_dec",
                                        qos->runtime_cost.step_decode_first_frame);
                cJSON_AddNumberToObject(step, "fst_v_render",
                                        qos->runtime_cost.step_first_framed_rendered);
                cJSON_AddNumberToObject(step, "fst_a_pkt_recv",
                                        qos->runtime_cost.step_first_audio_pkt_received);
                cJSON_AddNumberToObject(step, "fst_a_pkt_pre_dec",
                                        qos->runtime_cost.step_pre_decode_first_audio_pkt);
                cJSON_AddNumberToObject(step, "fst_a_pkt_dec",
                                        qos->runtime_cost.step_decode_first_audio_frame);
                cJSON_AddNumberToObject(step, "fst_a_render",
                                        qos->runtime_cost.step_first_audio_framed_rendered);
            }
        }

        if (qos->player_config.use_awesome_cache) {
            cJSON* ac_cache = cJSON_CreateObject();
            cJSON_AddItemToObject(videoinfo, "ac_cache", ac_cache);
            {
                // config
                cJSON_AddStringToObject(ac_cache, "cfg_cache_key", qos->ac_cache.cfg_cache_key);
                cJSON_AddNumberToObject(ac_cache, "data_source_type", qos->ac_cache.data_source_type);

                cJSON_AddNumberToObject(ac_cache, "total_bytes", qos->ac_cache.total_bytes);
                cJSON_AddNumberToObject(ac_cache, "cached_bytes", qos->ac_cache.cached_bytes);


                // for ffmpeg_adapter
                cJSON_AddNumberToObject(ac_cache, "adapter_error", qos->ac_cache.adapter_error);
                cJSON_AddStringToObject(ac_cache, "adapter_err_msg", cache_error_msg(qos->ac_cache.adapter_error));
                cJSON_AddNumberToObject(ac_cache, "adapter_read_cost_ms", qos->ac_cache.read_cost_ms);


                // for DownloadTask
                cJSON_AddNumberToObject(ac_cache, "con_timeout_ms", qos->ac_cache.con_timeout_ms);
                cJSON_AddNumberToObject(ac_cache, "read_timeout_ms", qos->ac_cache.read_timeout_ms);
                cJSON_AddNumberToObject(ac_cache, "sock_orig_size_kb", qos->ac_cache.sock_orig_size_kb);
                cJSON_AddNumberToObject(ac_cache, "sock_cfg_size_kb", qos->ac_cache.sock_cfg_size_kb);
                cJSON_AddNumberToObject(ac_cache, "sock_act_size_kb", qos->ac_cache.sock_act_size_kb);
                cJSON_AddNumberToObject(ac_cache, "dl_total_cost_ms", qos->ac_cache.download_total_cost_ms);
                cJSON_AddNumberToObject(ac_cache, "os_errno", qos->ac_cache.os_errno);

                if (qos->ac_cache.http_version[0]) {
                    cJSON_AddStringToObject(ac_cache, "http_version", qos->ac_cache.http_version);
                }

                if (qos->ac_cache.data_source_type == kDataSourceTypeAsyncV2) {
                    cJSON_AddNumberToObject(ac_cache, "upstream_type", qos->ac_cache.upstream_type);
                    cJSON_AddNumberToObject(ac_cache, "cached_bytes_on_open", qos->ac_cache.cached_bytes_on_play_start);
                    cJSON_AddNumberToObject(ac_cache, "resume_fail_cnt", qos->ac_cache.resume_file_fail_cnt);
                    cJSON_AddNumberToObject(ac_cache, "flush_fail_cnt", qos->ac_cache.flush_file_fail_cnt);
                    cJSON_AddNumberToObject(ac_cache, "curl_ret", qos->ac_cache.curl_ret);
                    cJSON_AddNumberToObject(ac_cache, "http_resp_code", qos->ac_cache.http_response_code);
                    cJSON_AddNumberToObject(ac_cache, "dl_bytes", qos->ac_cache.downloaded_bytes);
                    cJSON_AddNumberToObject(ac_cache, "recv_valid_bytes", qos->ac_cache.recv_valid_bytes);
                    if (qos->ac_cache.need_report_header) {
                        cJSON_AddStringToObject(ac_cache, "invalid_header", qos->ac_cache.invalid_header);
                    }
                } else if (qos->ac_cache.data_source_type == kDataSourceTypeDefault
                           || qos->ac_cache.data_source_type == kDataSourceTypeAsyncDownload) {
                    cJSON_AddNumberToObject(ac_cache, "upstream_type", qos->ac_cache.upstream_type);
                    cJSON_AddNumberToObject(ac_cache, "buffered_type", qos->ac_cache.buffered_type);
                    // for BufferedDataSource
                    cJSON_AddNumberToObject(ac_cache, "buffered_ds_size_kb", qos->ac_cache.buffered_datasource_size_kb);
                    cJSON_AddNumberToObject(ac_cache, "buffered_ds_seek_th_kb", qos->ac_cache.buffered_datasource_seek_threshold_kb);

                    // for general
                    cJSON_AddBoolToObject(ac_cache, "ignore_cache", qos->ac_cache.ignore_cache_on_error);
                    cJSON_AddNumberToObject(ac_cache, "reopen_cnt", qos->ac_cache.reopen_cnt);

                    cJSON_AddNumberToObject(ac_cache, "fs_last_error", qos->ac_cache.fs_error_code);

                    // for DownloadTask
                    // 这两个timeout值出现过之前下发0导致nativeCache不work的情况，暂时加入监控指标
                    cJSON_AddNumberToObject(ac_cache, "dl_feed_cost_ms", qos->ac_cache.download_feed_input_cost_ms);
                    // BufferedDataSource/TeeDataSource/DownloadTask的先手摆放顺序是有讲究的，
                    // 几个关键的xxx_cost_ms放在一起比较方便分析

                    // for CacheDataSource
                    cJSON_AddNumberToObject(ac_cache, "stop_reason", qos->ac_cache.stop_reason);

                    cJSON_AddNumberToObject(ac_cache, "http_retried_cnt", qos->ac_cache.http_retried_cnt);
                    cJSON_AddNumberToObject(ac_cache, "curl_byte_range_err", qos->ac_cache.curl_byte_range_error);

                    if (qos->ac_cache.data_source_type == kDataSourceTypeAsyncDownload) {
                        cJSON_AddNumberToObject(ac_cache, "byte_range_size", qos->ac_cache.byte_range_size);
                        cJSON_AddNumberToObject(ac_cache, "first_byte_range_length", qos->ac_cache.first_byte_range_length);
                        cJSON_AddNumberToObject(ac_cache, "dw_exit_reason", qos->ac_cache.download_exit_reason);
                        cJSON_AddNumberToObject(ac_cache, "r_from_upstream", qos->ac_cache.read_from_upstream);
                        cJSON_AddNumberToObject(ac_cache, "read_position", qos->ac_cache.read_position);
                        cJSON_AddNumberToObject(ac_cache, "bytes_remaining", qos->ac_cache.bytes_remaining);
                        cJSON_AddNumberToObject(ac_cache, "pre_dw_cnt", qos->ac_cache.pre_download_cnt);
                    }
                }


                cJSON_AddBoolToObject(ac_cache, "p2sp_enabled", qos->ac_cache.p2sp_enabled);
                if (qos->ac_cache.p2sp_enabled) {
                    cJSON_AddNumberToObject(ac_cache, "p2sp_cdn_bytes", qos->ac_cache.p2sp_cdn_bytes);
                    cJSON_AddNumberToObject(ac_cache, "p2sp_bytes_used", qos->ac_cache.p2sp_bytes_used);
                    cJSON_AddNumberToObject(ac_cache, "p2sp_bytes_repeated", qos->ac_cache.p2sp_bytes_repeated);
                    cJSON_AddNumberToObject(ac_cache, "p2sp_bytes_received", qos->ac_cache.p2sp_bytes_received);
                    cJSON_AddNumberToObject(ac_cache, "p2sp_bytes_requested", qos->ac_cache.p2sp_bytes_requested);
                    cJSON_AddNumberToObject(ac_cache, "p2sp_start", qos->ac_cache.p2sp_start);
                    cJSON_AddNumberToObject(ac_cache, "p2sp_error_code", qos->ac_cache.p2sp_error_code);
                    cJSON_AddNumberToObject(ac_cache, "p2sp_first_byte_duration", qos->ac_cache.p2sp_first_byte_duration);
                    cJSON_AddNumberToObject(ac_cache, "p2sp_first_byte_offset", qos->ac_cache.p2sp_first_byte_offset);
                    if (qos->ac_cache.p2sp_sdk_details) {
                        cJSON_AddStringToObject(ac_cache, "p2sp_sdk_details", qos->ac_cache.p2sp_sdk_details);
                    }
                }
            }
        }

        if (qos->vod_adaptive.is_vod_adaptive) {
            cJSON* vod_adaptive = cJSON_CreateObject();
            cJSON_AddItemToObject(videoinfo, "vod_adaptive", vod_adaptive);
            {
                // config
                cJSON_AddNumberToObject(vod_adaptive, "max_rate",
                                        qos->vod_adaptive.max_bitrate_kbps);
                cJSON_AddNumberToObject(vod_adaptive, "avg_rate",
                                        qos->vod_adaptive.avg_bitrate_kbps);
                cJSON_AddNumberToObject(vod_adaptive, "width",
                                        qos->vod_adaptive.width);
                cJSON_AddNumberToObject(vod_adaptive, "height",
                                        qos->vod_adaptive.height);
                cJSON_AddNumberToObject(vod_adaptive, "device_width",
                                        qos->vod_adaptive.device_width);
                cJSON_AddNumberToObject(vod_adaptive, "device_height",
                                        qos->vod_adaptive.device_height);
                cJSON_AddNumberToObject(vod_adaptive, "quality",
                                        qos->vod_adaptive.quality);
                cJSON_AddNumberToObject(vod_adaptive, "low_device",
                                        qos->vod_adaptive.low_device);
                cJSON_AddNumberToObject(vod_adaptive, "switch_code",
                                        qos->vod_adaptive.switch_code);
                cJSON_AddStringToObject(vod_adaptive, "representations",
                                        qos->vod_adaptive.representations_str != NULL
                                        ? qos->vod_adaptive.representations_str : "N/A");
                cJSON_AddStringToObject(vod_adaptive, "net_type",
                                        qos->vod_adaptive.net_type != NULL
                                        ? qos->vod_adaptive.net_type : "N/A");
                cJSON_AddStringToObject(vod_adaptive, "quality_show",
                                        qos->vod_adaptive.cur_quality_show != NULL
                                        ? qos->vod_adaptive.cur_quality_show : "N/A");

                cJSON_AddNumberToObject(vod_adaptive, "idle_time",
                                        qos->vod_adaptive.idle_last_request_ms);
                cJSON_AddNumberToObject(vod_adaptive, "short_bw",
                                        qos->vod_adaptive.short_throughput_kbps);
                cJSON_AddNumberToObject(vod_adaptive, "long_bw",
                                        qos->vod_adaptive.long_throughput_kbps);
                cJSON_AddNumberToObject(vod_adaptive, "rt_bw",
                                        qos->vod_adaptive.real_time_throughput_kbps);
                cJSON_AddStringToObject(vod_adaptive, "switch_reason",
                                        qos->vod_adaptive.switch_reason
                                        ? qos->vod_adaptive.switch_reason : "N/A");
                cJSON_AddNumberToObject(vod_adaptive, "mode",
                                        qos->vod_adaptive.algorithm_mode);

                cJSON_AddNumberToObject(vod_adaptive, "dl_time",
                                        qos->vod_adaptive.consumed_download_ms);
                cJSON_AddNumberToObject(vod_adaptive, "v_size",
                                        qos->vod_adaptive.actual_video_size_byte);
                cJSON_AddNumberToObject(vod_adaptive, "avg_dl_rate",
                                        qos->vod_adaptive.average_download_rate_kbps);
                cJSON_AddNumberToObject(vod_adaptive, "cached",
                                        qos->vod_adaptive.cached);

                cJSON* rate_config = cJSON_CreateObject();
                cJSON_AddItemToObject(vod_adaptive, "rate_config", rate_config);
                {
                    cJSON_AddNumberToObject(rate_config, "bitrate_init_level", qos->vod_adaptive.bitrate_init_level);
                    cJSON_AddNumberToObject(rate_config, "abs_res_low_device", qos->vod_adaptive.absolute_low_res_low_device);
                    cJSON_AddNumberToObject(rate_config, "adapt_4G", qos->vod_adaptive.adapt_under_4G);
                    cJSON_AddNumberToObject(rate_config, "adapt_wifi", qos->vod_adaptive.adapt_under_wifi);
                    cJSON_AddNumberToObject(rate_config, "abs_rate_4G", qos->vod_adaptive.absolute_low_rate_4G);
                    cJSON_AddNumberToObject(rate_config, "abs_rate_wifi", qos->vod_adaptive.absolute_low_res_wifi);
                    cJSON_AddNumberToObject(rate_config, "abs_res_4G", qos->vod_adaptive.absolute_low_res_4G);
                    cJSON_AddNumberToObject(rate_config, "abs_res_wifi", qos->vod_adaptive.absolute_low_res_wifi);
                    cJSON_AddNumberToObject(rate_config, "wifi_amend", qos->vod_adaptive.wifi_amend);
                    cJSON_AddNumberToObject(rate_config, "4G_amend", qos->vod_adaptive.fourG_amend);
                    cJSON_AddNumberToObject(rate_config, "res_amend", qos->vod_adaptive.resolution_amend);
                    cJSON_AddNumberToObject(rate_config, "policy", qos->vod_adaptive.priority_policy);
                    cJSON_AddNumberToObject(rate_config, "max_res", qos->vod_adaptive.max_resolution);
                }
            }
        }

        cJSON* exp = cJSON_CreateObject();
        cJSON_AddItemToObject(videoinfo, "exp_dcc", exp);
        {
            cJSON_AddNumberToObject(exp, "cfg_mbth_10", qos->exp_dcc.cfg_mbth_10);
            cJSON_AddNumberToObject(exp, "cfg_pre_read_ms", qos->exp_dcc.cfg_pre_read_ms);
            cJSON_AddBoolToObject(exp, "used", qos->exp_dcc.is_used);
            cJSON_AddNumberToObject(exp, "pre_read_ms", qos->exp_dcc.pre_read_ms_used);
            cJSON_AddNumberToObject(exp, "cmp_mark_kbps", qos->exp_dcc.cmp_mark_kbps);
            cJSON_AddNumberToObject(exp, "act_mb_ratio", qos->exp_dcc.actual_mb_ratio);
        }

        cJSON* sys_prof = cJSON_CreateObject();
        cJSON_AddItemToObject(videoinfo, "sys_prof", sys_prof);
        {
            int memory_usage = -1;
            if (qos->system_performance.sample_cnt > 0) {
                memory_usage = qos->system_performance.process_memory_size_kb / qos->system_performance.sample_cnt;
            }
            cJSON_AddNumberToObject(sys_prof, "memory",
                                    memory_usage > 0 ? memory_usage : -1);

            int cpu_usage = -1;
            if (qos->system_performance.sample_cnt > 0) {
                cpu_usage = qos->system_performance.process_cpu_pct / qos->system_performance.sample_cnt;
            }
            cJSON_AddNumberToObject(sys_prof, "cpu",
                                    (cpu_usage > 0 && cpu_usage <= 100) ? cpu_usage : -1);

            cJSON_AddNumberToObject(sys_prof, "cpu_cnt", qos->system_performance.process_cpu_cnt);
#if defined(__ANDROID__)
            // cpu_cnt_total,暂时只有android有，后续ios也看情况加上
            cJSON_AddNumberToObject(sys_prof, "cpu_cnt_t", qos->system_performance.device_cpu_cnt_total);
#endif
            cJSON_AddNumberToObject(sys_prof, "prof_cost_total", qos->system_performance.total_prof_cost_ms);
            cJSON_AddNumberToObject(sys_prof, "prof_cost_avg",
                                    qos->system_performance.sample_cnt > 0 ?
                                    qos->system_performance.total_prof_cost_ms / qos->system_performance.sample_cnt : 0);
        }

        if (qos->runtime_stat.v_hw_dec) {
            cJSON* hw_decode = cJSON_CreateObject();
            cJSON_AddItemToObject(videoinfo, "hw_decode", hw_decode);
            {
                cJSON_AddNumberToObject(hw_decode, "reset_session_cnt",
                                        qos->hw_decode.reset_session_cnt);
                cJSON_AddNumberToObject(hw_decode, "err_code",
                                        qos->hw_decode.err_code);

#if defined(__APPLE__)
                cJSON* video_tool_box = cJSON_CreateObject();
                cJSON_AddItemToObject(hw_decode, "video_tool_box", video_tool_box);
                {
                    cJSON_AddStringToObject(video_tool_box, "mode",
                                            qos->hw_decode.video_tool_box.mode == VIDEO_TOOL_BOX_ASYNC
                                            ? "async" : "sync");
                    cJSON_AddNumberToObject(video_tool_box, "pkt_cnt_on_err",
                                            qos->hw_decode.video_tool_box.pkt_cnt_on_err);
                    cJSON_AddNumberToObject(video_tool_box, "q_is_full_cnt",
                                            qos->hw_decode.video_tool_box.queue_is_full_cnt);
                    cJSON_AddNumberToObject(video_tool_box, "res_change",
                                            qos->hw_decode.video_tool_box.resolution_change);
                    cJSON_AddNumberToObject(video_tool_box, "hw_dec_err_cnt",
                                            qos->runtime_stat.v_tool_box_err_cnt);
                }
#elif defined(__ANDROID__)
                cJSON* media_codec = cJSON_CreateObject();
                cJSON_AddItemToObject(hw_decode, "mediacodec", media_codec);
                {
                    cJSON_AddNumberToObject(media_codec, "input_err_cnt",
                                            qos->runtime_stat.v_mediacodec_input_err_cnt);
                    cJSON_AddNumberToObject(media_codec, "input_err",
                                            qos->runtime_stat.v_mediacodec_input_err_code);
                    cJSON_AddNumberToObject(media_codec, "output_try_again_err_cnt",
                                            qos->runtime_stat.v_mediacodec_output_try_again_err_cnt);
                    cJSON_AddNumberToObject(media_codec, "output_buffer_changed_err_cnt",
                                            qos->runtime_stat.v_mediacodec_output_buffer_changed_err_cnt);
                    cJSON_AddNumberToObject(media_codec, "output_unknown_err_cnt",
                                            qos->runtime_stat.v_mediacodec_output_unknown_err_cnt);
                    cJSON_AddNumberToObject(media_codec, "output_err_cnt",
                                            qos->runtime_stat.v_mediacodec_output_err_cnt);
                    cJSON_AddNumberToObject(media_codec, "output_err",
                                            qos->runtime_stat.v_mediacodec_output_err_code);
                    cJSON_AddStringToObject(media_codec, "config_type",
                                            qos->runtime_stat.v_mediacodec_config_type);
                    cJSON_AddNumberToObject(media_codec, "max_cnt",
                                            qos->runtime_stat.v_mediacodec_codec_max_cnt);
                }
#endif
            }
        }
    }

    char* out = cJSON_Print(videoinfo);
    cJSON_Delete(videoinfo);

    return out;
}

char* KwaiQos_getBriefVideoStatJson(KwaiQos* qos) {
    KwaiQos_setBlockInfoStartPeriodIfNeed(qos);

    cJSON* videoinfo = cJSON_CreateObject();
    {
        //config
        cJSON* config = cJSON_CreateObject();
        cJSON_AddItemToObject(videoinfo, "config", config);
        {
            cJSON_AddStringToObject(config, "url",
                                    qos->player_config.filename ? qos->player_config.filename : "N/A");
            cJSON_AddStringToObject(config, "host",
                                    qos->player_config.host ? qos->player_config.host : "N/A");
            cJSON_AddStringToObject(config, "domain",
                                    qos->player_config.domain ? qos->player_config.domain : "N/A");
            cJSON_AddStringToObject(config, "server_ip",
                                    qos->player_config.server_ip ? qos->player_config.server_ip : "N/A");
        }

        //meta
        cJSON* meta = cJSON_CreateObject();
        cJSON_AddItemToObject(videoinfo, "meta", meta);
        {
            cJSON_AddNumberToObject(meta, "dur", qos->media_metadata.duration);
            cJSON_AddNumberToObject(meta, "a_dur", qos->media_metadata.audio_duration);
            cJSON_AddStringToObject(meta, "codec_v",
                                    qos->media_metadata.video_codec_info != NULL
                                    ? qos->media_metadata.video_codec_info : "N/A");
            cJSON_AddNumberToObject(meta, "width", qos->media_metadata.width);
            cJSON_AddNumberToObject(meta, "height", qos->media_metadata.height);
            cJSON_AddNumberToObject(meta, "bitrate", qos->media_metadata.bitrate);
        }

        //runtime stat
        cJSON* runtime_stat = cJSON_CreateObject();
        cJSON_AddItemToObject(videoinfo, "rt_stat", runtime_stat);
        {
            cJSON_AddNumberToObject(runtime_stat, "last_error", qos->runtime_stat.last_error);
            cJSON_AddNumberToObject(runtime_stat, "played_dur",
                                    KwaiQos_getAppPlayTotalDurationMs(qos));
            cJSON_AddNumberToObject(runtime_stat, "actual_played_dur",
                                    KwaiQos_getActualPlayedTotalDurationMs(qos));
            cJSON_AddNumberToObject(runtime_stat, "a_actual_played_dur",
                                    KwaiQos_getActualAudioPlayTotalDurationMs(qos));
            cJSON_AddNumberToObject(runtime_stat, "alive_dur",
                                    KwaiQos_getAlivePlayerTotalDurationMs(qos));
            cJSON_AddNumberToObject(runtime_stat, "block_cnt", qos->runtime_stat.block_cnt);
            cJSON_AddNumberToObject(runtime_stat, "block_dur", KwaiQos_getBufferTotalDurationMs(qos));
            cJSON_AddNumberToObject(runtime_stat, "loop_cnt", qos->runtime_stat.loop_cnt);
        }

        //runtime cost
        cJSON* runtime_cost = cJSON_CreateObject();
        cJSON_AddItemToObject(videoinfo, "rt_cost", runtime_cost);
        {
            cJSON_AddNumberToObject(runtime_cost, "first_screen", KwaiQos_getFirstScreenCostMs(qos));
        }

        if (qos->player_config.use_awesome_cache) {
            cJSON* ac_cache = cJSON_CreateObject();
            cJSON_AddItemToObject(videoinfo, "ac_cache", ac_cache);
            {
                cJSON_AddNumberToObject(ac_cache, "adapter_error", qos->ac_cache.adapter_error);
            }
        }
    }

    char* out = cJSON_Print(videoinfo);
    cJSON_Delete(videoinfo);

    return out;
}

void KwaiQos_getQosInfo(FFPlayer* ffp, KsyQosInfo* info) {
    if (!ffp || !ffp->is)
        return;

    VideoState* is = ffp->is;
    KwaiQos* qos = &ffp->kwai_qos;
    int audio_time_base_valid = 0;
    int video_time_base_valid = 0;

    if (is->audio_st)
        audio_time_base_valid = is->audio_st->time_base.den > 0 && is->audio_st->time_base.num > 0;
    if (is->video_st)
        video_time_base_valid = is->video_st->time_base.den > 0 && is->video_st->time_base.num > 0;

    if (is->audio_st) {
        info->audioBufferByteLength = is->audioq.size;
        info->audioTotalDataSize = ffp->i_audio_decoded_size + is->audioq.size;
        if (audio_time_base_valid)
            info->audioBufferTimeLength = (int)(is->audioq.duration *
                                                av_q2d(is->audio_st->time_base) * 1000);
    }

    if (is->video_st) {
        info->videoBufferByteLength = is->videoq.size;
        info->videoTotalDataSize = ffp->i_video_decoded_size + is->videoq.size;
        if (video_time_base_valid)
            info->videoBufferTimeLength = (int)(is->videoq.duration *
                                                av_q2d(is->video_st->time_base) * 1000);
    }

    info->totalDataBytes = ffp->is->bytes_read;

    if (ffp->is_live_manifest)
        info->rep_switch_cnt = ffp->kflv_player_statistic.kflv_stat.rep_switch_cnt;

    info->audioDelay = ffp->qos_delay_audio_render.period_avg;
    info->videoDelayRecv = ffp->qos_delay_video_recv.period_avg;
    info->videoDelayBefDec = ffp->qos_delay_video_before_dec.period_avg;
    info->videoDelayAftDec = ffp->qos_delay_video_after_dec.period_avg;
    info->videoDelayRender = ffp->qos_delay_video_render.period_avg;
    info->fst_video_pre_dec = (int)(qos->runtime_cost.step_pre_decode_first_video_pkt >= 0
                                    ? qos->runtime_cost.step_pre_decode_first_video_pkt : 0);
    info->fst_dropped_duration = (int)(qos->runtime_stat.begining_dropped_duration >= 0 ?
                                       qos->runtime_stat.begining_dropped_duration : 0);
    info->dropped_duration = (int) qos->runtime_stat.total_dropped_duration;

    info->fst_total = (int)(qos->runtime_cost.cost_first_screen >= 0
                            ? qos->runtime_cost.cost_first_screen : 0);

    info->fst_dns_analyze = (int)(qos->runtime_cost.cost_dns_analyze >= 0
                                  ? qos->runtime_cost.cost_dns_analyze : 0);
    info->fst_http_connect = (int)(qos->runtime_cost.cost_http_connect >= 0
                                   ? qos->runtime_cost.cost_http_connect : 0);
    info->fst_http_first_data = (int)(qos->runtime_cost.cost_http_first_data >= 0
                                      ? qos->runtime_cost.cost_http_first_data : 0);
    info->fst_input_open = (int)(qos->runtime_cost.step_av_input_open >= 0
                                 ? qos->runtime_cost.step_av_input_open : 0);
    info->fst_stream_find = (int)(qos->runtime_cost.step_av_find_stream_info >= 0
                                  ? qos->runtime_cost.step_av_find_stream_info : 0);
    info->fst_codec_open = (int)(qos->runtime_cost.step_open_decoder >= 0
                                 ? qos->runtime_cost.step_open_decoder : 0);
    info->fst_all_prepared = (int)(qos->runtime_cost.step_all_prepared >= 0
                                   ? qos->runtime_cost.step_all_prepared : 0);
    info->fst_wait_for_play = (int)(qos->runtime_cost.cost_wait_for_playing >= 0
                                    ? qos->runtime_cost.cost_wait_for_playing : 0);
    info->fst_video_pkt_recv = (int)(qos->runtime_cost.step_first_video_pkt_received >= 0
                                     ? qos->runtime_cost.step_first_video_pkt_received : 0);
    info->fst_video_dec = (int)(qos->runtime_cost.step_decode_first_frame >= 0
                                ? qos->runtime_cost.step_decode_first_frame
                                : 0);
    info->fst_video_render = (int)(qos->runtime_cost.step_first_framed_rendered >= 0
                                   ? qos->runtime_cost.step_first_framed_rendered : 0);

    memset(info->hostInfo, 0, KSY_QOS_STR_MAX_LEN);
    memset(info->vencInit, 0, KSY_QOS_STR_MAX_LEN);
    memset(info->aencInit, 0, KSY_QOS_STR_MAX_LEN);
    memset(info->vencDynamic, 0, KSY_QOS_STR_MAX_LEN);

    SDL_LockMutex(qos->dict_mutex);
    info->comment = KwaiQos_getComment(qos);
    if (!info->comment) {
        info->comment = mallocz(1);
    }

    if (qos->ic_metadata) {
//        avformat_close_input(&is->ic);
        // kwai-hostinfo: compatible with old version, non-encrypted
        AVDictionaryEntry* hostInfo = av_dict_get(qos->ic_metadata, "kwai-hostinfo", NULL, 0);
        if (hostInfo) {
            strncpy(info->hostInfo, hostInfo->value, KSY_QOS_STR_MAX_LEN - 1);
        }

        // kshi/ksvi/ksai: encrypted
        char tempStr[KSY_QOS_STR_MAX_LEN] = {0};
        hostInfo = av_dict_get(qos->ic_metadata, "kshi", NULL, 0);
        if (hostInfo) {
            int len = decryptStr(tempStr, hostInfo->value,
                                 FFMIN((int) strlen(hostInfo->value), KSY_QOS_STR_MAX_LEN - 1));
            strncpy(info->hostInfo, tempStr, len);
        }
        AVDictionaryEntry* vencInit = av_dict_get(qos->ic_metadata, "ksvi", NULL, 0);
        if (vencInit) {
            int len = decryptStr(tempStr, vencInit->value,
                                 FFMIN((int) strlen(vencInit->value), KSY_QOS_STR_MAX_LEN - 1));
            strncpy(info->vencInit, tempStr, len);
        }
        AVDictionaryEntry* aencInit = av_dict_get(qos->ic_metadata, "ksai", NULL, 0);
        if (aencInit) {
            int len = decryptStr(tempStr, aencInit->value,
                                 FFMIN((int) strlen(aencInit->value), KSY_QOS_STR_MAX_LEN - 1));
            strncpy(info->aencInit, tempStr, len);
        }
    }
    SDL_UnlockMutex(qos->dict_mutex);

    strncpy(info->vencDynamic, ffp->qos_venc_dyn_param, KSY_QOS_STR_MAX_LEN - 1);

    if (ffp->cache_actually_used) {
        AwesomeCacheRuntimeInfo* ac_rt_info = &ffp->cache_stat.ac_runtime_info;
        info->live_native_p2sp_enabled = ac_rt_info->p2sp_task.enabled;
        info->p2sp_download_bytes = ac_rt_info->p2sp_task.p2sp_download_bytes;
        info->cdn_download_bytes = ac_rt_info->p2sp_task.cdn_download_bytes;
    }
}

