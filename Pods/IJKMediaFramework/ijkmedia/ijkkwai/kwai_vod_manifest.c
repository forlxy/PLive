//
// Created by liuyuxin on 2018/4/2.
//

#include <libavkwai/cJSON.h>
#include <memory.h>
#include <libavformat/avformat.h>
#include "kwai_vod_manifest.h"
#include "../ijksdl/ijksdl_log.h"
#include "c_abr_engine.h"
#include "ff_ffplay_def.h"
#include "kwai_qos.h"

#define MAX_LEN_MAX_RESOLUTION  256
#define ENABLE_DUMP             1

static int kwai_vod_parser_representation(Representation* rp, cJSON* root) {
    if (!root) {
        return -1;
    }

    int len = cJSON_GetArraySize(root);
    for (int i = 0; i < len; i++) {
        cJSON* child_json = cJSON_GetArrayItem(root, i);

        switch (child_json->type) {
            case cJSON_Number:
                if (!strcmp(child_json->string, "maxBitrate")) {
                    rp->max_bitrate_kbps = (int)child_json->valuedouble;
                } else if (!strcmp(child_json->string, "avgBitrate")) {
                    rp->avg_bitrate_kbps = (int)child_json->valuedouble;
                } else if (!strcmp(child_json->string, "height")) {
                    rp->video_resolution.height = (float)child_json->valuedouble;
                } else if (!strcmp(child_json->string, "width")) {
                    rp->video_resolution.width = (float)child_json->valuedouble;
                } else if (!strcmp(child_json->string, "quality")) {
                    rp->quality = (float)child_json->valuedouble;
                } else if (!strcmp(child_json->string, "id")) {
                    rp->id = child_json->valueint;
                }
                break;
            case cJSON_String:
                if (!strcmp(child_json->string, "url")) {
                    strncpy(rp->url, child_json->valuestring, MAX_INFO_SIZE);
                } else if (!strcmp(child_json->string, "host")) {
                    strncpy(rp->host, child_json->valuestring, MAX_INFO_SIZE);
                } else if (!strcmp(child_json->string, "key")) {
                    strncpy(rp->key, child_json->valuestring, MAX_INFO_SIZE);
                } else if (!strcmp(child_json->string, "qualityShow")) {
                    strncpy(rp->quality_show, child_json->valuestring, MAX_INFO_SIZE);
                }
                break;
            case cJSON_NULL:
                ALOGE("jason error \n");
                break;
            case cJSON_False:
            case cJSON_True:
                if (!strcmp(child_json->string, "featureP2sp")) {
                    rp->feature_p2sp = (child_json->type == cJSON_True ? 1 : 0);
                }
                break;
            case cJSON_Object:
            case cJSON_Array:
                ALOGE("jason root should not include this \n");
                break;
        }
    }
    return 0;
}

static int kwai_vod_parser_representation_set(VodPlayList* playlist, cJSON* root) {
    if (!root) {
        return -1;
    }

    int len = cJSON_GetArraySize(root);
    playlist->rep_count = len;
    for (int i = 0; i < len; i++) {
        cJSON* child_json = cJSON_GetArrayItem(root, i);

        switch (child_json->type) {
            case cJSON_Object:
                kwai_vod_parser_representation(&playlist->rep[i], child_json);
                playlist->rep[i].representation_id = i;
                break;
            case cJSON_NULL:
                ALOGE("jason error \n");
                break;
            case cJSON_String:
            case cJSON_False:
            case cJSON_True:
            case cJSON_Number:
            case cJSON_Array:
                ALOGE("jason root should not include this \n");
                break;
        }
    }
    return 0;
}

static int kwai_vod_parser_adaptation(VodPlayList* playlist, cJSON* root) {
    if (!root) {
        return -1;
    }

    int len = cJSON_GetArraySize(root);
    for (int i = 0; i < len; i++) {
        cJSON* child_json = cJSON_GetArrayItem(root, i);

        switch (child_json->type) {
            case cJSON_Number:
                if (!strcmp(child_json->string, "adaptationId")) {
                    playlist->adaptation_id = (int)child_json->valuedouble;
                }
                break;
            case cJSON_Array:
                if (child_json->string && !strcmp(child_json->string, "representation")) {
                    kwai_vod_parser_representation_set(playlist, child_json);
                }
                break;
            case cJSON_String:
                if (!strcmp(child_json->string, "duration")) {
                    playlist->duration = atoi(child_json->valuestring);
                }
                break;
            case cJSON_NULL:
                ALOGE("jason error \n");
                break;
            case cJSON_False:
            case cJSON_True:
            case cJSON_Object:
                ALOGE("jason root should not include this \n");
                break;
        }
    }
    return 0;
}

