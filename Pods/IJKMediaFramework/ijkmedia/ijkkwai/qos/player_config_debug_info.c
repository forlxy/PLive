//
// Created by MarshallShuai on 2018/12/5.
//

#include <assert.h>
#include "ijkplayer/ijkplayer_internal.h"
#include "ijkkwai/kwai_qos.h"
#include "ijkkwai/cache/cache_statistic.h"
#include <awesome_cache/include/awesome_cache_runtime_info_c.h>
#include "ff_ffplay_def.h"
#include "player_config_debug_info.h"


void PlayerConfigDebugInfo_init(PlayerConfigDebugInfo* info) {
    memset(info, 0, sizeof(PlayerConfigDebugInfo));
}

void PlayerConfigDebugInfo_release(PlayerConfigDebugInfo* info) {
    if (!info->cacheUserAgent) {
        free(info->cacheUserAgent);
    }
}

void PlayerConfigDebugInfo_collect(PlayerConfigDebugInfo* info, struct IjkMediaPlayer* mp) {
    assert(mp);

    FFplayer* ffp = mp->ffplayer;
    KwaiQos* qos = &ffp->kwai_qos;
    AwesomeCacheRuntimeInfo* ac_rt_info = &ffp->cache_stat.ac_runtime_info;

    if (!ffp->is_live_manifest && !ffp->enable_vod_manifest) {
        strncpy(info->inputUrl, qos->player_config.filename ? qos->player_config.filename : "N/A", MAX_INPUT_URL_LEN);
    } else {
        strncpy(info->inputUrl, "is manifest video", MAX_INPUT_URL_LEN);
    }

    info->playerMaxBufDurMs = ffp->kwai_qos.player_config.max_buffer_dur_ms;
    info->playerStartOnPrepared = (bool) ffp->kwai_qos.player_config.start_on_prepared;

    info->cacheBufferDataSourceSizeKb = ac_rt_info->buffer_ds.buffered_datasource_size_kb;
    info->cacheSeekReopenTHKb = ac_rt_info->buffer_ds.buffered_datasource_seek_threshold_kb;

    info->cacheDataSourceType = AwesomeCacheRuntimeInfo_config_get_datas_source_type_str(
                                    ac_rt_info);
    info->cacheFlags = ac_rt_info->cache_applied_config.cache_flags;

    info->cacheProgressCbIntervalMs = ac_rt_info->cache_ds.progress_cb_interval_ms;

    info->cacheHttpType = AwesomeCacheRuntimeInfo_config_get_upstream_type_to_str(ac_rt_info);
    info->cacheCurlType = AwesomeCacheRuntimeInfo_config_get_download_task_type_to_str(ac_rt_info);
    info->cacheHttpMaxRetryCnt = ac_rt_info->http_ds.http_max_retry_cnt;

    info->cacheConnectTimeoutMs = ac_rt_info->download_task.con_timeout_ms;
    info->cacheReadTimeoutMs = ac_rt_info->download_task.read_timeout_ms;
    info->cacheCurlBufferSizeKb = ac_rt_info->download_task.curl_buffer_size_kb;
    info->cacheSocketOrigKb = ac_rt_info->download_task.sock_orig_size_kb;
    info->cacheSocketCfgKb = ac_rt_info->download_task.sock_cfg_size_kb;
    info->cacheSocketActKb = ac_rt_info->download_task.sock_act_size_kb;

    if (!info->cacheUserAgent && ac_rt_info->download_task.config_user_agent) {
        info->cacheUserAgent = strdup(ac_rt_info->download_task.config_user_agent);
    }

    snprintf(info->mediaCodecInfo, MAX_MEDIA_CODEC_INFO_LEN,
             "mediaCodecCnt: %d\n"
             "h264: %s\n"
             "h265: %s",
             ffp->mediacodec_max_cnt,
             (ffp->mediacodec_avc == 1 || ffp->mediacodec_all_videos == 1) ? "true" : "false",
             (ffp->mediacodec_hevc == 1 || ffp->mediacodec_all_videos == 1) ? "true" : "false");
}
