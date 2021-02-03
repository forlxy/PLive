//
//  live_qos_debug_info.h
//  IJKMediaFramework
//
//  Created by wangtao03 on 2018/4/27.
//  Copyright © 2018年 kuaishou. All rights reserved.
//

#ifndef live_qos_debug_info_h
#define live_qos_debug_info_h

#include <stdint.h>
#include <stdbool.h>
// for android bundle key
#define LiveQosDebugInfo_comment "comment"
#define LiveQosDebugInfo_vencInit "vencInit"
#define LiveQosDebugInfo_aencInit "aencInit"
#define LiveQosDebugInfo_vencDynamic "vencDynamic"
#define LiveQosDebugInfo_hostInfo "hostInfo"
#define LiveQosDebugInfo_host "host"
#define LiveQosDebugInfo_videoDecoder "videoDecoder"
#define LiveQosDebugInfo_audioDecoder "audioDecoder"
#define LiveQosDebugInfo_blockCnt     "blockCnt"
#define LiveQosDebugInfo_blockDuration     "blockDuration"
#define LiveQosDebugInfo_videoReadFramesPerSecond         "videoReadFramesPerSecond"
#define LiveQosDebugInfo_videoDecodeFramesPerSecond         "videoDecodeFramesPerSecond"
#define LiveQosDebugInfo_videoDisplayFramesPerSecond         "videoDisplayFramesPerSecond"
#define LiveQosDebugInfo_audioBufferTimeLength "audioBufferTimeLength"
#define LiveQosDebugInfo_videoBufferTimeLength "videoBufferTimeLength"
#define LiveQosDebugInfo_audioDelay "audioDelay"
#define LiveQosDebugInfo_videoDelayRecv "videoDelayRecv"
#define LiveQosDebugInfo_videoDelayBefDec "videoDelayBefDec"
#define LiveQosDebugInfo_videoDelayAftDec "videoDelayAftDec"
#define LiveQosDebugInfo_videoDelayRender "videoDelayRender"
#define LiveQosDebugInfo_droppedDurationBefFirstScreen "droppedDurationBefFirstScreen"
#define LiveQosDebugInfo_droppedDurationTotal "droppedDurationTotal"

#define LiveQosDebugInfo_audioBufferByteLength "audioBufferByteLength"
#define LiveQosDebugInfo_videoBufferByteLength "videoBufferByteLength"
#define LiveQosDebugInfo_audioTotalDataSize "audioTotalDataSize"
#define LiveQosDebugInfo_videoTotalDataSize "videoTotalDataSize"
#define LiveQosDebugInfo_totalDataSize "totalDataSize"

#define LiveQosDebugInfo_isLiveManifest "isLiveManifest"
#define LiveQosDebugInfo_kflvPlayingBitrate "kflvPlayingBitrate"
#define LiveQosDebugInfo_kflvBandwidthCurrent  "kflvBandwidthCurrent"
#define LiveQosDebugInfo_kflvBandwidthFragment "kflvBandwidthFragment"
#define LiveQosDebugInfo_kflvCurrentBufferMs "kflvCurrentBufferMs"
#define LiveQosDebugInfo_kflvEstimateBufferMs "kflvEstimateBufferMs"
#define LiveQosDebugInfo_kflvPredictedBufferMs "kflvPredictedBufferMs"
#define LiveQosDebugInfo_kflvSpeedupThresholdMs "kflvSpeedupThresholdMs"

#define LiveQosDebugInfo_firstScreenTimeDnsAnalyze "firstScreenTimeDnsAnalyze"
#define LiveQosDebugInfo_firstScreenTimeHttpConnect "firstScreenTimeHttpConnect"
#define LiveQosDebugInfo_firstScreenTimePktReceive "firstScreenTimePktReceive"

#define LiveQosDebugInfo_firstScreenTimeTotal "firstScreenTimeTotal"
#define LiveQosDebugInfo_firstScreenTimeWaitForPlay "firstScreenTimeWaitForPlay"
#define LiveQosDebugInfo_firstScreenTimeInputOpen "firstScreenTimeInputOpen"
#define LiveQosDebugInfo_firstScreenTimeStreamFind "firstScreenTimeStreamFind"
#define LiveQosDebugInfo_firstScreenTimeCodecOpen "firstScreenTimeCodecOpen"
#define LiveQosDebugInfo_firstScreenTimePreDecode "firstScreenTimePreDecode"
#define LiveQosDebugInfo_firstScreenTimeDecode "firstScreenTimeDecode"
#define LiveQosDebugInfo_firstScreenTimeRender "firstScreenTimeRender"
#define LiveQosDebugInfo_firstFrameReceived    "firstFrameReceived"

