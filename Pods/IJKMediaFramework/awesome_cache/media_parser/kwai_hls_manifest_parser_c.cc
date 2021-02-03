//
//  kwai_hls_manifest_parser.c
//  IJKMediaFramework
//
//  Created by 李金海 on 2019/7/3.
//  Copyright © 2019 kuaishou. All rights reserved.
//

#include <string.h>
#include "media/kwai_hls_manifest_parser_c.h"
#include "ac_log.h"
#include <stdlib.h>
//#include <malloc.h>

/**
 * json定义： https://wiki.corp.kuaishou.com/pages/viewpage.action?pageId=137982547
 **/
#define GETJSON_STRING(JSON, KEY, DEST) \
    do { \
        cJSON* item = cJSON_GetObjectItem(JSON, KEY); \
        DEST = item ? strdup(item->valuestring) : NULL; \
    } while(0)

#define GETJSON_INT(JSON, KEY, DEST) \
    do { \
        cJSON* item = cJSON_GetObjectItem(JSON, KEY); \
        DEST = item ? item->valueint : 0; \
    } while(0)

#define GETJSON_DOUBLE(JSON, KEY, DEST) \
    do { \
        cJSON* item = cJSON_GetObjectItem(JSON, KEY); \
        DEST = item ? item->valuedouble : 0; \
    } while(0)

int AdaptationSet_parse_hls_manifest_json(const char* json, AdaptationSet* adaptation_set) {
    if (!adaptation_set) {
        LOG_ERROR("output adaptation_set is null!");
        return -1;
    }
    cJSON* root = cJSON_Parse(json);
    int ret = 0;
    if (!root) {
        LOG_ERROR("Parse manifest json fail! json:%p", json);
        ret = -1;
        goto RETURN;
    }
    if (root->type == cJSON_Object) {
        cJSON* json_type = cJSON_GetObjectItem(root, "type");
        if (json_type->type != cJSON_String || strcmp(json_type->valuestring, "hls")) {
            LOG_ERROR("manifest type is invalid!");
            ret = -2;
            cJSON_Delete(root);
        }
        cJSON* json_adaptation_set = cJSON_GetObjectItem(root, "adaptationSet");
        if (json_adaptation_set && json_adaptation_set->type == cJSON_Object) {
            cJSON* json_reps = cJSON_GetObjectItem(json_adaptation_set, "representation");
            if (json_reps && json_reps->type == cJSON_Array) {
                int size = cJSON_GetArraySize(json_reps);
                adaptation_set->representations = (HlsRepresentation**)malloc(sizeof(HlsRepresentation*)*size);
                adaptation_set->n_rep = size;
                for (int i = 0; i < size; i++) {
                    HlsRepresentation* rep = (HlsRepresentation*)malloc(sizeof(HlsRepresentation));
                    memset((void*)rep, 0, sizeof(HlsRepresentation));
                    adaptation_set->representations[i] = rep;
                    cJSON* json_rep = cJSON_GetArrayItem(json_reps, i);
                    cJSON* json_backup_urls = cJSON_GetObjectItem(json_rep, "backupUrl");
                    if (json_backup_urls) {
                        int len = cJSON_GetArraySize(json_backup_urls);
                        rep->n_back_urls = len;
                        rep->backup_urls = (char**)malloc(sizeof(char*) * len);
                        for (int j = 0; j < len; j++) {
                            cJSON* json_url = cJSON_GetArrayItem(json_backup_urls, j);
                            if (json_url && json_url->type == cJSON_String) {
                                rep->backup_urls[j] = strdup(json_url->valuestring);
                            }
                        }
                    }
                    GETJSON_STRING(json_rep, "url", rep->url);
                    GETJSON_STRING(json_rep, "baseUrl", rep->base_url);
                    GETJSON_STRING(json_rep, "m3u8", rep->manifest_content);
                    GETJSON_STRING(json_rep, "m3u8Slice", rep->manifest_slice);
                    GETJSON_STRING(json_rep, "codecs", rep->codecs);
                    GETJSON_STRING(json_rep, "cacheKey", rep->cache_key);
                    GETJSON_INT(json_rep, "bandwidth", rep->bandwidth);
                    GETJSON_INT(json_rep, "averageBandwidth", rep->avg_bandwidth);
                    GETJSON_INT(json_rep, "width", rep->width);
                    GETJSON_INT(json_rep, "height", rep->height);
                    GETJSON_INT(json_rep, "frameRate", rep->frame_rate);
                    GETJSON_DOUBLE(json_rep, "duration", rep->duration);

                }
                ret = size;
            }
        }
    }
RETURN:
    if (root) {
        cJSON_Delete(root);
    }
    return ret;
}


AdaptationSet* AdaptationSet_create() {
    AdaptationSet* ret = (AdaptationSet*)malloc(sizeof(AdaptationSet));
    memset(ret, 0, sizeof(AdaptationSet));
    return ret;
}

void AdaptationSet_release(AdaptationSet** adaptation_set) {
    AdaptationSet* set = *adaptation_set;
    for (int i = 0; i < set->n_rep; i++) {
        HlsRepresentation* rep = set->representations[i];
        if (rep) {
            free(rep->url);
            for (int j = 0; j < rep->n_back_urls; j++) {
                free(rep->backup_urls[j]);
            }
            free(rep->backup_urls);
            free(rep->base_url);
            free(rep->manifest_content);
            free(rep->codecs);
        }
    }
    free(*adaptation_set);
    *adaptation_set = NULL;
}

HlsRepresentation* select_prefer_rep(AdaptationSet* adaptation_set, int prefer_bandwidth) {
    if (!adaptation_set || prefer_bandwidth <= 0) {
        return NULL;
    }
    HlsRepresentation* ret = NULL;
    int diff = INT32_MAX;
    for (int i = 0; i < adaptation_set->n_rep; i++) {
        HlsRepresentation* rep = adaptation_set->representations[i];
        if (rep) {
            if (prefer_bandwidth >= rep->bandwidth &&
                diff > (prefer_bandwidth - rep->bandwidth)) {
                diff = prefer_bandwidth - rep->bandwidth;
                ret = rep;
            }
        }
    }
    return ret;
}
