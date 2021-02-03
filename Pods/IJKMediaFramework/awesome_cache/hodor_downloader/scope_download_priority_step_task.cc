//
// Created by MarshallShuai on 2019-08-05.
//

#include <v2/cache/cache_v2_file_manager.h>
#include "scope_download_priority_step_task.h"
#include "v2/cache/cache_content_index_v2.h"
#include "data_spec.h"
#include "hodor_downloader/hodor_defs.h"


HODOR_NAMESPACE_START

const static bool kVerbose = false;

ScopeDownloadPriorityStepTask::ScopeDownloadPriorityStepTask(const DataSpec& spec,
                                                             const DownloadOpts& opts,
                                                             std::shared_ptr<AwesomeCacheCallback> callback,
                                                             int main_priority,
                                                             int sub_priority,
                                                             EvictStrategy evict_strategy)
    : DownloadPriorityStepTask(spec.key, main_priority, sub_priority),
      last_error_(kResultOK), callback_(std::move(callback)),
      evict_strategy_(evict_strategy) {
    spec_ = spec;
    download_opts_ = opts;
}

AcResultType ScopeDownloadPriorityStepTask::StepExecute(int thread_work_id) {
    // cache content需要在task执行的时候临时获取，拿到最新的信息（尤其是content-length），因为这个时候可能别的任务已经下载完了）
    cache_content_ = CacheV2FileManager::GetMediaDirManager()->Index()->GetCacheContent("", spec_.key, true);
    cache_content_->SetEvictStrategy(evict_strategy_);

    if (last_error_ != kResultOK) {
        LOG_ERROR("[ScopeDownloadPriorityStepTask::StepExecute] last error is already fail:%d", last_error_);
        // 这里暂时不return error，给task后重试的机会，因为每次StepExecute都有重新创建连接的机会
        // return last_error_;
    }
    // reset is_abort_by_interrupt_
    is_abort_by_interrupt_ = false;

    if (cache_content_->GetContentLength() > 0) {
        // 如果有content-length信息，先check是否任务已经下载完成（暂时只看分片是否全部缓存）,是的话，mark complete_=true
        auto cached_bytes = cache_content_->GetCachedBytes();
        auto fully_cached = cache_content_->IsFullyCached();
        if ((spec_.length > 0 && cached_bytes >= spec_.length) ||  fully_cached) {
            LOG_DEBUG("[ScopeDownloadPriorityStepTask::StepExecute]无需下载，已经完成, key:%s, spec_.length:%lld, cached_bytes:%lld, fully_cached:%d",
                      spec_.key.c_str(), spec_.length, cached_bytes, fully_cached);
            MarkComplete();
            return kResultOK;
        } else {
            if (scope_list_.empty()) {
                // 这个任务首次运行，先获取scope_list
                scope_list_ = cache_content_->GetScopeList();
            }
            if (scope_list_.empty()) {
                last_error_ = kHodorCurlStepTaskScopeListInnerError_1;
                return last_error_;
            }
            for (auto& scope : scope_list_) {
                if (fully_cached_scope_set_.find(scope->GetStartPosition()) == fully_cached_scope_set_.end()) {
                    if (scope->IsScopeFulllyCached()) {
                        // 已经缓存了
                        fully_cached_scope_set_.insert(scope->GetStartPosition());
                        continue;
                    } else {
                        return DownloadScope(scope, thread_work_id);
                    }
                } else {
                    // 已经缓存了
                    continue;
                }
            }

            // 不应该走到这的
            LOG_ERROR("[worker:%d][ScopeDownloadPriorityStepTask::StepExecute] kHodorCurlStepTaskScopeListInnerError_2",
                      thread_work_id);
            last_error_ = kHodorCurlStepTaskScopeListInnerError_2;
            return last_error_;
        }
    } else {
        // 如果没有content-length信息，则尝试Open第一个分片，并建立/记下 分片 list
        return DownloadScope(nullptr, thread_work_id);
    }
}


AcResultType ScopeDownloadPriorityStepTask::DownloadScope(std::shared_ptr<CacheScope> scope, int thread_work_id) {
    if (scope_downloader_ == nullptr) {
        scope_downloader_ = std::make_shared<AsyncScopeDataSource>(download_opts_, nullptr, callback_.get());
        scope_downloader_->SetUsage(AsyncScopeDataSource::Usage::DownloadTask);
    }

    int64_t expect_consume_len;
    if (scope == nullptr) {
        scope = cache_content_->ScopeForPosition(0);
    }

    expect_consume_len = spec_.length > 0 ? spec_.position + spec_.length - scope->GetStartPosition() : kLengthUnset;

    int64_t ret = scope_downloader_->Open(scope, spec_.uri, expect_consume_len);
    if (ret < 0) {
        LOG_ERROR("[worker:%d][ScopeDownloadPriorityStepTask::DownloadScope] scope_downloader_->Open error:%d",
                  thread_work_id, ret);
        last_error_ = static_cast<int32_t>(ret);
    } else {
        if (cache_content_->GetContentLength() <= 0) {
            cache_content_->SetContentLength(ret);
            CacheV2FileManager::GetMediaDirManager()->Index()->PutCacheContent(cache_content_);
        }
        last_error_ = scope_downloader_->WaitForDownloadFinish();
    }
    scope_downloader_->Close();

    if (last_error_ != kResultOK) {
        if (last_error_ == kLibcurlErrorBase - CURLE_ABORTED_BY_CALLBACK && is_abort_by_interrupt_) {
            // 修正取消场景下的last_error
            last_error_ = kResultOK;
        } else {
            LOG_ERROR("[worker:%d][ScopeDownloadPriorityStepTask::DownloadScope] WaitForDownloadFinish error:%d",
                      thread_work_id, last_error_);
        }
    } else {
        // check if complete
        // TODO 这里可以做成更高级/复杂的check方式。目前加载都是从0字节开始下，不需要这么完善，后续有需要再优化
        auto cached_bytes = cache_content_->GetCachedBytes();
        if (spec_.length > 0
            && cache_content_->GetCachedBytes() >= (spec_.position + spec_.length)) {
            LOG_INFO("[ScopeDownloadPriorityStepTask::DownloadScope], MarkComplete, cache_content_->GetCachedBytes(%lld) >= (spec_.position + spec_.length",
                     cached_bytes)
            MarkComplete();
        } else if (cache_content_->IsFullyCached()) {
            LOG_INFO("[ScopeDownloadPriorityStepTask::DownloadScope], MarkComplete, IsFullyCached = true",
                     cached_bytes)
            MarkComplete();
        }
    }

    if (kVerbose) {
        LOG_DEBUG("[worker:%d][ScopeDownloadPriorityStepTask::DownloadScope] scope:%lld~%lld, expect_consume_len:%lld, last_error_:%d",
                  thread_work_id, scope->GetStartPosition(), scope->GetEndPosition(), expect_consume_len, last_error_);
    }

    return last_error_;
}

float ScopeDownloadPriorityStepTask::GetProgressPercent() {

#if LOG_OVERALL_DOWNLOAD_STATUS
    if (cache_content_ && cache_content_->GetContentLength() > 0) {
        return cache_content_->GetCachedBytes() * 1.f / cache_content_->GetContentLength();
    } else {
        return 0;
    }
#else
    return 0;
#endif
}

AcResultType ScopeDownloadPriorityStepTask::LastError() {
    return last_error_;
}

void ScopeDownloadPriorityStepTask::Interrupt() {
    is_abort_by_interrupt_ = true;
    if (scope_downloader_) {
        scope_downloader_->Abort();
    }
}


HODOR_NAMESPACE_END
