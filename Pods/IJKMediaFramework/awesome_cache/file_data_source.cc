#include <assert.h>
#include <memory>
#include "file_data_source.h"
#include "constant.h"
#include "ac_log.h"

namespace kuaishou {
namespace cache {

#define RETURN_ON_ERROR(error) \
    { \
        stats_->OnError(error); \
        return error; \
    }

FileDataSource::FileDataSource() : FileDataSource(nullptr) {
}

FileDataSource::FileDataSource(std::shared_ptr<TransferListener<FileDataSource>> listener) :
    opened_(false),
    listener_(listener),
    bytes_remaining_(0) {
}

int64_t FileDataSource::Open(const DataSpec& spec) {
    stats_.reset(new FileDataSourceStats("FileSrc"));
    spec_ = spec;
    file_ = kpbase::File(spec.uri);

    stats_->SaveDataSpec(spec, file_.base_name());
    if (!file_.Exists()) {
        LOG_ERROR_DETAIL("FileDataSource::Open, fail, !file_.Exists()");
        RETURN_ON_ERROR(kResultFileDataSourceIOError_0);
    }
    file_length_ = file_.file_size();
    if (file_length_ < 0) {
        LOG_ERROR_DETAIL("FileDataSource::Open, fail, file_length:%lld", file_length_);
        RETURN_ON_ERROR(kResultFileDataSourceIOError_1);
    }
    stats_->OnFileOpened(file_length_);

    input_stream_.reset(new kpbase::InputStream(file_));
    read_bytes_total_ = 0;

    input_stream_->Skip(spec.position);
    if (!input_stream_->Good()) {
        Close();
        LOG_ERROR_DETAIL("FileDataSource::%s, input_stream_->Skip fail, pos:%lld, file_length_::%lld", __func__, spec.position, file_length_);
        RETURN_ON_ERROR(kResultFileDataSourceIOError_2);
    }

    bytes_remaining_ = spec.length == kLengthUnset ? file_length_ - spec.position : spec.length;
    if (bytes_remaining_ < 0) {
        Close();
        LOG_ERROR_DETAIL("FileDataSource::Open, fail kResultEndOfInput");
        RETURN_ON_ERROR(kResultEndOfInput);
    }

    opened_ = true;
    if (listener_ != nullptr) {
        listener_->OnTransferStart(this, spec);
    }

    return bytes_remaining_;
}

int64_t FileDataSource::Read(uint8_t* buf, int64_t offset, int64_t read_len) {
    assert(buf);

    if (!opened_) {
        RETURN_ON_ERROR(kResultExceptionSourceNotOpened_2)
    }

    int64_t bytes_read = 0;
    if (read_len == 0) {
        return 0;
    } else if (bytes_remaining_ <= 0) {
        RETURN_ON_ERROR(kResultEndOfInput)
    } else {
        size_t actual_to_read = (size_t) std::min(bytes_remaining_, read_len);

        bytes_read = input_stream_->Read(buf, offset, actual_to_read);
        if (bytes_read <= 0) {
            LOG_ERROR_DETAIL("FileDataSource::Read, fail, kResultFileDataSourceIOError_3, bytes_read:%lld", bytes_read);
            file_.Remove();
            RETURN_ON_ERROR(kResultFileDataSourceIOError_3);
        }
        stats_->OnReadBytes(bytes_read);
        read_bytes_total_ += bytes_read;
        bytes_remaining_ -= bytes_read;
        if (listener_ != nullptr) {
            listener_->OnBytesTransfered(this, bytes_read);
        }
    }

    return bytes_read;
}

AcResultType FileDataSource::Close() {
    if (input_stream_) {
        input_stream_.reset();
    }
    if (opened_) {
        opened_ = false;
        if (listener_) {
            listener_->OnTransferEnd(this);
        }
    }
    LOG_DEBUG("[FileDataSource::Close] spec_.position:%lld, spec_.length:%lld,"
              "file_length_/read/remaining:(%lld/%lld/%lld) \n",
              spec_.position, spec_.length, file_length_, read_bytes_total_, bytes_remaining_);
    return kResultOK;
}

Stats* FileDataSource::GetStats() {
    return stats_.get();
}

#undef RETURN_ON_ERROR
}
}

