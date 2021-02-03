#include <include/awesome_cache_runtime_info_c.h>
#include "advanced_cache_data_sink.h"
#include "ac_log.h"

namespace kuaishou {
namespace cache {
AdvancedCacheDataSink::AdvancedCacheDataSink(Cache* cache, AwesomeCacheRuntimeInfo* ac_rt_info,
                                             int64_t max_cache_size, int buffer_size) :
    cache_(cache),
    max_cache_file_size_(max_cache_size),
    buffer_size_(buffer_size <= 0 ? kDefaultBufferOutputStreamSize : buffer_size),
    data_spec_bytes_written_(0),
    output_stream_bytes_written_(0),
    current_span_bytes_remaining_(0),
    following_already_cached_(false),
    ac_rt_info_(ac_rt_info) {
    stats_.reset(new internal::AdvancedCacheDataSinkStats());
}

AdvancedCacheDataSink::~AdvancedCacheDataSink() {
    if (output_stream_) {
        Close();
    }
}

AcResultType AdvancedCacheDataSink::ReportError(AcResultType error) {
    stats_->OnError(error);
    return error;
}

int64_t AdvancedCacheDataSink::Open(const DataSpec& spec) {
    if (spec.length == kLengthUnset) {
        return ReportError(kResultSpecExceptionLengthUnset);
    }
    if (spec.key.empty()) {
        return ReportError(kResultSpecExceptionKeyUnset);
    }
    stats_->SaveDataSpec(spec);

    spec_ = spec;
    data_spec_bytes_written_ = 0;
    following_already_cached_ = false;

    return OpenNextOutputStream();
}

int64_t AdvancedCacheDataSink::Write(uint8_t* buf, int64_t offset, int64_t len) {
    if (following_already_cached_) {
        return kResultContentAlreadyCached;
    }

//    LOG_DEBUG("[AdvancedCacheDataSink][%s], offset:%lld, len:%lld", __func__, offset, len);
    int64_t bytes_written = 0;
    while (bytes_written < len) {
        if (data_spec_bytes_written_ == spec_.length) {
            AcResultType ret = CloseCurrentOutputStream();
            if (ret == kResultAdvanceDataSinkCloseFlushFail) {
                LOG_ERROR("[AdvancedCacheDataSink::Write], kResultAdvanceDataSinkCloseFlushFail");
                ac_rt_info_->sink.fs_error_code = ret;
            } else if (ret != kResultOK) {
                return ReportError(ret);
            }
            return bytes_written < len ? kResultCacheExceptionWriteExceedSpecLength : kResultOK;
        }
        if (output_stream_bytes_written_ == current_span_bytes_remaining_) {
            int32_t ret = CloseCurrentOutputStream();
            if (ret == kResultAdvanceDataSinkCloseFlushFail) {
                LOG_ERROR("[AdvancedCacheDataSink::Write], kResultAdvanceDataSinkCloseFlushFail");
                ac_rt_info_->sink.fs_error_code = ret;
            } else if (ret != kResultOK) {
                return ReportError(ret);
            }

            ret = OpenNextOutputStream();
            if (ret != kResultOK) {
                // for case kResultContentAlreadyCached, it means that the next span is already cached, and should not
                // write again, everything that already is written is OK, and will return kResultContentAlreadyCached in
                // the next write operation.
                return ret == kResultContentAlreadyCached ? kResultOK : ret;
            }
        }
        int64_t bytes_to_write = std::min(len - bytes_written, current_span_bytes_remaining_ - output_stream_bytes_written_);
        bytes_to_write = std::min(bytes_to_write, spec_.length - data_spec_bytes_written_);
        if (bytes_to_write < 0) {
            LOG_ERROR_DETAIL("[AdvancedCacheDataSink::Write], bytes_to_write(%lld) < 0)", bytes_to_write);
            return kResultTeeDataSinkInnerError_2;
        }
        ac_rt_info_->sink.bytes_not_commited += bytes_to_write;

        if (current_output_stream_ && !current_output_stream_write_error_) {
            current_output_stream_->Write(buf, offset + bytes_written, bytes_to_write);
            if (!current_output_stream_->Good()) {
                //此处不返回，进行容错
                //return ReportError(kResultAdvanceDataSinkWriteFail);
                LOG_ERROR("[AdvancedCacheDataSink::Write], kResultAdvanceDataSinkWriteFail");
                file_.Remove();
                current_output_stream_write_error_ = true;
                ac_rt_info_->sink.fs_error_code = kResultAdvanceDataSinkWriteFail;
            }
        }
        bytes_written += bytes_to_write;
        output_stream_bytes_written_ += bytes_to_write;
        data_spec_bytes_written_ += bytes_to_write;
        if (!current_output_stream_write_error_)
            stats_->OnByteWritten(bytes_to_write);
    }

    return kResultOK;
}

AcResultType AdvancedCacheDataSink::Close() {
    AcResultType ret = CloseCurrentOutputStream();
    if (ret != kResultOK) {
        LOG_ERROR("[AdvancedCacheDataSink::Close], CloseCurrentOutputStream error, ret:%d", ret);
    }
    if (ret == kResultAdvanceDataSinkCloseFlushFail) {
        ac_rt_info_->sink.fs_error_code = ret;
        ret = kResultOK;
    }

    output_stream_.reset();
    return ret;
}

Stats* AdvancedCacheDataSink::GetStats() {
    return stats_.get();
}

AcResultType AdvancedCacheDataSink::OpenNextOutputStream() {
    std::shared_ptr<CacheSpan> span;
    AcResultType error = kResultOK;
    output_stream_bytes_written_ = 0;
    current_span_bytes_remaining_ = 0;
    ac_rt_info_->sink.bytes_not_commited = 0;

    span = cache_->StartReadWriteNonBlocking(spec_.key, spec_.position + data_spec_bytes_written_, error, max_cache_file_size_);
    if (error != kResultOK) {
        return ReportError(error);
    }
    if (span == nullptr) {
        // should not be here.
        return kResultAdvanceDataSinkInnerError;
        // todo
    } else if (span->IsHoleSpan()) {
        int64_t max_length = spec_.length - data_spec_bytes_written_ < span->length ?
                             spec_.length - data_spec_bytes_written_ :
                             span->length;
        current_span_bytes_remaining_ = max_length;

        if (span->is_locked) {
            // don't write file, but calculate length.
            current_output_stream_ = nullptr;
            stats_->StartSpan(true, "", spec_.position + data_spec_bytes_written_);
        } else {
            // write file
            locked_span_ = span;
            file_ = cache_->StartFile(spec_.key, spec_.position + data_spec_bytes_written_, max_length, error);
            if (error != kResultOK) {
                return ReportError(error);
            }

            stats_->StartSpan(false, file_.file_name(), spec_.position + data_spec_bytes_written_);
            if (!output_stream_) {
                output_stream_.reset(new kpbase::BufferedOutputStream(buffer_size_, file_));
            } else {
                output_stream_->Reset(file_);
            }
            current_output_stream_ = output_stream_;
        }
        current_output_stream_write_error_ = false;
    } else if (span->is_cached) {
        // return end of stream.
        following_already_cached_ = true;
        return kResultContentAlreadyCached;
    }

    return kResultOK;
}

AcResultType AdvancedCacheDataSink::CloseCurrentOutputStream() {
    AcResultType ret = kResultOK;
    if (current_output_stream_ && !current_output_stream_write_error_) {
        current_output_stream_->Flush();
        if (current_output_stream_->Good()) {
            ret = cache_->CommitFile(file_);
        } else {
            ret = kResultAdvanceDataSinkCloseFlushFail;
            file_.Remove();
        }

        ac_rt_info_->cache_ds.cached_bytes = cache_->GetContentCachedBytes(spec_.key);
        ac_rt_info_->sink.bytes_not_commited = 0;
    }
    if (locked_span_) {
        cache_->ReleaseHoleSpan(locked_span_);
        locked_span_ = nullptr;
    }
    current_output_stream_.reset();
    return ret;
}

} // cache
} // kuaishou
