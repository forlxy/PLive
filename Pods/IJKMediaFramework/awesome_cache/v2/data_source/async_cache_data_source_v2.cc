//
// Created by MarshallShuai on 2019-06-30.
//

#include "async_cache_data_source_v2.h"
#include <utility>
#include "ac_log.h"
#include "v2/cache/cache_content_index_v2.h"
#include "v2/cache/cache_scope.h"
#include "v2/cache/cache_v2_settings.h"
#include "abr/abr_engine.h"
#include "v2/cache/cache_v2_file_manager.h"
#include "include/awesome_cache_runtime_info_c.h"
#include "hodor_downloader/hodor_downloader.h"

namespace kuaishou {
namespace cache {

AsyncCacheDataSourceV2::AsyncCacheDataSourceV2(const DataSourceOpts& opts,
                                               std::shared_ptr<CacheSessionListener> listener,
                                               AwesomeCacheRuntimeInfo* ac_rt_info):
    data_source_opts_(opts),
    cache_session_listener_(std::move(listener)),
    context_id_(opts.context_id),
    ac_rt_info_(ac_rt_info),
    last_error_(kResultOK),
    current_reading_position_(0) {

    if (opts.enable_vod_adaptive && ac_rt_info_) {
        ac_rt_info_->vod_adaptive.real_time_throughput_kbps =
            kuaishou::abr::AbrEngine::GetInstance()->get_real_time_throughput();
    }
}

int64_t AsyncCacheDataSourceV2::Open(const kuaishou::cache::DataSpec& spec) {
    // LOG_WARN("[%d][AsyncCacheDataSourceV2::Open] spec.position:%lld", context_id_, spec.position);

    spec_ = spec;
    cache_content_ = CacheV2FileManager::GetMediaDirManager()->Index()->GetCacheContent(spec.uri, spec.key, true);
    if (!cache_content_->IsFullyCached()) {
        HodorDownloader::GetInstance()->GetTrafficCoordinator()->OnPlayerDownloadStart(cache_content_->GetKey());
    }

    current_reading_position_ = spec.position;

    int64_t ret = OpenNextScope();
    // 老 callback 兼容
    if (cache_session_listener_ && current_scope_data_source_) {
        current_scope_data_source_->PostEventInCallbackThread([&] {
            cache_session_listener_->OnSessionStarted(cache_content_->GetKey(),
                                                      static_cast<uint64_t>(spec.position),
                                                      cache_content_->GetCachedBytes(),
                                                      static_cast<uint64_t>(cache_content_->GetContentLength()));
        });
    }
    if (ret < 0) {
        LOG_ERROR("[%d][AsyncCacheDataSourceV2::Open]OpenNextScrope fail, error:%lld", context_id_, ret);
        last_error_ = static_cast<int32_t>(ret);
    } else if (ret == 0) {
        last_error_ = kAsyncCacheInnerError_8;
        return last_error_;
    } else {
        // ret > 0
    }

    if (ac_rt_info_) {
        ac_rt_info_->cache_ds.total_bytes = ret;
        snprintf(ac_rt_info_->cache_ds.current_read_uri, DATA_SOURCE_URI_MAX_LEN, "%s", spec.uri.c_str());
    }

    return ret;
}

int64_t AsyncCacheDataSourceV2::OpenNextScope() {
    // LOG_WARN("[AsyncCacheDataSourceV2::OpenNextScope]");
    // OpenNextScope之前也要检测，不然CacheScope::ScopeForCacheContent里不太方便处理这种错误
    if (cache_content_->GetContentLength() > 0 && current_reading_position_ >= cache_content_->GetContentLength()) {
        return kAsyncCacheSpecPositionOverflow1;
    }

    current_scope_ = cache_content_->ScopeForPosition(current_reading_position_);
    if (!current_scope_data_source_) {
        current_scope_data_source_ = std::make_shared<AsyncScopeDataSource>(data_source_opts_.download_opts,
                                                                            cache_session_listener_,
                                                                            data_source_opts_.cache_callback,
                                                                            ac_rt_info_);
    }
    current_scope_data_source_->SetContextId(context_id_);

    // fixme
    int64_t expect_consume_len = spec_.length > 0 ? spec_.position + spec_.length - current_scope_->GetStartPosition() : kLengthUnset;
    int64_t ret = current_scope_data_source_->Open(current_scope_, spec_.uri, expect_consume_len);
    if (ret < 0) {
        LOG_ERROR("[%d][AsyncCacheDataSourceV2::OpenNextScope]current_scope_data_source_->Open fail, error:%lld", context_id_, ret);
        last_error_ = static_cast<int32_t>(ret);
        return ret;
    } else if (ret > 0) {
        last_error_ = kResultOK;
        if (cache_content_->GetContentLength() <= 0) {
            cache_content_->SetContentLength(ret);
        } else if (cache_content_->GetContentLength() != ret) {
            LOG_ERROR("[%d][AsyncCacheDataSourceV2::OpenNextScope] new content_length != old content_length (%lld!=%lld)",
                      context_id_, ret, cache_content_->GetContentLength());
            return kAsyncCacheInnerError_3;
        }
    } else {
        return kAsyncCacheInnerError_2;
    }

    if (current_scope_data_source_->ContainsPosition(current_reading_position_)) {
        // do seek
        int64_t seek_ret = current_scope_data_source_->Seek(
                               current_reading_position_ - current_scope_->GetStartPosition());
        if (seek_ret < 0) {
            LOG_ERROR(
                "[%d][AsyncCacheDataSourceV2::OpenNextScope] current_scope_data_source_->Seek() fail, error:%lld",
                context_id_, seek_ret);
            last_error_ = static_cast<int32_t>(seek_ret);
            return last_error_;
        } else {
            last_error_ = kResultOK;
            // do nothing ,should return ret(content-length)
        }
    } else if (current_reading_position_ >= cache_content_->GetContentLength()) {
        LOG_ERROR("")
        return kAsyncCacheSpecPositionOverflow2;
    } else {
        LOG_ERROR("[%d][AsyncCacheDataSourceV2::OpenNextScope] kAsyncCacheInnerError_6,current_reading_position_:%lld, cache_content_->GetContentLength():%lld ",
                  context_id_, current_reading_position_, cache_content_->GetContentLength());
        return kAsyncCacheInnerError_6;
    }

    return ret;
}


int64_t AsyncCacheDataSourceV2::Read(uint8_t* buf, int64_t offset, int64_t read_len) {
    if (last_error_ < 0) {
        LOG_ERROR("[%d]AsyncCacheDataSourceV2::Read] already has error， last_error_:%d", context_id_, last_error_);
        return last_error_;
    }

    if (current_reading_position_ > current_scope_data_source_->GetEndPosition() + 1) {
        LOG_ERROR("[%d][AsyncCacheDataSourceV2::Read]current_reading_position_(%lld) > current_scope_data_source_->GetEndPosition()(%lld) + 1",
                  context_id_, current_reading_position_, current_scope_data_source_->GetEndPosition());
        return kAsyncCacheInnerError_4;
    } else if ((spec_.length > 0 && current_reading_position_ >= spec_.position + spec_.length)
               || current_reading_position_ >= cache_content_->GetContentLength()) {
        return kResultEndOfInput;
    } else if (current_reading_position_ == current_scope_data_source_->GetEndPosition() + 1) {
        CloseCurrentScope();
        int64_t ret = OpenNextScope();
        if (ret < 0) {
            last_error_ = static_cast<int32_t>(ret);
            LOG_ERROR("[%d]AsyncCacheDataSourceV2::Read] OpenNextScope() fail, ret:%lld", context_id_, ret);
            return ret;
        }
    }

    int64_t ret = current_scope_data_source_->Read(buf, offset, read_len);
    if (ret < 0) {
        last_error_ = static_cast<int32_t>(ret);
        LOG_ERROR("[%d][AsyncCacheDataSourceV2::Read] current_scope_data_source_->Read error :%d",
                  context_id_, last_error_);
        return ret;
    } else {
        current_reading_position_ += ret;
    }
    return ret;
}

AcResultType AsyncCacheDataSourceV2::Close() {
    // LOG_WARN("[AsyncCacheDataSourceV2::Close]");
    if (cache_session_listener_) {
        if (current_scope_data_source_ == nullptr) {
            LOG_ERROR("[%d][AsyncCacheDataSourceV2::Close] warning, current_scope_data_source_ is null, can not post onSessionClosed ",
                      context_id_);
        } else {
            current_scope_data_source_->PostEventInCallbackThread([&] {
                // 上层不关注回调的字段，全部填0即可
                cache_session_listener_->OnSessionClosed(last_error_, 0, 0, 0, "", true);
            }, true);
        }
    }
    CloseCurrentScope();

    HodorDownloader::GetInstance()->GetTrafficCoordinator()->OnPlayerDownloadFinish(cache_content_->GetKey(),
                                                                                    last_error_ != kResultOK && !is_cache_abort_by_callback_error_code(last_error_));

    return kResultOK;
}

int64_t AsyncCacheDataSourceV2::Seek(int64_t pos) {
//    LOG_INFO("[%d][AsyncCacheDataSourceV2::Seek] pos:%lld", context_id_, pos);
    if (pos > cache_content_->GetContentLength() - 1) {
        return kAsyncCacheSeePosOverflow;
    }
    if (current_scope_data_source_->ContainsPosition(pos)) {
        int64_t ret = current_scope_data_source_->Seek(pos - current_scope_data_source_->GetStartPosition());
        if (ret < 0) {
            last_error_ = static_cast<int32_t>(ret);
            LOG_ERROR("[%d][AsyncCacheDataSourceV2::Seek] current_scope_data_source_->Seek pos:%lld, error :%d",
                      context_id_, pos, last_error_);
            return ret;
        } else {
            last_error_ = kResultOK;
        }
    } else {
        CloseCurrentScope();
        current_reading_position_ = pos;
        int64_t ret = OpenNextScope();
        if (ret < 0) {
            last_error_ = static_cast<int32_t>(ret);
            LOG_ERROR("[%d][AsyncCacheDataSourceV2::Seek] seek pos:%lld, OpenNextScope fail, error :%d",
                      context_id_, pos, last_error_);
            return ret;
        }
    }
    current_reading_position_ = pos;
    return pos;
}


void AsyncCacheDataSourceV2::CloseCurrentScope() {
    if (current_scope_data_source_) {
        current_scope_data_source_->Close();
    }
}


int64_t AsyncCacheDataSourceV2::ReOpen() {
    // do nothing for now
    return 0;
}

AsyncCacheDataSourceV2::~AsyncCacheDataSourceV2() = default;


} // namespace cache
} // namespace kuaishou

