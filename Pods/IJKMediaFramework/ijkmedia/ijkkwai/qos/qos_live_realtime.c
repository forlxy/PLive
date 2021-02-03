//
//  qos_live_realtime.c
//  IJKMediaFramework
//
//  Created by 帅龙成 on 26/02/2018.
//  Copyright © 2018 kuaishou. All rights reserved.
//

#include "qos_live_realtime.h"
#include "ijkkwai/kwai_qos.h"
#include <libavkwai/cJSON.h>
#include "ff_ffplay_def.h"
#include "ff_ffplay.h"


void QosLiveRealtime_init(QosLiveRealtime* qos, QosLiveAdaptive* qos_adaptive) {
    qos->index = 1;

    qos_adaptive->last_switch_mode = LIVE_MANIFEST_AUTO;
    qos_adaptive->cur_switch_mode = LIVE_MANIFEST_AUTO;
    qos_adaptive->auto_mode_set = false;
    qos_adaptive->mannual_mode_set = false;
}

static const char* get_cache_type(FFPlayer* ffp) {
    if (ffp->cache_actually_used) {
        switch (ffp->kwai_qos.ac_cache.data_source_type) {
            case kDataSourceTypeLiveNormal:
                return "normal";
            default:
                return "unknown";
        }
    } else {
        return "N/A";
    }
}

static char* get_kwaisign(FFPlayer* ffp) {
    if (ffp->cache_actually_used) {
        return ffp->cache_stat.ac_runtime_info.download_task.kwaisign;
    } else if (ffp->islive) {
        return ffp->live_kwai_sign;
    }

    return "N/A";
}

static char* get_x_ks_cache(FFPlayer* ffp) {
    if (ffp->islive) {
        if (ffp->cache_actually_used) {
            return ffp->cache_stat.ac_runtime_info.download_task.x_ks_cache;
        } else {
            return ffp->live_x_ks_cache;
        }
    }

    return "N/A";
}

static int64_t get_delay_stat_avg_value(DelayStat last_stat, DelayStat cur_stat) {
    int64_t diff_sum = cur_stat.total_sum - last_stat.total_sum;
    int diff_count = cur_stat.total_count - last_stat.total_count;
    if (diff_count != 0) {
        return diff_sum / diff_count;
    } else {
        return 0;
    }
}

static int64_t get_audio_pts_jump_backward_time_gap(KwaiQos* qos) {
    int64_t value = 0;
    if (qos->qos_live_realtime.audio_pts_jump_backward_index != qos->runtime_stat.audio_pts_jump_backward_index) {
        value = qos->runtime_stat.audio_pts_jump_backward_time_gap;
        qos->qos_live_realtime.audio_pts_jump_backward_index = qos->runtime_stat.audio_pts_jump_backward_index;
    }
    return value;
}

void QosLiveRealtime_set_app_qos_info(FFPlayer* ffp, const char* app_qos_info) {
    if (!ffp) {
        return;
    }

    KwaiQos* qos = &ffp->kwai_qos;

    cJSON* root = cJSON_Parse(app_qos_info);
    if (!root) {
        return;
    }

    SDL_LockMutex(qos->qos_live_realtime.app_qos_json_mutex);
    if (!qos->qos_live_realtime.app_qos_json) {
        qos->qos_live_realtime.app_qos_json = cJSON_CreateObject();
    }

    if (cJSON_Object == root->type) {
        int len = cJSON_GetArraySize(root);
        for (int i = 0; i < len; i++) {
            cJSON* child_json = cJSON_GetArrayItem(root, i);
            cJSON_DeleteItemFromObject(qos->qos_live_realtime.app_qos_json, child_json->string);
            cJSON_AddItemToObject(qos->qos_live_realtime.app_qos_json, child_json->string,
                                  cJSON_Duplicate(child_json, 1));
        }
    }
    SDL_UnlockMutex(qos->qos_live_realtime.app_qos_json_mutex);

    cJSON_Delete(root);
}

