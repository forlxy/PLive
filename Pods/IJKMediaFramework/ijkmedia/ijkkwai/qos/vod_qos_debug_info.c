//
//  qos_debug_info.c
//  IJKMediaFramework
//
//  Created by 帅龙成 on 02/04/2018.
//  Copyright © 2018 kuaishou. All rights reserved.
//

#include <string.h>
#include "ijksdl/ijksdl_aout.h"
#include "ijkkwai/kwai_qos.h"
#include "ijkkwai/kwaiplayer_lifecycle.h"
#include "ijkplayer_internal.h"
#include "vod_qos_debug_info.h"
#include "cache_statistic.h"
#include "cache_defs.h"
#include <ijkmedia/ijkkwai/kwai_error_code_manager.h>
#include <ijkmedia/ijkkwai/c_resource_monitor.h>
#include "ijkkwai/kwai_qos.h"

#include <awesome_cache/v2/cache/cache_def_v2.h>
#include <awesome_cache/include/cache_errors.h>
#include <awesome_cache/include/dcc_algorithm_c.h>
#include <awesome_cache/hodor_downloader/hodor_c.h>


void VodQosDebugInfo_init(VodQosDebugInfo* info) {
    assert(info);
    if (info) {
        memset(info, 0, sizeof(VodQosDebugInfo));
    }
}

void VodQosDebugInfo_release(VodQosDebugInfo* info) {
    av_freep(&info->metaComment);

    av_freep(&info->serverIp);
    av_freep(&info->host);
    av_freep(&info->domain);

    av_freep(&info->vodAdaptiveQosInfo.switchReason);
    av_freep(&info->vodAdaptiveQosInfo.bandwidthComputerProcess);
    av_freep(&info->vodAdaptiveQosInfo.representation_str);
    av_freep(&info->vodAdaptiveQosInfo.netType);
    av_freep(&info->vodAdaptiveQosInfo.url);
    av_freep(&info->vodAdaptiveQosInfo.host);
    av_freep(&info->vodAdaptiveQosInfo.key);
    av_freep(&info->vodAdaptiveQosInfo.reason);
}

const char* mp_state_to_str(int state) {
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
            return "UNKOWN";
    }
}

static void VodQosDebugInfo_collect_av_packet_buffer_info(VodQosDebugInfo* info, FFPlayer* ffp) {
    VideoState* is = ffp->is;
    if (!is) {
        return;
    }

    int audio_time_base_valid = 0;
    int video_time_base_valid = 0;

    if (is->audio_st)
        audio_time_base_valid = is->audio_st->time_base.den > 0 && is->audio_st->time_base.num > 0;
    if (is->video_st)
        video_time_base_valid = is->video_st->time_base.den > 0 && is->video_st->time_base.num > 0;

    bool hasAudioStream = is->audio_st && audio_time_base_valid;
    bool hasVideoStream = is->video_st && video_time_base_valid;

    snprintf(info->avQueueStatus, AV_QUEUE_STATUS_LEN,
             "a-packet: %3.1fs | %3.1fKB\n"
             "v-packet: %3.1fs | %3.1fKB\n"
             "a-frame : %d/%d | v-frame : %d/%d",
             hasAudioStream ? ffp->stat.audio_cache.duration / 1000 : -1.f,
             hasAudioStream ? ffp->stat.audio_cache.bytes / 1024 : -1.f,
             hasVideoStream ? ffp->stat.video_cache.duration / 1000 : -1.f,
             hasVideoStream ? ffp->stat.video_cache.bytes / 1024 : -1.f,
             ffp->is ? ffp->is->sampq.size : -1,
             ffp->is ? ffp->is->sampq.max_size : -1,
             ffp->is ? ffp->is->pictq.size : -1,
             ffp->is ? ffp->is->pictq.max_size : -1);

    info->audioPacketBufferMs = (int)(hasAudioStream ? ffp->stat.audio_cache.duration : 0);
    info->videoPacketBufferMs = (int)(hasVideoStream ? ffp->stat.video_cache.duration : 0);
}

/**
 * 这个函数调用的时候，mp/ffp都是确保 non-null的，但是对is的访问一定要注意判空，因为这个时候可能还没stream_open
 */
static void VodQosDebugInfo_collect_player_config(VodQosDebugInfo* info, FFplayer* ffp) {
    info->configMaxBufDurMs = ffp->dcc.max_buffer_dur_ms;
}

