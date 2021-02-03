//
//  file_writer_memory_data_source.cpp
//  KSYPlayerCore
//
//  Created by wangtao03 on 2017/11/22.
//  Copyright © 2017年 kuaishou. All rights reserved.
//

#include "file_writer_memory_data_source.h"
#include "ac_log.h"

namespace kuaishou {
namespace cache {

FileWriterMemoryDataSource::FileWriterMemoryDataSource(kpbase::File file, int64_t position, int32_t max_file_size) :
    max_file_size_(max_file_size),
    buf_(nullptr),
    position_(position),
    write_offset_(0),
    read_offset_(0),
    file_size_(0),
    file_(file),
    output_stream_(nullptr) {
    if (max_file_size_ > 0) {
        buf_ = new uint8_t[max_file_size_];
    }
}

FileWriterMemoryDataSource::~FileWriterMemoryDataSource() {
    delete[] buf_;
    buf_ = nullptr;

    output_stream_.reset();
    position_ = 0;
}

int64_t FileWriterMemoryDataSource::Open(const DataSpec& spec) {
    read_stats_.reset(new DefaultDataStats("FileWriterMemoryDataSource"));
    read_stats_->uri = "in-memory";
    read_stats_->pos = position_;
    read_stats_->bytes_total = max_file_size_;
    read_stats_->bytes_transfered = 0;
    read_stats_->error_code = 0;
    write_stats_.reset(new DefaultDataStats("FileWriterMemoryDataSink"));
    write_stats_->uri = file_.file_name();
    write_stats_->pos = position_;
    write_stats_->bytes_total = max_file_size_;
    write_stats_->bytes_transfered = 0;
    write_stats_->error_code = 0;
    if (output_stream_ == nullptr) {
        output_stream_.reset(new kpbase::BufferedOutputStream(kDefaultPageSizeBytes, file_));
    }
    return kResultOK;
}

int64_t FileWriterMemoryDataSource::Read(uint8_t* buf, int64_t offset, int64_t read_len) {
    if (file_size_ != 0 && read_offset_ >= file_size_) {
        LOG_DEBUG("[FileWriterMemoryDataSource]: End of Memory\n");
        return kResultEndOfInput;
    } else {
        if (write_offset_ > read_offset_) {
            int64_t max_read_len = std::min(read_len, (int64_t)(write_offset_ - read_offset_));
            memcpy(buf + offset, buf_ + read_offset_, max_read_len);
            read_offset_ += max_read_len;
            read_stats_->bytes_transfered += max_read_len;
            return max_read_len;
        } else {
            return 0;
        }
    }
}

int64_t FileWriterMemoryDataSource::Write(uint8_t* buf, int64_t offset, int64_t len) {
    if (max_file_size_ - write_offset_ >= len) {
        auto ret = output_stream_->Write(buf, offset, len);
        if (kpbase::kIoStreamOK == ret) {
            write_offset_ += len;
            write_stats_->bytes_transfered += len;
        } else {
            write_stats_->error_code = ret;
        }
        return ret;
    } else {
        return kResultMaxLenghtExceeded;
    }
}

uint8_t* FileWriterMemoryDataSource::Buf() {
    return buf_;
}

bool FileWriterMemoryDataSource::InFileWriterMemory(int64_t read_position) {
    int64_t buf_range = 0;
    if (file_size_ > 0) {
        buf_range = file_size_;
    } else {
        buf_range = max_file_size_;
    }

    if (read_position >= position_ && read_position < position_ + buf_range) {
        return true;
    }
    return false;
}

AcResultType FileWriterMemoryDataSource::Close() {
    file_size_ = write_offset_;

    read_stats_->bytes_total = read_offset_;
    write_stats_->bytes_total = write_offset_;

    output_stream_->Flush();
    if (output_stream_->Good()) {
        return kResultOK;
    } else {
        LOG_ERROR_DETAIL("FileWriterMemoryDataSource::Close, fail");
        return kResultFileExceptionIO;
    }
}

Stats* FileWriterMemoryDataSource::GetStats() {
    return read_stats_.get();
}

Stats* FileWriterMemoryDataSource::GetWriteStats() {
    return write_stats_.get();
}

} // namespace cache
} // namespace kuaishou