static int kwai_vod_parser_adaptation_set(VodPlayList* playlist, cJSON* root) {
    if (!root) {
        return -1;
    }

    int len = cJSON_GetArraySize(root);
    for (int i = 0; i < len; i++) {
        cJSON* child_json = cJSON_GetArrayItem(root, i);

        switch (child_json->type) {
            case cJSON_Object:
                kwai_vod_parser_adaptation(playlist, child_json);
                break;
            case cJSON_NULL:
                ALOGE("jason error \n");
                break;
            case cJSON_Number:
            case cJSON_False:
            case cJSON_True:
            case cJSON_String:
            case cJSON_Array:
                ALOGE("jason root should not include this \n");
                break;
        }
    }
    return 0;
}

int kwai_vod_parse_manifest(VodPlayList* playlist, const char* file_name) {

    cJSON* root = cJSON_Parse(file_name);
    if (!root) {
        return -1;
    }

    if (cJSON_Object == root->type) {
        int len = cJSON_GetArraySize(root);
        for (int i = 0; i < len; i++) {
            cJSON* child_json = cJSON_GetArrayItem(root, i);

            switch (child_json->type) {
                case cJSON_Array:
                    if (child_json->string && !strcmp(child_json->string, "adaptationSet")) {
                        kwai_vod_parser_adaptation_set(playlist, child_json);
                    }
                    break;
                case cJSON_NULL:
                    ALOGE("jason error \n");
                    break;
                case cJSON_False:
                case cJSON_True:
                case cJSON_String:
                case cJSON_Number:
                case cJSON_Object:
                    ALOGE("jason root should not include this \n");
                    break;
            }
        }
    }

    cJSON_Delete(root);

    return 0;
}

