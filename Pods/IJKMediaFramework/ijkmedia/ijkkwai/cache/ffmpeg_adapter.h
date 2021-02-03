//
//  test_zone.h
//  KSYPlayerCore
//
//  Created by 帅龙成 on 26/10/2017.
//  Copyright © 2017 kuaishou. All rights reserved.
//
#pragma once

#include <libavformat/avformat.h>
#include <stdbool.h>
#include "cache_session_listener_c.h"
#include "cache_statistic.h"
#include "awesome_cache_c.h"
#include "player_statistic_c.h"

// 一些静态util接口
bool AwesomeCache_util_is_fully_cached(const char* url, const char* cache_key);
bool AwesomeCache_util_is_globally_enabled();
bool AwesomeCache_util_is_url_white_list(const char* url);
bool AwesomeCache_util_use_custom_protocol(AVDictionary* format_opts);

#pragma mark format_opts related
DataSourceType kwai_get_cache_datasource_type(AVDictionary* format_opts);
UpstreamDataSourceType kwai_get_cache_datasource_upstream_type(AVDictionary* format_opts);

#pragma mark AvIoOpaqueWithDataSource API
typedef struct AvIoOpaqueWithDataSource AvIoOpaqueWithDataSource;

C_DataSourceOptions C_DataSourceOptions_from_options_dict(AVDictionary* options);
void C_DataSourceOptions_release(C_DataSourceOptions* option);
void AvIoOpaqueWithDataSource_releasep(AvIoOpaqueWithDataSource** pp);
void AvIoOpaqueWithDataSource_abort(AvIoOpaqueWithDataSource* opaque);
int64_t AvIoOpaqueWithDataSource_open(AvIoOpaqueWithDataSource* opaque, const char* uri,
                                      const char* cache_key);
void AvIoOpaqueWithDataSource_close(AvIoOpaqueWithDataSource* opaque, bool need_report);
int64_t AvIoOpaqueWithDataSource_reopen(AvIoOpaqueWithDataSource* opaque, bool need_report);
bool AvIoOpaqueWithDataSource_is_read_compelete(AvIoOpaqueWithDataSource* opaque);
int AvIoOpaqueWithDataSource_read(void* opaque, uint8_t* buf, int buf_size);
int64_t AvIoOpaqueWithDataSource_seek(void* opaque, int64_t offset, int32_t whence);

#pragma mark AwesomeCache_AVIOContext API
// AwesomeCache_AVIOContext_create return 0 for success ,other for failue
int AwesomeCache_AVIOContext_create(AVIOContext** result,
                                    AVDictionary** options,
                                    unsigned ffplayer_id,
                                    const char* session_uuid,
                                    const CCacheSessionListener* listener,
                                    const AwesomeCacheCallback_Opaque cache_callback,
                                    CacheStatistic* cache_stat,
                                    ac_player_statistic_t player_statistic
                                   );
void AwesomeCache_AVIOContext_releasep(AVIOContext** pp_avio);
// AwesomeCache_AVIOContext_open return <0 for failure, others for success
int64_t AwesomeCache_AVIOContext_open(AVIOContext* avio, const char* uri, const char* cache_key);
int64_t AwesomeCache_AVIOContext_reopen(AVIOContext* avio);
void AwesomeCache_AVIOContext_close(AVIOContext* avio);
void AwesomeCache_AVIOContext_abort(AVIOContext* avio);
char* AwesomeCache_AVIOContext_get_DataSource_StatsJsonString(AVIOContext* avio);
