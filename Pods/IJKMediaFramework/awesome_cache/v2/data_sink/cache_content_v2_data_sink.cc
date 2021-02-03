//
// Created by MarshallShuai on 2019-07-12.
//

#include "cache_content_v2_data_sink.h"
#include "v2/cache/cache_content_v2_with_scope.h"
#include "v2/cache/cache_v2_file_manager.h"
#include "stats/json_stats.h"
#include "ac_log.h"

HODOR_NAMESPACE_START


CacheContentV2WithScopeDataSink::CacheContentV2WithScopeDataSink(std::shared_ptr<CacheContentV2WithScope> content)
    : cache_content_(content), current_writen_len_(0), last_error_(kResultOK) {
    if (content->GetContentLength() <= 0) {
        last_error_ = kCacheContentV2DataSinkContentLengthInvalid;
    }
}

int64_t CacheContentV2WithScopeDataSink::Open(const DataSpec& spec) {
    if (spec.position != 0) {
        LOG_ERROR("[CacheContentV2WithScopeDataSink::Open] position(%lld) != 0, not support", spec.position);
        last_error_ = kCacheContentV2DataSinkNotSupportPositionNonZero;
        return last_error_;
    }

    return OpenNextScopeSink();
}

int64_t CacheContentV2WithScopeDataSink::Write(uint8_t* buf, int64_t offset, int64_t len) {
    if (last_error_ != kResultOK) {
        return last_error_;
    }

    if (nullptr == current_scope_sink_) {
        last_error_ = kCacheContentV2DataSinkInnerError_1;
        return last_error_;
    }
    if (len + current_writen_len_ > cache_content_->GetContentLength()) {
        return kCacheContentV2DataSinkNotOverflow;
    }

    int64_t total_write_len = 0;

    while (total_write_len < len) {
        int64_t to_write = std::min(len - total_write_len, current_scope_sink_->GetAvilableScopeRoom());
        if (to_write > 0) {
            auto ret = current_scope_sink_->Write(buf, offset, to_write);
            if (ret < 0) {
                LOG_ERROR("[CacheContentV2WithScopeDataSink::Write] current_scope_sink_->Write fail, error:%d", (int)ret);
                last_error_ = (AcResultType)ret;
                return last_error_;
            } else {
                current_writen_len_ += ret;
                total_write_len += ret;
                continue;
            }
        } else {
            CloseCurrentScopeSink();

            auto ret = OpenNextScopeSink();
            if (ret != kResultOK) {
                LOG_ERROR("[CacheContentV2WithScopeDataSink::Write] OpenNextScopeSink fail fail, error:%d", (int)ret);
                last_error_ = ret;
                return last_error_;
            }
        }
    }

    return total_write_len;
}
AcResultType CacheContentV2WithScopeDataSink::Close() {
    CloseCurrentScopeSink();
    return kResultOK;
}

Stats* CacheContentV2WithScopeDataSink::GetStats() {
    return new DummyStats("CacheContentV2WithScopeDataSink");
}

AcResultType CacheContentV2WithScopeDataSink::OpenNextScopeSink() {
    current_scope_ = cache_content_->ScopeForPosition(current_writen_len_);
    current_scope_sink_ = std::make_shared<CacheScopeDataSink>(current_scope_);
    last_error_ = current_scope_sink_->Open();
    if (last_error_ != kResultOK) {
        LOG_ERROR("[CacheContentV2WithScopeDataSink::OpenNextScopeSink]open scope sink fail, error:%d", last_error_);
    }
    return last_error_;
}

AcResultType CacheContentV2WithScopeDataSink::CloseCurrentScopeSink() {
    // notify register FileManager
    if (current_writen_len_ > 0 && current_scope_) {
        AcResultType ret = CacheV2FileManager::GetMediaDirManager()->CommitScopeFile(*current_scope_);
        if (ret != kResultOK) {
            return ret;
        }
    } else {
        LOG_WARN("[CacheContentV2WithScopeDataSink::Write]kCacheContentV2DataSinkInnerError_2, invalid state, current_writen_len_:%lld, current_scope_:%p",
                 current_writen_len_, current_scope_.get())
        return kCacheContentV2DataSinkInnerError_2;
    }

    if (current_scope_) {
        current_scope_.reset();
    }
    if (current_scope_sink_) {
        current_scope_sink_->Close();
        current_scope_sink_.reset();
    }
    return kResultOK;
}


HODOR_NAMESPACE_END