#define LiveQosDebugInfo_sourceDeviceType    "sourceDeviceType"

#define LiveQosDebugInfo_acType "acType"
#define LiveQosDebugInfo_p2spEnabled "p2spEnabled"
#define LiveQosDebugInfo_p2spUsedBytes "p2spUsedBytes"
#define LiveQosDebugInfo_cdnUsedBytes "cdnUsedBytes"
#define LiveQosDebugInfo_p2spDownloadBytes "p2spDownloadBytes"
#define LiveQosDebugInfo_cdnDownloadBytes "cdnDownloadBytes"
#define LiveQosDebugInfo_p2spSwitchAttempts "p2spSwitchAttempts"
#define LiveQosDebugInfo_cdnSwitchAttempts "cdnSwitchAttempts"
#define LiveQosDebugInfo_p2spSwitchSuccessAttempts "p2spSwitchSuccessAttempts"
#define LiveQosDebugInfo_cdnSwitchSuccessAttempts "cdnSwitchSuccessAttempts"
#define LiveQosDebugInfo_p2spSwitchDurationMs "p2spSwitchDurationMs"
#define LiveQosDebugInfo_cdnSwitchDurationMs "cdnSwitchDurationMs"

typedef struct LiveQosDebugInfo {
    char* playUrl;
    char* metaComment;
    char* metaVideoDecoderInfo;
    char* metaAudioDecoderInfo;
    char* metaVideoDecoderDynamicInfo;
    char* host;
    char* comment;

    char* hostInfo;
    char* vencInit;
    char* aencInit;

    // time ms unit
    int audioBufferTimeLength;
    int videoBufferTimeLength;
    int audioDelay;
    int videoDelayRecv;
    int videoDelayBefDec;
    int videoDelayAftDec;
    int videoDelayRender;
    int droppedDurationBefFirstScreen;
    int droppedDurationTotal;
    int speedupThresholdMs;

    // byte unit
    int audioBufferByteLength;
    int videoBufferByteLength;
    int64_t audioTotalDataSize; // size of audio data since start playing
    int64_t videoTotalDataSize; // size of video data since start playing
    int64_t totalDataBytes; // total audio and video data size since start playing
    // Manifest
    bool isLiveManifest;
    int kflvPlayingBitrate;
    int kflvBandwidthCurrent;
    int kflvBandwidthFragment;
    int kflvCurrentBufferMs;
    int kflvEstimateBufferMs;
    int kflvPredictedBufferMs;
    int kflvSpeedupThresholdMs;

    int blockCnt;
    int64_t blockDuration;
    float videoReadFramesPerSecond;
    float videoDecodeFramesPerSecond;
    float videoDisplayFramesPerSecond;
    // 首屏相关，单位都是毫秒
    int64_t costDnsAnalyze;
    int64_t costHttpConnect;
    int64_t costFirstHttpData;
    int64_t decodedDataSize;
    int64_t totalCostFirstScreen;

#define STEP_COST_INFO_LEN 1024
    char firstScreenStepCostInfo[STEP_COST_INFO_LEN];

    int64_t stepCostWaitForPlay;
    int64_t stepCostOpenInput;
    int64_t stepCostFindStreamInfo;
    int64_t stepCostOpenDecoder;
    int64_t stepCostPreFirstFrameDecode;
    int64_t stepCostAfterFirstFrameDecode;
    int64_t stepCostFirstFrameRender;

    int64_t stepCostFirstFrameReceived;

    int sourceDeviceType;

    // awesome cache
    int ac_type;

    // p2sp related
    int p2sp_enabled;

    int64_t p2sp_used_bytes;
    int64_t cdn_used_bytes;

    int64_t p2sp_download_bytes;
    int64_t cdn_download_bytes;

    int p2sp_switch_attempts;
    int cdn_switch_attempts;

    int p2sp_switch_success_attempts;
    int cdn_switch_success_attempts;

    int p2sp_switch_duration_ms;
    int cdn_switch_duration_ms;

#define CPU_INFO_LEN 64
    char cpuInfo[CPU_INFO_LEN];
#define MEMORY_INFO_LEN 64
    char memoryInfo[MEMORY_INFO_LEN];

} LiveQosDebugInfo;

void LiveQosDebugInfo_init(LiveQosDebugInfo* info);
void LiveQosDebugInfo_release(LiveQosDebugInfo* info);
struct IjkMediaPlayer;
void LiveQosDebugInfo_collect(LiveQosDebugInfo* info, struct IjkMediaPlayer* mp);

#endif /* live_qos_debug_info_h */
