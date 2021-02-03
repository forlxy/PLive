//
// Created by MarshallShuai on 2019-07-12.
//

#include <stats/json_stats.h>
#include "cache_scope_data_sink.h"
#include "ac_log.h"
#include "cache_errors.h"
#include <file.h>
#include "v2/cache/cache_v2_settings.h"

HODOR_NAMESPACE_START

static const bool kVerbose = false;

CacheScopeDataSink::CacheScopeDataSink(std::shared_ptr<CacheScope> scope)
    : cache_scope_(std::move(scope)), current_writen_len_(0), last_error_(kResultOK) {

}

AcResultType CacheScopeDataSink::Open() {

    auto file_path = cache_scope_->GetCacheFilePath();
    if (file_path.empty()) {
        LOG_ERROR("[CacheScopeDataSink::Open] file_name is null, can not flush, key:%s",
                  cache_scope_->GetKey().c_str());
        last_error_ = kCacheScopeSinkPathEmpty;
        return last_error_;
    }

    file_ = kpbase::File(file_path);
    if (file_.Exists()) {
        file_.Remove();
        if (kVerbose) {
            LOG_WARN("[AsyncScopeDataSource::FlushToScopeCacheFileIfNeeded]file not exist, path:%s",
                     file_.path().c_str());
        }
    }

    output_stream_ = std::make_shared<kpbase::OutputStream>(file_, false);
    if (!output_stream_->Good()) {
        LOG_ERROR("[CacheScopeDataSink::Open] open OutputStream fail, key:%s",
                  cache_scope_->GetKey().c_str());
        last_error_ = kCacheScopeSinkOpenFail;
        return last_error_;
    }

    return kResultOK;
}

int64_t CacheScopeDataSink::Write(uint8_t* buf, int64_t offset, int64_t len) {
    if (last_error_ != kResultOK) {
        return last_error_;
    }

    int64_t to_write = std::min(GetAvilableScopeRoom(), len);
    if (to_write <= 0) {
        last_error_ = kCacheScopeSinkWriteOverflow;
        return last_error_;
    }
    auto ret = output_stream_->Write(buf + offset, 0, to_write);
    if (ret < 0) {
        LOG_ERROR("[CacheScopeDataSink::Write] write file fail, error:%d, path:%s",
                  ret, file_.path().c_str());
        file_.Remove();
        last_error_ = kCacheScopeSinkWriteFail;
        return last_error_;
    } else {
        if (kVerbose) {
            LOG_VERBOSE("[CacheScopeDataSink::Write] :) write file success, path:%s", file_.path().c_str());
        }
        current_writen_len_ += to_write;
        return to_write;
    }
}

void CacheScopeDataSink::Close() {
    output_stream_.reset();
    file_ = kpbase::File();
}

int64_t CacheScopeDataSink::GetAvilableScopeRoom() {
    return cache_scope_->GetActualLength() - current_writen_len_;
}


HODOR_NAMESPACE_END