static void VodAdaptiveQosInfo_collect(VodQosDebugInfo* info, KwaiQos* kwai_qos,
                                       AwesomeCacheRuntimeInfo* ac_rt_info) {
    assert(info);
    assert(kwai_qos);
    assert(ac_rt_info);

    info->vodAdaptiveQosInfo.isVodAdaptive = kwai_qos->vod_adaptive.is_vod_adaptive;
    if (!info->vodAdaptiveQosInfo.isVodAdaptive) {
        snprintf(info->vodAdaptiveInfo, VOD_ADAPTIVE_INFO_LEN, "多码率源: No\n");
        return;
    }

    info->vodAdaptiveQosInfo.cached = (uint32_t) kwai_qos->vod_adaptive.cached;

    if (!info->vodAdaptiveQosInfo.switchReason) {
        info->vodAdaptiveQosInfo.switchReason = av_strdup(kwai_qos->vod_adaptive.switch_reason);
    }
    if (!info->vodAdaptiveQosInfo.bandwidthComputerProcess) {
        info->vodAdaptiveQosInfo.bandwidthComputerProcess = av_strdup(
                                                                kwai_qos->vod_adaptive.bandwidth_computer_process);
    }
    info->vodAdaptiveQosInfo.idleLastRequestMs = kwai_qos->vod_adaptive.idle_last_request_ms;
    info->vodAdaptiveQosInfo.maxBitrateKbps = kwai_qos->vod_adaptive.max_bitrate_kbps;
    info->vodAdaptiveQosInfo.avgBitrateKbps = kwai_qos->vod_adaptive.avg_bitrate_kbps;
    info->vodAdaptiveQosInfo.width = kwai_qos->vod_adaptive.width;
    info->vodAdaptiveQosInfo.height = kwai_qos->vod_adaptive.height;
    info->vodAdaptiveQosInfo.deviceWidth = kwai_qos->vod_adaptive.device_width;
    info->vodAdaptiveQosInfo.deviceHeight = kwai_qos->vod_adaptive.device_height;
    info->vodAdaptiveQosInfo.quality = kwai_qos->vod_adaptive.quality;
    info->vodAdaptiveQosInfo.lowDevice = kwai_qos->vod_adaptive.low_device;
    info->vodAdaptiveQosInfo.switchCode = kwai_qos->vod_adaptive.switch_code;
    info->vodAdaptiveQosInfo.algorithmMode = kwai_qos->vod_adaptive.algorithm_mode;

    info->vodAdaptiveQosInfo.consumedDownloadMs = ac_rt_info->vod_adaptive.consumed_download_time_ms;
    info->vodAdaptiveQosInfo.actualVideoSizeBytes = ac_rt_info->vod_adaptive.actual_video_size_byte;
    if (info->vodAdaptiveQosInfo.consumedDownloadMs != 0) {
        info->vodAdaptiveQosInfo.avgDownloadRateKbps = (uint32_t)(
                                                           (info->vodAdaptiveQosInfo.actualVideoSizeBytes * 8000) /
                                                           (info->vodAdaptiveQosInfo.consumedDownloadMs * 1024));
    }
    info->vodAdaptiveQosInfo.shortThroughputKbps = kwai_qos->vod_adaptive.short_throughput_kbps;
    info->vodAdaptiveQosInfo.longThroughputKbps = kwai_qos->vod_adaptive.long_throughput_kbps;
    info->vodAdaptiveQosInfo.realTimeThroughputKbps = ac_rt_info->vod_adaptive.real_time_throughput_kbps;


    if (!info->vodAdaptiveQosInfo.netType) {
        info->vodAdaptiveQosInfo.netType = av_strdup(kwai_qos->vod_adaptive.net_type);
    }

    if (!info->vodAdaptiveQosInfo.url) {
        info->vodAdaptiveQosInfo.url = av_strdup(kwai_qos->vod_adaptive.cur_url);
    }
    if (!info->vodAdaptiveQosInfo.host) {
        info->vodAdaptiveQosInfo.host = av_strdup(kwai_qos->vod_adaptive.cur_host);
    }
    if (!info->vodAdaptiveQosInfo.key) {
        info->vodAdaptiveQosInfo.key = av_strdup(kwai_qos->vod_adaptive.cur_key);
    }

    if (!info->vodAdaptiveQosInfo.representation_str) {
        info->vodAdaptiveQosInfo.representation_str = av_strdup(
                                                          kwai_qos->vod_adaptive.representations_str);
    }

    if (!info->vodAdaptiveQosInfo.reason) {
        info->vodAdaptiveQosInfo.reason = av_strdup(kwai_qos->vod_adaptive.detail_switch_reason);
    }

    // rate config
    info->vodAdaptiveQosInfo.wifi_amend = kwai_qos->vod_adaptive.wifi_amend;
    info->vodAdaptiveQosInfo.fourG_amend = kwai_qos->vod_adaptive.fourG_amend;
    info->vodAdaptiveQosInfo.resolution_amend = kwai_qos->vod_adaptive.resolution_amend;
    info->vodAdaptiveQosInfo.absolute_low_rate_4G = kwai_qos->vod_adaptive.absolute_low_rate_4G;
    info->vodAdaptiveQosInfo.absolute_low_rate_wifi = kwai_qos->vod_adaptive.absolute_low_rate_wifi;
    info->vodAdaptiveQosInfo.absolute_low_res_4G = kwai_qos->vod_adaptive.absolute_low_res_4G;
    info->vodAdaptiveQosInfo.absolute_low_res_wifi = kwai_qos->vod_adaptive.absolute_low_res_wifi;
    info->vodAdaptiveQosInfo.absolute_low_res_low_device = kwai_qos->vod_adaptive.absolute_low_res_low_device;
    info->vodAdaptiveQosInfo.adapt_under_wifi = kwai_qos->vod_adaptive.adapt_under_wifi;
    info->vodAdaptiveQosInfo.adapt_under_4G = kwai_qos->vod_adaptive.adapt_under_4G;
    info->vodAdaptiveQosInfo.adapt_under_other_net = kwai_qos->vod_adaptive.adapt_under_other_net;
    info->vodAdaptiveQosInfo.short_keep_interval = kwai_qos->vod_adaptive.short_keep_interval;
    info->vodAdaptiveQosInfo.long_keep_interval = kwai_qos->vod_adaptive.long_keep_interval;
    info->vodAdaptiveQosInfo.rate_addapt_type = kwai_qos->vod_adaptive.rate_addapt_type;
    info->vodAdaptiveQosInfo.bandwidth_estimation_type = kwai_qos->vod_adaptive.bandwidth_estimation_type;
    info->vodAdaptiveQosInfo.bitrate_init_level = kwai_qos->vod_adaptive.bitrate_init_level;
    info->vodAdaptiveQosInfo.default_weight = kwai_qos->vod_adaptive.default_weight;
    info->vodAdaptiveQosInfo.block_affected_interval = kwai_qos->vod_adaptive.block_affected_interval;
    info->vodAdaptiveQosInfo.priority_policy = kwai_qos->vod_adaptive.priority_policy;
    info->vodAdaptiveQosInfo.device_width_threshold = kwai_qos->vod_adaptive.device_width_threshold;
    info->vodAdaptiveQosInfo.device_hight_threshold = kwai_qos->vod_adaptive.device_hight_threshold;
    info->vodAdaptiveQosInfo.max_resolution = kwai_qos->vod_adaptive.max_resolution;

    int audioRate;
    if (info->vodAdaptiveQosInfo.maxBitrateKbps >= VOD_ADAPTIVE_AUDIO_RATE_THRESHOLD) {
        audioRate = VOD_ADAPTIVE_HIGH_AUDIO_RATE;
    } else {
        audioRate = VOD_ADAPTIVE_LOW_AUDIO_RATE;
    }

    snprintf(info->vodAdaptiveInfo, VOD_ADAPTIVE_INFO_LEN,
             "已经缓存: %s\n"
             "视频分辨率:w:%d, h:%d, quality:%.2f\n"
             "视频码率: avg: %d+%d=%d, max: %d+%d=%d\n"
             "原因：%s\n"
             "\n===App下发参数===\n"
             "设备分辨率：w:%d, h:%d\n"
             "网络类型：%s\n"
             "低端手机：%s\n"
             "手动切换: %d\n"
             "模式: %d\n"
             "manifest(不包含选中源):%s\n"
             "\n===算法模块===\n"
             "带宽计算:%s\n"
             "下载速度:%d\n"
             "带宽:short:%d kbps, long:%d kbps real:%d kbps\n"
             "距上次观看间隔:%lld ms\n"
             "参考信息:%s\n"
             "\n===初始化参数===\n"
             "amend: wifi=%.2f, 4G=%.2f, res=%.2f\n"
             "absolute low: rate4G=%d, rateWifi=%d, res4G=%d, resWifi=%d, reslowdev=%d\n"
             "adapt: wifi=%d, 4G=%d, others=%d\n"
             "keepInterval: short=%d, long=%d\n"
             "rateType:%d, estimateType:%d, bitrateinitLevel:%d\n"
             "weight:%.2f, blockInterval:%d, priority:%d\n"
             "devWidthTHR:%d, devHeightTHR:%d, maxRes:%d\n"
             "\n===视频地址信息===\n"
             "url:%s\n"
             "host:%s\n"
             "key:%s\n",
             info->vodAdaptiveQosInfo.cached != 0 ? "Yes" : "No",
             (int)info->vodAdaptiveQosInfo.width, (int)info->vodAdaptiveQosInfo.height,
             info->vodAdaptiveQosInfo.quality,
             (int)info->vodAdaptiveQosInfo.avgBitrateKbps, audioRate,
             (int)info->vodAdaptiveQosInfo.avgBitrateKbps + audioRate,
             (int)info->vodAdaptiveQosInfo.maxBitrateKbps, audioRate,
             (int)info->vodAdaptiveQosInfo.maxBitrateKbps + audioRate,
             info->vodAdaptiveQosInfo.reason != NULL ? info->vodAdaptiveQosInfo.reason : "N/A",
             // app input
             (int)info->vodAdaptiveQosInfo.deviceWidth, (int)info->vodAdaptiveQosInfo.deviceHeight,
             info->vodAdaptiveQosInfo.netType != NULL ? info->vodAdaptiveQosInfo.netType : "N/A",
             info->vodAdaptiveQosInfo.lowDevice != 0 ? "Yes" : "No",
             info->vodAdaptiveQosInfo.switchCode,
             info->vodAdaptiveQosInfo.algorithmMode,
             info->vodAdaptiveQosInfo.representation_str != NULL
             ? info->vodAdaptiveQosInfo.representation_str : "N/A",
             // algorithm info
             info->vodAdaptiveQosInfo.bandwidthComputerProcess != NULL
             ? info->vodAdaptiveQosInfo.bandwidthComputerProcess : "N/A",
             (int)info->vodAdaptiveQosInfo.avgDownloadRateKbps,
             (int)info->vodAdaptiveQosInfo.shortThroughputKbps,
             (int)info->vodAdaptiveQosInfo.longThroughputKbps,
             (int)info->vodAdaptiveQosInfo.realTimeThroughputKbps,
             (long long int)info->vodAdaptiveQosInfo.idleLastRequestMs,
             info->vodAdaptiveQosInfo.switchReason != NULL ? info->vodAdaptiveQosInfo.switchReason
             : "N/A",
             // rate config info
             info->vodAdaptiveQosInfo.wifi_amend, info->vodAdaptiveQosInfo.fourG_amend,
             info->vodAdaptiveQosInfo.resolution_amend,
             (int)info->vodAdaptiveQosInfo.absolute_low_rate_4G,
             (int)info->vodAdaptiveQosInfo.absolute_low_rate_wifi,
             (int)info->vodAdaptiveQosInfo.absolute_low_res_4G,
             (int)info->vodAdaptiveQosInfo.absolute_low_res_wifi,
             (int)info->vodAdaptiveQosInfo.absolute_low_res_low_device,
             (int)info->vodAdaptiveQosInfo.adapt_under_wifi, (int)info->vodAdaptiveQosInfo.adapt_under_4G,
             (int)info->vodAdaptiveQosInfo.adapt_under_other_net,
             (int)info->vodAdaptiveQosInfo.short_keep_interval,
             (int)info->vodAdaptiveQosInfo.long_keep_interval,
             (int)info->vodAdaptiveQosInfo.rate_addapt_type,
             (int)info->vodAdaptiveQosInfo.bandwidth_estimation_type,
             (int)info->vodAdaptiveQosInfo.bitrate_init_level,
             info->vodAdaptiveQosInfo.default_weight,
             (int)info->vodAdaptiveQosInfo.block_affected_interval,
             (int)info->vodAdaptiveQosInfo.priority_policy,
             (int)info->vodAdaptiveQosInfo.device_width_threshold,
             (int)info->vodAdaptiveQosInfo.device_hight_threshold,
             (int)info->vodAdaptiveQosInfo.max_resolution,
             // video address info
             info->vodAdaptiveQosInfo.url != NULL ? info->vodAdaptiveQosInfo.url : "N/A",
             info->vodAdaptiveQosInfo.host != NULL ? info->vodAdaptiveQosInfo.host : "N/A",
             info->vodAdaptiveQosInfo.key != NULL ? info->vodAdaptiveQosInfo.key : "N/A");
}

