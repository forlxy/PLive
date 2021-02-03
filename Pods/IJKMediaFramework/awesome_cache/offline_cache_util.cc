//
//  offline_cache.cc
//  IJKMediaPlayer
//
//  Created by wangtao03 on 2017/12/6.
//  Copyright © 2017年 kuaishou. All rights reserved.
//

#include "offline_cache_util.h"
#include "utility.h"
#include "ac_log.h"
#include "cache_manager.h"

namespace kuaishou {
namespace cache {

OfflineCacheUtil::OfflineCacheUtil(const std::string& url,
                                   const std::string& cache_key,
                                   const DataSourceOpts& opts,
                                   TaskListener* listener) :
    terminate_cache_thread_(false),
    listener_(listener),
#ifdef TESTING
    async_source_opend_(false),
    data_source_type_(opts.type),
    pause_at_middle_(opts.pause_at_middle),
#endif
    offline_data_source_(CacheManager::GetInstance()->CreateDataSource(const_cast<DataSourceOpts&>(opts), nullptr, &ac_rt_info_)) {
    cache_thread_ = std::thread(&OfflineCacheUtil::CacheThread, this, url, cache_key, opts.type);
    LOG_DEBUG("[OfflineCacheUtil]: Thread(%d)'s sub cache_thread_(%d)\n", std::this_thread::get_id(), cache_thread_.get_id());
}

#ifdef TESTING
int32_t OfflineCacheUtil::AsyncRead(uint8_t* buf, int32_t offset, int32_t read_len) {
    if (kDataSourceTypeDefault == data_source_type_) {
        return kResultOfflineCacheUnSupported;
    }

    if (!async_source_opend_) {
        async_source_open_.Wait();
        async_source_opend_ = true;
    }

    return offline_data_source_->Read(buf, offset, read_len);
}
#endif

void OfflineCacheUtil::StopOfflineCache() {
    terminate_cache_thread_ = true;

    offline_data_source_->Close();
    std::string stats = offline_data_source_->GetStats()->ToString();
    size_t print_len = 0;
    LOG_DEBUG("\n\n\n\n[OfflineCacheUtil]: Thread(%d) Stats\n\n", cache_thread_.get_id());
    while (print_len < stats.size()) {
        // print 4000 characters per time.
        size_t pos = print_len + 1023;
        if (pos > stats.size()) {
            pos = stats.size();
        }
        auto sub_str = stats.substr(print_len, pos);
        LOG_DEBUG("%s\n", sub_str.c_str());
        print_len = pos;
    }

    if (cache_thread_.joinable()) {
        cache_thread_.join();
    }

    offline_data_source_.reset(nullptr);
}

void OfflineCacheUtil::CacheThread(const std::string& url, const std::string& cache_key, DataSourceType type) {
    AcResultType err = 0;

    LOG_DEBUG("[OfflineCacheUtil]: Cache Thread(%d) Started\n", std::this_thread::get_id());

    DataSpec spec = DataSpec().WithUri(url).WithKey(cache_key != "" ? cache_key : std::string());

    int64_t ret = offline_data_source_->Open(spec);
    if (ret < 0) {
        LOG_ERROR_DETAIL("[OfflineCacheUtil::CacheThread(%d)], data_source open err(%d), Thread Exit\n", std::this_thread::get_id(), ret);
        // fix me: error callback here
        listener_->OnTaskFailed(kTaskFailReasonOpenDataSource);
        return;
    }

#ifdef TESTING
    if (kDataSourceTypeAsyncDownload == type) {
        async_source_open_.Signal();
    }
#endif

    int64_t length = ret;
    int64_t total_read = 0;

    // only exec for cache data source
    if (kDataSourceTypeDefault == type) {
        uint8_t* buffer = new uint8_t[kDefaultBufferSizeBytes];
        int64_t bytes_read = 0;

        while (!terminate_cache_thread_ && total_read != length) {
#ifdef TESTING
            if (pause_at_middle_) {
                if (total_read > 2 * 1024 * 1024 * 1.5) {
                    break;
                }
            }
#endif
            bytes_read = offline_data_source_->Read(buffer, 0, std::min(length - total_read, (int64_t)kDefaultBufferSizeBytes));
            if (kResultEndOfInput == bytes_read) {
                break;
            } else if (bytes_read >= 0) {
                total_read += bytes_read;
                listener_->onTaskProgress(total_read, length);
            } else {
                err = (int32_t)bytes_read;
                // fix me: error callback here
                listener_->OnTaskFailed(kTaskFailReasonReadFail);
                break;
            }
        }

        if (total_read == length)
            listener_->OnTaskSuccessful();
        else
            listener_->OnTaskCancelled();

        delete [] buffer;
        buffer = nullptr;
    }

    LOG_DEBUG("[OfflineCacheUtil]: Cache Thread(%d) Exit\n", std::this_thread::get_id());

    // fix me: total bytes cached callback
}

} // cache
} // kuaishou
