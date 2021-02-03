//
// Created by MarshallShuai on 2019-09-25.
//

#include "single_file_download_priority_step_task.h"
#include "file.h"
#include "ac_log.h"
#include "v2/cache/cache_content_index_v2.h"
#include "v2/cache/cache_v2_file_manager.h"
#include "data_spec.h"
#include "hodor_downloader/hodor_defs.h"
#include <io_stream.h>

HODOR_NAMESPACE_START

using namespace kuaishou::kpbase;

const static bool kVerbose = false;

SingleFileDownloadPriorityStepTask::SingleFileDownloadPriorityStepTask(const DataSpec& spec,
                                                                       const DownloadOpts& opts,
                                                                       std::shared_ptr<AwesomeCacheCallback> callback,
                                                                       int main_priority,
                                                                       int sub_priority)
    : DownloadPriorityStepTask(spec.key, main_priority, sub_priority),
      last_error_(kResultOK), ac_callback_(callback),
      recv_data_len_(0),
      progress_cb_interval_ms_(opts.progress_cb_interval_ms) {
    spec_ = spec;
    download_opts_ = opts;
    cache_content_ = CacheV2FileManager::GetResourceDirManager()->Index()->GetCacheContent("", spec_.key, true);
    data_buf_ = new uint8_t[step_download_bytes_];
    if (!data_buf_) {
        last_error_ = kHodorSingFileDownloadTaskOOM;
    }
}

SingleFileDownloadPriorityStepTask::~SingleFileDownloadPriorityStepTask() {
    delete []data_buf_;
}

AcResultType SingleFileDownloadPriorityStepTask::StepExecute(int thread_work_id) {
    if (last_error_ != kResultOK) {
        LOG_ERROR("[SingleFileDownloadPriorityStepTask::StepExecute] last error is already fail:%d", last_error_);
        // 这里暂时不return error，给task后重试的机会，因为每次StepExecute都有重新创建连接的机会
        // return last_error_;
    }
    // reset is_abort_by_interrupt_
    is_abort_by_interrupt_ = false;
    if (ac_callback_) {
        ac_cb_info_ = std::shared_ptr<AcCallbackInfo>(AcCallbackInfoFactory::CreateCallbackInfo());
        ac_cb_info_->SetDataSourceType(kDataSourceTypeAsyncV2);
        ac_cb_info_->SetCacheKey(cache_content_->GetKey());
    }

    if (cache_content_->GetContentLength() > 0) {
        content_length_unknown_on_open_ = false;
        cache_content_file_ = cache_content_->GetCacheContentCacheFile();
        if (ac_cb_info_) {
            ac_cb_info_->SetTotalBytes(cache_content_->GetContentLength());
            total_cached_bytes_so_far_ = cache_content_->GetCachedBytes(false);
            ac_cb_info_->SetCachedBytes(total_cached_bytes_so_far_);
        }
        // 这里实时获取大小，避免文件已经被改动过了（因为FileManager里获取的都是缓存的值）
        auto file_size = cache_content_->GetCachedBytes(true);
        if (file_size <= 0) {
            return DownloadScope(0, step_download_bytes_, thread_work_id);
        } else if (file_size > cache_content_->GetContentLength()) {
            LOG_WARN("[worker:%d][SingleFileDownloadPriorityStepTask::StepExecute] exist cache_content_file_ size(%lld) > contentLength(%lld),will delete cache cache_content_file_ and force re download",
                     thread_work_id, file_size, cache_content_->GetContentLength());
            cache_content_file_.Remove();
            // 刷新content length
            content_length_unknown_on_open_ = true;
            return DownloadScope(0, step_download_bytes_, thread_work_id);
        } else if (file_size == cache_content_->GetContentLength()) {
            LOG_INFO("[worker:%d][SingleFileDownloadPriorityStepTask::StepExecute]file_size（%lld) == contentLength MarkComplete", thread_work_id, file_size);
            MarkComplete();
            return kResultOK;
        } else {
            // download next scope
            int64_t to_download_bytes = cache_content_->GetContentLength() - file_size;
            to_download_bytes = to_download_bytes >= step_download_bytes_ ? step_download_bytes_ : to_download_bytes;
            return DownloadScope(file_size, to_download_bytes, thread_work_id);
        }
    } else {
        content_length_unknown_on_open_ = true;
        // 如果没有content-length信息，则尝试Open第一个分片，并建立/记下 分片 list
        return DownloadScope(0, step_download_bytes_, thread_work_id);
    }
}


