//
// Created by MarshallShuai on 2019/4/19.
//
#include "ff_ffplay_module_read_thread.h"

#include <libavformat/avformat.h>
#include <libavformat/url.h>
#include "ijkkwai/shared/kwai_priv_nal_c.h"
#include <ijkmedia/ijkplayer/ijkavformat/ijkavformat.h>

#if defined(CONFIG_KS_AUDIOPROCESS) || defined(__APPLE__)
#include <ijkmedia/ijkkwai/c_audio_process.h>
#endif

#include "ff_cmdutils.h"
#include "ff_fferror.h"
#include "ff_ffplay_def.h"
#include "ff_ffplay_internal.h"
#include "ff_ffplay.h"

#include "ijkkwai/kwai_error_code_manager.h"
#include "ijkkwai/kwai_priv_aac_parser.h"
#include "ijkkwai/kwai_error_code_manager_ff_convert.h"
#include "ff_ffplay_clock.h"
#include "ijkkwai/kwai_live_delay_stat.h"
#include "ijkkwai/kwai_io_queue_observer.h"


// audio want to be the master stream to be synced, the video_duration/audio_duration should be smaller than following value.
#define AUDIO_AS_MASTER_TYPE_MIN_AV_TIMES (10)
// audio want to be the master stream to be synced, it should be longer than following value.
#define AUDIO_AS_MASTER_TYPE_MIN_DURATION (1 * AV_TIME_BASE)

// when stream is about to end, it will seek to start only when the time to the end of stream is smaller than following value(s)
#define STREAM_SEEK_TO_START_THRESHOLD (0.1)

// cache seek的一个阈值，小于这个阈值则不会触发cache seek
#define SEEK_CACHE_MIN_PACKETS      50

// check_stream_complete_status return value
#define STREAM_COMPLETE_STATUS_NORMAL     0  // read_thread继续执行
#define STREAM_COMPLETE_STATUS_SEEK       1  // 播放到结尾后有seek操作，read_thread continue
#define STREAM_COMPLETE_STATUS_EXIT       2  // 异常，退出read_thread

/**
 * read thread的有时间过滤逻辑的 ffp_check_buffering
 */
static void ffp_read_thread_check_buffering(FFPlayer* ffp, bool is_eof) {
    if (is_eof && ffp->buffer_update_control.eof_reported) {
        return;
    }
    ffp->buffer_update_control.eof_reported = is_eof;

    VideoState* is = ffp->is;
    if (ffp->packet_buffering) {
        int64_t cur_ts_ms = SDL_GetTickHR();
        int64_t diff = cur_ts_ms - ffp->buffer_update_control.last_check_buffering_ts_ms;

        if (!ffp->enable_modify_block) {
            if (abs((int)(diff)) > BUFFERING_CHECK_PER_MILLISECONDS) {
                ffp_check_buffering_l(ffp, is_eof);
            } else if (is->buffering_on) {
                ffp_check_buffering_l(ffp, is_eof);
            } else if (is_eof) {
                // eof的时候需要稳定check
                ALOGV("%s, is_eof = true", __func__);
                ffp_check_buffering_l(ffp, is_eof);
            }
        } else {
            if (is->buffering_on) {
                ffp_check_buffering_l(ffp, is_eof);
            } else if (is_eof) {
                // eof的时候需要稳定check
                ALOGV("%s, is_eof = true", __func__);
                ffp_check_buffering_l(ffp, is_eof);
            }
        }

        ffp->buffer_update_control.last_check_buffering_ts_ms = cur_ts_ms;
    }
}

static int decode_interrupt_cb(void* ctx) {
    FFPlayer* ffp = ctx;
    if (!ffp) {
        ALOGI("[decode_interrupt_cb], ffp = null!");
        return -1;
    }

    VideoState* is = ffp->is;
    if (!is) {
        ALOGI("[%u][decode_interrupt_cb], is = null!", ffp->session_id);
        return 1;
    }

    if (is->abort_request) {
        ALOGI("[%u][decode_interrupt_cb], is->abort_request = %d", ffp->session_id,
              is->abort_request);  // 重要日志，勿删
    }

    if ((is->paused == 0 || is->buffering_on) && is->read_start_time > 0 && (av_gettime_relative() - is->read_start_time) > ffp->timeout) {
        is->interrupt_exit = 1;
        ALOGI("[%u] decode_interrupt_cb interrupt_exit true timeout=%lld.", ffp->session_id,
              ffp->timeout);
        return 1;
    }

    if (is->interrupt_exit) {
        ALOGI("[%u] decode_interrupt_cb interrupt_exit true exit.", ffp->session_id);
        return 1;
    }

    if (is->abort_request || is->read_abort_request)
        ALOGI("[%u] decode_interrupt_cb, abort_request=%d, read_abort_request=%d\n", ffp->session_id, is->abort_request, is->read_abort_request);

    return (is->abort_request || is->read_abort_request);
}

static int is_realtime(AVFormatContext* s) {
    if (!strcmp(s->iformat->name, "rtp")
        || !strcmp(s->iformat->name, "rtsp")
        || !strcmp(s->iformat->name, "sdp")
       )
        return 1;

    if (s->pb && (!strncmp(s->filename, "rtp:", 4)
                  || !strncmp(s->filename, "udp:", 4)
                  || !strncmp(s->filename, "rtmp:", 5)
                 )
       )
        return 1;
    return 0;
}

// 获取rtmp直播流里的信息 ip/domain/stream_id等，主要用来获取ip，其他字段暂时不重要
static void kwai_collect_live_http_context_info_if_needed(FFPlayer* ffp, AVFormatContext* ic) {
    if (!ffp || !ffp->is) {
        return;
    }
    VideoState* is = ffp->is;

    if (!ffp->cache_actually_used
        && (av_stristart(is->filename, "http", NULL) || av_stristart(is->filename, "rtmp", NULL))) {
        if (ic != NULL && ic->pb != NULL) {
            URLContext* context = (URLContext*)(ic->pb->opaque);
            if (context != NULL && context->prot != NULL && context->prot->name != NULL) {
                URLContext* tcpContext = NULL;
                if (strcmp(context->prot->name, "rtmp") == 0) {
                    tcpContext = (URLContext*) qyrtmp_get_tcpstream(context);
                    char* streamId = qyrtmp_get_stream_id(context);
                    char* domain = qyrtmp_get_domain(context);
                    KwaiQos_setStreamId(&ffp->kwai_qos, streamId);
                    KwaiQos_setDomain(&ffp->kwai_qos, domain);
                } else if (strcmp(context->prot->name, "http") == 0) {
                    tcpContext = (URLContext*) qyhttp_get_tcpstream(context);
                }

                if (tcpContext != NULL) {
                    char* ip = ff_qytcp_get_ip(tcpContext);
                    if (ip != NULL) {
                        av_freep(&is->server_ip);
                        is->server_ip = av_strdup(ip);
                    }
                }
            }
        }
    }
}

static void kwai_collect_http_meta_info(FFPlayer* ffp, AVFormatContext* ic) {
    if (!ffp) {
        return;
    }

    int tinfo_connect_time = -1;
    int tinfo_first_data_time = -1;
    int dns_time = -1;
    int http_code = -1;
    char const* server_ip = NULL;
    char const* kwaisign = NULL;
    char const* x_ks_cache = NULL;
    char const* kwai_http_redirect = NULL;

    AVDictionaryEntry* entry = NULL;

    AVDictionaryEntry* tinfo_cache = NULL;
    AVDictionaryEntry* tinfo_redirect = NULL;
    AVDictionaryEntry* tinfo_content_range = NULL;
    AVDictionaryEntry* tinfo_content_length = NULL;

    if (ffp->cache_actually_used) {
        // If cache is enabled, use ac_runtime_info before format_opts
        AwesomeCacheRuntimeInfo* qos = &ffp->cache_stat.ac_runtime_info;
        KwaiQos_updateConnectInfo(&ffp->kwai_qos, qos);

        kwaisign = qos->download_task.kwaisign;
        x_ks_cache = qos->download_task.x_ks_cache;
        server_ip = qos->download_task.resolved_ip;
        //server_ip 使用最后一次链接ip，方便定位问题,时间统计第一次链接耗时，用于首屏分析
        if (strlen(qos->connect_infos[0].resolved_ip) != 0) {
            dns_time = qos->connect_infos[0].http_dns_analyze_ms;
            tinfo_connect_time = qos->connect_infos[0].http_connect_ms;
            tinfo_first_data_time = qos->connect_infos[0].http_first_data_ms;
        } else {
            dns_time = qos->download_task.http_dns_analyze_ms;
            tinfo_connect_time = qos->download_task.http_connect_ms;
            tinfo_first_data_time = qos->download_task.http_first_data_ms;
        }
    } else {
        AVDictionary* dict = (ffp->is_live_manifest && ic) ? ic->metadata : ffp->format_opts;

        if ((entry = av_dict_get(dict, "analyze_dns_time", NULL, 0))) {
            dns_time = atoi(entry->value);
        }
        if ((entry = av_dict_get(dict, "http_code", NULL, 0))) {
            http_code = atoi(entry->value);
        }
        if ((entry = av_dict_get(dict, "connect_time", NULL, AV_DICT_IGNORE_SUFFIX))) {
            tinfo_connect_time = atoi(entry->value);
        }
        if ((entry = av_dict_get(dict, "first_data_time", NULL, AV_DICT_IGNORE_SUFFIX))) {
            tinfo_first_data_time = atoi(entry->value);
        }
        if ((entry = av_dict_get(dict, "server_ip", NULL, AV_DICT_IGNORE_SUFFIX))) {
            server_ip = entry->value;
        }
        if ((entry = av_dict_get(dict, "kwaisign", NULL, 0))) {
            kwaisign = entry->value;
        }
        if (ffp->islive || ffp->is_live_manifest) {
            if ((entry = av_dict_get(dict, "x_ks_cache", NULL, 0))) {
                x_ks_cache = entry->value;
            }
            if ((entry = av_dict_get(dict, "kwai_http_redirect", NULL, 0))) {
                kwai_http_redirect = entry->value;
            }
        }
    }

    if (dns_time != -1) {
        ijkmeta_set_int64_l(ffp->meta, IJKM_KEY_HTTP_ANALYZE_DNS, dns_time);
        KwaiQos_setDnsAnalyzeCostMs(&ffp->kwai_qos, dns_time);
    }

    if (server_ip) {
        av_freep(&ffp->is->server_ip);
        ffp->is->server_ip = av_strdup(server_ip);
    }

    if (http_code != -1)
        ijkmeta_set_int64_l(ffp->meta, IJKM_KEY_HTTP_CODE, http_code);

    if (kwaisign)
        av_strlcpy(ffp->live_kwai_sign, kwaisign, CDN_KWAI_SIGN_MAX_LEN);

    if (x_ks_cache)
        av_strlcpy(ffp->live_x_ks_cache, x_ks_cache, CDN_X_KS_CACHE_MAX_LEN);

    if (kwai_http_redirect) {
        if (ffp->http_redirect_info) {
            av_freep(&ffp->http_redirect_info);
        }

        ffp->http_redirect_info = av_strdup(kwai_http_redirect);
    }

    if ((tinfo_cache = av_dict_get(ffp->format_opts, "http_x_cache", NULL, AV_DICT_IGNORE_SUFFIX)))
        ijkmeta_set_string_l(ffp->meta, IJKM_KEY_HTTP_X_CACHE, tinfo_cache->value);
    if ((tinfo_redirect = av_dict_get(ffp->format_opts, "http_redirect", NULL,
                                      AV_DICT_IGNORE_SUFFIX)))
        ijkmeta_set_string_l(ffp->meta, IJKM_KEY_HTTP_REDIRECT, tinfo_redirect->value);
    if ((tinfo_content_range = av_dict_get(ffp->format_opts, "http_content_range", NULL,
                                           AV_DICT_IGNORE_SUFFIX)))
        ijkmeta_set_string_l(ffp->meta, IJKM_KEY_HTTP_CONTENT_RANGE, tinfo_content_range->value);
    if ((tinfo_content_length = av_dict_get(ffp->format_opts, "http_content_length", NULL,
                                            AV_DICT_IGNORE_SUFFIX)))
        ijkmeta_set_string_l(ffp->meta, IJKM_KEY_HTTP_CONTENT_LENGHT, tinfo_content_length->value);

    if (tinfo_connect_time != -1) {
        ijkmeta_set_int64_l(ffp->meta, IJKM_KEY_HTTP_CONNECT_TIME, tinfo_connect_time);
        KwaiQos_setHttpConnectCostMs(&ffp->kwai_qos, tinfo_connect_time);
    }
    if (tinfo_first_data_time != -1) {
        ijkmeta_set_int64_l(ffp->meta, IJKM_KEY_HTTP_FIRST_DATA_TIME,
                            tinfo_first_data_time);
        KwaiQos_setHttpFirstDataCostMs(&ffp->kwai_qos, tinfo_first_data_time);
    }

    // fast fix av_dict_get error,暂时也用不到streamId
//    if (ic && (streamId = av_dict_get(ic->metadata, "streamId", NULL, AV_DICT_IGNORE_SUFFIX)))
//        ijkmeta_set_string_l(ffp->meta, IJKM_KEY_STREAMID, streamId->value);
}

void kwai_collect_AVStream_info_to_video_state(VideoState* is, AVFormatContext* ic, int audio_stream, int video_stream) {

    if (!ic || !is) {
        ALOGE("[%s] invalid state, some thing is null, ic:%p is:%p\n",
              __func__, ic, is);
        return;
    }

    if (video_stream >= 0) {
        AVStream* video_st = ic->streams[video_stream];
        //get probe fps
        if (video_st->avg_frame_rate.den && video_st->avg_frame_rate.num) {
            is->probe_fps = (float) av_q2d(video_st->avg_frame_rate);
        }
        is->video_duration = av_rescale_q(video_st->duration, video_st->time_base,
                                          AV_TIME_BASE_Q);
    }

    if (audio_stream >= 0) {
        AVStream* audio_st = ic->streams[audio_stream];
        is->audio_duration = av_rescale_q(audio_st->duration, audio_st->time_base,
                                          AV_TIME_BASE_Q);
    }

    if (is->audio_duration != AV_NOPTS_VALUE && is->video_duration != AV_NOPTS_VALUE) {
        if (is->audio_duration < AUDIO_AS_MASTER_TYPE_MIN_DURATION
            && is->audio_duration != 0
            && (is->video_duration / is->audio_duration) > AUDIO_AS_MASTER_TYPE_MIN_AV_TIMES) {
            is->av_aligned = 0;
        }
    }
}


