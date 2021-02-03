//
// Created by liuyuxin on 2018/4/2.
//

#ifndef IJKPLAYER_C_ABR_ENGINE_H
#define IJKPLAYER_C_ABR_ENGINE_H

#include "stdint.h"
#include "hodor_config.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct VodPlayList VodPlayList;
typedef struct VodRateAdaptConfig VodRateAdaptConfig;
typedef struct VodRateAdaptConfigA1 VodRateAdaptConfigA1;

HODOR_EXPORT int c_abr_engine_adapt_profiles(VodPlayList* pl, char* buf, int len);
HODOR_EXPORT void c_abr_engine_adapt_init(VodRateAdaptConfig* vod_rate_config, VodRateAdaptConfigA1* vod_rate_config_a1);
HODOR_EXPORT uint64_t c_abr_get_idle_last_request_time();
HODOR_EXPORT void c_abr_get_switch_reason(char* buf, int len);
HODOR_EXPORT const char* c_abr_get_detail_switch_reason();
HODOR_EXPORT void c_abr_get_vod_resolution(VodPlayList* pl, char* buf, int len);
HODOR_EXPORT uint32_t c_abr_get_short_throughput_kbps(uint32_t algorithm_mode);
HODOR_EXPORT uint32_t c_abr_get_long_throughput_kbps(uint32_t algorithm_mode);

#ifdef __cplusplus
}
#endif
#endif //IJKPLAYER_C_ABR_ENGINE_H