AcResultType SingleFileDownloadPriorityStepTask::DownloadScope(int64_t start_pos, int64_t expect_download_bytes, int thread_work_id) {
    if (kVerbose) {
        LOG_DEBUG("[SingleFileDownloadPriorityStepTask::DownloadScope] start_pos:%lld, expect_download_bytes:%lld", start_pos, expect_download_bytes);
    }
    recv_data_len_ = 0;
    scope_download_task_ = ScopeTask::CreateTask(download_opts_, this, ac_rt_info_);
    start_download_position_ = start_pos;
    expect_download_bytes_ = expect_download_bytes;

    DataSpec spec = spec_;
    spec.WithPosition(start_pos).WithLength(expect_download_bytes);
    if (ac_cb_info_) {
        ac_cb_info_->SetCurrentUri(spec_.uri);
    }

    int64_t ret = scope_download_task_->Open(spec);
    if (ret < 0) {
        last_error_ = (int)ret;
        LOG_ERROR("[SingleFileDownloadPriorityStepTask::DownloadScope] open scope task fail, ret:%d", last_error_);
        return last_error_;
    }
    scope_download_task_->WaitForTaskFinish();
    scope_download_task_->Close();

    if (last_error_ != kResultOK) {
        if (last_error_ == kLibcurlErrorBase - CURLE_ABORTED_BY_CALLBACK && is_abort_by_interrupt_) {
            // 修正取消场景下的last_error
            last_error_ = kResultOK;
        } else {
            LOG_ERROR("[worker:%d][SingleFileDownloadPriorityStepTask::DownloadScope] after WaitForDownloadFinish error:%d",
                      thread_work_id, last_error_);
        }
    } else {
        AppendDataToFile();
        // check if complete
        if (last_error_ == kResultOK && (cache_content_file_.file_size() == cache_content_->GetContentLength())) {
            MarkComplete();
        }
    }

    if (kVerbose) {
        LOG_DEBUG("[worker:%d][SingleFileDownloadPriorityStepTask::DownloadScope] start_pos:%lld, expect_download_bytes_:%lld, last_error_:%d",
                  thread_work_id, start_pos, expect_download_bytes_, last_error_);
    }

    return last_error_;
}

float SingleFileDownloadPriorityStepTask::GetProgressPercent() {

#if LOG_OVERALL_DOWNLOAD_STATUS
    if (cache_content_->GetContentLength() > 0) {
        return cache_content_->GetCachedBytes() * 1.f / cache_content_->GetContentLength();
    } else {
        return 0;
    }
#else
    return 0;
#endif
}

AcResultType SingleFileDownloadPriorityStepTask::LastError() {
    return last_error_;
}

void SingleFileDownloadPriorityStepTask::Interrupt() {
    is_abort_by_interrupt_ = true;
    if (scope_download_task_) {
        scope_download_task_->Abort();
    }
}

void SingleFileDownloadPriorityStepTask::AppendDataToFile() {
    OutputStream os(cache_content_file_, true);
    os.Write(data_buf_, 0, recv_data_len_, true);
    if (!os.Good()) {
        LOG_ERROR("[SingleFileDownloadPriorityStepTask::AppendDataToFile] fail, return:%d", kHodorSingFileDownloadTaskFlushFileFail);
        last_error_ = kHodorSingFileDownloadTaskFlushFileFail;
    } else {
        CacheV2FileManager::GetResourceDirManager()->OnCacheContentFileFlushed(*cache_content_);
    }
}


void SingleFileDownloadPriorityStepTask::OnConnectionInfoParsed(const ConnectionInfoV2& info) {
    if (info.content_length > 0) {
        // content_length_unknown_on_open_ 表示之前CacheContent没有length或者缓存文件长度有问题（比如大于CacheContent的ContentLength），需要重新走流程
        if (content_length_unknown_on_open_ && info.GetFileLength() > 0) {
            cache_content_->UpdateContentLengthAndFileName(info.GetFileLength());
            CacheV2FileManager::GetResourceDirManager()->Index()->PutCacheContent(cache_content_);
            CacheV2FileManager::GetResourceDirManager()->Index()->Store();
            cache_content_file_ = cache_content_->GetCacheContentCacheFile();

        } else if (kVerbose) {
            LOG_DEBUG("[SingleFileDownloadPriorityStepTask::DownloadScope]no to SetContentLength and  PutCacheContent, "
                      "content_length_unknown_on_open_:%lld, GetFileLength:%lld",
                      content_length_unknown_on_open_, scope_download_task_->GetConnectionInfo().GetFileLength())
        }

        if (ac_callback_) {
            ac_cb_info_->SetContentLength(info.content_length);
            ac_cb_info_->SetTotalBytes(info.GetFileLength());
        }

        // 有可能 info.content_length < expect_download_bytes_，这个时候需要更新下expect_download_bytes_
        if (content_length_unknown_on_open_) {
            expect_download_bytes_ = std::min(info.content_length, expect_download_bytes_);
        }
    } else {
        // no need to set last_error_, we will catch this error later
        LOG_ERROR("[ SingleFileDownloadPriorityStepTask::OnConnectionInfoParsed], connection_info.content_length(%lld) invalid!",
                  info.content_length);
    }

    if (kVerbose) {
        LOG_DEBUG("[SingleFileDownloadPriorityStepTask::OnConnectionInfoParsed], connection_info.content_length(%lld), file_len:%lld",
                  info.content_length, info.GetFileLength());
    }
}

