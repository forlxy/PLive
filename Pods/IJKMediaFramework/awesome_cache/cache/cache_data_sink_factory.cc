//
// Created by 帅龙成 on 30/10/2017.
//

#include "cache_data_sink_factory.h"
#include "cache/cache_data_sink.h"
#include "cache/advanced_cache_data_sink.h"

namespace kuaishou {
namespace cache {

CacheDataSinkFactory::CacheDataSinkFactory(Cache* cache, int64_t max_cache_file_size, int32_t buffer_size) :
    cache_(cache),
    max_cache_file_size_(max_cache_file_size),
    buffer_size_(buffer_size) {
}

DataSink* CacheDataSinkFactory::CreateDataSink(AwesomeCacheRuntimeInfo* ac_rt_nfo) {
//  return new CacheDataSink(cache_, max_cache_file_size_, buffer_size_);
    return new AdvancedCacheDataSink(cache_, ac_rt_nfo, max_cache_file_size_, buffer_size_);
}

} // namespace cache
} // namespace kuaishou