char* QosLiveRealtime_collect(FFPlayer* ffp, int first, int last,
                              int64_t start_time, int64_t duration, int64_t collectInterval) {
    if (!ffp) {
        return NULL;
    }

    KwaiQos* qos = &ffp->kwai_qos;

    KwaiQos_setBlockInfoStartPeriodIfNeed(qos);

    cJSON* play_info = cJSON_CreateObject();
    {
        // player version
        cJSON_AddStringToObject(play_info, "ver", qos->basic.sdk_version != NULL
                                ? qos->basic.sdk_version : "N/A");
        // system-info
        cJSON_AddNumberToObject(play_info, "memory_size",
                                qos->system_performance.last_process_memory_size_kb);
        cJSON_AddNumberToObject(play_info, "proc_cpu", qos->system_performance.last_process_cpu);
        cJSON_AddNumberToObject(play_info, "sys_cpu", qos->system_performance.last_system_cpu);

        // meta
        cJSON_AddStringToObject(play_info, "play_url", ffp_get_playing_url(ffp));
        cJSON_AddStringToObject(play_info, "domain",
                                ffp_get_property_string(ffp, FFP_PROP_STRING_DOMAIN));
        cJSON_AddStringToObject(play_info, "stream_id",
                                ffp_get_property_string(ffp, FFP_PROP_STRING_STREAM_ID));
        cJSON_AddStringToObject(play_info, "server_ip",
                                ffp_get_property_string(ffp, FFP_PROP_STRING_SERVER_IP));
        cJSON_AddStringToObject(play_info, "codec_a",
                                qos->media_metadata.audio_codec_info != NULL
                                ? qos->media_metadata.audio_codec_info : "N/A");
        cJSON_AddStringToObject(play_info, "codec_v",
                                qos->media_metadata.video_codec_info != NULL
                                ? qos->media_metadata.video_codec_info : "N/A");
        cJSON_AddStringToObject(play_info, "tsc_group",
                                qos->media_metadata.transcoder_group != NULL
                                ? qos->media_metadata.transcoder_group : "N/A");
        cJSON_AddStringToObject(play_info, "overlay_format",
                                KwaiQos_getOverlayOutputFormatString(qos->player_config.overlay_format));
        cJSON_AddNumberToObject(play_info, "last_high_water_mark",
                                qos->player_config.last_high_water_mark_in_ms);
        cJSON_AddNumberToObject(play_info, "v_width", ffp->is ? ffp->is->width : 0);
        cJSON_AddNumberToObject(play_info, "v_height", ffp->is ? ffp->is->height : 0);
        cJSON_AddNumberToObject(play_info, "fps", qos->media_metadata.fps);
        cJSON_AddNumberToObject(play_info, "source_device_type", ffp->source_device_type);
        cJSON_AddBoolToObject(play_info, "is_audio_only", ffp->is_audio_reloaded);
        cJSON_AddNumberToObject(play_info, "mix_type", ffp->mix_type);

        // Qos time&index info
        cJSON_AddNumberToObject(play_info, "play_start_time", 0); // place-holder, app最终
        cJSON_AddNumberToObject(play_info, "tick_start", start_time);
        cJSON_AddNumberToObject(play_info, "tick_duration", duration);
        cJSON_AddNumberToObject(play_info, "collect_interval", collectInterval);
        cJSON_AddNumberToObject(play_info, "index", qos->qos_live_realtime.index++);
        cJSON_AddNumberToObject(play_info, "first_report_flag", first);
        cJSON_AddNumberToObject(play_info, "last_report_flag", last);
        cJSON_AddNumberToObject(play_info, "start_on_prepared", ffp->start_on_prepared);

        // 卡顿率指标
        cJSON_AddNumberToObject(play_info, "retry_cnt",
                                0); // space take-up, will be update in App-layer
        cJSON_AddNumberToObject(play_info, "block_count", qos->runtime_stat.block_cnt -
                                qos->qos_live_realtime.block_count);
        qos->qos_live_realtime.block_count = qos->runtime_stat.block_cnt;
        cJSON_AddNumberToObject(play_info, "buffer_time", KwaiQos_getBufferTotalDurationMs(qos) -
                                qos->qos_live_realtime.buffer_time);
        qos->qos_live_realtime.buffer_time = KwaiQos_getBufferTotalDurationMs(qos);
        if (ffp->is) {
            cJSON_AddNumberToObject(play_info, "paused", ffp->is->paused);
            // live播放App主动pause播放器的监控
            cJSON_AddNumberToObject(play_info, "pause_req", ffp->is->pause_req);
        }
        cJSON_AddNumberToObject(play_info, "kbytes_received",
                                (ffp->is ? (int64_t)((ffp->is->bytes_read -
                                                      qos->qos_live_realtime.kbytes_read) / 1024) : 0));
        qos->qos_live_realtime.kbytes_read = ffp->is ? ffp->is->bytes_read : 0;
        // 起播卡顿
        if (last) {
            cJSON_AddNumberToObject(play_info, "block_cnt_start_period",
                                    qos->runtime_stat.block_cnt_start_period);
            cJSON_AddNumberToObject(play_info, "block_duration_start_period",
                                    qos->runtime_stat.block_duration_start_period);
        }
        // block info
        char* block_info = KwaiQos_getRealTimeBlockInfo(qos);
        cJSON_AddStringToObject(play_info, "block_info", block_info ? block_info : "[]");
        av_freep(&block_info);
        // audioonly时间戳跳变
        cJSON_AddNumberToObject(play_info, "audio_pts_jump_cnt", qos->runtime_stat.audio_pts_jump_forward_cnt -
                                qos->qos_live_realtime.audio_pts_jump_forward_cnt);
        qos->qos_live_realtime.audio_pts_jump_forward_cnt = qos->runtime_stat.audio_pts_jump_forward_cnt;
        cJSON_AddNumberToObject(play_info, "audio_pts_jump_dur", qos->runtime_stat.audio_pts_jump_forward_duration -
                                qos->qos_live_realtime.audio_pts_jump_forward_duration);
        qos->qos_live_realtime.audio_pts_jump_forward_duration = qos->runtime_stat.audio_pts_jump_forward_duration;
        cJSON_AddNumberToObject(play_info, "audio_pts_late_dur", get_audio_pts_jump_backward_time_gap(qos));

        // video时间戳回退
        cJSON_AddNumberToObject(play_info, "video_ts_rollback_cnt", qos->runtime_stat.video_ts_rollback_cnt -
                                qos->qos_live_realtime.video_ts_rollback_cnt);
        qos->qos_live_realtime.video_ts_rollback_cnt = qos->runtime_stat.video_ts_rollback_cnt;
        cJSON_AddNumberToObject(play_info, "video_ts_rollback_dur", qos->runtime_stat.video_ts_rollback_duration -
                                qos->qos_live_realtime.video_ts_rollback_duration);
        qos->qos_live_realtime.video_ts_rollback_duration = qos->runtime_stat.video_ts_rollback_duration;


        // a/v buffer时长、高低水位、同步状态
        cJSON_AddNumberToObject(play_info, "v_buf_len", ffp->stat.video_cache.duration);
        cJSON_AddNumberToObject(play_info, "a_buf_len", ffp->stat.audio_cache.duration);
        cJSON_AddNumberToObject(play_info, "cur_high_water_mark", ffp->dcc.current_high_water_mark_in_ms);
        cJSON_AddNumberToObject(play_info, "max_av_diff", (int)(1000 * ffp_get_property_float(ffp,
                                                                FFP_PROP_FLOAT_MAX_AVDIFF_REALTIME,
                                                                0.0)));
        cJSON_AddNumberToObject(play_info, "min_av_diff", (int)(1000 * ffp_get_property_float(ffp,
                                                                FFP_PROP_FLOAT_MIN_AVDIFF_REALTIME,
                                                                0.0)));

        // error：绿屏、花屏及其他错误
        cJSON_AddNumberToObject(play_info, "error_code", ffp->kwai_error_code);
        cJSON_AddNumberToObject(play_info, "h265_ps_change",
                                qos->runtime_stat.v_hevc_paramete_set_change_cnt -
                                qos->qos_live_realtime.v_hevc_paramete_set_change_cnt);
        qos->qos_live_realtime.v_hevc_paramete_set_change_cnt =
            qos->runtime_stat.v_hevc_paramete_set_change_cnt;
        cJSON_AddNumberToObject(play_info, "h265_ps_update_fail",
                                qos->runtime_stat.v_hevc_paramete_set_update_fail_cnt -
                                qos->qos_live_realtime.v_hevc_paramete_set_update_fail_cnt);
        qos->qos_live_realtime.v_hevc_paramete_set_update_fail_cnt =
            qos->runtime_stat.v_hevc_paramete_set_update_fail_cnt;

        if (qos->runtime_stat.v_hw_dec) {
            cJSON* hw_decode = cJSON_CreateObject();
            cJSON_AddItemToObject(play_info, "hw_decode", hw_decode);
            {
                cJSON_AddNumberToObject(hw_decode, "reset_session_cnt",
                                        qos->hw_decode.reset_session_cnt -
                                        qos->qos_live_realtime.reset_session_cnt);
                qos->qos_live_realtime.reset_session_cnt = qos->hw_decode.reset_session_cnt;
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
                                            qos->hw_decode.video_tool_box.pkt_cnt_on_err -
                                            qos->qos_live_realtime.pkt_cnt_on_err);
                    qos->qos_live_realtime.pkt_cnt_on_err = qos->hw_decode.video_tool_box.pkt_cnt_on_err;
                    cJSON_AddNumberToObject(video_tool_box, "q_is_full_cnt",
                                            qos->hw_decode.video_tool_box.queue_is_full_cnt -
                                            qos->qos_live_realtime.queue_is_full_cnt);
                    qos->qos_live_realtime.queue_is_full_cnt = qos->hw_decode.video_tool_box.queue_is_full_cnt;
                    cJSON_AddNumberToObject(video_tool_box, "res_change",
                                            qos->hw_decode.video_tool_box.resolution_change);
                    cJSON_AddNumberToObject(video_tool_box, "hw_dec_err_cnt",
                                            qos->runtime_stat.v_tool_box_err_cnt -
                                            qos->qos_live_realtime.v_tool_box_err_cnt);
                    qos->qos_live_realtime.v_tool_box_err_cnt = qos->runtime_stat.v_tool_box_err_cnt;
                }
#elif defined(__ANDROID__)
                cJSON* media_codec = cJSON_CreateObject();
                cJSON_AddItemToObject(hw_decode, "mediacodec", media_codec);
                {
                    cJSON_AddNumberToObject(media_codec, "input_err_cnt",
                                            qos->runtime_stat.v_mediacodec_input_err_cnt -
                                            qos->qos_live_realtime.v_mediacodec_input_err_cnt);
                    qos->qos_live_realtime.v_mediacodec_input_err_cnt = qos->runtime_stat.v_mediacodec_input_err_cnt;
                    cJSON_AddNumberToObject(media_codec, "input_err",
                                            qos->runtime_stat.v_mediacodec_input_err_code);
                    cJSON_AddNumberToObject(media_codec, "output_try_again_err_cnt",
                                            qos->runtime_stat.v_mediacodec_output_try_again_err_cnt -
                                            qos->qos_live_realtime.v_mediacodec_output_try_again_err_cnt);
                    qos->qos_live_realtime.v_mediacodec_output_try_again_err_cnt =
                        qos->runtime_stat.v_mediacodec_output_try_again_err_cnt;
                    cJSON_AddNumberToObject(media_codec, "output_buffer_changed_err_cnt",
                                            qos->runtime_stat.v_mediacodec_output_buffer_changed_err_cnt -
                                            qos->qos_live_realtime.v_mediacodec_output_buffer_changed_err_cnt);
                    qos->qos_live_realtime.v_mediacodec_output_buffer_changed_err_cnt =
                        qos->runtime_stat.v_mediacodec_output_buffer_changed_err_cnt;
                    cJSON_AddNumberToObject(media_codec, "output_unknown_err_cnt",
                                            qos->runtime_stat.v_mediacodec_output_unknown_err_cnt -
                                            qos->qos_live_realtime.v_mediacodec_output_unknown_err_cnt);
                    qos->qos_live_realtime.v_mediacodec_output_unknown_err_cnt =
                        qos->runtime_stat.v_mediacodec_output_unknown_err_cnt;
                    cJSON_AddNumberToObject(media_codec, "output_err_cnt",
                                            qos->runtime_stat.v_mediacodec_output_err_cnt -
                                            qos->qos_live_realtime.v_mediacodec_output_err_cnt);
                    qos->qos_live_realtime.v_mediacodec_output_err_cnt = qos->runtime_stat.v_mediacodec_output_err_cnt;
                    cJSON_AddNumberToObject(media_codec, "output_err",
                                            qos->runtime_stat.v_mediacodec_output_err_code);
                    cJSON_AddStringToObject(media_codec, "config_type",
                                            qos->runtime_stat.v_mediacodec_config_type);
                    cJSON_AddNumberToObject(media_codec, "max_cnt",
                                            qos->runtime_stat.v_mediacodec_codec_max_cnt);
                }
#endif
            }
        } else {
            cJSON_AddNumberToObject(play_info, "v_dec_errors",
                                    ffp->error_count - qos->qos_live_realtime.dec_err_cnt);
            qos->qos_live_realtime.dec_err_cnt = ffp->error_count;
        }

        // frame drop-rate
        cJSON_AddNumberToObject(play_info, "speed_up_threshold",
                                ffp->is_live_manifest ? ffp->i_buffer_time_max_live_manifest : ffp->i_buffer_time_max);
        cJSON_AddNumberToObject(play_info, "dropped_packet_duration",
                                qos->runtime_stat.total_dropped_duration -
                                qos->qos_live_realtime.dropped_packet_duration);
        qos->qos_live_realtime.dropped_packet_duration = qos->runtime_stat.total_dropped_duration;
        cJSON_AddNumberToObject(play_info, "read_video_frames",
                                qos->runtime_stat.v_read_frame_count -
                                qos->qos_live_realtime.read_video_frames);
        qos->qos_live_realtime.read_video_frames = qos->runtime_stat.v_read_frame_count;
        cJSON_AddNumberToObject(play_info, "decoded_video_frames",
                                qos->runtime_stat.v_decode_frame_count -
                                qos->qos_live_realtime.decoded_video_frames);
        qos->qos_live_realtime.decoded_video_frames = qos->runtime_stat.v_decode_frame_count;
        cJSON_AddNumberToObject(play_info, "rendered_video_frames",
                                qos->runtime_stat.render_frame_count -
                                qos->qos_live_realtime.rendered_video_frames);
        qos->qos_live_realtime.rendered_video_frames = qos->runtime_stat.render_frame_count;
        cJSON_AddNumberToObject(play_info, "rendered_audio_frames",
                                qos->runtime_stat.render_sample_count -
                                qos->qos_live_realtime.rendered_audio_frames);
        qos->qos_live_realtime.rendered_audio_frames = qos->runtime_stat.render_sample_count;
        cJSON_AddNumberToObject(play_info, "silence_audio_frames",
                                qos->runtime_stat.silence_sample_count -
                                qos->qos_live_realtime.silence_audio_frames);
        qos->qos_live_realtime.silence_audio_frames = qos->runtime_stat.silence_sample_count;

        // 端到端延迟，原来在java层处理
        cJSON_AddNumberToObject(play_info, "a_render_delay",
                                get_delay_stat_avg_value(ffp->qos_delay_audio_render,
                                                         qos->qos_live_realtime.a_render_delay));
        qos->qos_live_realtime.a_render_delay = ffp->qos_delay_audio_render;
        cJSON_AddNumberToObject(play_info, "v_recv_delay",
                                get_delay_stat_avg_value(ffp->qos_delay_video_recv,
                                                         qos->qos_live_realtime.v_recv_delay));
        qos->qos_live_realtime.v_recv_delay = ffp->qos_delay_video_recv;
        cJSON_AddNumberToObject(play_info, "v_pre_dec_delay",
                                get_delay_stat_avg_value(ffp->qos_delay_video_before_dec,
                                                         qos->qos_live_realtime.v_pre_dec_delay));
        qos->qos_live_realtime.v_pre_dec_delay = ffp->qos_delay_video_before_dec;
        cJSON_AddNumberToObject(play_info, "v_post_dec_delay",
                                get_delay_stat_avg_value(ffp->qos_delay_video_after_dec,
                                                         qos->qos_live_realtime.v_post_dec_delay));
        qos->qos_live_realtime.v_post_dec_delay = ffp->qos_delay_video_after_dec;
        cJSON_AddNumberToObject(play_info, "v_render_delay",
                                get_delay_stat_avg_value(ffp->qos_delay_video_render,
                                                         qos->qos_live_realtime.v_render_delay));
        qos->qos_live_realtime.v_render_delay = ffp->qos_delay_video_render;

        // speed change, 原来在java层处理的
        {
            cJSON* speed_change = cJSON_CreateObject();
            cJSON_AddItemToObject(play_info, "speed_chg_metric", speed_change);
            {
                int delta_up_2_duration = ffp->qos_speed_change.up_2_duration -
                                          qos->qos_live_realtime.speed_change_stat.up_2_duration;
                cJSON_AddNumberToObject(speed_change, "1.50x", delta_up_2_duration);
                qos->qos_live_realtime.speed_change_stat.up_2_duration = ffp->qos_speed_change.up_2_duration;

                int delta_up_1_duration = ffp->qos_speed_change.up_1_duration -
                                          qos->qos_live_realtime.speed_change_stat.up_1_duration;
                cJSON_AddNumberToObject(speed_change, "1.25x", delta_up_1_duration);
                qos->qos_live_realtime.speed_change_stat.up_1_duration = ffp->qos_speed_change.up_1_duration;

                int delta_down_duration = ffp->qos_speed_change.down_duration -
                                          qos->qos_live_realtime.speed_change_stat.down_duration;
                cJSON_AddNumberToObject(speed_change, "0.75x", delta_down_duration);
                qos->qos_live_realtime.speed_change_stat.down_duration = ffp->qos_speed_change.down_duration;

                cJSON_AddNumberToObject(speed_change, "1.00x", duration -
                                        delta_up_2_duration - delta_up_1_duration - delta_down_duration);
            }
        }

        // 首屏以及细分
        if (qos->qos_live_realtime.got_first_screen == 0) {

            cJSON* rt_cost = cJSON_CreateObject();
            cJSON_AddItemToObject(play_info, "rt_cost", rt_cost);
            {
                cJSON_AddNumberToObject(rt_cost, "first_screen",
                                        FFMAX(qos->runtime_cost.cost_first_screen, 0));

                // input_open steps
                cJSON_AddNumberToObject(rt_cost, "dns_analyze",
                                        FFMAX(qos->runtime_cost.cost_dns_analyze, 0));
                cJSON_AddNumberToObject(rt_cost, "http_connect",
                                        FFMAX(qos->runtime_cost.cost_http_connect, 0));
                cJSON_AddNumberToObject(rt_cost, "http_first_data",
                                        FFMAX(qos->runtime_cost.cost_http_first_data, 0));

                // http_redirect
                char* http_redirect = ffp_get_property_string(ffp,
                                                              FFP_PROP_STRING_HTTP_REDIRECT_INFO);
                if (http_redirect) {
                    cJSON_AddItemToObject(rt_cost, "http_redirect",
                                          cJSON_CreateString(http_redirect));
                }

                // first_screen steps
                cJSON* step = cJSON_CreateObject();
                cJSON_AddItemToObject(rt_cost, "step", step);
                {
                    cJSON_AddNumberToObject(step, "input_open",
                                            FFMAX(qos->runtime_cost.step_av_input_open, 0));
                    cJSON_AddNumberToObject(step, "find_stream_info",
                                            FFMAX(qos->runtime_cost.step_av_find_stream_info, 0));
                    cJSON_AddNumberToObject(step, "dec_opened",
                                            FFMAX(qos->runtime_cost.step_open_decoder, 0));
                    cJSON_AddNumberToObject(step, "all_prepared",
                                            FFMAX(qos->runtime_cost.step_all_prepared, 0));
                    cJSON_AddNumberToObject(step, "wait_for_play",
                                            FFMAX(qos->runtime_cost.cost_wait_for_playing,
                                                  0));
                    cJSON_AddNumberToObject(step, "fst_v_pkt_recv",
                                            FFMAX(qos->runtime_cost.step_first_video_pkt_received,
                                                  0));
                    cJSON_AddNumberToObject(step, "fst_v_pkt_pre_dec",
                                            FFMAX(qos->runtime_cost.step_pre_decode_first_video_pkt,
                                                  0));
                    cJSON_AddNumberToObject(step, "fst_v_pkt_dec",
                                            FFMAX(qos->runtime_cost.step_decode_first_frame, 0));
                    cJSON_AddNumberToObject(step, "fst_v_render",
                                            FFMAX(qos->runtime_cost.step_first_framed_rendered, 0));
                }
            }

            // 首个音频包渲染
            cJSON_AddNumberToObject(play_info, "first_a_sample",
                                    FFMAX(qos->runtime_cost.cost_first_sample, 0));

            if (qos->runtime_cost.step_av_input_open > 0) {
                cJSON_AddStringToObject(play_info, "kwai_sign", get_kwaisign(ffp));
                cJSON_AddStringToObject(play_info, "x_ks_cache", get_x_ks_cache(ffp));
            }

            if (KwaiQos_getFirstScreenCostMs(qos) > 0) {
                if (ffp->is && ffp->is->video_st) {
                    if (qos->runtime_cost.cost_first_screen > 0) {
                        qos->qos_live_realtime.got_first_screen = 1;
                    }
                } else {
                    if (qos->runtime_cost.cost_first_sample > 0) {
                        qos->qos_live_realtime.got_first_screen = 1;
                    }
                }
            }
        }

        // 直播多码率相关
        cJSON_AddBoolToObject(play_info, "is_live_manifest", ffp->is_live_manifest == 1);
        if (ffp->is_live_manifest) {
            cJSON_AddNumberToObject(play_info, "switch_time_gap",
                                    ffp->kflv_player_statistic.kflv_stat.rep_switch_gap_time);
            cJSON_AddNumberToObject(play_info, "switch_point_a_buf_time",
                                    ffp->kflv_player_statistic.kflv_stat.switch_point_a_buffer_ms);
            cJSON_AddNumberToObject(play_info, "switch_point_v_buf_time",
                                    ffp->kflv_player_statistic.kflv_stat.switch_point_v_buffer_ms);
            cJSON_AddNumberToObject(play_info, "cached_tag_dur_ms",
                                    ffp->kflv_player_statistic.kflv_stat.cached_tag_dur_ms);
            cJSON_AddNumberToObject(play_info, "switch_cnt",
                                    ffp->kflv_player_statistic.kflv_stat.rep_switch_cnt -
                                    qos->qos_live_realtime.rep_switch_cnt);
            qos->qos_live_realtime.rep_switch_cnt = ffp->kflv_player_statistic.kflv_stat.rep_switch_cnt;

            cJSON_AddNumberToObject(play_info, "switch_flag", KwaiQos_getLiveManifestSwitchFlag(qos));

            cJSON_AddNumberToObject(play_info, "cur_rep_http_reading_error",
                                    KFlvPlayerStatistic_get_http_reading_error(&ffp->kflv_player_statistic));

            cJSON_AddNumberToObject(play_info, "cur_rep_http_open_time",
                                    ffp->kflv_player_statistic.kflv_stat.cur_rep_http_open_time);

            cJSON_AddNumberToObject(play_info, "cur_rep_flv_header_time",
                                    ffp->kflv_player_statistic.kflv_stat.cur_rep_read_header_time);

            int64_t cur_rep_read_start_time = ffp->kflv_player_statistic.kflv_stat.cur_rep_read_start_time;
            if (qos->qos_live_realtime.cur_rep_read_start_time != cur_rep_read_start_time) {
                if (cur_rep_read_start_time == 0)
                    qos->qos_live_realtime.adaptive_gop_info_collect_cnt = 0;

                long cur_rep_first_data_time = ffp->kflv_player_statistic.kflv_stat.cur_rep_first_data_time;
                if (cur_rep_first_data_time == 0) {
                    cJSON_AddNumberToObject(play_info, "cur_rep_first_data_time",
                                            (qos->qos_live_realtime.adaptive_gop_info_collect_cnt +
                                             1) * 10);
                    qos->qos_live_realtime.adaptive_gop_info_collect_cnt++;
                } else {
                    qos->qos_live_realtime.cur_rep_first_data_time =
                        cur_rep_first_data_time - cur_rep_read_start_time;
                    cJSON_AddNumberToObject(play_info, "cur_rep_first_data_time",
                                            qos->qos_live_realtime.cur_rep_first_data_time);
                    cJSON_AddNumberToObject(play_info, "cur_rep_switch_time",
                                            qos->qos_live_realtime.cur_rep_first_data_time);
                    qos->qos_live_realtime.adaptive_gop_info_collect_cnt = 0;
                    qos->qos_live_realtime.cur_rep_read_start_time = cur_rep_read_start_time;
                }
            } else {
                cJSON_AddNumberToObject(play_info, "cur_rep_first_data_time",
                                        qos->qos_live_realtime.cur_rep_first_data_time);
                cJSON_AddNumberToObject(play_info, "cur_rep_switch_time", 0);
            }

            cJSON_AddNumberToObject(play_info, "bitrate_downloading",
                                    KFlvPlayerStatistic_get_downloading_bitrate(&ffp->kflv_player_statistic));
            cJSON_AddNumberToObject(play_info, "bitrate_playing",
                                    KFlvPlayerStatistic_get_playing_bitrate(&ffp->kflv_player_statistic));
        }

        // p2sp
        AwesomeCacheRuntimeInfo* ac_rt_info = &ffp->cache_stat.ac_runtime_info;
        cJSON_AddStringToObject(play_info, "ac_type_str", get_cache_type(ffp));
        cJSON_AddNumberToObject(play_info, "p2sp_enabled", ac_rt_info->p2sp_task.enabled);
        if (ac_rt_info->p2sp_task.enabled) {
            // used_bytes
            cJSON_AddNumberToObject(play_info, "p2sp_used_bytes",
                                    ac_rt_info->p2sp_task.p2sp_used_bytes -
                                    qos->qos_live_realtime.p2sp_used_bytes);
            qos->qos_live_realtime.p2sp_used_bytes = ac_rt_info->p2sp_task.p2sp_used_bytes;
            cJSON_AddNumberToObject(play_info, "cdn_used_bytes",
                                    ac_rt_info->p2sp_task.cdn_used_bytes -
                                    qos->qos_live_realtime.cdn_used_bytes);
            qos->qos_live_realtime.cdn_used_bytes = ac_rt_info->p2sp_task.cdn_used_bytes;

            // bytes_count
            cJSON_AddNumberToObject(play_info, "p2sp_bytes_count",
                                    ac_rt_info->p2sp_task.p2sp_download_bytes -
                                    qos->qos_live_realtime.p2sp_download_bytes);
            qos->qos_live_realtime.p2sp_download_bytes = ac_rt_info->p2sp_task.p2sp_download_bytes;
            cJSON_AddNumberToObject(play_info, "cdn_bytes_count",
                                    ac_rt_info->p2sp_task.cdn_download_bytes -
                                    qos->qos_live_realtime.cdn_download_bytes);
            qos->qos_live_realtime.cdn_download_bytes = ac_rt_info->p2sp_task.cdn_download_bytes;

            // switch_attempts
            cJSON_AddNumberToObject(play_info, "p2sp_switch_attempts",
                                    ac_rt_info->p2sp_task.p2sp_switch_attempts -
                                    qos->qos_live_realtime.p2sp_switch_attempts);
            qos->qos_live_realtime.p2sp_switch_attempts = ac_rt_info->p2sp_task.p2sp_switch_attempts;
            cJSON_AddNumberToObject(play_info, "cdn_switch_attempts",
                                    ac_rt_info->p2sp_task.cdn_switch_attempts -
                                    qos->qos_live_realtime.cdn_switch_attempts);
            qos->qos_live_realtime.cdn_switch_attempts = ac_rt_info->p2sp_task.cdn_switch_attempts;

            // switch_success_attempts
            cJSON_AddNumberToObject(play_info, "p2sp_switch_success_attempts",
                                    ac_rt_info->p2sp_task.p2sp_switch_success_attempts -
                                    qos->qos_live_realtime.p2sp_switch_success_attempts);
            qos->qos_live_realtime.p2sp_switch_success_attempts = ac_rt_info->p2sp_task.p2sp_switch_success_attempts;
            cJSON_AddNumberToObject(play_info, "cdn_switch_success_attempts",
                                    ac_rt_info->p2sp_task.cdn_switch_success_attempts -
                                    qos->qos_live_realtime.cdn_switch_success_attempts);
            qos->qos_live_realtime.cdn_switch_success_attempts = ac_rt_info->p2sp_task.cdn_switch_success_attempts;

            // switch_duration_ms
            cJSON_AddNumberToObject(play_info, "p2sp_switch_duration_ms",
                                    ac_rt_info->p2sp_task.p2sp_switch_duration_ms -
                                    qos->qos_live_realtime.p2sp_switch_duration_ms);
            qos->qos_live_realtime.p2sp_switch_duration_ms = ac_rt_info->p2sp_task.p2sp_switch_duration_ms;
            cJSON_AddNumberToObject(play_info, "cdn_switch_duration_ms",
                                    ac_rt_info->p2sp_task.cdn_switch_duration_ms -
                                    qos->qos_live_realtime.cdn_switch_duration_ms);
            qos->qos_live_realtime.cdn_switch_duration_ms = ac_rt_info->p2sp_task.cdn_switch_duration_ms;
        }

        SDL_LockMutex(qos->qos_live_realtime.app_qos_json_mutex);
        //AppQosInfo
        cJSON* appQosInfo = qos->qos_live_realtime.app_qos_json;
        if (appQosInfo && cJSON_Object == appQosInfo->type) {
            int len = cJSON_GetArraySize(appQosInfo);
            for (int i = 0; i < len; i++) {
                cJSON* child_json = cJSON_GetArrayItem(appQosInfo, i);
                cJSON_DeleteItemFromObject(play_info, child_json->string);
                cJSON_AddItemToObject(play_info, child_json->string, cJSON_Duplicate(child_json, 1));
            }
        }
        SDL_UnlockMutex(qos->qos_live_realtime.app_qos_json_mutex);
    }

    char* out = cJSON_Print(play_info);
    cJSON_Delete(play_info);

    return out;
}