static const char* STOP_REASON_STRING[] = {
    "STOP_REASON_UNKNOWN",
    "STOP_REASON_FINISHED",
    "STOP_REASON_CANCELLED",
    "STOP_REASON_FAILED",
    "STOP_REASON_TIMEOUT",
    "STOP_REASON_NO_CONTENT_LENGTH",
    "STOP_REASON_CONTENT_LENGTH_INVALID",
    "STOP_REASON_END",
};
static inline void VodQosDebugInfo_update_download_speed_info(VodQosDebugInfo* info,
                                                              AwesomeCacheRuntimeInfo* ac_rt_info) {
    info->downloadCurrentSpeedKbps = ac_rt_info->download_task.speed_cal_current_speed_kbps;
    if (ac_rt_info->cache_applied_config.data_source_type != kDataSourceTypeAsyncV2) {
        if (info->cacheErrorCode != 0) {
            snprintf(info->downloadSpeedInfo, DOWNLOAD_SPEED_INFO_MAX_LEN, "错误码:%d,错误原因:%s",
                     info->cacheErrorCode, STOP_REASON_STRING[info->cacheStopReason]);
        } else if (info->cacheIsReadingCachedFile) {
            snprintf(info->downloadSpeedInfo, DOWNLOAD_SPEED_INFO_MAX_LEN, "正在消费缓存文件...");
        } else {
            snprintf(info->downloadSpeedInfo, DOWNLOAD_SPEED_INFO_MAX_LEN, "%s : %d kbps\n"
                     "%s : %d kbps\n"
                     "%s : %d kbps\n"
                     "%s : %d kbps\n",
                     "算法参考网速", DccAlgorithm_get_current_speed_mark(),
                     "本次瞬时速度", ac_rt_info->download_task.speed_cal_current_speed_kbps,
                     "本次平均速度", ac_rt_info->download_task.speed_cal_avg_speed_kbps,
                     "本次测速结果", ac_rt_info->download_task.speed_cal_mark_speed_kbps);
        }
    } else {
        if (info->cacheErrorCode != 0) {
            snprintf(info->downloadSpeedInfo, DOWNLOAD_SPEED_INFO_MAX_LEN, "%s : %d kbps\n"
                     "%s : %d kbps\n"
                     "%s : %d kbps\n"
                     "错误码:%d,错误原因:%s",
                     "算法参考网速", DccAlgorithm_get_current_speed_mark(),
                     "本次瞬时速度", ac_rt_info->download_task.speed_cal_current_speed_kbps,
                     "上次平均速度", ac_rt_info->download_task.speed_cal_avg_speed_kbps,
                     info->cacheErrorCode, STOP_REASON_STRING[info->cacheStopReason]);
        } else {
            snprintf(info->downloadSpeedInfo, DOWNLOAD_SPEED_INFO_MAX_LEN, "%s : %d kbps\n"
                     "%s : %d kbps\n",
                     "本 次 瞬 时 速 度", ac_rt_info->download_task.speed_cal_current_speed_kbps,
                     "上次分片的平均速度", DccAlgorithm_get_current_speed_mark());
        }
    }
}