static void ffp_check_use_vod_buffer_checker(FFPlayer* ffp) {
    if (ffp->islive) {
        //直播设置成true后，默认用的策略是kStrategyStartPlayBlockByNone,随时可以起播
        //上层启用mUseSpbBuffer，策略会改成kStrategyStartPlayBlockByTimeMs，才会使用起播buffer
        KwaiPacketQueueBufferChecker_set_enable(&ffp->kwai_packet_buffer_checker, true, kStartPlayCheckeDisableNone);
    } else if (!ffp->cache_actually_used) {
        KwaiPacketQueueBufferChecker_set_enable(&ffp->kwai_packet_buffer_checker, false, kStartPlayCheckDisableCacheNotUsed);
    } else {
        KwaiPacketQueueBufferChecker_set_enable(&ffp->kwai_packet_buffer_checker, true, kStartPlayCheckeDisableNone);
    }
}

static void ffp_check_use_dcc_algorithm(FFPlayer* ffp, AVFormatContext* ic) {
    DccAlgorithm* alg = &ffp->dcc_algorithm;
    if (!alg->config_enabled || !ic) {
        snprintf(alg->status, DCC_ALG_STATUS_MAX_LEN, "%s", "未开启 | it not enabled in config");
        return;
    }

    ffp->dcc.max_buffer_dur_ms =
        DccAlgorithm_get_pre_read_duration_ms(alg,
                                              ffp->dcc.max_buffer_dur_ms,
                                              fftime_to_milliseconds(ic->duration),
                                              ic->bit_rate / 1024);
}

static void ffp_ac_player_statistic_set_initial(FFPlayer* ffp, AVFormatContext* ic) {
    ac_player_statistic_set_bitrate(ffp->player_statistic, (int)ic->bit_rate);
    ac_player_statistic_set_pre_read(ffp->player_statistic, ffp->dcc.max_buffer_dur_ms);
    ac_player_statistic_update(ffp->player_statistic);
}

static int kwai_cache_seek(FFPlayer* ffp, int64_t seek_target, int64_t seek_min, int64_t seek_max) {
    int ret = -1;
    VideoState* is = ffp->is;

    if (!ffp->enable_cache_seek)
        return ret;

    int64_t seek_timestamp = 0;
    double seek_point_pts_time = 0.0;
    int64_t pts;

    if (is->video_stream >= 0) {
        seek_timestamp = av_rescale_q(seek_target, AV_TIME_BASE_Q,
                                      is->ic->streams[is->video_stream]->time_base);
        pts = packet_queue_get_first_packet_pts(&is->videoq);
        if (pts == -1 || AV_NOPTS_VALUE == pts || seek_timestamp < pts ||
            is->videoq.max_pts < seek_timestamp) {
            return ret;
        }
    } else if (is->audio_stream >= 0) {
        seek_timestamp = av_rescale_q(seek_target, AV_TIME_BASE_Q,
                                      is->ic->streams[is->audio_stream]->time_base);
        pts = packet_queue_get_first_packet_pts(&is->audioq);
        if (pts == -1 || AV_NOPTS_VALUE == pts || seek_timestamp < pts ||
            is->audioq.max_pts < seek_timestamp) {
            return ret;
        }
    } else {
        return ret;
    }

    //ALOGE("[%u] %s:%d current pos:%llu, seek pos:%llu, seek timestamp:%llu\n",
    //                            ffp->session_id, __FUNCTION__, __LINE__, current_pos, seek_target, seek_timestamp);

    if (ffp->packet_buffering) {
        int64_t key_frame_pts = 0;

        if (is->video_stream >= 0) {
            //There is video stream.

            key_frame_pts = packet_queue_seek(&is->videoq, seek_timestamp, 1);
            if (key_frame_pts == 0) {
                return ret;
            }

            is->videoq.serial++;

            seek_point_pts_time = av_q2d(is->video_st->time_base) * key_frame_pts;

            packet_queue_delete_elements_until_by_pts(&is->videoq, av_q2d(is->video_st->time_base),
                                                      seek_point_pts_time);
            if (is->audio_stream >= 0) {
                is->audioq.serial++;
                packet_queue_delete_elements_until_by_pts(&is->audioq,
                                                          av_q2d(is->audio_st->time_base),
                                                          seek_point_pts_time);

                SDL_AoutClearAudio(ffp->aout, ffp->is_loop_seek);
            }

            if (is->videoq.nb_packets > SEEK_CACHE_MIN_PACKETS)
                is->cache_seeked = 1;

            // in cache_seek success, should flush video decoder
            if (ffp->node_vdec) {
                ffpipenode_flush(ffp->node_vdec);
            }
            ret = 0;
        } else if (is->audio_stream >= 0 && is->audioq.max_pts >= seek_timestamp) {
            //There is only audio stream.
            key_frame_pts = packet_queue_seek(&is->audioq, seek_timestamp, 0);
            if (key_frame_pts == 0) {
                return ret;
            }

            is->audioq.serial++;

            seek_point_pts_time = av_q2d(is->audio_st->time_base) * key_frame_pts;

            packet_queue_delete_elements_until_by_pts(&is->audioq, av_q2d(is->audio_st->time_base),
                                                      seek_point_pts_time);
            if (is->audioq.nb_packets > SEEK_CACHE_MIN_PACKETS)
                is->cache_seeked = 1;

            SDL_AoutClearAudio(ffp->aout, ffp->is_loop_seek);
            ret = 0;
        }
    }

    return ret;
}

static int kwai_seek_file(FFPlayer* ffp, int64_t seek_target, int64_t seek_min, int64_t seek_max) {
    VideoState* is = ffp->is;
    ALOGI("[%u][seek] avformat_seek_file ic:%p, iformat:%p ",
          ffp->session_id, is->ic, is->ic == NULL ? 0 : is->ic->iformat);
    int ret = avformat_seek_file(is->ic, -1, seek_min, seek_target, seek_max,
                                 is->seek_flags);
    if (ret < 0) {
        ALOGE("[%u] %s: error while seeking\n", ffp->session_id, is->ic->filename);
        return ret;
    } else {
        if (is->audio_stream >= 0) {
            packet_queue_flush(&is->audioq);
            packet_queue_put(&is->audioq, &flush_pkt, NULL);
            SDL_AoutClearAudio(ffp->aout, ffp->is_loop_seek);
        }
#ifdef FFP_MERGE
        if (is->subtitle_stream >= 0) {
            packet_queue_flush(&is->subtitleq);
            packet_queue_put(&is->subtitleq, &flush_pkt);
        }
#endif
        if (is->video_stream >= 0) {
            if (ffp->node_vdec) {
                ffpipenode_flush(ffp->node_vdec);
            }
            packet_queue_flush(&is->videoq);
            packet_queue_put(&is->videoq, &flush_pkt, NULL);
        }
        if (is->seek_flags & AVSEEK_FLAG_BYTE) {
            set_clock(&is->extclk, NAN, 0);
        } else {
            set_clock(&is->extclk, seek_target / (double) AV_TIME_BASE, 0);
        }
        ffp_on_clock_changed(ffp, &is->extclk);

        is->show_frame_count = 0;
        is->latest_seek_load_serial = is->videoq.serial;
        is->latest_seek_load_start_at = av_gettime();
    }
    return 0;
}

/**
 * 只有满足一下3种case才能确定是正常取消，才能纠正错误码为0
 * 1.原来ffp没发生错误的情况下(ffp->error=0)
 * 2.当前错误确定是abort_by_callback或者exit
 * 3.当前是下一次循环就确定要退出了(is->abort_request = 1);
 */
static inline bool is_abort_by_callback_scinario(FFPlayer* ffp, int err) {
    VideoState* is = ffp->is;

    bool is_abort_by_callback = err < 0
                                && (is->abort_request || is->read_abort_request)
                                && (ffp->error == 0)
                                && (ffp->kwai_error_code == 0)
                                // 在openInput失败的时候，可能返回EOF(5013)/INDA(5011),而不是AVERROR_EXIT
                                // 另一方面，上面的条件加上下面这个 ac_is_abort_by_callback_error_code 已经足够
                                // 判断出是正常退出情况了
                                && (ac_is_abort_by_callback_error_code(ffp->cache_stat.ffmpeg_adapter_qos.adapter_error));
    return is_abort_by_callback;
}

static int ffp_pkt_in_play_range(FFPlayer* ffp, AVFormatContext* ic, AVPacket* pkt) {
    int64_t stream_start_time = 0;
    int64_t pkt_ts = 0;
    int pkt_in_play_range = 0;

    stream_start_time = ic->streams[pkt->stream_index]->start_time;
    pkt_ts = pkt->pts == AV_NOPTS_VALUE ? pkt->dts : pkt->pts;

    pkt_in_play_range = ffp->duration == AV_NOPTS_VALUE ||
                        (pkt_ts -
                         (stream_start_time != AV_NOPTS_VALUE ? stream_start_time : 0)) *
                        av_q2d(ic->streams[pkt->stream_index]->time_base) -
                        (double)(ffp->start_time != AV_NOPTS_VALUE ? ffp->start_time : 0) /
                        1000000
                        <= ((double) ffp->duration / 1000000);

    return pkt_in_play_range;
}

/**
 * 直播逻辑有追帧逻辑，可能会造成当前packet直接丢弃的逻辑
 * @return 1: pkt不会被丢弃；0: pkt丢弃
 */
static int live_pkt_in_play_range_due_to_chasing(FFPlayer* ffp, AVPacket* pkt,
                                                 int pkt_in_play_range, bool audio_only) {
    VideoState* is = ffp->is;

    if (is->chasing_enabled && AV_SYNC_AUDIO_MASTER == get_master_sync_type(is)) {
        if (pkt->stream_index == is->audio_stream || audio_only) {
            int i_buffer_time_max_base = is->i_buffer_time_max;
            if (ffp->is_live_manifest)
                i_buffer_time_max_base = ffp->i_buffer_time_max_live_manifest;
            if (0 == is->chasing_status) {
                if (ffp->audio_speed_change_enable) {
                    if (ffp->stat.audio_cache.duration > i_buffer_time_max_base) {
                        if (ffp->stat.audio_cache.duration >
                            i_buffer_time_max_base + KS_AUDIO_BUFFER_SPEED_UP_2_THR_MS) {
                            is->chasing_status = 1;
                        } else if (ffp->stat.audio_cache.duration >
                                   i_buffer_time_max_base + KS_AUDIO_BUFFER_SPEED_UP_1_THR_MS) {
                            is->audio_speed_percent = KS_AUDIO_PLAY_SPEED_UP_2;
                        } else {
                            is->audio_speed_percent = KS_AUDIO_PLAY_SPEED_UP_1;
                        }
                    } else if (ffp->stat.audio_cache.duration <
                               KS_AUDIO_BUFFER_SPEED_DOWN_THR_MS) {
                        is->audio_speed_percent = KS_AUDIO_PLAY_SPEED_DOWN;
                    } else {
                        if (!LiveAbsTimeControl_is_enable(&ffp->live_abs_time_control)) {
                            is->audio_speed_percent = KS_AUDIO_PLAY_SPEED_NORMAL;
                        }
                    }
                } else {
                    if (ffp->stat.audio_cache.duration > i_buffer_time_max_base) {
                        is->chasing_status = 1;
                    }
                }
            }

            if (ffp->stat.audio_cache.duration < i_buffer_time_max_base - 300 &&
                1 == is->chasing_status) {
                is->chasing_status = 0;
                is->audio_speed_percent = KS_AUDIO_PLAY_SPEED_NORMAL;
            }
            if (is->chasing_status) {
                pkt_in_play_range = 0;
            }

            SDL_LockMutex(is->audioq.mutex);
            if (is->audioq.last_pkt && is->audio_st) {
                int64_t current_audio_pts = pkt->pts * av_q2d(is->audio_st->time_base) * 1000;
                int64_t diff =
                    is->audioq.last_pkt->pkt.pts * av_q2d(is->audio_st->time_base) * 1000 -
                    current_audio_pts;
                if (diff > 0 && diff < 6000) {
                    pkt_in_play_range = 0;
                }
            }
            SDL_UnlockMutex(is->audioq.mutex);

        } else if (pkt->stream_index == is->video_stream) {
#ifdef DROP_VIDEO_FRAME_BEFORE_DECODER
            SDL_LockMutex(is->audioq.mutex);
            if (is->audioq.last_pkt) {
                double diff = (pkt->pts * av_q2d(is->video_st->time_base) - is->audioq.last_pkt->pkt.pts * av_q2d(is->audio_st->time_base));
                if (0 != is->show_frame_count) { //don't drop frame if the first frame is not displayed
                    if (diff > MAX_LIVE_AV_PTS_GAP_THRESHOLD && !is->buffering_on) {
                        pkt_in_play_range = 0;
                        ffp->last_packet_drop = 1;
                    } else {
                        if (!(pkt->flags & AV_PKT_FLAG_KEY) && ffp->last_packet_drop) { //drop all frames till a key frame
                            pkt_in_play_range = 0;
                            ffp->last_packet_drop = 1;
                        } else {
                            ffp->last_packet_drop = 0;
                        }
                    }
                }
            }
            SDL_UnlockMutex(is->audioq.mutex);
#endif
        } else {

        }
    }

    return pkt_in_play_range;
}

