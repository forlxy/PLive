//
// Created by MarshallShuai on 2018/10/31.
//

#include <string.h>
#include "cache_statistic.h"

#define FfmpegAdapterQos_adapter_error_unset -9999
void FfmpegAdapterQos_init(FfmpegAdapterQos* qos) {
    if (!qos) {
        return;
    }
    memset(qos, 0, sizeof(FfmpegAdapterQos));
    qos->adapter_error = FfmpegAdapterQos_adapter_error_unset;
}

void FfmpegAdapterQos_release(FfmpegAdapterQos* qos) {
    // do nothing for now
}

void CacheStatistic_init(CacheStatistic* self) {
    FfmpegAdapterQos_init(&self->ffmpeg_adapter_qos);
    AwesomeCacheRuntimeInfo_init(&self->ac_runtime_info);
}

void CacheStatistic_release(CacheStatistic* self) {
    AwesomeCacheRuntimeInfo_release(&self->ac_runtime_info);
    FfmpegAdapterQos_release(&self->ffmpeg_adapter_qos);
}