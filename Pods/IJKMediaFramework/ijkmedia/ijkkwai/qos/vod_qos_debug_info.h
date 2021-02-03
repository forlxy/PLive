//
//  vod_qos_debug_info.h
//  IJKMediaFramework
//
//  Created by 帅龙成 on 02/04/2018.
//  Copyright © 2018 kuaishou. All rights reserved.
//

#ifndef vod_qos_debug_info_h
#define vod_qos_debug_info_h

#include <stdint.h>
#include <stdbool.h>
#include <awesome_cache/include/awesome_cache_runtime_info_c.h>
#include <awesome_cache/include/dcc_algorithm_c.h>


#define VOD_ADAPTIVE_AUDIO_RATE_THRESHOLD   1900
#define VOD_ADAPTIVE_HIGH_AUDIO_RATE   96
#define VOD_ADAPTIVE_LOW_AUDIO_RATE    48
#define VOD_ADAPTIVE_VIDEO_ADDRESS_INFO_LEN    256
#define VOD_ADAPTIVE_DCC_ALGORITHM_INFO_LEN   256
#define VOD_ADAPTIVE_APP_INPUT_INFO_LEN       512
#define VOD_ADAPTIVE_RATE_CONFIG_INFO_LEN       512

typedef enum VodQosDebugInfoMediaType {
    VodQosDebugInfoMediaType_VOD = 0,
    VodQosDebugInfoMediaType_LIVE,
    VodQosDebugInfoMediaType_KFLV
} VodQosDebugInfoMediaType;

typedef struct VodAdaptiveQosInfo {
    uint32_t maxBitrateKbps;
    uint32_t avgBitrateKbps;
    uint32_t width;
    uint32_t height;
    uint32_t deviceWidth;
    uint32_t deviceHeight;
    float quality;
    int32_t lowDevice;
    int32_t switchCode;
    int32_t algorithmMode;
    uint64_t consumedDownloadMs;
    uint64_t actualVideoSizeBytes;
    uint32_t avgDownloadRateKbps;
    uint64_t idleLastRequestMs;
    uint32_t shortThroughputKbps;
    uint32_t longThroughputKbps;
    uint32_t realTimeThroughputKbps;
    uint32_t cached;
    char*    representation_str;
    char*    switchReason;
    char*    bandwidthComputerProcess;
    char*    netType;
    char*    url;
    char*    host;
    char*    key;
    char*    reason;
    uint32_t isVodAdaptive;

    // rate config
    uint32_t rate_addapt_type;
    uint32_t bandwidth_estimation_type;
    uint32_t absolute_low_res_low_device;
    uint32_t adapt_under_4G;
    uint32_t adapt_under_wifi;
    uint32_t adapt_under_other_net;
    uint32_t absolute_low_rate_4G;
    uint32_t absolute_low_rate_wifi;
    uint32_t absolute_low_res_4G;
    uint32_t absolute_low_res_wifi;
    uint32_t short_keep_interval;
    uint32_t long_keep_interval;
    uint32_t bitrate_init_level;
    float default_weight;
    uint32_t block_affected_interval;
    float wifi_amend;
    float fourG_amend;
    float resolution_amend;
    uint32_t device_width_threshold;
    uint32_t device_hight_threshold;
    uint32_t priority_policy;
    uint32_t max_resolution;
} VodAdaptiveQosInfo;

