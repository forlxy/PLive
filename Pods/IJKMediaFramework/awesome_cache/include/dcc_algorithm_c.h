//
// Created by MarshallShuai on 2018/11/3.
//
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include "hodor_config.h"

typedef enum DccAlgorithm_NetworkType {
    Net_UNKNOWN = -1,
    Net_WIFI = 0,
    Net_4G = 1,
    Net_OTHERS = 2,
} DccAlgorithm_NetworkType;

typedef struct {
#define DCC_ALG_STATUS_MAX_LEN (256)
    char status[DCC_ALG_STATUS_MAX_LEN + 1];
    // config
    int config_enabled; // 是否下发开启
    int config_mark_bitrate_th_10;
    int config_dcc_pre_read_ms;

    // actual result
    bool qos_used;  // 是否达到算法阈值而开启了
    int qos_dcc_pre_read_ms_used;
    float qos_dcc_actual_mb_ratio;
    int cmp_mark_kbps;
} DccAlgorithm;

HODOR_EXPORT void DccAlgorithm_init(DccAlgorithm* alg);

HODOR_EXPORT int DccAlgorithm_get_pre_read_duration_ms(DccAlgorithm* alg,
                                                       int default_ret,
                                                       int64_t meta_dur_ms,
                                                       int64_t meta_bitrate_kbps);

HODOR_EXPORT void DccAlgorithm_update_speed_mark(int mark_kbps);
HODOR_EXPORT int DccAlgorithm_get_current_speed_mark();
HODOR_EXPORT void DccAlgorithm_onNetworkChange(DccAlgorithm_NetworkType net_type);
#ifdef __cplusplus
}
#endif