int VodRateAdaptConfig_parse_config(VodRateAdaptConfig* rate_config, char* config_string) {
    if (!config_string) {
        return -1;
    }

    cJSON* root = cJSON_Parse(config_string);
    if (!root) {
        return -1;
    }

    if (cJSON_Object == root->type) {
        int len = cJSON_GetArraySize(root);
        for (int i = 0; i < len; i++) {
            cJSON* child_json = cJSON_GetArrayItem(root, i);

            switch (child_json->type) {
                case cJSON_Number:
                    if (!strcmp(child_json->string, "rate_adapt_type")) {
                        rate_config->rate_addapt_type = child_json->valueint;
                    } else if (!strcmp(child_json->string, "bandwidth_estimation_type")) {
                        rate_config->bandwidth_estimation_type = child_json->valueint;
                    } else if (!strcmp(child_json->string, "device_width_threshold")) {
                        rate_config->device_width_threshold = child_json->valueint;
                    } else if (!strcmp(child_json->string, "device_hight_threshold")) {
                        rate_config->device_hight_threshold = child_json->valueint;
                    } else if (!strcmp(child_json->string, "absolute_low_res_low_device")) {
                        rate_config->absolute_low_res_low_device = child_json->valueint;
                    } else if (!strcmp(child_json->string, "adapt_under_4G")) {
                        rate_config->adapt_under_4G = child_json->valueint;
                    } else if (!strcmp(child_json->string, "adapt_under_wifi")) {
                        rate_config->adapt_under_wifi = child_json->valueint;
                    } else if (!strcmp(child_json->string, "adapt_under_other_net")) {
                        rate_config->adapt_under_other_net = child_json->valueint;
                    } else if (!strcmp(child_json->string, "absolute_low_rate_4G")) {
                        rate_config->absolute_low_rate_4G = child_json->valueint;
                    } else if (!strcmp(child_json->string, "absolute_low_rate_wifi")) {
                        rate_config->absolute_low_rate_wifi = child_json->valueint;
                    } else if (!strcmp(child_json->string, "absolute_low_res_4G")) {
                        rate_config->absolute_low_res_4G = child_json->valueint;
                    } else if (!strcmp(child_json->string, "absolute_low_res_wifi")) {
                        rate_config->absolute_low_res_wifi = child_json->valueint;
                    } else if (!strcmp(child_json->string, "short_keep_interval")) {
                        rate_config->short_keep_interval = child_json->valueint;
                    } else if (!strcmp(child_json->string, "long_keep_interval")) {
                        rate_config->long_keep_interval = child_json->valueint;
                    } else if (!strcmp(child_json->string, "bitrate_init_level")) {
                        rate_config->bitrate_init_level = child_json->valueint;
                    } else if (!strcmp(child_json->string, "default_weight")) {
                        rate_config->default_weight = (float)child_json->valuedouble;
                    } else if (!strcmp(child_json->string, "block_affected_interval")) {
                        rate_config->block_affected_interval = child_json->valueint;
                    } else if (!strcmp(child_json->string, "wifi_amend")) {
                        rate_config->wifi_amend = (float)child_json->valuedouble;
                    } else if (!strcmp(child_json->string, "fourG_amend")) {
                        rate_config->fourG_amend = (float)child_json->valuedouble;
                    } else if (!strcmp(child_json->string, "resolution_amend")) {
                        rate_config->resolution_amend = (float)child_json->valuedouble;
                    } else if (!strcmp(child_json->string, "priority_policy")) {
                        rate_config->priority_policy = child_json->valueint;
                    }
                    break;
                case cJSON_Object:
                    break;
                case cJSON_NULL:
                    ALOGD("jason type NULL\n");
                    break;
                case cJSON_Array:
                case cJSON_False:
                case cJSON_True:
                    ALOGD("adapt config json not include tye %d\n", child_json->type);
                    break;
            }
        }

    }

    cJSON_Delete(root);
    return 0;
}

int VodRateAdaptConfigA1_parse_config(VodRateAdaptConfigA1* rate_config, char* config_string) {
    if (!config_string) {
        return -1;
    }
    cJSON* root = cJSON_Parse(config_string);
    if (!root) {
        return -1;
    }

    if (cJSON_Object == root->type) {
        int len = cJSON_GetArraySize(root);
        for (int i = 0; i < len; i++) {
            cJSON* child_json = cJSON_GetArrayItem(root, i);

            switch (child_json->type) {
                case cJSON_Number:
                    if (!strcmp(child_json->string, "short_keep_interval")) {
                        rate_config->short_keep_interval = child_json->valueint;
                    } else if (!strcmp(child_json->string, "long_keep_interval")) {
                        rate_config->long_keep_interval = child_json->valueint;
                    } else if (!strcmp(child_json->string, "bitrate_init_level")) {
                        rate_config->bitrate_init_level = child_json->valueint;
                    } else if (!strcmp(child_json->string, "max_resolution")) {
                        rate_config->max_resolution = child_json->valueint;
                    }
                    break;
                case cJSON_Object:
                    break;
                case cJSON_NULL:
                    ALOGD("jason type NULL\n");
                    break;
                case cJSON_Array:
                case cJSON_False:
                case cJSON_True:
                    ALOGD("adapt config json not include tye %d\n", child_json->type);
                    break;
            }
        }

    }

    cJSON_Delete(root);
    return 0;
}

static int kwai_vod_get_int_val_opt(AVDictionary* opts, const char* key, int def) {
    AVDictionaryEntry* sizeKbEntry = av_dict_get(opts, key, NULL, 0);
    if (sizeKbEntry) {
        return (int) strtod(sizeKbEntry->value, NULL);
    } else {
        return def;
    }
}

static void kwai_vod_get_str_val_opt(AVDictionary* opts, const char* key, char** dst) {
    AVDictionaryEntry* sizeKbEntry = av_dict_get(opts, key, NULL, 0);
    if (sizeKbEntry) {
        *dst = strdup(sizeKbEntry->value);
    } else {
        *dst = NULL;
    }
}