typedef struct VodQosDebugInfo {
    int alivePlayerCnt;

    // 视频meta
    float metaFps;
    int metaWidth;
    int metaHeight;
    int metaDurationMs;
    int64_t bitrate;
    char* fileName;
    char* metaComment;
#define AV_DECODER_INFO_LEN 128
    char metaVideoDecoderInfo[AV_DECODER_INFO_LEN + 1];
    // 因为metaAudioDecoderInfo包含多个字段的信息，一次可能获取不全，所以每次都要update,所以用栈变量
    char metaAudioDecoderInfo[AV_DECODER_INFO_LEN + 1];

    VodQosDebugInfoMediaType mediaType;

#define FULL_ERROR_MSG_MAX_LEN 256
    char fullErrorMsg[FULL_ERROR_MSG_MAX_LEN + 1];
#define TRANSCODE_TYPE_MAX_LEN 16
    char transcodeType[TRANSCODE_TYPE_MAX_LEN + 1];
    int lastError;
    const char* currentState;
    char* serverIp;
    char* host;
    char* domain;
#define STEP_COST_INFO_LEN 1024
    char firstScreenStepCostInfo[STEP_COST_INFO_LEN];

    int64_t firstScreenWithoutAppCost;
    int64_t totalCostFirstScreen;

#define BLOCK_STATUS_LEN 128
    // config
    int configMaxBufDurMs;
    char blockStatus[BLOCK_STATUS_LEN];

#define DROP_FRAME_LEN 128
    char dropFrame[DROP_FRAME_LEN];

    // inner
    int audioPacketBufferMs;
    int videoPacketBufferMs;
#define AV_QUEUE_STATUS_LEN 128
    char avQueueStatus[AV_QUEUE_STATUS_LEN];
#define DCC_STATUS_LEN 128
    char dccStatus[DCC_STATUS_LEN];

    int playableDurationMs;

    int currentPositionMs;
    int ffpLoopCnt;

#define AOUT_INFO_MAX_LEN 256
    char aoutInfoString[AOUT_INFO_MAX_LEN + 1];
#define PLAYER_CONFIG_INFO_MAX_LEN 128
    char playerConfigInfo[PLAYER_CONFIG_INFO_MAX_LEN + 1];
    bool usePreLoad;
    int preLoadedMsWhenAbort;
    bool preLoadFinish;
    int preLoadMs;

    // 启播buffer
    bool startPlayBlockUsed;
#define START_PLAY_BLOCK_STATUS_MAX_LEN 128
    char startPlayBlockStatus[START_PLAY_BLOCK_STATUS_MAX_LEN];

    // cache打开的时候才有
    bool cacheEnabled;
    const char* cacheDataSourceType;
    const char* cacheUpstreamType;
    char cacheCurrentReadingUri[DATA_SOURCE_URI_MAX_LEN + 1];
    int64_t cacheTotalBytes;
    int64_t cacheDownloadedBytes;
    int cacheReopenCntBySeek;
    int cacheStopReason;
    int cacheErrorCode;
    bool cacheIsReadingCachedFile;

#define DOWNLOAD_SPEED_INFO_MAX_LEN 256
    int downloadCurrentSpeedKbps;
    char downloadSpeedInfo[DOWNLOAD_SPEED_INFO_MAX_LEN];

    bool dccAlgConfigEnabled;
    bool dccAlgUsed;
    int dccAlgCurrentSpeedMarkKbps;
    char dccAlgStatus[DCC_ALG_STATUS_MAX_LEN + 1];

    // vod adaptive
    VodAdaptiveQosInfo vodAdaptiveQosInfo;

#define VOD_ADAPTIVE_INFO_LEN 2048
    char vodAdaptiveInfo[VOD_ADAPTIVE_INFO_LEN + 1];
    char httpVersion[HTTP_VERSION_MAX_LEN + 1];

#define VOD_P2SP_STATUS_INFO_LEN 128
    // vod p2sp
    bool vodP2spEnabled; // enabled
    char vodP2spStatus[VOD_P2SP_STATUS_INFO_LEN + 1];

#define CPU_INFO_LEN 64
    char cpuInfo[CPU_INFO_LEN];
#define MEMORY_INFO_LEN 64
    char memoryInfo[MEMORY_INFO_LEN];
#define AUTO_TEST_TAGS_LEN 512
    char autoTestTags[AUTO_TEST_TAGS_LEN];

#define CUSTOM_STRING_LEN 256
    char customString[CUSTOM_STRING_LEN];

#define CACHE_V2_INFO_MAX_LEN 512
    char cacheV2Info[CACHE_V2_INFO_MAX_LEN];
} VodQosDebugInfo;

void VodQosDebugInfo_init(VodQosDebugInfo* info);
void VodQosDebugInfo_release(VodQosDebugInfo* info);
struct IjkMediaPlayer;
void VodQosDebugInfo_collect(VodQosDebugInfo* info, struct IjkMediaPlayer* mp);

#endif /* vod_qos_debug_info_h */