static int handle_live_private_nal(FFPlayer* ffp, AVFormatContext* ic, AVPacket* pkt, AVPacketTime* v_pkttime, int64_t ptsMs) {

    enum AVCodecID video_codec_id = ic->streams[pkt->stream_index]->codec->codec_id;

    if (AV_CODEC_ID_H264 == video_codec_id || AV_CODEC_ID_HEVC == video_codec_id) {
        KwaiPrivNal* kwaiPrivNal = NULL;
        if (KwaiPrivNal_init2(&kwaiPrivNal, pkt->data, pkt->size, video_codec_id, 0)) {
            int64_t vdynLen = QOS_VENC_DYN_PARAM_LEN;
            if (KwaiPrivNal_getElemString(kwaiPrivNal, "vdyn", ffp->qos_venc_dyn_param, &vdynLen)) {
                //ALOGI("[KwaiPrivNal] vdyn: %s\n", ffp->qos_venc_dyn_param);
            }
            int64_t abs_time = 0;
            if (KwaiPrivNal_getElemInt64(kwaiPrivNal, "abst", &abs_time)) {
                //ALOGI("[KwaiPrivNal] abst: %lld\n", absTime);
                if (pkt->pts != AV_NOPTS_VALUE) {
                    ffp->qos_pts_offset = abs_time - ptsMs;
                    ffp->qos_pts_offset_got = true;
                }

                v_pkttime->abs_pts = abs_time;
            }

            int32_t mix_type = 0;
            if (KwaiPrivNal_getElemInt32(kwaiPrivNal, "mixt", &mix_type) &&
                mix_type != ffp->mix_type) {
                LiveType lt;
                memset(&lt, 0, sizeof(LiveType));
                lt.time = ptsMs;
                lt.value = mix_type;
                live_type_queue_put(&ffp->is->live_type_queue, &lt);
                ALOGI("[%u] live_type_queue_put mix_type:%d, last_mix_type:%d, ptsMs:%lld, size:%d\n", ffp->session_id, mix_type, ffp->mix_type, ptsMs, ffp->is->live_type_queue.nb_livetype);

                ffp->mix_type = mix_type;
            }

            int32_t source_device_type = 0;
            if (KwaiPrivNal_getElemInt32(kwaiPrivNal, "vcapdev", &source_device_type)) {
                ffp->source_device_type = source_device_type;
            }

            int kwaiNalLen = KwaiPrivNal_getNalLen(kwaiPrivNal);
            KwaiPrivNal_free(kwaiPrivNal);

            int origSize = pkt->size - kwaiNalLen;
            if (origSize > 0) {
                memmove(pkt->data, pkt->data + kwaiNalLen, origSize);
                av_shrink_packet(pkt, origSize);
            } else {
                av_packet_unref(pkt);
            }
            return origSize;
        }
    }

    // return non-zero for forward processing
    return pkt->size > 0 ? pkt->size : 1;
}

static void ffp_live_voice_comment(FFPlayer* ffp, int64_t pts, AVFormatContext* ic) {
    if (NULL == ffp || NULL == ffp->is || NULL == ic) {
        ALOGE("[%u] ffp_live_voice_comment return \n", ffp->session_id);
        return;
    }

    AVDictionaryEntry* voice_com = av_dict_get(ic->metadata, "vcsp", NULL, 0);

    if (voice_com) {
        int64_t voice_comment_time = strtoll(voice_com->value, NULL, 10);
        if (ffp->live_voice_comment_time != voice_comment_time) {

            ffp->live_voice_comment_time = voice_comment_time;

            if (voice_comment_time < pts && llabs(pts - voice_comment_time) >= KWAI_LIVE_VOICE_COMMENT_AVPKT_TIME_DIFF) {
                ALOGE("[%u] voice_com Discarded, vc_time(%lld) < pkt->pts(%lld)\n", ffp->session_id, voice_comment_time, pts);
            } else {
                VoiceComment vc;
                memset(&vc, 0, sizeof(VoiceComment));
                vc.time = voice_comment_time;
                AVDictionaryEntry* voice_com_str = av_dict_get(ic->metadata, "vc", NULL,
                                                               0);
                if (voice_com_str) {
                    int voice_comment_len = (int) strlen(voice_com_str->value);
                    vc.com_len =
                        voice_comment_len < KWAI_LIVE_VOICE_COMMENT_LEN ? voice_comment_len :
                        KWAI_LIVE_VOICE_COMMENT_LEN - 1;
                    strncpy(vc.comment, voice_com_str->value, vc.com_len);
                    ALOGI("[%u] voice_com, vc_time=%lld, pkt->pts=%lld, vc=%s, vc_len=%d\n",
                          ffp->session_id, vc.time, pts, vc.comment, vc.com_len);
                }

                live_voice_comment_queue_put(&ffp->is->vc_queue, &vc);
            }
        }
    }
}

/*
 * 非主动退出的情况下，read_thread主动检查流的结束状态，并对返回值做如下处理：
 *  STREAM_COMPLETE_STATUS_EXIT: 异常，退出read_thread
 *  STREAM_COMPLETE_STATUS_NORMAL: read_thread继续执行
 *  STREAM_COMPLETE_STATUS_SEEK: 播放到结尾后有seek操作，read_thread continue
 */
static int check_stream_complete_status(FFPlayer* ffp, int* completed, SDL_mutex* wait_mutex) {
    VideoState* is = NULL;

    if (!ffp || !ffp->is) {
        ALOGE("[%d] %s, Invalid FFPlayer\n", ffp->session_id, __FUNCTION__);
        return STREAM_COMPLETE_STATUS_EXIT;
    } else {
        is = ffp->is;
    }

    if ((!is->paused || *completed) &&
        (!is->audio_st || (is->auddec.finished == is->audioq.serial &&
                           frame_queue_nb_remaining(&is->sampq) == 0)) &&
        (!is->video_st || (is->viddec.finished == is->videoq.serial &&
                           frame_queue_nb_remaining(&is->pictq) == 0))) {

        if (!ffp->kwai_error_code && !ffp->error && !is->interrupt_exit &&
            (is->video_st || is->audio_st)
            && ((!is->video_st ||
                 (int64_t)((get_clock(&is->vidclk) + STREAM_SEEK_TO_START_THRESHOLD) *
                           AV_TIME_BASE) < is->video_duration)
                && (!is->audio_st ||
                    (int64_t)((get_clock(&is->audclk) + STREAM_SEEK_TO_START_THRESHOLD) *
                              AV_TIME_BASE) < is->audio_duration))) {
            //sleep 10millisecond
            av_usleep(10000);
            // continue;
            return STREAM_COMPLETE_STATUS_SEEK;
        }

        KwaiQos_onPlayToEnd(&ffp->kwai_qos);
        if (ffp->expect_use_cache) {
            KwaiQos_setAwesomeCacheIsFullyCachedOnLoop(&ffp->kwai_qos,
                                                       ffp->islive ? false :
                                                       AwesomeCache_util_is_fully_cached(
                                                           is->filename, ffp->cache_key));
        }

        if (ffp->error < 0 && ffp->error != AVERROR_EOF) {
            if (ffp->enable_loop_on_error) {
                stream_seek(is, ffp->start_time != AV_NOPTS_VALUE ? ffp->start_time : 0, 0, 0);
                ffp->is_loop_seek = true;
                return STREAM_COMPLETE_STATUS_SEEK;
            } else {
                return STREAM_COMPLETE_STATUS_EXIT;
            }
        }

        if (ffp->loop != 1 && (!ffp->loop || --ffp->loop)) {
            stream_seek(is, ffp->start_time != AV_NOPTS_VALUE ? ffp->start_time : 0, 0, 0);
            ffp->is_loop_seek = true;
            ffp_notify_msg1(ffp, FFP_MSG_PLAY_TO_END);
            // continue;
            return STREAM_COMPLETE_STATUS_SEEK;
        } else if (ffp->autoexit) {
            //ret = AVERROR_EOF; // no used
            ffp->kwai_error_code = convert_to_kwai_error_code(AVERROR_EOF);
            ffp_notify_msg1(ffp, FFP_MSG_PLAY_TO_END);
            // goto fail;
            return STREAM_COMPLETE_STATUS_EXIT;
        } else {
            ffp_statistic_l(ffp);
            if (*completed) {
                ALOGI("[%u] ffp_toggle_buffering: eof\n", ffp->session_id);
                SDL_LockMutex(wait_mutex);
                while (!is->abort_request && !is->seek_req && !is->read_abort_request)
                    SDL_CondWaitTimeout(is->continue_read_thread, wait_mutex, 100);
                SDL_UnlockMutex(wait_mutex);
                if (!is->abort_request && !is->read_abort_request) {
                    // continue;
                    return STREAM_COMPLETE_STATUS_SEEK;
                }
            } else {
                *completed = 1;
                ffp->auto_resume = 0;

                ffp_notify_msg1(ffp, FFP_MSG_PLAY_TO_END);
                ffp_toggle_buffering(ffp, 0, 1);
                toggle_pause(ffp, 1);
                if (ffp->kwai_error_code || ffp->error || is->interrupt_exit) {
                    ALOGI("[%u] ffp_toggle_buffering: error: %d\n", ffp->session_id, ffp->error);
                    if (ffp->error) {
                        ffp->kwai_error_code = convert_to_kwai_error_code(ffp->error);
                    }
                    ffp_notify_msg3(ffp, FFP_MSG_ERROR, ffp->kwai_error_code, 0);
                } else {
                    ALOGI("[%u] ffp_toggle_buffering: completed: OK\n", ffp->session_id);
                    ffp_notify_msg1(ffp, FFP_MSG_COMPLETED);
                }
            }
        }
    }

    return STREAM_COMPLETE_STATUS_NORMAL;
}

/*
 * 检查video时间戳回退，当前用于直播CDN fake数据问题分析使用
 *  1.以关键帧为边界检查video时间戳回退，如果发生回退：flush video pkt queue(保持原来的逻辑)
 *  2.回退时长信息打点
 */
static inline void check_live_video_pkt_timestamp_rollback(FFPlayer* ffp, AVPacket* pkt) {
    VideoState* is = NULL;

    if (!ffp || !ffp->is) {
        ALOGE("[%d] %s, Invalid FFPlayer\n", ffp->session_id, __FUNCTION__);
        return;
    }

    is = ffp->is;

    if (is->prev_keyframe_dts > 0 && (pkt->dts <= is->prev_keyframe_dts)) {
        packet_queue_flush(&is->videoq);
        packet_queue_put(&is->videoq, &flush_pkt, NULL);
        KwaiQos_onVideoTimestampRollback(&ffp->kwai_qos, (is->prev_keyframe_dts - pkt->dts));
        ALOGE("[%u] [%s] is->prev_keyframe_dts:%"
              PRId64
              ", pkt->dts:%"
              PRId64, ffp->session_id, __FUNCTION__, is->prev_keyframe_dts, pkt->dts);
    }
    is->prev_keyframe_dts = pkt->dts;
}