void VodQosDebugInfo_collect(VodQosDebugInfo* info, struct IjkMediaPlayer* mp) {
    assert(info);
    assert(mp);
    assert(mp->ffplayer);

    info->alivePlayerCnt = KwaiPlayerLifeCycle_get_current_alive_cnt_unsafe();
    FFPlayer* ffp = mp->ffplayer;

    KwaiQos* kwai_qos = &ffp->kwai_qos;

    if (ffp->is_live_manifest) {
        info->mediaType = VodQosDebugInfoMediaType_KFLV;
    } else if (ffp->islive) {
        info->mediaType = VodQosDebugInfoMediaType_LIVE;
    } else {
        info->mediaType = VodQosDebugInfoMediaType_VOD;
    }

    info->lastError = ffp->kwai_error_code;
    info->currentState = mp_state_to_str(mp->mp_state);

    // configs
    VodQosDebugInfo_collect_player_config(info, mp->ffplayer);

    info->metaWidth = ffp->kwai_qos.media_metadata.width;
    info->metaHeight = ffp->kwai_qos.media_metadata.height;
    info->metaFps = ffp->kwai_qos.media_metadata.fps;
    info->metaDurationMs = (int) ffp->kwai_qos.media_metadata.duration;
    info->bitrate = ffp->kwai_qos.media_metadata.bitrate;

    if (!info->fileName && kwai_qos->player_config.filename) {
        info->fileName = av_strdup(kwai_qos->player_config.filename);
    }

    // only collect once for these meta datas
    if (!info->metaComment && kwai_qos->media_metadata.comment) {
        info->metaComment = av_strdup(kwai_qos->media_metadata.comment);
    }

    if (kwai_qos->media_metadata.video_codec_info) {
        snprintf(info->metaVideoDecoderInfo, AV_DECODER_INFO_LEN, "%s, %lld kbps",
                 kwai_qos->media_metadata.video_codec_info != NULL
                 ? kwai_qos->media_metadata.video_codec_info : "NA",
                 kwai_qos->media_metadata.video_bit_rate / 1000);
    }

    if (kwai_qos->media_metadata.audio_codec_info) {
        snprintf(info->metaAudioDecoderInfo, AV_DECODER_INFO_LEN, "%s, %s, %lld kbps, %dHz, %d",
                 kwai_qos->media_metadata.audio_codec_info != NULL
                 ? kwai_qos->media_metadata.audio_codec_info : "NA",
                 kwai_qos->media_metadata.audio_profile,
                 kwai_qos->media_metadata.audio_bit_rate / 1000,
                 kwai_qos->media_metadata.sample_rate,
                 kwai_qos->media_metadata.channels);
    }

    if (!info->serverIp) {
        info->serverIp = av_strdup(kwai_qos->player_config.server_ip);
    }
    if (!info->host) {
        info->host = av_strdup(kwai_qos->player_config.host);
    }
    if (!info->domain) {
        info->domain = av_strdup(kwai_qos->player_config.domain);
    }
    snprintf(info->transcodeType, TRANSCODE_TYPE_MAX_LEN, "%s",
             kwai_qos->player_config.trancode_type);

    // 首屏完成后就不再重复获取
    if (info->totalCostFirstScreen <= 0) {
        info->firstScreenWithoutAppCost = kwai_qos->runtime_cost.cost_first_screen;
        info->totalCostFirstScreen = kwai_qos->runtime_cost.cost_total_first_screen;

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
                 "首帧渲染:%lldms\n"
                 "--------------\n"
                 "purePreDemux/WaitForPlay/开播前暂停:%lld/%lld/%lld\n"
                 "total_first_screen:%lld|first_screen:%lld|all_prepared:%lld\n"
                 "render_ready:%lld\n",
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
                 kwai_qos->runtime_cost.step_first_framed_rendered,
                 // ------------
                 // 这一行为了对齐上面的3个%lld，不要拆成多行
                 kwai_qos->runtime_cost.cost_pure_pre_demux, kwai_qos->runtime_cost.cost_wait_for_playing, kwai_qos->runtime_cost.cost_pause_at_first_screen,
                 kwai_qos->runtime_cost.cost_total_first_screen, kwai_qos->runtime_cost.cost_first_screen, kwai_qos->runtime_cost.cost_prepare_ms,
                 kwai_qos->runtime_cost.cost_first_render_ready
                );
    }

    snprintf(info->blockStatus, BLOCK_STATUS_LEN, "%d次 | %lldms",
             kwai_qos->runtime_stat.block_cnt,
             KwaiQos_getBufferTotalDurationMs(kwai_qos));

    snprintf(info->dropFrame, DROP_FRAME_LEN, "decode: %d | render: %d",
             kwai_qos->runtime_stat.v_decoded_dropped_frame, kwai_qos->runtime_stat.v_render_dropped_frame);

    VodQosDebugInfo_collect_av_packet_buffer_info(info, ffp);

    // dcc status
    snprintf(info->dccStatus, DCC_STATUS_LEN,
             "strategy:%d, finished:%d\n"
             "max_buf_bsp:%d秒, max_buf:%d秒",
             ffp->dcc.max_buf_dur_strategy,
             ffp->dcc.max_buf_strategy_finished,
             ffp->dcc.max_buffer_dur_bsp_ms / 1000,
             ffp->dcc.max_buffer_dur_ms / 1000);

    snprintf(info->cpuInfo, CPU_INFO_LEN,
             "手机CPU核数:%d\n"
             "手机总CPU:%d%%\n"
             "本进程CPU:%d%%",
             get_process_cpu_num(), get_system_cpu_usage(), get_process_cpu_usage());

    snprintf(info->memoryInfo, MEMORY_INFO_LEN,
             "内存占用:%d", get_process_memory_size_kb());

    info->usePreLoad = (bool) kwai_qos->player_config.use_pre_load;
    info->preLoadFinish = (bool) kwai_qos->player_config.pre_load_finish;
    info->preLoadedMsWhenAbort = ffp->pre_demux ? ffp->pre_demux->pre_loaded_ms_when_abort : 0;
    info->preLoadMs = kwai_qos->player_config.pre_load_duraion_ms;

    info->currentPositionMs = (int)ijkmp_get_current_position(mp);
    info->playableDurationMs =
        info->currentPositionMs + FFMIN(info->audioPacketBufferMs, info->videoPacketBufferMs);
    info->ffpLoopCnt = ffp->loop;

    memset(info->playerConfigInfo, 0, PLAYER_CONFIG_INFO_MAX_LEN);
    snprintf(info->playerConfigInfo, PLAYER_CONFIG_INFO_MAX_LEN,
             "[loop:%d][expect_use_cache:%d]",
             ffp->loop, kwai_qos->player_config.use_awesome_cache);

    SDL_Aout* aout = ffp->aout;
    if (aout) {
        memset(info->aoutInfoString, 0, AOUT_INFO_MAX_LEN);
        snprintf(info->aoutInfoString, AOUT_INFO_MAX_LEN,
                 "play_cnt[2]:(%d/%d) \npause_cnt:%d \nflush_cnt[4]:(%d/%d/%d/%d) \nsilent cnt/bytes:(%d/%d) \nset_speed_cnt:%d",
                 aout->qos.play_cnt_1, aout->qos.play_cnt_2,
                 aout->qos.pause_cnt,
                 aout->qos.flush_cnt_1, aout->qos.flush_cnt_2, aout->qos.flush_cnt_3,
                 aout->qos.flush_cnt_4,
                 aout->qos.silent_buf_cnt, aout->qos.silent_buf_total_bytes,
                 aout->qos.set_speed_cnt);
    }

    // 启播buffer状态
    info->startPlayBlockUsed = ffp->kwai_qos.runtime_stat.start_play_block_used;
    if (!ffp->kwai_qos.runtime_stat.start_play_block_used) {
        snprintf(info->startPlayBlockStatus, START_PLAY_BLOCK_STATUS_MAX_LEN,
                 "未开启");
    } else {
        snprintf(info->startPlayBlockStatus, START_PLAY_BLOCK_STATUS_MAX_LEN,
                 "已开启 | 开播阈值:%dms,实际缓冲:%dms | 耗时上限:%lldms,实际耗时:%lldms",
                 ffp->kwai_qos.runtime_stat.start_play_block_th,
                 ffp->kwai_packet_buffer_checker.current_buffer_ms,
                 ffp->kwai_packet_buffer_checker.self_max_life_cycle_ms,
                 ffp->kwai_packet_buffer_checker.self_life_cycle_cost_ms
                );
    }

    info->cacheEnabled = ffp->cache_actually_used;
    if (ffp->cache_actually_used) {

        info->cacheErrorCode = ffp->cache_stat.ffmpeg_adapter_qos.adapter_error;
//        info->cacheErrorMsg = cache_error_msg(info->cacheErrorCode); // todo 加上jni回传

        AwesomeCacheRuntimeInfo* ac_rt_info = &ffp->cache_stat.ac_runtime_info;

        info->cacheDataSourceType = AwesomeCacheRuntimeInfo_config_get_datas_source_type_str(
                                        ac_rt_info);
        info->cacheReopenCntBySeek = ac_rt_info->buffer_ds.reopen_cnt_by_seek;

        strncpy(info->cacheCurrentReadingUri, ac_rt_info->cache_ds.current_read_uri,
                DATA_SOURCE_URI_MAX_LEN);
        info->cacheTotalBytes = ac_rt_info->cache_ds.total_bytes;
        if (ac_rt_info->cache_applied_config.data_source_type == kDataSourceTypeAsyncV2) {
            info->cacheDownloadedBytes = ac_rt_info->cache_ds.async_v2_cached_bytes;
        } else {
            info->cacheDownloadedBytes = (ac_rt_info->cache_ds.cached_bytes +
                                          ac_rt_info->sink.bytes_not_commited);
        }
        info->cacheIsReadingCachedFile = ac_rt_info->cache_ds.is_reading_file_data_source;

        info->cacheStopReason = ac_rt_info->download_task.stop_reason;

        // speed related
        VodQosDebugInfo_update_download_speed_info(info, ac_rt_info);

        // dccAlg
        info->dccAlgConfigEnabled = ffp->dcc_algorithm.config_enabled == 1;
        info->dccAlgUsed = ffp->dcc_algorithm.qos_used;
        strncpy(info->dccAlgStatus, ffp->dcc_algorithm.status, DCC_ALG_STATUS_MAX_LEN);

        if (ac_rt_info->download_task.http_version[0]) {
            strncpy(info->httpVersion, ac_rt_info->download_task.http_version, HTTP_VERSION_MAX_LEN);
        }

        // for vod adaptive
        VodAdaptiveQosInfo_collect(info, kwai_qos, ac_rt_info);

        info->vodP2spEnabled = ac_rt_info->vod_p2sp.enabled;
        if (info->vodP2spEnabled) {
            snprintf(info->vodP2spStatus, VOD_P2SP_STATUS_INFO_LEN,
                     "SdkVer:%s\n" \
                     "CDN: %lld KB, cnt %d\n" \
                     "P2SP: %s, bt %lld(ms), fz %lld, st %lld, req %lld, use %lld, rcv %lld, rep %lld KB, cnt %d, err %d",
                     ac_rt_info->vod_p2sp.p2sp_sdk_version,
                     ac_rt_info->vod_p2sp.cdn_bytes / 1024,
                     ac_rt_info->vod_p2sp.cdn_open_count,
                     ac_rt_info->vod_p2sp.on ? "on" : "off",
                     ac_rt_info->vod_p2sp.player_buffer_ms,
                     ac_rt_info->vod_p2sp.file_length / 1024,
                     ac_rt_info->vod_p2sp.p2sp_start / 1024,
                     ac_rt_info->vod_p2sp.p2sp_bytes_requested / 1024,
                     ac_rt_info->vod_p2sp.p2sp_bytes_used / 1024,
                     ac_rt_info->vod_p2sp.p2sp_bytes_received / 1024,
                     ac_rt_info->vod_p2sp.p2sp_bytes_repeated / 1024,
                     ac_rt_info->vod_p2sp.p2sp_open_count,
                     ac_rt_info->vod_p2sp.p2sp_error_code);
        }

        // cacheV2 status
        snprintf(info->cacheV2Info, CACHE_V2_INFO_MAX_LEN,
                 "-------------------------------------- \n"
                 "scope_kb: set/current : %d/%d\n"
                 "预加载字节数: %.1fkb\n"
                 "码率(单位byte): %lld\n"
                 "对应读取字节数: open_input/find_stream_info/first_audio/first_video: \n"
                 "              %lld/%lld/%lld/%lld \n"
                 "前几秒对应字节数: 1s/2s/3s : %lld/%lld/%lld",
                 ac_rt_info->cache_v2_info.scope_max_size_kb_of_settting,
                 ac_rt_info->cache_v2_info.scope_max_size_kb_of_cache_content,
                 ffp->cache_stat.ac_runtime_info.cache_v2_info.cached_bytes_on_play_start * 1.f / KB,
                 ffp->kwai_io_queue_observer.byterate / KB,
                 ffp->kwai_io_queue_observer.read_bytes_on_open_input / KB,
                 ffp->kwai_io_queue_observer.read_bytes_on_find_stream_info / KB,
                 ffp->kwai_io_queue_observer.read_bytes_on_fst_audio_pkt / KB,
                 ffp->kwai_io_queue_observer.read_bytes_on_fst_video_pkt / KB,
                 ffp->kwai_io_queue_observer.read_bytes_on_1s / KB,
                 ffp->kwai_io_queue_observer.read_bytes_on_2s / KB,
                 ffp->kwai_io_queue_observer.read_bytes_on_3s / KB);
    }

    if (info->cacheErrorCode == 0 && info->lastError == 0) {
        sprintf(info->fullErrorMsg, "");
    }

    // 自动化测试tag
    snprintf(info->autoTestTags, AUTO_TEST_TAGS_LEN, "[session_uuid:%s]", ffp->session_uuid);

    snprintf(info->customString, CUSTOM_STRING_LEN, "[playerId:%d][player_cnt:%d]", ffp->session_id, info->alivePlayerCnt);

    // check runtime data
}