void SingleFileDownloadPriorityStepTask::OnReceiveData(uint8_t* buf, int64_t data_len) {
    size_t to_copy = (size_t) std::min(step_download_bytes_ - recv_data_len_, data_len);
    if (data_len > to_copy) {
        // 这块逻辑上不会走到，目前打个日志观察下
        LOG_ERROR("[SingleFileDownloadPriorityStepTask::OnReceiveData]wanring receive data_len(%lld) 大于 剩余scope buffer长度(%lld)",
                  data_len, to_copy);
    }

    if (to_copy > 0) {
        memcpy(data_buf_ + recv_data_len_, buf, data_len);
        recv_data_len_ += data_len;

        total_cached_bytes_so_far_ += data_len;
        if (ac_cb_info_) {
            ac_cb_info_->SetProgressPosition(total_cached_bytes_so_far_);
        }
        // notify progress
        ThrottleNotifyProgressToCallbacks(total_cached_bytes_so_far_);
    }
}

void SingleFileDownloadPriorityStepTask::OnDownloadComplete(int32_t error, int32_t stop_reason) {
    // todo 这里可以实现retry逻辑
    if (error != 0) {
        last_error_ = error;
    } else if (stop_reason != kDownloadStopReasonCancelled && expect_download_bytes_ != recv_data_len_) {
        // 不应该走到这的，没出错，又没下载完？以后如果能出现，看下是什么导致的
        // 如果content_length是0，是有可能走到这的，因为不会走write_callback流程。同时curl_ret也是返回0的。
        LOG_ERROR("[SingleFileDownloadPriorityStepTask::OnDownloadComplete] should not got here, stop_reason:%lld, expect_download_bytes_:%lld, recv_data_len_:%lld",
                  stop_reason, expect_download_bytes_, recv_data_len_)
        last_error_ = kAsyncCacheInnerError_9;
    }

    if (kVerbose) {
        LOG_VERBOSE("[SingleFileDownloadPriorityStepTask::OnDownloadComplete] error:%d, stop_reason:%d, recv_data_len_:%lld, total_cached_bytes_so_far_:%lld",
                    error, stop_reason, recv_data_len_, total_cached_bytes_so_far_);
    }
    // notify finish
    if (ac_callback_ && ac_cb_info_) {
        ac_cb_info_->SetStopReason(static_cast<DownloadStopReason>(stop_reason));
        ac_cb_info_->SetErrorCode(error);
        ac_cb_info_->SetProductContext(download_opts_.product_context);
        ac_cb_info_->SetProgressPosition(total_cached_bytes_so_far_);
        AcCallbackInfo::CopyConnectionInfoIntoAcCallbackInfo(
            scope_download_task_->GetConnectionInfo(), *ac_cb_info_);

        ac_callback_->onDownloadFinish(ac_cb_info_);
    }
}

void SingleFileDownloadPriorityStepTask::SetStepDownloadBytes(int64_t bytes) {
    if (bytes <= 0) {
        LOG_WARN("[SingleFileDownloadPriorityStepTask::SetStepDownloadBytes] bytes:%lld invalid, return");
        return;
    }
    step_download_bytes_ = bytes;
}


void SingleFileDownloadPriorityStepTask::ThrottleNotifyProgressToCallbacks(int64_t total_progress_bytes) {
    // throttle
    auto cur_ts = kpbase::SystemUtil::GetCPUTime();
    if (cur_ts - last_notify_progress_ts_ms_ > progress_cb_interval_ms_) {
        if (kVerbose) {
            LOG_DEBUG("[SingleFileDownloadPriorityStepTask::OnReceiveData] cur_ts(%llu) - last_notify_progress_ts_ms_(%lld):%lld, progress_cb_interval_ms_:%lld",
                      cur_ts, last_notify_progress_ts_ms_,
                      (int64_t)(cur_ts - last_notify_progress_ts_ms_), progress_cb_interval_ms_);
        }

        last_notify_progress_ts_ms_ = cur_ts;

        // 新的callback
        if (ac_callback_) {
            LOG_DEBUG("[SingleFileDownloadPriorityStepTask::ThrottleNotifyProgressToCallbacks][progressBlock] total_progress_bytes:%lld", total_progress_bytes);
            ac_cb_info_->SetProgressPosition(total_progress_bytes);
            ac_callback_->onSessionProgress(ac_cb_info_);
        }
    }
}

HODOR_NAMESPACE_END