int video_read_thread(void* arg) {
    FFPlayer* ffp = arg;
    VideoState* is = ffp->is;
    AVFormatContext* ic = NULL;
    int err, i __unused;
    AVPacket pkt1, *pkt = &pkt1;
    AVDictionary** opts;
    int orig_nb_streams;
    AVPacketTime* p_pkttime = NULL;
    SDL_mutex* wait_mutex = SDL_CreateMutex();
    int64_t audio_bytes_read = 0;
    bool first_v_pkt = true;
    bool first_a_pkt = true;
    int pkt_in_play_range = 0;
    int completed = 0;

    if (!wait_mutex) {
        ALOGE("[%u] video_read_thread: SDL_CreateMutex(): %s\n", ffp->session_id, SDL_GetError());
        err = AVERROR(ENOMEM);
        ffp->kwai_error_code = convert_to_kwai_error_code(err);
        goto fail;
    }

    if (!is) {
        ALOGE("[%u] video_read_thread: VideoState is NULL\n", ffp->session_id);
        err = EIJK_NULL_IS_PTR;
        ffp->kwai_error_code = convert_to_kwai_error_code(err);
        goto fail;
    }

    if (ffp->is_audio_reloaded && !ffp->last_only_audio_pts_updated) {
        SDL_LockMutex(wait_mutex);
        SDL_CondWait(is->continue_video_read_thread, wait_mutex);
        SDL_UnlockMutex(wait_mutex);
    }

    if (is->abort_request) {
        ALOGI("[%u] video_read_thread: abort_request=%d\n", ffp->session_id, is->abort_request);
        goto fail;
    }

    is->read_abort_request = 1;

    if (is->audio_read_tid) {
        SDL_CondSignal(is->continue_audio_read_thread);
        SDL_CondSignal(is->continue_read_thread);
        SDL_WaitThread(is->audio_read_tid, NULL);
        is->audio_read_tid = NULL;
        ffp->is_audio_reloaded = false;
        ffp->last_only_audio_pts_updated = false;
    }

    is->read_abort_request = 0;
    is->read_start_time = 0;
    is->audio_speed_percent = KS_AUDIO_PLAY_SPEED_NORMAL;
    ffp->kwai_error_code = 0;

    // 返回前台重新拉video的时候，prev_keyframe_dts需要reset
    // 否则，在前后台快速切换重新拉流的时候会误报时间戳回退
    is->prev_keyframe_dts = 0;

    is->eof = 0;
    audio_bytes_read = is->bytes_read;

    ic = avformat_alloc_context();
    if (!ic) {
        ALOGE("[%u] video_read_thread: Could not allocate context.\n", ffp->session_id);
        err = AVERROR(ENOMEM);
        ffp->kwai_error_code = convert_to_kwai_error_code(err);
        goto fail;
    }

    ic->interrupt_callback.callback = decode_interrupt_cb;
    ic->interrupt_callback.opaque = ffp;
    ic->fps_probe_size = 0;

    if (is->http_headers) {
        av_dict_set(&ffp->format_opts, "headers", is->http_headers, 0);
    }

    if (ffp->timeout > 0) {
        av_dict_set_int(&ffp->format_opts, "timeout", ffp->timeout, 0);
    }

    err = ffp_avformat_open_input(ffp, &ic, ffp->input_filename, NULL, &ffp->format_opts);
    if (err < 0) {
        print_error((ffp->is_live_manifest ? "Live Manifest" : ffp->input_filename), err);
        ffp->kwai_error_code = convert_to_kwai_error_code(err);
        goto fail;
    }

    av_format_inject_global_side_data(ic);

    opts = setup_find_stream_info_opts(ic, ffp->codec_opts);
    orig_nb_streams = ic->nb_streams;

    err = avformat_find_stream_info(ic, opts);

    for (i = 0; i < orig_nb_streams; i++)
        av_dict_free(&opts[i]);
    av_freep(&opts);

    if (err < 0) {
        ALOGE("[%u] %s: could not find codec parameters\n", ffp->session_id, ffp->input_filename);
        ffp->kwai_error_code = convert_to_kwai_error_code(err);
        goto fail;
    }

    AVPacketTime v_pkttime;
    memset(&v_pkttime, 0, sizeof(AVPacketTime));

    for (;;) {
        if (is->interrupt_exit) {
            ffp->kwai_error_code = EIJK_KWAI_READ_DATA_IO_TIMEOUT;
            break;
        }
        if (is->abort_request || is->read_abort_request) {
            ALOGI("[%u] video_read_thread read_abort_request=%d\n", ffp->session_id,
                  is->read_abort_request);
            break;
        }

        KwaiQos_onSystemPerformance(&ffp->kwai_qos);

        err = check_stream_complete_status(ffp, &completed, wait_mutex);
        if (err == STREAM_COMPLETE_STATUS_EXIT) {
            goto fail;
        } else if (err == STREAM_COMPLETE_STATUS_SEEK) {
            continue;
        }

        is->read_start_time = av_gettime_relative();
        err = av_read_frame(ic, pkt);

        if (ic && ic->pb) {
            is->bytes_read = audio_bytes_read + ic->pb->bytes_read;
        } else {
            is->bytes_read = ffp->i_video_decoded_size + ffp->i_audio_decoded_size;
        }

        if (err < 0) {
            int pb_eof = 0;
            int pb_error = 0;

            if ((err == AVERROR_EOF || avio_feof(ic->pb)) && !is->eof) {
                pb_eof = 1;
            }

            if (ic->pb && ic->pb->error) {
                pb_eof = 1;
                pb_error = ic->pb->error;
            }

            if (err == AVERROR_EXIT) {
                pb_eof = 1;
                pb_error = AVERROR_EXIT;
            }

            if (pb_eof) {
                if (is->video_stream >= 0 && !(ffp->is_audio_reloaded)) {
                    packet_queue_put_nullpacket(&is->videoq, is->video_stream);
                }

                if (is->audio_stream >= 0 && !(ffp->is_audio_reloaded)) {
                    packet_queue_put_nullpacket(&is->audioq, is->audio_stream);
                }

                is->eof = 1;
            }

            if (pb_error) {
                if (is->video_stream >= 0 && !(ffp->is_audio_reloaded)) {
                    packet_queue_put_nullpacket(&is->videoq, is->video_stream);
                }

                if (is->audio_stream >= 0 && !(ffp->is_audio_reloaded)) {
                    packet_queue_put_nullpacket(&is->audioq, is->audio_stream);
                }

                is->eof = 1;
                ffp->error = pb_error;
                ffp->kwai_error_code = convert_to_kwai_error_code(ffp->error);
                ALOGE("[%u] video_read_thread, has pb_error, av_read_frame , ffp->kwai_error_code:%d, ret:%d(%s/%s), ffp->error:%d(%s/%s)\n",
                      ffp->session_id, ffp->kwai_error_code,
                      err, get_error_code_fourcc_string_macro(err), ffp_get_error_string(err), ffp->error,
                      get_error_code_fourcc_string_macro(ffp->error), ffp_get_error_string(ffp->error));
                if (ffp->error == AVERROR_EXIT) {
                    ALOGE("[%u] video_read_thread, ffp->error == AVERROR_EXIT, goto fail \n", ffp->session_id);
                    break;
                }
            } else {
                ffp->error = 0;
            }

            if (is->abort_request || is->read_abort_request) {
                ALOGE("[%u] video_read_thread Do quit exit \n", ffp->session_id);
                break;
            }

            if (is->eof) {
                ffp_toggle_buffering(ffp, 0, 1);
                SDL_Delay(100);
            }
            SDL_LockMutex(wait_mutex);
            SDL_CondWaitTimeout(is->continue_read_thread, wait_mutex, 10);
            SDL_UnlockMutex(wait_mutex);
            ffp_statistic_l(ffp);
            continue;
        } else {
            is->eof = 0;
        }

        if (pkt->stream_index == is->audio_stream) {
            if (first_a_pkt) {
                if (ffp->last_only_audio_pts < pkt->pts) {
                    ALOGE("[%u] video_read_thread, Audio Jump, ffp->last_only_audio_pts=%lld, pkt->pts=%lld\n",
                          ffp->session_id, ffp->last_only_audio_pts, pkt->pts);
                    KwaiQos_onAudioPtsJumpForward(&ffp->kwai_qos, pkt->pts - ffp->last_only_audio_pts);
                } else {
                    KwaiQos_onAudioPtsJumpBackward(&ffp->kwai_qos, ffp->last_only_audio_pts - pkt->pts);
                }
                first_a_pkt = false;
            }
            if (ffp->last_only_audio_pts >= pkt->pts) {
                ALOGI("[%u] video_read_thread ffp->last_only_audio_pts=%lld, pkt->pts=%lld, continue \n",
                      ffp->session_id, ffp->last_only_audio_pts, pkt->pts);
                av_packet_unref(pkt);
                continue;
            }
        }

        // handle live voice-comment
        if (pkt->stream_index == is->audio_stream || pkt->stream_index == is->video_stream) {
            ffp_live_voice_comment(ffp, pkt->pts, ic);
        }

        // qos_dts_duration and kwai private a/v data
        if (pkt->stream_index == is->audio_stream) {
            int64_t dtsMs = av_rescale_q(pkt->dts, ic->streams[pkt->stream_index]->time_base,
            (AVRational) {1, 1000});

            ffp->qos_dts_duration = dtsMs - is->first_dts;

            // handle aac-padding
            if (AV_CODEC_ID_AAC == ic->streams[pkt->stream_index]->codec->codec_id) {
                handlePrivDataInAac(ffp, pkt);
            }
        } else if (pkt->stream_index == is->video_stream) {
            int64_t ptsMs = av_rescale_q(pkt->pts, ic->streams[pkt->stream_index]->time_base,
            (AVRational) {1, 1000});
            int64_t dtsMs = av_rescale_q(pkt->dts, ic->streams[pkt->stream_index]->time_base,
            (AVRational) {1, 1000});
            if (pkt->pts != AV_NOPTS_VALUE && ffp->qos_pts_offset_got && ffp->wall_clock_updated) {
                DelayStat_calc_pts_delay(&ffp->qos_delay_video_recv, ffp->wall_clock_offset,
                                         ffp->qos_pts_offset, ptsMs);
            }

            ffp->qos_dts_duration = dtsMs - is->first_dts;

            if (handle_live_private_nal(ffp, ic, pkt, &v_pkttime, ptsMs) <= 0) {
                av_packet_unref(pkt);
                continue;
            }
        }

        pkt_in_play_range = ffp_pkt_in_play_range(ffp, ic, pkt);

        // live frame hurry-up & drop condition
        if (ffp->last_audio_pts_updated && ffp->first_reloaded_v_frame_rendered) {
            pkt_in_play_range = live_pkt_in_play_range_due_to_chasing(ffp, pkt, pkt_in_play_range, false);
        }

        if (pkt_in_play_range == 0) {
            KwaiQos_onDropPacket(&ffp->kwai_qos, pkt, ic, is->audio_stream, is->video_stream, ffp->session_id);
        }

        if (pkt->stream_index == is->audio_stream && pkt_in_play_range) {
            // fixme: may need further consideration of the exit condition
            if (ffp->is_audio_reloaded && ffp->last_audio_pts_updated) {
                SDL_CondSignal(is->continue_audio_read_thread);
                av_packet_unref(pkt);
                continue;
            } else {
                ffp->last_audio_pts = pkt->pts;
                if (!ffp->last_audio_pts_updated) {
                    ffp->last_audio_pts_updated = true;
                }
            }

            packet_queue_put(&is->audioq, pkt, p_pkttime);
        } else if (pkt->stream_index == is->video_stream && pkt_in_play_range
                   &&
                   !(is->video_st && (is->video_st->disposition & AV_DISPOSITION_ATTACHED_PIC))) {

            if (is->dts_of_last_frame != AV_NOPTS_VALUE && pkt->dts != AV_NOPTS_VALUE
                && pkt->dts > is->dts_of_last_frame && (is->realtime || pkt->duration <= 0)) {
                pkt->duration = pkt->dts - is->dts_of_last_frame;
            }

            if (is->realtime && ((pkt->flags & AV_PKT_FLAG_KEY) == AV_PKT_FLAG_KEY)) {
                check_live_video_pkt_timestamp_rollback(ffp, pkt);
            }

            is->dts_of_last_frame = pkt->dts;

            if (first_v_pkt) {
                first_v_pkt = false;
                ffp->reloaded_video_first_pts = pkt->pts;
                ALOGI("[%u] video_read_thread, first video pkt->pts=%lld \n", ffp->session_id, pkt->pts);
            }

            packet_queue_put(&is->videoq, pkt, &v_pkttime);
            v_pkttime.abs_pts = 0;
        } else {
            av_packet_unref(pkt);
        }

        ffp_statistic_l(ffp);

        ffp_read_thread_check_buffering(ffp, false);
    }

fail:
    if (ic) {
        ALOGI("[%u][video_read_thread] ffp_avformat_close_input", ffp->session_id);
        ffp_avformat_close_input(ffp, &ic);
    }

    // for native p2sp
    ffp_close_release_AwesomeCache_AVIOContext(ffp);

    if (p_pkttime != NULL) {
        av_freep(&p_pkttime);
    }
    if (is != NULL && !is->read_abort_request && !is->abort_request) {
        toggle_pause(ffp, 1);
        ffp_notify_msg3(ffp, FFP_MSG_ERROR, ffp->kwai_error_code, 0);
    }
    SDL_DestroyMutex(wait_mutex);

    ALOGI("[%u][video_read_thread] EXIT", ffp->session_id);
    return 0;
}

