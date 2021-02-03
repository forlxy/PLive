#pragma once
#include "offline_cache_util.h"
#include "data_source_seekable.h"

using namespace kuaishou::cache;

struct ACDataSource {
    DataSpec spec;
    std::unique_ptr<DataSource> data_source;

    std::shared_ptr<OfflineCacheUtil> offline_cache_util;

    bool data_source_seekable = false;
    bool need_report = false;   //控制seesionstart、sessionclose 对外通知
};