void dump_config(VodRateAdaptConfig* rate_config);
void VodRateAdaptConfig_init(VodRateAdaptConfig* rate_config);
int get_cached_vodplaylist_index(VodPlayList* playlist, int switch_code);
void dump_config_a1(VodRateAdaptConfigA1* rate_config);
void VodRateAdaptConfigA1_init(VodRateAdaptConfigA1* rate_config);

int kwai_vod_manifest_init(KwaiQos* qos, AVDictionary* fmt_options, VodPlayList* playlist, const char* file_name) {
    VodRateAdaptConfig rate_config;
    char* config_string = NULL;
    VodRateAdaptConfigA1 rate_config_a1;
    char* config_string_a1 = NULL;

    memset(playlist, 0, sizeof(VodPlayList));
    playlist->device_resolution.width = kwai_vod_get_int_val_opt(fmt_options, "device-resolution-width", 0);
    playlist->device_resolution.height = kwai_vod_get_int_val_opt(fmt_options, "device-resolution-height", 0);
    playlist->net_type = kwai_vod_get_int_val_opt(fmt_options, "device-network-type", 0);
    playlist->low_device = kwai_vod_get_int_val_opt(fmt_options, "low-device", 0);
    playlist->signal_strength = kwai_vod_get_int_val_opt(fmt_options, "signal-strength", 0);
    playlist->switch_code = kwai_vod_get_int_val_opt(fmt_options, "switch-code", 0);
    kwai_vod_get_str_val_opt(fmt_options, "abr-config-string", &config_string);
    playlist->algorithm_mode = kwai_vod_get_int_val_opt(fmt_options, "adaptive-algorithm-mode", 0);
    kwai_vod_get_str_val_opt(fmt_options, "adaptive-a1-config-string", &config_string_a1);
    if (playlist->algorithm_mode) {
        // 在A1模式下，强制关闭手动模式
        playlist->switch_code = 0;
    }

    if (kwai_vod_parse_manifest(playlist, file_name) < 0) {
        if (config_string != NULL) {
            freep((void**)&config_string);
        }
        return -1;
    }
    VodRateAdaptConfig_init(&rate_config);
    VodRateAdaptConfigA1_init(&rate_config_a1);

    int index = -1;
    char vod_resolution[MAX_LEN_MAX_RESOLUTION + 1] = {0};
    index = get_cached_vodplaylist_index(playlist, playlist->switch_code);
    if (index < 0) {
        if (config_string) {
            VodRateAdaptConfig_parse_config(&rate_config, config_string);
        }
        if (config_string_a1) {
            VodRateAdaptConfigA1_parse_config(&rate_config_a1, config_string_a1);
        }
#if ENABLE_DUMP
        dump_config(&rate_config);
        dump_config_a1(&rate_config_a1);
#endif
        if (config_string || config_string_a1) {
            c_abr_engine_adapt_init(&rate_config, &rate_config_a1);
        } else {
            c_abr_engine_adapt_init(NULL, NULL);
        }

        index = c_abr_engine_adapt_profiles(playlist, vod_resolution, MAX_LEN_MAX_RESOLUTION);
        playlist->cached = 0;
    } else {
        c_abr_get_vod_resolution(playlist, vod_resolution, MAX_LEN_MAX_RESOLUTION);
        playlist->cached = 1;
    }

    if (config_string != NULL) {
        freep((void**)&config_string);
    }

    KwaiQos_onVodAdaptive(qos, playlist, index, vod_resolution, &rate_config, &rate_config_a1);

    return index;
}

/*
 * switch_code：码率选择模式，0位自动，其他值时为标清、高清、超清之一
 * 返回值：>=0表示本地已经cached的Representation下标index，<0表示本地没有cached
 */
int get_cached_vodplaylist_index(VodPlayList* playlist, int switch_code) {
    for (int i = 0; i < playlist->rep_count; i++) {
        bool switch_check_cached = (switch_code > 0) && (playlist->rep[i].id == switch_code);
        if (switch_code == 0 || switch_check_cached) {
            if (ac_is_fully_cached(playlist->rep[i].url, playlist->rep[i].key)) {
                return i;
            } else {
                // 没有完全缓存的情况下，从cache中获取已经缓存的数据。
                playlist->rep[i].cached_bytes = ac_get_cached_bytes_by_key(playlist->rep[i].url, playlist->rep[i].key);
            }
            if (switch_check_cached) {
                break;
            }
        }
    }
    return -1;
}