int audio_read_thread(void* arg) {
    FFPlayer* ffp = arg;
    VideoState* is = ffp->is;
    AVFormatContext* ic = NULL;
    int err, i __unused;
    AVPacket pkt1, *pkt = &pkt1;
    AVDictionary** opts;
    int orig_nb_streams;
    AVPacketTime* p_pkttime = NULL;
    SDL_mutex* wait_mutex = SDL_CreateMutex();
    int64_t video_bytes_read = 0;
    bool first_a_pkt = true;
    int completed = 0;

    if (!wait_mutex) {
        ALOGE("[%u] audio_read_thread: SDL_CreateMutex(): %s\n", ffp->session_id, SDL_GetError());
        err = AVERROR(ENOMEM);
        ffp->kwai_error_code = convert_to_kwai_error_code(err);
        goto fail;
    }

    if (!is) {
        ALOGE("[%u] audio_read_thread: VideoState is NULL\n", ffp->session_id);
        err = EIJK_NULL_IS_PTR;
        ffp->kwai_error_code = convert_to_kwai_error_code(err);
        goto fail;
    }

    if (!ffp->first_video_frame_rendered || (ffp->is_video_reloaded && !ffp->last_audio_pts_updated)) {
        SDL_LockMutex(wait_mutex);
        SDL_CondWait(is->continue_audio_read_thread, wait_mutex);
        SDL_UnlockMutex(wait_mutex);
    }

    if (is->abort_request) {
        ALOGI("[%u] audio_read_thread: abort_request=%d\n", ffp->session_id, is->abort_request);
        goto fail;
    }

    is->read_abort_request = 1;

    // 首次退后台ff_read结束前(is->read_tid != NULL)不会等待video_read_tid
    if (is->video_read_tid) {
        SDL_CondSignal(is->continue_video_read_thread);
        SDL_CondSignal(is->continue_read_thread);
        SDL_WaitThread(is->video_read_tid, NULL);
        is->video_read_tid = NULL;
        ffp->is_video_reloaded = false;
        ffp->first_reloaded_v_frame_rendered = 0;
        ffp->last_audio_pts_updated = false;
        ffp->reloaded_video_first_pts = AV_NOPTS_VALUE;
    }

    if (is->read_tid) {
        SDL_CondSignal(is->continue_read_thread);
        SDL_WaitThread(is->read_tid, NULL);
        is->read_tid = NULL;
    }

    packet_queue_flush(&is->videoq);
    packet_queue_put(&is->videoq, &flush_pkt, NULL);

    is->read_abort_request = 0;
    is->read_start_time = 0;
    is->audio_speed_percent = KS_AUDIO_PLAY_SPEED_NORMAL;
    ffp->kwai_error_code = 0;

    is->eof = 0;
    video_bytes_read = is->bytes_read;

    ic = avformat_alloc_context();
    if (!ic) {
        ALOGE("[%u] audio_read_thread: Could not allocate context.\n", ffp->session_id);
        err = AVERROR(ENOMEM);
        ffp->kwai_error_code = convert_to_kwai_error_code(err);
        goto fail;
    }

    ic->interrupt_callback.callback = decode_interrupt_cb;
    ic->interrupt_callback.opaque = ffp;
    ic->fps_probe_size = 0;

    if (is->http_headers) {
        av_dict_set(&ffp->format_opts, "headers", is->http_headers, 0);
    }

    if (ffp->timeout > 0) {
        av_dict_set_int(&ffp->format_opts, "timeout", ffp->timeout, 0);
    }

    err = ffp_avformat_open_input(ffp, &ic, ffp->reload_audio_filename, NULL, &ffp->format_opts);
    if (err < 0) {
        print_error((ffp->is_live_manifest ? "Live Manifest" : ffp->reload_audio_filename), err);
        ffp->kwai_error_code = convert_to_kwai_error_code(err);
        goto fail;
    }

    av_format_inject_global_side_data(ic);

    opts = setup_find_stream_info_opts(ic, ffp->codec_opts);
    orig_nb_streams = ic->nb_streams;

    err = avformat_find_stream_info(ic, opts);

    for (i = 0; i < orig_nb_streams; i++)
        av_dict_free(&opts[i]);
    av_freep(&opts);

    if (err < 0) {
        ALOGE("[%u] %s: could not find codec parameters\n", ffp->session_id, ffp->reload_audio_filename);
        ffp->kwai_error_code = convert_to_kwai_error_code(err);
        goto fail;
    }

    for (;;) {
        if (is->interrupt_exit) {
            ffp->kwai_error_code = EIJK_KWAI_READ_DATA_IO_TIMEOUT;
            break;
        }
        if (is->abort_request || is->read_abort_request) {
            ALOGI("[%u] audio_read_thread read_abort_request=%d\n", ffp->session_id,
                  is->read_abort_request);
            break;
        }

        KwaiQos_onSystemPerformance(&ffp->kwai_qos);

        err = check_stream_complete_status(ffp, &completed, wait_mutex);
        if (err == STREAM_COMPLETE_STATUS_EXIT) {
            goto fail;
        } else if (err == STREAM_COMPLETE_STATUS_SEEK) {
            continue;
        }

        is->read_start_time = av_gettime_relative();
        err = av_read_frame(ic, pkt);

        if (ic && ic->pb) {
            is->bytes_read = video_bytes_read + ic->pb->bytes_read;
        } else {
            is->bytes_read = ffp->i_video_decoded_size + ffp->i_audio_decoded_size;
        }

        if (err < 0) {
            int pb_eof = 0;
            int pb_error = 0;

            if ((err == AVERROR_EOF || avio_feof(ic->pb)) && !is->eof) {
                pb_eof = 1;
            }

            if (ic->pb && ic->pb->error) {
                pb_eof = 1;
                pb_error = ic->pb->error;
            }

            if (err == AVERROR_EXIT) {
                pb_eof = 1;
                pb_error = AVERROR_EXIT;
            }

            if (pb_eof) {
                if (is->audio_stream >= 0 && !(ffp->is_video_reloaded)) {
                    packet_queue_put_nullpacket(&is->audioq, is->audio_stream);
                }
                is->eof = 1;
            }

            if (pb_error) {
                if (is->audio_stream >= 0 && !(ffp->is_video_reloaded)) {
                    packet_queue_put_nullpacket(&is->audioq, is->audio_stream);
                }

                is->eof = 1;
                ffp->error = pb_error;
                ffp->kwai_error_code = convert_to_kwai_error_code(ffp->error);
                ALOGE("[%u] audio_read_thread, has pb_error, av_read_frame , ffp->kwai_error_code:%d, ret:%d(%s/%s), ffp->error:%d(%s/%s)\n",
                      ffp->session_id, ffp->kwai_error_code,
                      err, get_error_code_fourcc_string_macro(err), ffp_get_error_string(err), ffp->error,
                      get_error_code_fourcc_string_macro(ffp->error), ffp_get_error_string(ffp->error));
                if (ffp->error == AVERROR_EXIT) {
                    ALOGE("[%u] audio_read_thread, ffp->error == AVERROR_EXIT, goto fail \n", ffp->session_id);
                    break;
                }
            } else {
                ffp->error = 0;
            }

            if (is->abort_request || is->read_abort_request) {
                ALOGE("[%u] audio_read_thread Do quit exit \n", ffp->session_id);
                break;
            }

            if (is->eof) {
                ffp_toggle_buffering(ffp, 0, 1);
                SDL_Delay(100);
            }
            SDL_LockMutex(wait_mutex);
            SDL_CondWaitTimeout(is->continue_read_thread, wait_mutex, 10);
            SDL_UnlockMutex(wait_mutex);
            // video component is still there, can't use ffp_audio_statistic_l(ffp);
            ffp_statistic_l(ffp);
            continue;
        } else {
            is->eof = 0;
        }

        if (ic->streams[pkt->stream_index]->codec->codec_type == AVMEDIA_TYPE_AUDIO) {
            if (first_a_pkt) {
                if (ffp->last_audio_pts < pkt->pts) {
                    ALOGE("[%u] audio_read_thread, Audio Jump, ffp->last_audio_pts=%lld, pkt->pts=%lld",
                          ffp->session_id, ffp->last_audio_pts, pkt->pts);
                    KwaiQos_onAudioPtsJumpForward(&ffp->kwai_qos, pkt->pts - ffp->last_audio_pts);
                } else {
                    KwaiQos_onAudioPtsJumpBackward(&ffp->kwai_qos, ffp->last_audio_pts - pkt->pts);
                }
                first_a_pkt = false;
            }
            if (ffp->last_audio_pts >= pkt->pts) {
                ALOGI("[%u] audio_read_thread ffp->last_audio_pts=%lld, pkt->pts=%lld, continue",
                      ffp->session_id, ffp->last_audio_pts, pkt->pts);
                av_packet_unref(pkt);
                continue;
            }

            if (ffp->is_video_reloaded && ffp->last_only_audio_pts_updated) {
                SDL_CondSignal(is->continue_video_read_thread);
                av_packet_unref(pkt);
                continue;
            } else {
                ffp->last_only_audio_pts = pkt->pts;
                if (!ffp->last_only_audio_pts_updated) {
                    ffp->last_only_audio_pts_updated = true;
                }
            }

            // qos_dts_duration
            int64_t dtsMs = av_rescale_q(pkt->dts, ic->streams[pkt->stream_index]->time_base,
            (AVRational) {1, 1000});
            ffp->qos_dts_duration = dtsMs - is->first_dts;

            // handle live voice-comment
            ffp_live_voice_comment(ffp, pkt->pts, ic);

            // handle aac-padding
            if (AV_CODEC_ID_AAC == ic->streams[pkt->stream_index]->codec->codec_id) {
                handlePrivDataInAac(ffp, pkt);
            }

            int pkt_in_play_range = ffp_pkt_in_play_range(ffp, ic, pkt);

            // live frame hurry-up & drop condition
            pkt_in_play_range = live_pkt_in_play_range_due_to_chasing(ffp, pkt, pkt_in_play_range, true);

            if (pkt_in_play_range == 0) {
                KwaiQos_onDropPacket(&ffp->kwai_qos, pkt, ic, pkt->stream_index, -1, ffp->session_id);
                av_packet_unref(pkt);
            } else {
                packet_queue_put(&is->audioq, pkt, p_pkttime);
            }
        } else {
            // for non-audio pkt
            av_packet_unref(pkt);
        }

        // video component is still there, can't use ffp_audio_statistic_l(ffp);
        ffp_statistic_l(ffp);

        ffp_read_thread_check_buffering(ffp, false);
    }

fail:
    if (ic) {
        ALOGI("[%u] audio_read_thread ffp_avformat_close_input", ffp->session_id);
        ffp_avformat_close_input(ffp, &ic);
    }

    if (p_pkttime != NULL) {
        av_freep(&p_pkttime);
    }
    if (is && !is->read_abort_request && !is->abort_request) {
        toggle_pause(ffp, 1);
        ffp_notify_msg3(ffp, FFP_MSG_ERROR, ffp->kwai_error_code, 0);
    }
    SDL_DestroyMutex(wait_mutex);

    ALOGI("[%u] audio_read_thread EXIT", ffp->session_id);
    return 0;
}

static int open_audio_component_thread(void* arg) {
    int ret = 0;
    FFPlayer* ffp = arg;

    if (!ffp) {
        ALOGE("%s, Invalid FFPlayer\n", __FUNCTION__);
        return -1;
    }

    ret = stream_component_open(ffp, ffp->st_index[AVMEDIA_TYPE_AUDIO]);
    if (ret < 0) {
        ffp->kwai_error_code = EIJK_KWAI_UNSUPPORT_ACODEC;
        goto fail;  // stop player for unsupported audio codec for now
    }

fail:
    ALOGI("[%u] open_audio_component_thread return %d\n", ffp->session_id, ret);
    return ret;
}

static int open_video_component_thread(void* arg) {
    int ret = 0;
    FFPlayer* ffp = arg;

    if (!ffp) {
        ALOGE("%s, Invalid FFPlayer\n", __FUNCTION__);
        return -1;
    }

    ret = stream_component_open(ffp, ffp->st_index[AVMEDIA_TYPE_VIDEO]);
    if (ret < 0) {
        ffp->kwai_error_code = EIJK_KWAI_UNSUPPORT_VCODEC;
        goto fail;  // stop player for unsupported audio codec for now
    }

fail:
    ALOGE("[%u] open_video_component_thread return %d\n", ffp->session_id, ret);
    return ret;
}

/**
 * 异步打开解码器，目前只在ios使用，在android上没有收益所以不打开
 */
static int async_open_stream_component(FFPlayer* ffp) {
    int ret = 0;

    if (!ffp || !ffp->is) {
        ALOGE("%s, Invalid FFPlayer\n", __FUNCTION__);
        return -1;
    }

    if (ffp->st_index[AVMEDIA_TYPE_AUDIO] >= 0) {
        ffp->is->a_component_open_tid = SDL_CreateThreadEx(&(ffp->is->_a_component_open_tid),
                                                           open_audio_component_thread, ffp,
                                                           "audio_component_thread");
        if (!(ffp->is->a_component_open_tid)) {
            ALOGE("[%u] SDL_CreateThread(audio_component_thread): %s\n", ffp->session_id,
                  SDL_GetError());
            ret = -1;
            goto fail;
        }
    } else {
        video_state_set_av_sync_type(ffp->is, AV_SYNC_VIDEO_MASTER);
    }

    if (ffp->st_index[AVMEDIA_TYPE_VIDEO] >= 0) {
        ffp->is->v_component_open_tid = SDL_CreateThreadEx(&(ffp->is->_v_component_open_tid),
                                                           open_video_component_thread, ffp,
                                                           "video_component_thread");
        if (!(ffp->is->v_component_open_tid)) {
            ALOGE("[%u] SDL_CreateThread(video_component_thread): %s\n", ffp->session_id,
                  SDL_GetError());
            ret = -1;
            goto fail;
        }
    }

fail:
    if (ffp->is->a_component_open_tid) {
        SDL_WaitThread(ffp->is->a_component_open_tid, NULL);
        ffp->is->a_component_open_tid = NULL;
    }

    if (ffp->is->v_component_open_tid) {
        SDL_WaitThread(ffp->is->v_component_open_tid, NULL);
        ffp->is->v_component_open_tid = NULL;
    }

    return ret;
}

/*
 * 处理seek req，调用kwai_seek_file或者kwai_cache_seek 重置packet_queue，设置精准seek req
 */
static void seek_file_and_update_status(FFPlayer* ffp, VideoState* is, bool is_seek_at_start) {
    int64_t seek_target = is->seek_pos;
    int64_t seek_min = is->seek_rel > 0 ? seek_target - is->seek_rel + 2 : INT64_MIN;
    int64_t seek_max = is->seek_rel < 0 ? seek_target - is->seek_rel - 2 : INT64_MAX;
    bool need_notify_accurate_seek = false;
    int ret = 0;
    ffp_toggle_buffering(ffp, 1, 0);
    ffp_notify_msg3(ffp, FFP_MSG_BUFFERING_UPDATE, 0, 0);
    if (!is_seek_at_start) {
        KwaiQos_onSeekStart(&ffp->kwai_qos);
        if (is->video_stream >= 0) {
            KwaiQos_onFirstFrameAfterSeekStart(&ffp->kwai_qos);
        }
        if (!ffp->enable_cache_seek ||
            kwai_cache_seek(ffp, seek_target, seek_min, seek_max) < 0) {
            kwai_seek_file(ffp, seek_target, seek_min, seek_max);
        }
    }

    ffp->dcc.current_high_water_mark_in_ms = ffp->dcc.first_high_water_mark_in_ms;

#ifdef FFP_MERGE
    if (is->paused) {
        step_to_next_frame(is);
    }
#endif
    SDL_LockMutex(ffp->is->play_mutex);
    if (ffp->auto_resume) {
        is->pause_req = 0;
        if (ffp->packet_buffering) {
            is->buffering_on = 1;
        }
        ffp->auto_resume = 0;
        stream_update_pause_l(ffp);
    }
    if (is->pause_req && !is_seek_at_start) {
        step_to_next_frame_l(ffp);
    }
    SDL_UnlockMutex(ffp->is->play_mutex);

    if (seek_target == 0) {
        ffp->enable_accurate_seek = 0;
    } else {
        ffp->enable_accurate_seek = ffp->cached_enable_accurate_seek;
    }

    if (ffp->enable_accurate_seek) {
        is->drop_aframe_count = 0;
        is->drop_vframe_count = 0;
        SDL_LockMutex(is->accurate_seek_mutex);
        is->accurate_seek_notify = 0;
        if (is->video_stream >= 0) {
            is->video_accurate_seek_req = 1;
        }
        if (is->audio_stream >= 0) {
            is->audio_accurate_seek_req = 1;
            if (!is->video_accurate_seek_req && is->pause_req) {
                need_notify_accurate_seek = true;
            }
        }
        SDL_CondSignal(is->audio_accurate_seek_cond);
        SDL_CondSignal(is->video_accurate_seek_cond);
        SDL_UnlockMutex(is->accurate_seek_mutex);
    } else {
        SDL_LockMutex(is->accurate_seek_mutex);
        if (is->video_stream >= 0) {
            is->video_accurate_seek_req = 0;
        }
        if (is->audio_stream >= 0) {
            is->audio_accurate_seek_req = 0;
        }
        SDL_UnlockMutex(is->accurate_seek_mutex);
    }
    KwaiQos_onSeekEnd(&ffp->kwai_qos);
    KwaiQos_onFirstPacketAfterSeekStart(&ffp->kwai_qos);
    ffp->audio_render_after_seek_need_notify = 1;
    ffp->video_rendered_after_seek_need_notify = 1;
    ffp_notify_msg3(ffp, FFP_MSG_SEEK_COMPLETE, (int) fftime_to_milliseconds(seek_target),
                    ret);
    ffp_toggle_buffering(ffp, 1, 0);
    //pause状态下精准seek audio不解码，pause状态下对于只有音频流直接返回通知
    if (need_notify_accurate_seek) {
        need_notify_accurate_seek = false;
        is->accurate_seek_notify = 1;
        ffp_notify_msg2(ffp, FFP_MSG_ACCURATE_SEEK_COMPLETE,
                        (int)(fftime_to_milliseconds(seek_target)));
    }
}


