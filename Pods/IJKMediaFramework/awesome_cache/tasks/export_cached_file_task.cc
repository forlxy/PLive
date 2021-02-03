#include <thread>
#include "utility.h"

#include "export_cached_file_task.h"
#include "cache_defs.h"
#include "io_stream.h"
#include "ac_log.h"

namespace kuaishou {
namespace cache {

namespace {
static const int kReadBufferSizeBytes = 4 * 1024;
static const int kReportProgressIntervalMs = 100;
}

ExportCachedFileTask::ExportCachedFileTask(std::unique_ptr<BufferedDataSource> data_source, const DataSpec& spec,
                                           const std::string& file_name, TaskListener* listener)
    : data_source_(std::move(data_source))
    , spec_(spec)
    , file_(kpbase::File(file_name))
    , buf_(new uint8_t[kReadBufferSizeBytes])
    , listener_(listener) {
}

ExportCachedFileTask::~ExportCachedFileTask() {
    delete [] buf_;
    buf_ = nullptr;
    LOG_DEBUG("~ExportCachedFileTask");
}

void ExportCachedFileTask::RunInternal() {
    if (!file_.parent().Exists()) {
        bool ret = kpbase::File::MakeDirectories(file_.parent());
        if (!ret) {
            listener_->OnTaskFailed(kTaskFailReasonWriteFile);
            return;
        }
    }
    kpbase::OutputStream ostream(file_);
    if (!file_.Exists()) {
        if (listener_) {
            listener_->OnTaskFailed(kTaskFailReasonWriteFile);
        }
        return;
    }
    auto total_bytes = data_source_->Open(spec_);

    if (total_bytes < 0) {
        listener_->OnTaskFailed(kTaskFailReasonOpenDataSource);
        data_source_->Close();
        return;
    }
    int64_t read_bytes = 0;
    bool has_error = false;

    uint64_t last_report_progress_ts_ms = 0;
    while (!stop_signal_ && read_bytes < total_bytes) {
        auto bytes = data_source_->Read(buf_, 0, kReadBufferSizeBytes);
        if (bytes < 0 && bytes != kResultEndOfInput) {
            listener_->OnTaskFailed(kTaskFailReasonReadFail);
            has_error = true;
            break;
        }
        if (bytes == 0) {
            LOG_ERROR("[ExportCachedFileTask::RunInternal], read data return bytes = 0", __func__);
            listener_->OnTaskFailed(kTaskFailReasonReadFail);
            has_error = true;
            break;
        }

        read_bytes += bytes;
        ostream.Write(buf_, 0, bytes);

        uint64_t cur = kpbase::SystemUtil::GetCPUTime();
        if (cur - last_report_progress_ts_ms > kReportProgressIntervalMs) {
            listener_->onTaskProgress(read_bytes, total_bytes);
            last_report_progress_ts_ms = cur;
        }
        if (!ostream.Good()) {
            listener_->OnTaskFailed(kTaskFailReasonWriteFile);
            has_error = true;
            break;
        }
    }
    if (stop_signal_ && (total_bytes > 0 ? read_bytes < total_bytes : true)) {
        listener_->OnTaskCancelled();
    } else if (!has_error) {
        listener_->OnTaskSuccessful();
    }
    data_source_->Close();
}

}
} // namespace kuaishou::cache