void VodRateAdaptConfig_init(VodRateAdaptConfig* rate_config) {
    rate_config->rate_addapt_type = 0;
    rate_config->bandwidth_estimation_type = 0;
    rate_config->absolute_low_res_low_device = 0;
    rate_config->adapt_under_4G = 0;
    rate_config->adapt_under_wifi = 0;
    rate_config->adapt_under_other_net = 0;
    rate_config->absolute_low_rate_4G = 0;
    rate_config->absolute_low_rate_wifi = 0;
    rate_config->absolute_low_res_4G = 0;
    rate_config->absolute_low_res_wifi = 0;
    rate_config->short_keep_interval = 60000;          //带宽存储短时间窗口，默认60s.
    rate_config->long_keep_interval = 600000;          //带宽存储长时间窗口，默认10分钟.
    rate_config->bitrate_init_level = 0;
    rate_config->default_weight = 1.0;
    rate_config->block_affected_interval = 10000;
    rate_config->wifi_amend = 0.7;
    rate_config->fourG_amend = 0.3;
    rate_config->resolution_amend = 0.6;
    rate_config->device_width_threshold = 720;
    rate_config->device_hight_threshold = 960;
    rate_config->priority_policy = 1;
}

void VodRateAdaptConfigA1_init(VodRateAdaptConfigA1* rate_config) {
    rate_config->short_keep_interval = 30000;          //带宽存储短时间窗口，默认30s.
    rate_config->long_keep_interval = 180000;          //带宽存储长时间窗口，默认3分钟.
    rate_config->bitrate_init_level = 12;
    rate_config->max_resolution = 720 * 960;           //720p is defined as 720*960
}

void dump_config(VodRateAdaptConfig* rate_config) {
    ALOGD("[rate_config] rate_addapt_type: %d, bandwidth_estimation_type: %d \n",
          rate_config->rate_addapt_type, rate_config->bandwidth_estimation_type);
    ALOGD("[rate_config] adapt_under_4G: %d, adapt_under_wifi: %d, adapt_under_other_net: %d\n",
          rate_config->adapt_under_4G, rate_config->adapt_under_wifi,
          rate_config->adapt_under_other_net);
    ALOGD("[rate_config] absolute_low_res_low_device: %d, absolute_low_rate_4G: %d, "
          "absolute_low_rate_wifi: %d, absolute_low_res_4G: %d, absolute_low_res_wifi: %d\n",
          rate_config->absolute_low_res_low_device, rate_config->absolute_low_rate_4G,
          rate_config->absolute_low_rate_wifi, rate_config->absolute_low_res_4G,
          rate_config->absolute_low_res_wifi);
    ALOGD("[rate_config] short_keep_interval: %d, long_keep_interval: %d \n",
          rate_config->short_keep_interval, rate_config->long_keep_interval);
    ALOGD("[rate_config] bitrate_init_level: %d, block_affected_interval: %d\n",
          rate_config->bitrate_init_level, rate_config->block_affected_interval);
    ALOGD("[rate_config] default_weight: %f, wifi_amend: %f, fourG_amend: %f, resolution_amend: %f\n",
          rate_config->default_weight, rate_config->wifi_amend,
          rate_config->fourG_amend, rate_config->resolution_amend);
    ALOGD("[rate_config] device_width_threshold: %d, device_hight_threshold: %d \n",
          rate_config->device_width_threshold, rate_config->device_hight_threshold);
    ALOGD("[rate_config] priority_policy: %d \n", rate_config->priority_policy);
}

void dump_config_a1(VodRateAdaptConfigA1* rate_config) {
    ALOGD("[rate_config] short_keep_interval: %d, long_keep_interval: %d \n",
          rate_config->short_keep_interval, rate_config->long_keep_interval);
    ALOGD("[rate_config] bitrate_init_level: %d, max_resolution: %d \n",
          rate_config->bitrate_init_level, rate_config->max_resolution);
}
