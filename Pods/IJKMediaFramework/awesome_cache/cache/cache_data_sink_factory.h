//
// Created by 帅龙成 on 30/10/2017.
//
#pragma once
#include "data_sink.h"
#include "cache/cache.h"
#include "awesome_cache_runtime_info_c.h"

namespace kuaishou {
namespace cache {
class CacheDataSinkFactory final {
  public:
    CacheDataSinkFactory(Cache* cache, int64_t max_cache_file_size, int32_t buffer_size = kDefaultBufferOutputStreamSize);

    DataSink* CreateDataSink(AwesomeCacheRuntimeInfo* ac_rt_nfo) ;
  private:
    Cache* cache_;
    const int64_t max_cache_file_size_;
    const int32_t buffer_size_;
};
} // namespace cache
} // namespace kuaishou