/* this thread gets the stream from the disk or the network */
int read_thread(void* arg) {
    FFPlayer* ffp = arg;
    VideoState* is = ffp->is;
    AVFormatContext* ic = NULL;
    int err, i, ret __unused;
    int st_index[AVMEDIA_TYPE_NB];
    AVPacket pkt1, *pkt = &pkt1;
    int completed = 0;
    int pkt_in_play_range = 0;
    AVDictionaryEntry* t;
    AVDictionary** opts;
    int orig_nb_streams;
    SDL_mutex* wait_mutex = SDL_CreateMutex();
    int scan_all_pmts_set = 0;
    AVPacketTime* p_pkttime = NULL;

    memset(st_index, -1, sizeof(st_index));
    is->last_video_stream = is->video_stream = -1;
    is->last_audio_stream = is->audio_stream = -1;
#ifdef FFP_MERGE
    is->last_subtitle_stream = is->subtitle_stream = -1;
#endif
    is->eof = 0;

    ffp->cached_enable_accurate_seek = ffp->enable_accurate_seek;

    //此处需要进行初始化，避免因后续出现错误走到fail语句，在destyoy时引起crash
    if (!wait_mutex) {
        av_log(NULL, AV_LOG_FATAL, "SDL_CreateMutex(): %s\n", SDL_GetError());
        ret = AVERROR(ENOMEM);
        goto fail;
    }

    ic = avformat_alloc_context();
    if (!ic) {
        av_log(NULL, AV_LOG_FATAL, "Could not allocate context.\n");
        ret = AVERROR(ENOMEM);
        goto fail;
    }
    ic->interrupt_callback.callback = decode_interrupt_cb;
    ic->interrupt_callback.opaque = ffp;
    //fps_probe_size设置为0说明find_stream_info不进行fps的提取，为3时ffmpeg会重新计算提取fps要解析packet数，一般在10以上
    ic->fps_probe_size = 0;

    if (!av_dict_get(ffp->format_opts, "scan_all_pmts", NULL, AV_DICT_MATCH_CASE)) {
        av_dict_set(&ffp->format_opts, "scan_all_pmts", "1", AV_DICT_DONT_OVERWRITE);
        scan_all_pmts_set = 1;
    }
    if (av_stristart(is->filename, "rtmp", NULL) ||
        av_stristart(is->filename, "rtsp", NULL)) {
        ALOGW("[%u] remove 'timeout' option for rtmp.\n", ffp->session_id);
        av_dict_set(&ffp->format_opts, "timeout", NULL, 0);
    }
    if (ffp->iformat_name)
        is->iformat = av_find_input_format(ffp->iformat_name);

    // 目前只对短视频采用开播的音量渐进
    if (!ffp->islive) {
        AudioVolumeProgress_enable(&ffp->audio_vol_progress, 0, ffp->fade_in_end_time_ms, 0.3, 1.0);
    }
    if (ffp->seek_at_start > 0) {
        av_dict_set_int(&ffp->format_opts, "seek_at_start", milliseconds_to_fftime(ffp->seek_at_start), 0);
        ClockTracker_update_is_seeking(&ffp->clock_tracker, true, ffp->seek_at_start);
    }
    av_dict_set(&ffp->format_opts, "enable_seek_forward", !ffp->enable_accurate_seek && ffp->enable_seek_forward_offset ? "1" : "0", 0);

    err = ffp_avformat_open_input(ffp, &ic, is->filename, is->iformat, &ffp->format_opts);
    if (ffp->islive) {
        kwai_collect_live_http_context_info_if_needed(ffp, ic);
    }

    kwai_collect_http_meta_info(ffp, ic);
    KwaiQos_collectPlayerNetInfo(ffp);

    if (err < 0) {
        if (is_abort_by_callback_scinario(ffp, err)) {
            // 用户退出的场景，不需要记录错误码
        } else {
            print_error((ffp->is_live_manifest ? "Live Manifest" : is->filename), err);
            ffp->kwai_error_code = convert_to_kwai_error_code(err);
        }
        goto fail;
    }
    KwaiIoQueueObserver_on_open_input(&ffp->kwai_io_queue_observer, ffp);

    if (ffp->islive) {
        ic->flags |= AVFMT_FLAG_KEEP_SIDE_DATA; // avoid merging side_data into pkt->data
    }

    // ffp_check_use_vod_buffer_checker 必须在解码器打开之前确认是否使用，否则会先渲染出来
    ffp_check_use_vod_buffer_checker(ffp);
    KwaiQos_collectStartPlayBlock(&ffp->kwai_qos, &ffp->kwai_packet_buffer_checker);

    KwaiQos_onInputOpened(&ffp->kwai_qos);
    KwaiQos_setInputFormat(&ffp->kwai_qos, ic->iformat ? ic->iformat->name : NULL);

    if (scan_all_pmts_set)
        av_dict_set(&ffp->format_opts, "scan_all_pmts", NULL, AV_DICT_MATCH_CASE);

    if ((t = av_dict_get(ffp->format_opts, "", NULL, AV_DICT_IGNORE_SUFFIX))) {
        ALOGE("[%u] Option %s not found.\n", ffp->session_id, t->key);
#ifdef FFP_MERGE
        ret = AVERROR_OPTION_NOT_FOUND;
        goto fail;
#endif
    }
    is->ic = ic;

    if (ffp->is_live_manifest)
        SDL_CondSignal(is->continue_kflv_thread);


    if (ffp->genpts)
        ic->flags |= AVFMT_FLAG_GENPTS;

    av_format_inject_global_side_data(ic);

    opts = setup_find_stream_info_opts(ic, ffp->codec_opts);
    orig_nb_streams = ic->nb_streams;

    err = avformat_find_stream_info(ic, opts);

    KwaiIoQueueObserver_on_find_stream_info(&ffp->kwai_io_queue_observer, ffp);

    for (i = 0; i < orig_nb_streams; i++)
        av_dict_free(&opts[i]);
    av_freep(&opts);

    if (err < 0) {
        ALOGE("[%u] %s: could not find codec parameters\n", ffp->session_id, is->filename);
        ret = -1;
        ffp->kwai_error_code = convert_to_kwai_error_code(err);
        goto fail;
    }

    if (ic->pb)
        ic->pb->eof_reached = 0;

    if (ffp->seek_by_bytes < 0)
        ffp->seek_by_bytes =
            !!(ic->iformat->flags & AVFMT_TS_DISCONT) && strcmp("ogg", ic->iformat->name);

    is->max_frame_duration = (ic->iformat->flags & AVFMT_TS_DISCONT) ? 10.0 : 3600.0;
    is->max_frame_duration = 10.0;
    ALOGD("[%u][%s] max_frame_duration: %.3f\n", ffp->session_id, __func__, is->max_frame_duration);

#ifdef FFP_MERGE
    if (!window_title && (t = av_dict_get(ic->metadata, "title", NULL, 0)))
        window_title = av_asprintf("%s - %s", t->value, input_filename);

#endif
    is->realtime = (is_realtime(ic) || (0 == ic->duration) || ffp->islive);
    if (is->realtime) {
        ffp->dcc.next_high_water_mark_in_ms = 200;
        // use backend configuration to replace hardcode
        //ffp->dcc.last_high_water_mark_in_ms = 4000;
        ffp->enable_cache_seek = 0;
    }
    /* if seeking requested, we execute it */
    if (ffp->start_time != AV_NOPTS_VALUE && !is->realtime) {
        int64_t timestamp;

        timestamp = ffp->start_time;
        /* add the stream start time */
        if (ic->start_time != AV_NOPTS_VALUE)
            timestamp += ic->start_time;
        ALOGI("[%u][before play] avformat_seek_file ic:%p, iformat:%p ",
              ffp->session_id, ic, ic == NULL ? 0 : ic->iformat);
        ret = avformat_seek_file(ic, -1, INT64_MIN, timestamp, INT64_MAX, 0);
        if (ret < 0) {
            ALOGW("[%u] %s: could not seek to position %0.3f\n", ffp->session_id, is->filename,
                  (double) timestamp / AV_TIME_BASE);
        }
    }

    if (ffp->show_status)
        av_dump_format(ic, 0, is->filename, 0);

    int video_stream_count = 0;
    int audio_stream_count = 0;
    int h264_stream_count = 0;
    int first_h264_stream = -1;
    for (i = 0; i < ic->nb_streams; i++) {
        AVStream* st = ic->streams[i];
        enum FFAVMediaType type = st->codec->codec_type;
        st->discard = AVDISCARD_ALL;
        if (ffp->wanted_stream_spec[type] && st_index[type] == -1)
            if (avformat_match_stream_specifier(ic, st, ffp->wanted_stream_spec[type]) > 0)
                st_index[type] = i;

        AVCodecContext* codec = ic->streams[i]->codec;
        if (codec->codec_type == AVMEDIA_TYPE_VIDEO) {
            video_stream_count++;
            if (codec->codec_id == AV_CODEC_ID_H264) {
                h264_stream_count++;
                if (first_h264_stream < 0)
                    first_h264_stream = i;
            }
        } else if (codec->codec_type == AVMEDIA_TYPE_AUDIO) {
            audio_stream_count++;
        }
    }
    if (video_stream_count > 1 && st_index[AVMEDIA_TYPE_VIDEO] < 0) {
        st_index[AVMEDIA_TYPE_VIDEO] = first_h264_stream;
        ALOGW("[%u] multiple video stream found, prefer first h264 stream: %d\n", ffp->session_id,
              first_h264_stream);
    }
    if (!ffp->video_disable)
        st_index[AVMEDIA_TYPE_VIDEO] =
            av_find_best_stream(ic, AVMEDIA_TYPE_VIDEO,
                                st_index[AVMEDIA_TYPE_VIDEO], -1, NULL, 0);
    if (!ffp->audio_disable)
        st_index[AVMEDIA_TYPE_AUDIO] =
            av_find_best_stream(ic, AVMEDIA_TYPE_AUDIO,
                                st_index[AVMEDIA_TYPE_AUDIO],
                                st_index[AVMEDIA_TYPE_VIDEO],
                                NULL, 0);
#ifdef FFP_MERGE
    if (!ffp->video_disable && !ffp->subtitle_disable)
        st_index[AVMEDIA_TYPE_SUBTITLE] =
            av_find_best_stream(ic, AVMEDIA_TYPE_SUBTITLE,
                                st_index[AVMEDIA_TYPE_SUBTITLE],
                                (st_index[AVMEDIA_TYPE_AUDIO] >= 0 ?
                                 st_index[AVMEDIA_TYPE_AUDIO] :
                                 st_index[AVMEDIA_TYPE_VIDEO]),
                                NULL, 0);
#endif

    is->show_mode = ffp->show_mode;
#ifdef FFP_MERGE
    if (st_index[AVMEDIA_TYPE_VIDEO] >= 0) {
        AVStream* st = ic->streams[st_index[AVMEDIA_TYPE_VIDEO]];
        AVCodecContext* avctx = st->codec;
        AVRational sar = av_guess_sample_aspect_ratio(ic, st, NULL);
        if (avctx->width)
            set_default_window_size(avctx->width, avctx->height, sar);
    }
#endif

    KwaiQos_setStreamInfo(&ffp->kwai_qos, audio_stream_count, video_stream_count);
    KwaiQos_onStreamInfoFound(&ffp->kwai_qos);
    kwai_collect_AVStream_info_to_video_state(is, ic, st_index[AVMEDIA_TYPE_AUDIO], st_index[AVMEDIA_TYPE_VIDEO]);
    KwaiQos_copyAvformatContextMetadata(&ffp->kwai_qos, ic);

    AbLoop_on_play_start(&ffp->ab_loop, ffp);
    BufferLoop_update_pos(&ffp->buffer_loop, ffp_get_duration_l(ffp));
    bool is_seek_at_start = false;
    /* offset should be seeked*/
    if (ffp->seek_at_start > 0) {
        ffp_seek_to_l(ffp, ffp->seek_at_start);
        is_seek_at_start = true;
        // hls 启播seek在probe之前处理，open_input之后不再重复调用avformat_seek_file, 在predemux之前seek，保证有启播seek场景predemux数据有效
        if (!is_hls(ic)) {
            int64_t seek_target = is->seek_pos;
            int64_t seek_min = is->seek_rel > 0 ? seek_target - is->seek_rel + 2 : INT64_MIN;
            int64_t seek_max = is->seek_rel < 0 ? seek_target - is->seek_rel - 2 : INT64_MAX;
            ALOGI("[%u][seek_at_start] avformat_seek_file ic:%p, iformat:%p time:%lld", ffp->session_id,
                  ic, ic == NULL ? 0 : ic->iformat, seek_target);
            ret = avformat_seek_file(ic, -1, seek_min, seek_target, seek_max, 0);
            if (ret < 0) {
                ALOGE("[%u][seek_at_start] %s: could not seek to position %0.3f\n", ffp->session_id,
                      is->filename, (double) seek_target / AV_TIME_BASE);
            }
        }
    }

    bool msg_prepared_notified = false;
    if (ffp_is_pre_demux_enabled(ffp) && ffp_is_pre_demux_ver1_enabled(ffp)) {
        ffp->start_on_prepared = 0; // 为了能让ijkmp的状态正确变为paused
        ffp_notify_msg1(ffp, FFP_MSG_PREPARED);
        msg_prepared_notified = true;
        if ((ret = PreDemux_pre_demux_ver1(ffp,
                                           ic, pkt,
                                           st_index[AVMEDIA_TYPE_AUDIO],
                                           st_index[AVMEDIA_TYPE_VIDEO]
                                          )) < 0) {
            if (is_abort_by_callback_scinario(ffp, ret)) {
                // 用户退出的场景，不需要记录错误码
            } else {
                ffp->kwai_error_code = convert_to_kwai_error_code(ret);
            }
            goto fail;
        }
    }
    if (ffp_is_pre_demux_enabled(ffp) && ffp_is_pre_demux_ver2_enabled(ffp)) {
        ffp->start_on_prepared = 0; // 为了能让ijkmp的状态正确变为paused
        ffp_notify_msg1(ffp, FFP_MSG_PREPARED);
        msg_prepared_notified = true;
        if ((ret = PreDemux_pre_demux_ver2(ffp,
                                           ic, pkt,
                                           st_index[AVMEDIA_TYPE_AUDIO],
                                           st_index[AVMEDIA_TYPE_VIDEO]
                                          )) < 0) {
            if (is_abort_by_callback_scinario(ffp, ret)) {
                // 用户退出的场景，不需要记录错误码
            } else {
                ffp->kwai_error_code = convert_to_kwai_error_code(ret);
            }
            goto fail;
        }
    }

    KwaiQos_onPreDemuxFinish(&ffp->kwai_qos, ffp);

    //在stream_component_open前先start packet queue，避免音频在视频解码前就开始渲染
    if (st_index[AVMEDIA_TYPE_AUDIO] >= 0) {
        if (!packet_queue_is_started(&is->audioq)) {
            packet_queue_start(&is->audioq);
        }
    }

    if (st_index[AVMEDIA_TYPE_VIDEO] >= 0) {
        if (!packet_queue_is_started(&is->videoq)) {
            packet_queue_start(&is->videoq);
        }
    } else {
        is->video_refresh_abort_request = 1;
    }

#if defined(__ANDROID__)
    check_mediacodec_availability(ffp, st_index[AVMEDIA_TYPE_VIDEO]);
#endif

    if (!ffp->async_stream_component_open ||
        !use_video_hardware_decoder(ffp, st_index[AVMEDIA_TYPE_VIDEO])) {
        ALOGI("[%u] use sync_stream_component_open\n", ffp->session_id);
        if (st_index[AVMEDIA_TYPE_AUDIO] >= 0) {
            ret = stream_component_open(ffp, st_index[AVMEDIA_TYPE_AUDIO]);
            if (ret < 0) {
                ffp->kwai_error_code = EIJK_KWAI_UNSUPPORT_ACODEC;
                goto fail;  // stop player for unsupported audio codec for now
            }
        } else {
            video_state_set_av_sync_type(is, AV_SYNC_VIDEO_MASTER);
        }

        ret = -1;
        if (st_index[AVMEDIA_TYPE_VIDEO] >= 0) {
            ret = stream_component_open(ffp, st_index[AVMEDIA_TYPE_VIDEO]);
            if (ret < 0) {
                ffp->kwai_error_code = EIJK_KWAI_UNSUPPORT_VCODEC;
            }
        }
    } else {
        // multi-thread initialize decoder
        ffp->st_index[AVMEDIA_TYPE_AUDIO] = st_index[AVMEDIA_TYPE_AUDIO];
        ffp->st_index[AVMEDIA_TYPE_VIDEO] = st_index[AVMEDIA_TYPE_VIDEO];

        ALOGI("[%u] use async_stream_component_open\n", ffp->session_id);
        ret = async_open_stream_component(ffp);

        if (ret < 0 || ffp->kwai_error_code == EIJK_KWAI_UNSUPPORT_ACODEC
            || ffp->kwai_error_code == EIJK_KWAI_UNSUPPORT_VCODEC) {
            goto fail;
        }
    }

#ifdef FFP_MERGE
    if (st_index[AVMEDIA_TYPE_SUBTITLE] >= 0) {
        stream_component_open(ffp, st_index[AVMEDIA_TYPE_SUBTITLE]);
    }
#endif

    KwaiQos_onHardwareDec(&ffp->kwai_qos, ffp->hardware_vdec);
    KwaiQos_onDecoderOpened(&ffp->kwai_qos);

    KwaiQos_collectPlayerMetaInfo(ffp);

    KwaiQos_setEnableModifyBlock(&ffp->kwai_qos, ffp->enable_modify_block);

    if (is->video_stream < 0 && is->audio_stream < 0) {
        ALOGE("[%u] Failed to open file '%s' or configure filtergraph\n", ffp->session_id,
              is->filename);
        ret = -1;
        goto fail;
    }

    if (is->show_mode == SHOW_MODE_NONE) {
        is->show_mode = ret >= 0 ? SHOW_MODE_VIDEO : SHOW_MODE_RDFT;
    }

    KwaiQos_setEnableAudioGain(&ffp->kwai_qos, ffp->audio_gain.enabled);

    AVDictionaryEntry* entry_comment = av_dict_get(ic->metadata, "comment", NULL, 0);
    // kwai audio gain
#if defined(CONFIG_KS_AUDIOPROCESS) || defined(__APPLE__)
    if (!ffp->islive && ffp->audio_gain.enabled) {
        if (entry_comment) {
            AudioGain_parse_config(&ffp->audio_gain, entry_comment->value);
        }
    }
    if (!ffp->islive && ffp->audio_gain.enabled && ffp->audio_gain.audio_str) {
        AudioProcessor_init(&ffp->audio_gain.audio_processor, is->audio_filter_src.freq, is->audio_filter_src.channels,
                            ffp->audio_gain.audio_str);
        KwaiQos_setAudioStr(&ffp->kwai_qos, ffp->audio_gain.audio_str);
    }

    if (!ffp->islive && ffp->audio_gain.enabled && ffp->audio_gain.enable_audio_compress) {
        AudioCompressProcessor_init(&ffp->audio_gain.audio_compress_processor, is->audio_filter_src.freq, is->audio_filter_src.channels,
                                    ffp->audio_gain.make_up_gain, ffp->audio_gain.normalnize_gain);
    }

    if (ffp->enable_audio_spectrum) {
        AudioSpectrumProcessor_init(&ffp->audio_spectrum_processor, is->audio_filter_src.freq,
                                    is->audio_filter_src.channels);
    }
#endif

    if (entry_comment) {
        KwaiQos_getTranscoderVersionFromComment(&ffp->kwai_qos, entry_comment->value);
    }

    ijkmeta_set_avformat_context_l(ffp->meta, ic);
    ffp->stat.bit_rate = ic->bit_rate;
    if (st_index[AVMEDIA_TYPE_VIDEO] >= 0)
        ijkmeta_set_int64_l(ffp->meta, IJKM_KEY_VIDEO_STREAM, st_index[AVMEDIA_TYPE_VIDEO]);
    if (st_index[AVMEDIA_TYPE_AUDIO] >= 0)
        ijkmeta_set_int64_l(ffp->meta, IJKM_KEY_AUDIO_STREAM, st_index[AVMEDIA_TYPE_AUDIO]);
    if (strcmp(ffp->is->_read_tid.name, "stream_re") && is_realtime(ic)) {
        ffp->start_time = ic->start_time;
    }

    if (is->audio_stream >= 0) {
        is->audioq.is_buffer_indicator = 1;
        is->buffer_indicator_queue = &is->audioq;
    } else if (is->video_stream >= 0) {
        is->videoq.is_buffer_indicator = 1;
        is->buffer_indicator_queue = &is->videoq;
    } else {
        assert("invalid streams");
    }

    if (0 == ffp->infinite_buffer && is->realtime) {
        ffp->infinite_buffer = 1;
    }

    if (ffp_is_pre_demux_enabled(ffp)) {
        // do nothing ,need not to pause
    } else if (!ffp->start_on_prepared) {
        // pre-decode mode，其中一个重要是pause audio/video的render线程，让他们进入一个while等待循环
        // 此时尚未开播，不需要调用SDL_AoutPause， 针对iOS AudioQueuePause 属于耗时操作，会影响首屏
        is->pause_req = 1;
        is->paused = is->audclk.paused = is->vidclk.paused = is->extclk.paused = 1;
    } else {
        // start_on_prepared = true situation, go on without pause
    }

    if (is->video_st && is->video_st->codec) {
        AVCodecContext* avctx = is->video_st->codec;
        ffp_notify_msg3(ffp, FFP_MSG_VIDEO_SIZE_CHANGED, avctx->width, avctx->height);
        ffp_notify_msg3(ffp, FFP_MSG_SAR_CHANGED, avctx->sample_aspect_ratio.num,
                        avctx->sample_aspect_ratio.den);
    }

    ffp->prepared = true;
    if (!msg_prepared_notified) {
        ffp_notify_msg1(ffp, FFP_MSG_PREPARED);
        msg_prepared_notified = true;
    }
    KwaiQos_onAllPrepared(&ffp->kwai_qos);

    ALOGI("[%u][%s] after AllPrepared, ffp status, use_pre_demux_ver:%d, start_on_prepared:%d, auto_resume:%d \n",
          ffp->session_id, __func__, ffp_is_pre_demux_enabled(ffp), ffp->start_on_prepared, ffp->auto_resume);

    if (ffp->auto_resume) {
        ffp->auto_resume = 0;
        ffp_notify_msg1(ffp, FFP_REQ_START);
    }


    KwaiQos_onAppStartPlayer(ffp);

    // kwai code start
    // 带宽控制设置以及qos收集
    ffp_check_use_dcc_algorithm(ffp, ic);
    KwaiQos_collectDccAlg(&ffp->kwai_qos, &ffp->dcc_algorithm);
    ffp_ac_player_statistic_set_initial(ffp, ic);

    bool first_packt_after_seek_got = false;

    AVPacketTime v_pkttime;
    memset(&v_pkttime, 0, sizeof(AVPacketTime));
    // kwai code end

    KwaiPacketQueueBufferChecker_on_lifecycle_start(&ffp->kwai_packet_buffer_checker);

    KwaiQos_onReadThreadForLoopStart(&ffp->kwai_qos);
    for (;;) {
        if (is->interrupt_exit) {
            ffp->kwai_error_code = EIJK_KWAI_READ_DATA_IO_TIMEOUT;
            break;
        }
        if (is->abort_request || is->read_abort_request) {
            break;
        }
        KwaiQos_onSystemPerformance(&ffp->kwai_qos);
#ifdef FFP_MERGE
        if (is->paused != is->last_paused) {
            is->last_paused = is->paused;
            if (is->paused)
                is->read_pause_return = av_read_pause(ic);
            else
                av_read_play(ic);
        }
#endif
#if CONFIG_RTSP_DEMUXER || CONFIG_MMSH_PROTOCOL
        if (is->paused &&
            (!strcmp(ic->iformat->name, "rtsp") ||
             (ic->pb && !strncmp(ffp->input_filename, "mmsh:", 5)))) {
            /* wait 10 ms to avoid trying to get another packet */
            /* XXX: horrible */
            SDL_Delay(10);
            continue;
        }
#endif
        first_packt_after_seek_got = true;
        if (is->seek_req) {
            seek_file_and_update_status(ffp, is, is_seek_at_start);
            is_seek_at_start = false;
            completed = 0;
            is->queue_attachments_req = 1;
            is->eof = 0;
            is->last_video_dts_ms = -1;
            is->last_audio_dts_ms = -1;
            first_packt_after_seek_got = false;
            if (is->seek_cached_pos >= 0 && is->seek_cached_pos != is->seek_pos) {
                is->seek_pos = is->seek_cached_pos;
                is->seek_rel = is->seek_cached_rel;
                is->seek_flags = is->seek_cached_flags;
                SDL_LockMutex(is->cached_seek_mutex);
                is->seek_cached_pos = -1;
                SDL_UnlockMutex(is->cached_seek_mutex);
                continue;
            } else {
                is->seek_cached_pos = -1;
                is->seek_req = 0;
            }
            // seek后更新一下player statistic
            ffp_statistic_l(ffp);
        } // if (is->seek_req) END

        if (is->queue_attachments_req) {
            if (is->video_st && (is->video_st->disposition & AV_DISPOSITION_ATTACHED_PIC)) {
                AVPacket copy;
                if ((ret = av_copy_packet(&copy, &is->video_st->attached_pic)) < 0)
                    goto fail;
                packet_queue_put(&is->videoq, &copy, NULL);
                packet_queue_put_nullpacket(&is->videoq, is->video_stream);
            }
            is->queue_attachments_req = 0;
        }

        if ((ffp->infinite_buffer < 1 || is->paused) && !is->seek_req &&
#ifdef FFP_MERGE
            (is->audioq.size + is->videoq.size + is->subtitleq.size > MAX_QUEUE_SIZE
#else
            ((((is->cache_seeked)
               || (is->audioq.size + is->videoq.size > ffp->dcc.max_buffer_size)
               || (is->audio_st && ffp->stat.audio_cache.duration > FFDemuxCacheControl_current_max_buffer_dur_ms(&ffp->dcc))
               || (!is->audio_st && is->video_st && ffp->stat.video_cache.duration > FFDemuxCacheControl_current_max_buffer_dur_ms(&ffp->dcc))
              )
              && (is->audioq.nb_packets > MIN_MIN_FRAMES || is->audio_stream < 0 || is->audioq.abort_request)
              && (is->videoq.nb_packets > MIN_MIN_FRAMES || is->video_stream < 0 || is->videoq.abort_request)
             )
#endif
             || ((is->audioq.nb_packets > MIN_FRAMES
                  || is->audio_stream < 0
                  || is->audioq.abort_request)
                 && (is->videoq.nb_packets > MIN_FRAMES
                     || is->video_stream < 0
                     || is->videoq.abort_request
                     || (is->video_st && (is->video_st->disposition & AV_DISPOSITION_ATTACHED_PIC)))
#ifdef FFP_MERGE
                 && (is->subtitleq.nb_packets > MIN_FRAMES || is->subtitle_stream < 0 || is->subtitleq.abort_request))))
#else
                )
            ))
