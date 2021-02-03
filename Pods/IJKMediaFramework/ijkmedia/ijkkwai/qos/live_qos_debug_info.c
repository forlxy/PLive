//
//  live_qos_debug_info.c
//  IJKMediaFramework
//
//  Created by wangtao03 on 2018/4/27.
//  Copyright © 2018年 kuaishou. All rights reserved.
//

#include <string.h>
#include <ijkmedia/ijkkwai/c_resource_monitor.h>
#include "ijkkwai/kwai_qos.h"
#include "ijkplayer_internal.h"
#include "live_qos_debug_info.h"
#include "str_encrypt.h"
void LiveQosDebugInfo_init(LiveQosDebugInfo* info) {
    assert(info);
    memset(info, 0, sizeof(LiveQosDebugInfo));
    info->ac_type = -1;
}

void LiveQosDebugInfo_release(LiveQosDebugInfo* info) {
    av_freep(&info->metaComment);
    av_freep(&info->host);
    av_freep(&info->metaAudioDecoderInfo);
    av_freep(&info->metaVideoDecoderInfo);
    av_freep(&info->metaVideoDecoderDynamicInfo);
    av_freep(&info->hostInfo);
    av_freep(&info->vencInit);
    av_freep(&info->aencInit);
}

static void getMetaInfo(KwaiQos* kwai_qos, LiveQosDebugInfo* info) {
    if (kwai_qos->ic_metadata) {
        // kshi/ksvi/ksai: encrypted
        char tempStr[KSY_QOS_STR_MAX_LEN] = {0};
        AVDictionaryEntry* hostInfo = av_dict_get(kwai_qos->ic_metadata, "kshi", NULL, 0);
        if (hostInfo && !info->hostInfo) {
            decryptStr(tempStr, hostInfo->value,
                       FFMIN((int) strlen(hostInfo->value), KSY_QOS_STR_MAX_LEN - 1));
            info->hostInfo = av_strdup(tempStr);
        }

        // kwai-hostinfo: compatible with old version, non-encrypted
        hostInfo = av_dict_get(kwai_qos->ic_metadata, "kwai-hostinfo", NULL, 0);
        if (hostInfo && !info->hostInfo) {
            info->hostInfo = av_strdup(hostInfo->value);
        }

        AVDictionaryEntry* vencInit = av_dict_get(kwai_qos->ic_metadata, "ksvi", NULL, 0);
        if (vencInit && !info->vencInit) {
            memset(tempStr, 0, KSY_QOS_STR_MAX_LEN);
            decryptStr(tempStr, vencInit->value,
                       FFMIN((int) strlen(vencInit->value), KSY_QOS_STR_MAX_LEN - 1));
            info->vencInit = av_strdup(tempStr);
        }
        AVDictionaryEntry* aencInit = av_dict_get(kwai_qos->ic_metadata, "ksai", NULL, 0);
        if (aencInit && !info->aencInit) {
            memset(tempStr, 0, KSY_QOS_STR_MAX_LEN);
            decryptStr(tempStr, aencInit->value,
                       FFMIN((int) strlen(aencInit->value), KSY_QOS_STR_MAX_LEN - 1));
            info->aencInit = av_strdup(tempStr);
        }
    }
}

