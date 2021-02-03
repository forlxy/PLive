//
// Created by MarshallShuai on 2018/12/5.
//

#pragma once

#include <stdbool.h>

/**
 * PlayerConfg单独
 */
typedef struct PlayerConfigDebugInfo {

    bool collectFinish;
#define MAX_INPUT_URL_LEN 512
    char inputUrl[MAX_INPUT_URL_LEN];

    int playerMaxBufDurMs;
    bool playerStartOnPrepared;

    int cacheBufferDataSourceSizeKb;
    int cacheSeekReopenTHKb;

    const char* cacheDataSourceType;
    int cacheFlags;

    int cacheProgressCbIntervalMs;

    const char* cacheHttpType;
    const char* cacheCurlType;
    int cacheHttpMaxRetryCnt;

    int cacheConnectTimeoutMs;
    int cacheReadTimeoutMs;
    int cacheSocketOrigKb;
    int cacheSocketCfgKb;
    int cacheSocketActKb;
    int cacheCurlBufferSizeKb;
    char* cacheHeader;
    char* cacheUserAgent;

#define MAX_MEDIA_CODEC_INFO_LEN 512
    char mediaCodecInfo[MAX_MEDIA_CODEC_INFO_LEN];
} PlayerConfigDebugInfo;

void PlayerConfigDebugInfo_init(PlayerConfigDebugInfo* info);
void PlayerConfigDebugInfo_release(PlayerConfigDebugInfo* info);
struct IjkMediaPlayer;
void PlayerConfigDebugInfo_collect(PlayerConfigDebugInfo* info, struct IjkMediaPlayer* mp);
