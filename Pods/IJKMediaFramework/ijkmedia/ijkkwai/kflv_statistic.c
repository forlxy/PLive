//
//  kflv_statistic.c
//  KSYPlayerCore
//
//  Created by 帅龙成 on 07/02/2018.
//  Copyright © 2018 kuaishou. All rights reserved.
//

#include "kflv_statistic.h"
#include <libavutil/time.h>
#include <libavkwai/cJSON.h>
#include "ijkkwai/kwai_error_code_manager_ff_convert.h"

void KFlvPlayerStatistic_collect_playing_info(KFlvPlayerStatistic* dst, AVFormatContext* s) {
    if (!dst || !s) {
        return;
    }
    AVDictionaryEntry* entry = av_dict_get(s->metadata, KFLV_STAT_DICT_KEY, NULL, 0);
    if (entry) {
        KFlvStatistic* kflv_stat = (KFlvStatistic*)(intptr_t)strtol(entry->value, NULL, 10);
        // av_log(NULL, AV_LOG_INFO, "[kflv_stat], kflv_stat ptr:%p \n", kflv_stat);
        dst->kflv_stat = *kflv_stat;
    }
}

static int get_initial_info_from_representation(KFlvPlayerStatistic* stat, cJSON* root) {
    int init_id = -1;
    int len = cJSON_GetArraySize(root);
    for (int i = 0; i < len; i++) {
        cJSON* child_json = cJSON_GetArrayItem(root, i);
        switch (child_json->type) {
            case cJSON_Number:
                if (!strcmp(child_json->string, "id")) {
                    init_id = (int)child_json->valuedouble;
                } else if (!strcmp(child_json->string, "bitrate")) {
                    stat->init_bitrate = (int)child_json->valuedouble;
                }
                break;
            case cJSON_String:
                if (!strcmp(child_json->string, "url")) {
                    memset(stat->init_url, 0, MAX_URL_SIZE);
                    strcpy(stat->init_url, child_json->valuestring);
                }
                break;
            default:
                break;
        }
    }

    if (init_id == stat->init_index) {
        return 0;
    }

    return -1;
}


static int get_initial_info_from_adaptation(KFlvPlayerStatistic* stat, cJSON* root) {
    int len = cJSON_GetArraySize(root);
    for (int i = 0; i < len; i++) {
        cJSON* child_json = cJSON_GetArrayItem(root, i);

        switch (child_json->type) {
            case cJSON_Array:
                if (child_json->string && !strcmp(child_json->string, "representation")) {
                    int len = cJSON_GetArraySize(child_json);
                    for (int i = 0; i < len; i++) {
                        cJSON* root_json = cJSON_GetArrayItem(child_json, i);
                        if (get_initial_info_from_representation(stat, root_json) == 0) {
                            return 0;
                        }
                    }
                }
                break;
            default:
                break;
        }
    }
    return -1;
}

void KFlvPlayerStatistic_collect_initial_info(KFlvPlayerStatistic* stat, char* filename) {
    cJSON* root = cJSON_Parse(filename);

    if (!root) {
        return;
    }

    if (cJSON_Object == root->type) {
        int len = cJSON_GetArraySize(root);
        for (int i = 0; i < len; i++) {
            cJSON* child_json = cJSON_GetArrayItem(root, i);

            switch (child_json->type) {
                case cJSON_Object:
                    if (child_json->string && !strcmp(child_json->string, "adaptationSet")) {
                        if (get_initial_info_from_adaptation(stat, child_json) == 0) {
                            goto SUCCESS;
                        }
                    }
                    break;
                default:
                    break;
            }
        }

    }

SUCCESS:
    cJSON_Delete(root);
}

uint32_t KFlvPlayerStatistic_get_playing_bitrate(KFlvPlayerStatistic* stat) {
    if (stat->kflv_stat.cur_decoding_flv_index >= 0 &&
        stat->kflv_stat.cur_decoding_flv_index < stat->kflv_stat.flv_nb) {
        return stat->kflv_stat.flvs[stat->kflv_stat.cur_decoding_flv_index].total_bandwidth_kbps;
    } else {
        return stat->init_bitrate;
    }
}

uint32_t KFlvPlayerStatistic_get_downloading_bitrate(KFlvPlayerStatistic* stat) {
    if (stat->kflv_stat.bitrate_downloading > 0) {
        return stat->kflv_stat.bitrate_downloading;
    } else {
        return stat->init_bitrate;
    }
}

char* KFlvPlayerStatistic_get_playing_url(KFlvPlayerStatistic* stat) {
    if (stat->kflv_stat.cur_decoding_flv_index >= 0 &&
        stat->kflv_stat.cur_decoding_flv_index < stat->kflv_stat.flv_nb) {
        return stat->kflv_stat.cur_playing_url;
    } else {
        return stat->init_url;
    }
}

int KFlvPlayerStatistic_get_http_reading_error(KFlvPlayerStatistic* stat) {
    if (stat->kflv_stat.cur_rep_http_reading_error != 0) {
        return convert_to_kwai_error_code(stat->kflv_stat.cur_rep_http_reading_error);
    } else {
        return 0;
    }
}
