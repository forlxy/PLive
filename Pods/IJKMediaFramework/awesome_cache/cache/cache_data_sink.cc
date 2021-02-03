#include "cache_data_sink.h"
#include "ac_log.h"
namespace kuaishou {
namespace cache {
#define RETURN_ERROR(error) \
    { \
        stats_->error_code = error; \
        return error; \
    }

namespace internal {
json CacheDataSinkStats::StatSpan::ToJson() {
    return json{
        {"uri", uri},
        {"pos", pos},
        {"length", length},
    };
}

CacheDataSinkStats::CacheDataSinkStats() : DefaultDataStats("CacheDataSink") {
}

void CacheDataSinkStats::StartSpan(std::string uri, int64_t position) {
    spans_.push_back(StatSpan{
        .uri = uri,
        .pos = position,
        .length = 0,
    });
}

void CacheDataSinkStats::AppendSpan(int64_t len) {
    if (spans_.size() > 0) {
        spans_[spans_.size() - 1].length += len;
    }
}

void CacheDataSinkStats::FillJson() {
    DefaultDataStats::FillJson();
    for (auto& span : spans_) {
        stats_["files"].push_back(span.ToJson());
    }
}

} // internal

CacheDataSink::CacheDataSink(Cache* cache, int64_t max_cache_size, int buffer_size) :
    cache_(cache),
    max_cache_file_size_(max_cache_size),
    buffer_size_(buffer_size <= 0 ? kDefaultBufferOutputStreamSize : buffer_size),
    data_spec_bytes_written_(0),
    output_stream_bytes_written_(0) {
}

CacheDataSink::~CacheDataSink() {
    if (output_stream_) {
        Close();
    }
}

int64_t CacheDataSink::Open(const DataSpec& spec) {
    stats_.reset(new internal::CacheDataSinkStats());
    if (spec.length == kLengthUnset) {
        RETURN_ERROR(kResultSpecExceptionLengthUnset);
    }
    if (spec.key.empty()) {
        RETURN_ERROR(kResultSpecExceptionKeyUnset);
    }
    stats_->uri = spec.uri;
    stats_->pos = spec.position;
    stats_->bytes_total = spec.length;
    spec_ = spec;
    data_spec_bytes_written_ = 0;
    stats_->bytes_transfered = 0;
    return OpenNextOutputStream();
}

int64_t CacheDataSink::Write(uint8_t* buf, int64_t offset, int64_t len) {
    if (!output_stream_) {
        return kResultExceptionSourceNotOpened_1;
    }
    int64_t bytes_written = 0;
    while (bytes_written < len) {
        if (data_spec_bytes_written_ == spec_.length) {
            CloseCurrentOutputStream();
            return bytes_written < len ? kResultCacheExceptionWriteExceedSpecLength : kResultOK;
        }
        if (output_stream_bytes_written_ == max_cache_file_size_) {
            CloseCurrentOutputStream();
            int ret = OpenNextOutputStream();
            if (ret != kResultOK) {
                return ret;
            }
        }
        int64_t bytes_to_write = std::min(len - bytes_written, max_cache_file_size_ - output_stream_bytes_written_);
        bytes_to_write = std::min(bytes_to_write, spec_.length - data_spec_bytes_written_);
        // just let it write ignoring errors. In case there is a error, it will be handled when closing current stream.
        output_stream_->Write(buf, offset + bytes_written, bytes_to_write);
        bytes_written += bytes_to_write;
        output_stream_bytes_written_ += bytes_to_write;
        data_spec_bytes_written_ += bytes_to_write;
        stats_->AppendSpan(bytes_to_write);
        stats_->bytes_transfered += bytes_to_write;
    }
    return kResultOK;
}

AcResultType CacheDataSink::Close() {
    CloseCurrentOutputStream();
    output_stream_.reset();
    return kResultOK;
}

AcResultType CacheDataSink::OpenNextOutputStream() {
    int64_t max_length = spec_.length == kLengthUnset ?
                         max_cache_file_size_ : std::min(spec_.length - data_spec_bytes_written_, max_cache_file_size_);


    AcResultType result;
    std::shared_ptr<CacheSpan> span = cache_->StartReadWriteNonBlocking(spec_.key, spec_.position + data_spec_bytes_written_, result, max_cache_file_size_);
    if (result != kResultOK) {
        return result;
    }

    file_ = cache_->StartFile(spec_.key, spec_.position + data_spec_bytes_written_, max_length, result);
    if (result != kResultOK) {
        RETURN_ERROR(result);
    }
    stats_->StartSpan(file_.file_name(), spec_.position + data_spec_bytes_written_);
    if (!output_stream_) {
        output_stream_.reset(new kpbase::BufferedOutputStream(buffer_size_, file_));
    } else {
        output_stream_->Reset(file_);
    }
    current_output_stream_ = output_stream_;
    output_stream_bytes_written_ = 0;
    return kResultOK;
}

void CacheDataSink::CloseCurrentOutputStream() {
    if (!current_output_stream_) {
        return;
    }
    current_output_stream_->Flush();
    if (current_output_stream_->Good()) {
        cache_->CommitFile(file_);
    } else {
        file_.Remove();
    }
    current_output_stream_.reset();
}

Stats* CacheDataSink::GetStats() {
    return stats_.get();
}

#undef RETURN_ERROR
} // cache
} // kuaishou
