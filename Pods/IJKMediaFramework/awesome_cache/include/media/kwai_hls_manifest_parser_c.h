//
//  kwai_hls_manifest_parser.h
//  IJKMediaFramework
//
//  Created by 李金海 on 2019/7/3.
//  Copyright © 2019 kuaishou. All rights reserved.
//

#pragma once

#include <stdio.h>
#include "libavkwai/cJSON.h"
#include "hodor_config.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct HlsRepresentation_ {
    char* url;
    char* base_url;
    char* manifest_content;
    char* manifest_slice;
    char* codecs;
    char* cache_key;
    char** backup_urls;
    int32_t n_back_urls;
    int32_t bandwidth;
    int32_t avg_bandwidth;
    int32_t width;
    int32_t height;
    int32_t frame_rate;
    double duration;
} HlsRepresentation;

typedef struct AdaptationSet_ {
    HlsRepresentation** representations;
    int32_t n_rep;
} AdaptationSet;

HODOR_EXPORT AdaptationSet* AdaptationSet_create();
HODOR_EXPORT void AdaptationSet_release(AdaptationSet** adaptation_set);
HODOR_EXPORT int  AdaptationSet_parse_hls_manifest_json(const char* json, AdaptationSet* adaptation_set);
HODOR_EXPORT HlsRepresentation* select_prefer_rep(AdaptationSet* adaptation_set, int prefer_bandwidth);

#ifdef __cplusplus
}
#endif
