//
//  offline_cache.h
//  IJKMediaPlayer
//
//  Created by wangtao03 on 2017/12/6.
//  Copyright © 2017年 kuaishou. All rights reserved.
//
#pragma once
#include <stdio.h>
#include <thread>
#include <include/task_listener.h>
#include "cache_defs.h"
#include "cache_opts.h"
#include "data_source.h"
#include "event.h"
#include "awesome_cache_runtime_info_c.h"

namespace kuaishou {
namespace cache {

class OfflineCacheUtil {
  public:
    OfflineCacheUtil(const std::string& url,
                     const std::string& cache_key,
                     const DataSourceOpts& opts,
                     TaskListener* listener = nullptr);

    // For Simulate dual playback with Async Mode
#ifdef TESTING
    int32_t AsyncRead(uint8_t* buf, int32_t offset, int32_t read_len);
#endif
    void StopOfflineCache();
    bool OnTaskThread() {
        return std::this_thread::get_id() == cache_thread_.get_id();
    }
  private:
    void CacheThread(const std::string& url, const std::string& cache_key, DataSourceType type);
  private:
    std::thread cache_thread_;
    std::atomic<bool> terminate_cache_thread_;
    std::unique_ptr<DataSource> offline_data_source_;
    TaskListener* listener_;
    AwesomeCacheRuntimeInfo ac_rt_info_;
#ifdef TESTING
    DataSourceType data_source_type_;
    kuaishou::kpbase::Event async_source_open_;
    std::atomic<bool> async_source_opend_;
    bool pause_at_middle_;
#endif
};

} // cache
} // kuaishou