void LiveQosDebugInfo_collect(LiveQosDebugInfo* info, struct IjkMediaPlayer* mp) {
    assert(info);
    assert(mp);
    assert(mp->ffplayer);

    FFPlayer* ffp = mp->ffplayer;

    KwaiQos* kwai_qos = &ffp->kwai_qos;
    VideoState* is = ffp->is;
    int audio_time_base_valid = 0;
    int video_time_base_valid = 0;

    if (!is) {
        return;
    }

    if (is->audio_st) {
        audio_time_base_valid = is->audio_st->time_base.den > 0 && is->audio_st->time_base.num > 0;
    }
    if (is->video_st) {
        video_time_base_valid = is->video_st->time_base.den > 0 && is->video_st->time_base.num > 0;
    }

    if (is->audio_st) {
        info->audioBufferByteLength = is->audioq.size;
        info->audioTotalDataSize = ffp->i_audio_decoded_size + is->audioq.size;
        if (audio_time_base_valid) {
            info->audioBufferTimeLength = (int)(is->audioq.duration *
                                                av_q2d(is->audio_st->time_base) * 1000);
        }
    }

    if (is->video_st) {
        info->videoBufferByteLength = is->videoq.size;
        info->videoTotalDataSize = ffp->i_video_decoded_size + is->videoq.size;
        if (video_time_base_valid) {
            info->videoBufferTimeLength = (int)(is->videoq.duration *
                                                av_q2d(is->video_st->time_base) * 1000);
        }
    }

    info->decodedDataSize = ffp->i_video_decoded_size + ffp->i_audio_decoded_size;
    info->totalDataBytes = ffp->is->bytes_read;

    info->audioDelay = ffp->qos_delay_audio_render.period_avg;
    info->videoDelayRecv = ffp->qos_delay_video_recv.period_avg;
    info->videoDelayBefDec = ffp->qos_delay_video_before_dec.period_avg;
    info->videoDelayAftDec = ffp->qos_delay_video_after_dec.period_avg;
    info->videoDelayRender = ffp->qos_delay_video_render.period_avg;
    info->droppedDurationBefFirstScreen = (int)(kwai_qos->runtime_stat.begining_dropped_duration >= 0 ?
                                                kwai_qos->runtime_stat.begining_dropped_duration : 0);
    info->droppedDurationTotal = (int) kwai_qos->runtime_stat.total_dropped_duration;
    info->speedupThresholdMs = ffp->is_live_manifest ? ffp->i_buffer_time_max_live_manifest : ffp->i_buffer_time_max;

    info->isLiveManifest = ffp->is_live_manifest == 1;
    if (info->isLiveManifest) {
        if (ffp->kflv_player_statistic.kflv_stat.cur_decoding_flv_index >= 0  &&
            ffp->kflv_player_statistic.kflv_stat.cur_decoding_flv_index < ffp->kflv_player_statistic.kflv_stat.flv_nb) {
            info->kflvPlayingBitrate = ffp->kflv_player_statistic.kflv_stat.flvs[ffp->kflv_player_statistic.kflv_stat.cur_decoding_flv_index].total_bandwidth_kbps;
        }
        info->kflvBandwidthCurrent = ffp->kflv_player_statistic.kflv_stat.bandwidth_current;
        info->kflvBandwidthFragment = ffp->kflv_player_statistic.kflv_stat.bandwidth_fragment;
        info->kflvCurrentBufferMs = ffp->kflv_player_statistic.kflv_stat.current_buffer_ms;
        info->kflvEstimateBufferMs = ffp->kflv_player_statistic.kflv_stat.estimate_buffer_ms;
        info->kflvPredictedBufferMs = ffp->kflv_player_statistic.kflv_stat.predicted_buffer_ms;
        info->kflvSpeedupThresholdMs = ffp->kflv_player_statistic.kflv_stat.speed_up_threshold;
    }

    info->blockCnt = kwai_qos->runtime_stat.block_cnt;
    info->blockDuration = KwaiQos_getBufferTotalDurationMs(kwai_qos);
    info->videoReadFramesPerSecond = ffp->stat.vrps;
    info->videoDecodeFramesPerSecond = ffp->stat.vdps;
    info->videoDisplayFramesPerSecond = ffp->stat.vfps;

    info->sourceDeviceType = ffp->source_device_type;
    // only collect once for these meta datas
    if (!info->metaComment && kwai_qos->media_metadata.comment) {
        info->metaComment = av_strdup(kwai_qos->media_metadata.comment);
    }

    snprintf(info->cpuInfo, CPU_INFO_LEN,
             "手机CPU核数:%d\n"
             "手机总CPU:%d%%\n"
             "本进程CPU:%d%%",
             get_process_cpu_num(), get_system_cpu_usage(), get_process_cpu_usage());

    snprintf(info->memoryInfo, MEMORY_INFO_LEN,
             "内存占用:%d", get_process_memory_size_kb());

    av_freep(&info->playUrl);
    info->playUrl = av_strdup(ffp_get_playing_url(ffp));

    av_freep(&info->host);
    info->host = av_strdup(kwai_qos->player_config.host);

    if (!info->metaAudioDecoderInfo && kwai_qos->media_metadata.audio_codec_info) {
        info->metaAudioDecoderInfo = av_strdup(kwai_qos->media_metadata.audio_codec_info);
    }
    if (!info->metaVideoDecoderInfo && kwai_qos->media_metadata.video_codec_info) {
        info->metaVideoDecoderInfo = av_strdup(kwai_qos->media_metadata.video_codec_info);
    }
    av_freep(&info->metaVideoDecoderDynamicInfo);
    info->metaVideoDecoderDynamicInfo = av_strdup(ffp->qos_venc_dyn_param);

    getMetaInfo(kwai_qos, info);

    // 首屏完成后就不再重复获取
    if (info->totalCostFirstScreen <= 0) {
        info->totalCostFirstScreen = kwai_qos->runtime_cost.cost_first_screen;

        info->stepCostWaitForPlay = kwai_qos->runtime_cost.cost_wait_for_playing;
        info->stepCostOpenInput = kwai_qos->runtime_cost.step_av_input_open;
        info->stepCostFindStreamInfo = kwai_qos->runtime_cost.step_av_find_stream_info;
        info->stepCostOpenDecoder = kwai_qos->runtime_cost.step_open_decoder;
        info->stepCostFirstFrameReceived = kwai_qos->runtime_cost.step_first_video_pkt_received;

        info->stepCostPreFirstFrameDecode = kwai_qos->runtime_cost.step_pre_decode_first_video_pkt;
        info->stepCostAfterFirstFrameDecode = kwai_qos->runtime_cost.step_decode_first_frame;
        info->stepCostFirstFrameRender = kwai_qos->runtime_cost.step_first_framed_rendered;

        info->costDnsAnalyze = kwai_qos->runtime_cost.cost_dns_analyze;
        info->costHttpConnect = kwai_qos->runtime_cost.cost_http_connect;
        info->costFirstHttpData = kwai_qos->runtime_cost.cost_http_first_data;

        snprintf(info->firstScreenStepCostInfo, STEP_COST_INFO_LEN,
                 "DNS解析:%lldms\n"
                 "HTTP建连:%lldms\n"
                 "首个数据包:%lldms\n"
                 "打开流媒体:%lldms\n"
                 "解析流信息:%lldms\n"
                 "PreDemux耗时:%lldms\n"
                 "打开解码器:%lldms\n"
                 "AllPrepared耗时:%lldms\n"
                 "WaitForPlay耗时:%lldms\n"
                 "收到首个packet:%lldms\n"
                 "解码器收到首帧:%lldms\n"
                 "首帧解码:%lldms\n"
                 "首帧渲染:%lldms\n",
                 kwai_qos->runtime_cost.cost_dns_analyze,
                 kwai_qos->runtime_cost.cost_http_connect,
                 kwai_qos->runtime_cost.cost_http_first_data,
                 kwai_qos->runtime_cost.step_av_input_open,
                 kwai_qos->runtime_cost.step_av_find_stream_info,
                 kwai_qos->runtime_cost.step_pre_demux_including_waiting,
                 kwai_qos->runtime_cost.step_open_decoder,
                 kwai_qos->runtime_cost.step_all_prepared,
                 kwai_qos->runtime_cost.cost_wait_for_playing,
                 kwai_qos->runtime_cost.step_first_video_pkt_received,
                 kwai_qos->runtime_cost.step_pre_decode_first_video_pkt,
                 kwai_qos->runtime_cost.step_decode_first_frame,
                 kwai_qos->runtime_cost.step_first_framed_rendered
                );
    }

    if (ffp->cache_actually_used) {
        info->ac_type = ffp->kwai_qos.ac_cache.data_source_type;

        AwesomeCacheRuntimeInfo* ac_rt_info = &ffp->cache_stat.ac_runtime_info;

        info->p2sp_enabled = ac_rt_info->p2sp_task.enabled;
        info->p2sp_used_bytes = ac_rt_info->p2sp_task.p2sp_used_bytes;
        info->cdn_used_bytes = ac_rt_info->p2sp_task.cdn_used_bytes;
        info->p2sp_download_bytes = ac_rt_info->p2sp_task.p2sp_download_bytes;
        info->cdn_download_bytes = ac_rt_info->p2sp_task.cdn_download_bytes;
        info->p2sp_switch_attempts = ac_rt_info->p2sp_task.p2sp_switch_attempts;
        info->cdn_switch_attempts = ac_rt_info->p2sp_task.cdn_switch_attempts;
        info->p2sp_switch_success_attempts = ac_rt_info->p2sp_task.p2sp_switch_success_attempts;
        info->cdn_switch_success_attempts = ac_rt_info->p2sp_task.cdn_switch_success_attempts;
        info->p2sp_switch_duration_ms = ac_rt_info->p2sp_task.p2sp_switch_duration_ms;
        info->cdn_switch_duration_ms = ac_rt_info->p2sp_task.cdn_switch_duration_ms;
    }
}