#endif
        {
            if (!is->eof) {
                ffp_toggle_buffering(ffp, 0, 1);
                is->cache_seeked = 0;
            }

            SDL_LockMutex(wait_mutex);
            SDL_CondWaitTimeout(is->continue_read_thread, wait_mutex, 10);
            SDL_UnlockMutex(wait_mutex);
            continue;
        }

        if (ffp->exit_on_dec_error) {
            // 播放到文件末尾忽略解码错误，防止io错误导致loop受影响，改功能只在exit_on_dec_error 打开有效，目前只针对A1项目
            if (ffp->v_dec_err < 0 && is->video_stream >= 0
                && !(is->viddec.finished == is->videoq.serial && frame_queue_nb_remaining(&is->pictq) == 0)) {
                ffp->kwai_error_code = EIJK_KWAI_DEC_ERR;
                goto fail;
            }
        }
        if (ffp->buffer_loop.enable && ffp->error == EIJK_KWAI_BLOCK_ERR) {
            ffp->kwai_error_code = ffp->error;
            goto fail;
        }

        ret = check_stream_complete_status(ffp, &completed, wait_mutex);
        if (ret == STREAM_COMPLETE_STATUS_EXIT) {
            goto fail;
        } else if (ret == STREAM_COMPLETE_STATUS_SEEK) {
            continue;
        }

        if (ffp->islive) {
            KwaiRotateControl_update(&ffp->kwai_rotate_control, ic->metadata);
        }

        pkt->flags = 0;
        is->read_start_time = av_gettime_relative();
        ret = av_read_frame(ic, pkt);

        if (pkt->pts < 0) {
            if (pkt->stream_index == is->audio_stream && ffp->audio_pts_invalid && is->audio_st)
                ffp->audio_invalid_duration += av_rescale_q(pkt->duration, is->audio_st->time_base,
                                                            AV_TIME_BASE_Q);
            else if (pkt->stream_index == is->video_stream && ffp->video_pts_invalid && is->video_st)
                ffp->video_invalid_duration += av_rescale_q(pkt->duration, is->video_st->time_base,
                                                            AV_TIME_BASE_Q);
        } else {
            if (pkt->stream_index == is->audio_stream)
                ffp->audio_pts_invalid = false;
            else if (pkt->stream_index == is->video_stream)
                ffp->video_pts_invalid = false;
        }

        if (0 == ret) {
            // kwai code start: qos + privNal
            if (ffp->islive &&
                (pkt->stream_index == is->audio_stream || pkt->stream_index == is->video_stream)) {
                ffp_live_voice_comment(ffp, pkt->pts, ffp->is->ic);
            }
            if (ffp->islive && pkt->stream_index == is->audio_stream
                && AV_CODEC_ID_AAC == ic->streams[pkt->stream_index]->codec->codec_id) {
                handlePrivDataInAac(ffp, pkt);
            }


            if (pkt->stream_index == is->audio_stream) {
                ffp_kwai_collect_dts_info(ffp, pkt, is->audio_stream, is->video_stream, ic->streams[pkt->stream_index]);
                KwaiQos_onAudioPacketReceived(&ffp->kwai_qos);
                KwaiIoQueueObserver_on_read_audio_frame(&ffp->kwai_io_queue_observer, ffp);
                if (ffp->islive && ffp->use_aligned_pts && !is->is_audio_pts_aligned && is->video_first_pts_ms != -1) {
                    int64_t ptsMs = av_rescale_q(pkt->pts, ic->streams[pkt->stream_index]->time_base,
                    (AVRational) {1, 1000});
                    if (pkt->pts != AV_NOPTS_VALUE && ptsMs < is->video_first_pts_ms) {
                        continue;
                    }
                }
            } else if (pkt->stream_index == is->video_stream) {
                KwaiQos_onVideoPacketReceived(&ffp->kwai_qos);
                KwaiIoQueueObserver_on_read_video_frame(&ffp->kwai_io_queue_observer, ffp);
                int64_t ptsMs = av_rescale_q(pkt->pts, ic->streams[pkt->stream_index]->time_base,
                (AVRational) {1, 1000});
                if (ffp->islive && is->video_first_pts_ms == -1 && pkt->pts != AV_NOPTS_VALUE) {
                    is->video_first_pts_ms = ptsMs;
                }

                // fixme 这段逻辑应该是只有live用得到，不应该每次都走
                if (pkt->pts != AV_NOPTS_VALUE && ffp->qos_pts_offset_got && ffp->wall_clock_updated) {
                    DelayStat_calc_pts_delay(&ffp->qos_delay_video_recv, ffp->wall_clock_offset,
                                             ffp->qos_pts_offset, ptsMs);
                }

                if (ffp->islive && handle_live_private_nal(ffp, ic, pkt, &v_pkttime, ptsMs) <= 0) {
                    continue;
                }

                ffp_kwai_collect_dts_info(ffp, pkt, is->audio_stream, is->video_stream, ic->streams[pkt->stream_index]);
            }
            if (!first_packt_after_seek_got) {
                KwaiQos_onFirstPacketAfterSeekEnd(&ffp->kwai_qos);
            }
#ifdef FFP_SHOW_DTS
            ALOGD("[%u] qos_dts_duration: %lld, first_dts: %lld\n", ffp->session_id, ffp->qos_dts_duration, is->first_dts);
#endif
            // kwai code end: qos + privNal
        }

        // 直播kbytes依赖is->bytes_read， 例如：10s没读到数据retry的逻辑就是依赖这个值
        if (is->ic && is->ic->pb) {
            is->bytes_read = ffp->is->ic->pb->bytes_read;
        } else if (ffp->is_live_manifest) {
            is->bytes_read = ffp->kflv_player_statistic.kflv_stat.total_bytes_read;
        } else {
            is->bytes_read = ffp->i_video_decoded_size + ffp->i_audio_decoded_size;
        }

        if (ret < 0) {
            ffp->kwai_packet_buffer_checker.on_read_frame_error(&ffp->kwai_packet_buffer_checker, ffp);
//            ALOGE("ffplay read_thread after av_read_frame, ret:%d(%s/%s)", ret, get_error_code_fourcc_string_macro(ret), ffp_get_error_string(ret));
            if (is_abort_by_callback_scinario(ffp, ret)) {
                // exit immediately with ffp->kwai_error_code = 0
                break;
            }

            int pb_eof = 0;
            int pb_error = 0;

            //此处ic->pb对应的是m3u8 content的读取，不应该用来判断ts文件结尾。avio_feof会再尝试读取m3u8 content，若出错会覆盖ts分片的读数据错误码
            if (is_hls(ic)) {
                if (ret == AVERROR_EOF && !is->eof) {
                    pb_eof = 1;
                }
            } else {
                if ((ret == AVERROR_EOF || avio_feof(ic->pb)) && !is->eof) {
                    pb_eof = 1;
                }
            }

            if (ic->pb && ic->pb->error) {
                pb_eof = 1;
                pb_error = ic->pb->error;
            }
            if (ret == AVERROR_EXIT) {
                pb_eof = 1;
                pb_error = AVERROR_EXIT;
            }

            if (ffp->is_live_manifest && ret == AVERROR_EOF) {
                pb_eof = 1;
                pb_error = AVERROR_EOF;
            }

            if (pb_eof) {
                if (is->video_stream >= 0 && !(ffp->is_audio_reloaded))
                    packet_queue_put_nullpacket(&is->videoq, is->video_stream);
                if (is->audio_stream >= 0 && !(ffp->is_audio_reloaded))
                    packet_queue_put_nullpacket(&is->audioq, is->audio_stream);
#ifdef FFP_MERGE
                if (is->subtitle_stream >= 0)
                    packet_queue_put_nullpacket(&is->subtitleq, is->subtitle_stream);
#endif
                is->eof = 1;
            }

            if (pb_error) {
                if (is->video_stream >= 0 && !(ffp->is_audio_reloaded))
                    packet_queue_put_nullpacket(&is->videoq, is->video_stream);
                if (is->audio_stream >= 0 && !(ffp->is_audio_reloaded))
                    packet_queue_put_nullpacket(&is->audioq, is->audio_stream);
#ifdef FFP_MERGE
                if (is->subtitle_stream >= 0)
                    packet_queue_put_nullpacket(&is->subtitleq, is->subtitle_stream);
#endif
                is->eof = 1;
                ffp->error = pb_error;
                ffp->kwai_error_code = convert_to_kwai_error_code(ffp->error);
                ALOGE("[%u] has pb_error, av_read_frame , ffp->kwai_error_code:%d, ret:%d(%s/%s), ffp->error:%d(%s/%s)\n",
                      ffp->session_id, ffp->kwai_error_code,
                      ret, get_error_code_fourcc_string_macro(ret), ffp_get_error_string(ret),
                      ffp->error, get_error_code_fourcc_string_macro(ffp->error), ffp_get_error_string(ffp->error));
                if (ffp->error == AVERROR_EXIT) {
                    ALOGE("[%u] ffp->error == AVERROR_EXIT, goto fail \n", ffp->session_id);
                }

            } else {
                ffp->error = 0;
            }

            if (is->abort_request || is->read_abort_request) {
                ALOGE("[%u][%s] Do quit exit \n", ffp->session_id, __FUNCTION__);
                break;
            }

            if (ffp->error < 0 || is->eof) {
                ffp_toggle_buffering(ffp, 0, 1);
                // eof的时候也需要最后执行一次buffering，不然 playable_duration和bufferUpdateListener都无法跑到100%
                ffp_read_thread_check_buffering(ffp, true);
                SDL_Delay(100);
            }
            SDL_LockMutex(wait_mutex);
            SDL_CondWaitTimeout(is->continue_read_thread, wait_mutex, 10);
            SDL_UnlockMutex(wait_mutex);
            ffp_statistic_l(ffp);
            continue;
        } else {
            is->eof = 0;
        } // if (ret < 0)

        if (pkt->flags & AV_PKT_FLAG_DISCONTINUITY) {
            if (is->audio_stream >= 0) {
                packet_queue_put(&is->audioq, &flush_pkt, NULL);
            }
#ifdef FFP_MERGE
            if (is->subtitle_stream >= 0) {
                packet_queue_put(&is->subtitleq, &flush_pkt, NULL);
            }
#endif
            if (is->video_stream >= 0) {
                packet_queue_put(&is->videoq, &flush_pkt, NULL);
            }
        }

        // check if packet in play range
        pkt_in_play_range = ffp_pkt_in_play_range(ffp, ic, pkt);
        if (is->realtime) {
            pkt_in_play_range = live_pkt_in_play_range_due_to_chasing(ffp, pkt, pkt_in_play_range, false);
        }

        // put packet into related queue if in play range
        if (pkt->stream_index == is->audio_stream && pkt_in_play_range) {
#ifdef FFP_SHOW_DEMUX_PTS
            ALOGI("[%u] demuxer -> ist_index:%d type:audio "
                  "pkt_pts:%s pkt_pts_time:%s pkt_dts:%s pkt_dts_time:%s, "
                  "count: %d\n",
                  ffp->session_id, pkt->stream_index,
                  av_ts2str(pkt->pts), av_ts2timestr(pkt->pts, &is->audio_st->time_base),
                  av_ts2str(pkt->dts), av_ts2timestr(pkt->dts, &is->audio_st->time_base),
                  is->audioq.nb_packets);
#endif

            if (ffp->is_audio_reloaded && ffp->first_video_frame_rendered) {
                continue;
            } else {
                ffp->last_audio_pts = pkt->pts;
            }

            packet_queue_put(&is->audioq, pkt, p_pkttime);
            // ffp_debug_show_queue_status("put_audioq", ffp);
        } else if (pkt->stream_index == is->video_stream && pkt_in_play_range
                   &&
                   !(is->video_st && (is->video_st->disposition & AV_DISPOSITION_ATTACHED_PIC))) {
#ifdef FFP_SHOW_DEMUX_PTS
            ALOGI("[%u] demuxer -> ist_index:%d type:video "
                  "pkt_pts:%s pkt_pts_time:%s pkt_dts:%s pkt_dts_time:%s\n",
                  ffp->session_id, pkt->stream_index,
                  av_ts2str(pkt->pts), av_ts2timestr(pkt->pts, &is->video_st->time_base),
                  av_ts2str(pkt->dts), av_ts2timestr(pkt->dts, &is->video_st->time_base));
#endif

            if (is->dts_of_last_frame != AV_NOPTS_VALUE && pkt->dts != AV_NOPTS_VALUE
                && pkt->dts > is->dts_of_last_frame && (is->realtime || pkt->duration <= 0)) {
                pkt->duration = pkt->dts - is->dts_of_last_frame;
            }

            if (is->realtime && ((pkt->flags & AV_PKT_FLAG_KEY) == AV_PKT_FLAG_KEY)) {
                check_live_video_pkt_timestamp_rollback(ffp, pkt);
            }

            is->dts_of_last_frame = pkt->dts;

            packet_queue_put(&is->videoq, pkt, &v_pkttime);
            // ffp_debug_show_queue_status("put_videoq", ffp);
            v_pkttime.abs_pts = 0;
#ifdef FFP_MERGE
        } else if (pkt->stream_index == is->subtitle_stream && pkt_in_play_range) {
            packet_queue_put(&is->subtitleq, pkt);
#endif
        } else {
            KwaiQos_onDropPacket(&ffp->kwai_qos, pkt, ic, is->audio_stream, is->video_stream, ffp->session_id);
            av_packet_unref(pkt);
        } // put packet into queue logic

        ffp_statistic_l(ffp);

        ffp_read_thread_check_buffering(ffp, false);
    } // read_frame for loop

    ret = 0;
