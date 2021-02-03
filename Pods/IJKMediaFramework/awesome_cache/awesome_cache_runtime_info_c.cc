//
//  awesome_cache_runtime_info_c.c
//  IJKMediaFramework
//
//  Created by 帅龙成 on 2018/10/29.
//  Copyright © 2018 kuaishou. All rights reserved.
//

#include <include/awesome_cache_runtime_info_c.h>
#include "string.h"
#include "awesome_cache_runtime_info_c.h"
#include "utility.h"

using namespace kuaishou;

void AwesomeCacheRuntimeInfo_init(AwesomeCacheRuntimeInfo* info) {
    memset(info, 0, sizeof(AwesomeCacheRuntimeInfo));
    info->cache_v2_info.cached_bytes_on_play_start = -1;
}

void AwesomeCacheRuntimeInfo_release(AwesomeCacheRuntimeInfo* info) {
    if (info->download_task.config_header) {
        free(info->download_task.config_header);
        info->download_task.config_header = nullptr;
    }
    if (info->download_task.config_user_agent) {
        free(info->download_task.config_user_agent);
        info->download_task.config_user_agent = nullptr;
    }
    if (info->vod_p2sp.sdk_details) {
        free(info->vod_p2sp.sdk_details);
        info->vod_p2sp.sdk_details = nullptr;
    }
}

void AwesomeCacheRuntimeInfo_cache_ds_init(AwesomeCacheRuntimeInfo* info) {
    memset(&info->cache_ds, 0, sizeof(info->cache_ds));
    memset(&info->vod_adaptive, 0, sizeof(info->vod_adaptive));
}

void AwesomeCacheRuntimeInfo_download_task_init(AwesomeCacheRuntimeInfo* info) {
    memset(&info->download_task, 0, sizeof(info->download_task));
}

int AwesomeCacheRuntimeInfo_download_task_get_transfer_cost_ms(AwesomeCacheRuntimeInfo* info) {
    if (info->download_task.ts_download_end_ms >= info->download_task.ts_download_start_ms) {
        return (int)(info->download_task.ts_download_end_ms >= info->download_task.ts_download_start_ms);
    } else if (info->download_task.ts_download_start_ms <= 0) {
        return 0;
    } else {
        return static_cast<int>(kpbase::SystemUtil::GetCPUTime() - info->download_task.ts_download_start_ms);
    }
}

void AwesomeCacheRuntimeInfo_download_task_start(AwesomeCacheRuntimeInfo* info) {
    info->download_task.ts_download_start_ms = kpbase::SystemUtil::GetCPUTime();
}
void AwesomeCacheRuntimeInfo_download_task_end(AwesomeCacheRuntimeInfo* info) {
    info->download_task.ts_download_end_ms = kpbase::SystemUtil::GetCPUTime();
}

void AwesomeCacheRuntimeInfo_download_task_set_config_user_agent(AwesomeCacheRuntimeInfo* info, const char* val) {
    if (!info || !val) {
        return;
    }
    if (info->download_task.config_user_agent) {
        free(info->download_task.config_user_agent);
    }
    info->download_task.config_user_agent = strdup(val);
}

void AwesomeCacheRuntimeInfo_download_task_set_config_header(AwesomeCacheRuntimeInfo* info, const char* val) {
    if (!info || !val) {
        return;
    }
    if (info->download_task.config_header) {
        free(info->download_task.config_header);
    }
    info->download_task.config_header = strdup(val);
}

const char* AwesomeCacheRuntimeInfo_config_get_datas_source_type_str(AwesomeCacheRuntimeInfo* info) {
    switch (info->cache_applied_config.data_source_type) {
        case kDataSourceTypeDefault:
            return "CacheSync";
        case kDataSourceTypeAsyncDownload:
            return "CacheASync";
        case kDataSourceTypeLiveNormal:
            return "LiveNormal";
        case kDataSourceTypeLiveAdaptive:
            return "LiveAdaptive";
        case kDataSourceTypeSegment:
            return "Segment";
        case kDataSourceTypeAsyncV2:
            return "CacheAsyncV2";
        default:
            return "unknown";
    }
}

const char* AwesomeCacheRuntimeInfo_config_get_upstream_type_to_str(AwesomeCacheRuntimeInfo* info) {
    switch (info->cache_applied_config.upstream_type) {
        case kDefaultHttpDataSource:
            return "Curl";
        case kMultiDownloadHttpDataSource:
            return "CurlMulti";
        case kP2spHttpDataSource:
            return "P2sp";
        case kCronetHttpDataSource:
            return "Cronet";
        default:
            return "unknown";
    }
}

const char* AwesomeCacheRuntimeInfo_config_get_buffered_datasource_type_str(
    AwesomeCacheRuntimeInfo* info) {
    switch (info->cache_applied_config.buffered_type) {
        case kBufferedDataSource:
            return "BufferOld";
        default:
            return "unknown";
    }
}

const char* AwesomeCacheRuntimeInfo_config_get_download_task_type_to_str(AwesomeCacheRuntimeInfo* info) {
    switch (info->cache_applied_config.curl_type) {
        case kCurlTypeAsyncDownload:
            return "CurlAsync";
        case kCurlTypeDefault:
            return "CurlDefault";
        default:
            return "Unknown";
    }
}