fail:
    KwaiQos_onError(&ffp->kwai_qos, ffp->kwai_error_code);
    KwaiQos_collectAwesomeCacheInfoOnce(&ffp->kwai_qos, ffp);

    ALOGI("[%u][read_thread] going to exit, ic:%p, is->ic:%p, ffp->kwai_error_code:%d",
          ffp->session_id, ic, is->ic, ffp->kwai_error_code);
    if (ic && !is->ic) {
        ffp_avformat_close_input(ffp, &ic);
    }

    // 如果底层连接超时，avformat_open_input失败后，会导致ic=null,这个时候ffp_avformat_close_input不会被执行，
    // 所以ffp_close_release_AwesomeCache_AVIOContext的执行不能依赖ic和is->ic的值，而要无条件执行
    ffp_close_release_AwesomeCache_AVIOContext(ffp);


    if (p_pkttime != NULL) {
        av_freep(&p_pkttime);
    }
    if (!is->read_abort_request && (!ffp->prepared || !is->abort_request)) {
        KwaiQos_onPausePlayer(&ffp->kwai_qos);
        toggle_pause(ffp, 1);
        ffp_notify_msg3(ffp, FFP_MSG_ERROR, ffp->kwai_error_code, 0);
    }
    SDL_DestroyMutex(wait_mutex);
    ALOGI("[%u][read_thread] EXIT", ffp->session_id);
    return 0;
}

