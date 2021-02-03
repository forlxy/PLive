#include <thread>
#include <include/awesome_cache_runtime_info_c.h>
#include <assert.h>

#include "buffered_data_source.h"
#include "../ijkmedia/ijksdl/ijksdl_log.h"
#include "ac_log.h"

namespace kuaishou {
namespace cache {

BufferedDataSource::BufferedDataSource(int buffer_size_bytes, int seek_reopen_threshold,
                                       std::unique_ptr<DataSource> data_source,
                                       int context_id,
                                       AwesomeCacheRuntimeInfo* ac_rt_info)
    : data_source_(std::move(data_source))
    , buf_write_offset_(0)
    , buf_read_offset_(0)
    , buffer_size_(buffer_size_bytes)
    , buf_(new uint8_t[buffer_size_bytes])
    , seek_reopen_threshold_(seek_reopen_threshold)
    , skip_buf_(new uint8_t[seek_reopen_threshold])
    , error_(0)
    , stopping_(false)
    , ac_rt_info_(ac_rt_info) {
    ac_rt_info_->buffer_ds.buffered_datasource_seek_threshold_kb = seek_reopen_threshold / 1024;
    ac_rt_info_->buffer_ds.buffered_datasource_size_kb = buffer_size_bytes / 1024;
    ac_rt_info_->buffer_ds.reopen_cnt_by_seek = 0;
    SetContextId(context_id);
    LOG_DEBUG("[BufferedDataSource:BufferedDataSource], buffer_size_:%dkb", buffer_size_ / 1024);
}

BufferedDataSource::~BufferedDataSource() {
    std::lock_guard<std::mutex> lg(read_lock_);

    delete [] buf_;
    buf_ = nullptr;

    delete [] skip_buf_;
    skip_buf_ = nullptr;
}

int64_t BufferedDataSource::Open(const DataSpec& spec) {
    spec_ = spec;
    int64_t ret = data_source_->Open(spec);

    buf_read_offset_ = buf_write_offset_ = 0;

    return ret;
}

int64_t BufferedDataSource::Read(uint8_t* dst, int64_t offset, int64_t to_read_total_len) {
    std::lock_guard<std::mutex> lg(read_lock_);
    int64_t total_read_len = 0;
    int64_t last_read_ret = 0;
    while (total_read_len < to_read_total_len) {
        int64_t len = std::min(to_read_total_len - total_read_len, buf_write_offset_ - buf_read_offset_);
        if (len) {
            // buf里还有内容，先消费完
            memcpy(dst + offset + total_read_len, buf_ + buf_read_offset_, len);
            buf_read_offset_ += len;

            total_read_len += len;
            // 一次内存拷贝也算last_read_ret
            last_read_ret = len;
            continue;
        } else {
            // move forward spec_.position  and reset buf_write_pos_/buf_read_pos
            int64_t to_read = to_read_total_len - total_read_len;
            if (to_read > buffer_size_) {
                // 不需要缓存，直接从数据源读取
                len = data_source_->Read(dst, offset + total_read_len, to_read);
                if (len < 0) {
                    last_read_ret = len;
                    break;
                } else if (len == 0) {
                    last_read_ret = kResultBufferDataSourceReadNoData;
                    break;
                } else {
                    total_read_len += len;
                    spec_.position += len;
                    last_read_ret = len;
                }
            } else {
                // 要读的数据小于 buf的长度，先激进地fill buffer
                last_read_ret = FillBuffer();

                assert(last_read_ret != 0);
                if (last_read_ret > 0) {
                    spec_.position += buf_write_offset_;
                    buf_read_offset_ = 0;
                    buf_write_offset_ = last_read_ret;
                } else {
                    break;
                }
            }
        }
    }

    assert(last_read_ret != 0);

    if (last_read_ret > 0) {
        return total_read_len;
    } else if (last_read_ret == kResultEndOfInput && total_read_len > 0) {
        return total_read_len;
    } else {
        return last_read_ret;
    }
}

int64_t BufferedDataSource::FillBuffer() {
    int64_t read_bytes = 0;
    int64_t last_read_ret = 0;
    while (buffer_size_ > read_bytes) {
        last_read_ret = data_source_->Read(buf_, read_bytes, buffer_size_ - read_bytes);
        if (last_read_ret < 0) {
            break;
        } else if (last_read_ret == 0) {
            last_read_ret = kResultBufferDataSourceFillBufferNoData;
            break;
        } else {
            read_bytes += last_read_ret;
        }
    }

    if (read_bytes > 0) {
        return read_bytes;
    } else {
        // 只存在 last_read_ret < 0的情况,不存在last_read_ret = 0的情况
        if (last_read_ret == kResultEndOfInput && read_bytes > 0) {
            return read_bytes;
        }  else {
            return last_read_ret;
        }
    }
}

AcResultType BufferedDataSource::Close() {
    buf_read_offset_ = buf_write_offset_ = 0;
    return data_source_->Close();
}

void BufferedDataSource::LimitCurlSpeed() {
    if (data_source_) {
        data_source_->LimitCurlSpeed();
    }
}

Stats* BufferedDataSource::GetStats() {
    return data_source_->GetStats();
}

int64_t BufferedDataSource::Seek(int64_t pos) {
    if (pos >= spec_.position && pos <= spec_.position + buf_write_offset_) {
        // pos in buf, just set ptr.
        buf_read_offset_ =  pos - spec_.position;
    } else if (pos < spec_.position || pos - (spec_.position + buf_write_offset_) > seek_reopen_threshold_) {
        // seek.
        this->Close();
        int64_t ret = this->Open(spec_.WithPosition(pos));
        ac_rt_info_->buffer_ds.reopen_cnt_by_seek++;
        if (ret < 0) {
            LOG_ERROR("[%d] BufferedDataSource::Seek pos:%lld, error, ret:%d",
                      GetContextId(), pos, (int)ret);
        } else {
            LOG_INFO("[%d] BufferedDataSource::Seek pos:%lld, ret:%lld",
                     GetContextId(), pos, ret);
        }
        return ret;
    } else {
        // skip.
        spec_.position += buf_write_offset_;
        buf_read_offset_ = buf_write_offset_ = 0;
        int64_t bytes_to_skip = pos - spec_.position;
        std::lock_guard<std::mutex> lg(read_lock_);
        while (bytes_to_skip) {
            int64_t ret = data_source_->Read(skip_buf_, 0, bytes_to_skip);
            if (ret < 0) {
                LOG_ERROR_DETAIL("[%d] [%s] data_source_->Read() for skip error, ret:%d, pos:%lld, spec_.position:%lld",
                                 GetContextId(), __func__, ret, pos, spec_.position);
                return ret;
            } else if (ret == 0) {
                LOG_ERROR_DETAIL("[%d] [%s] return kResultBufferDataSourceReadNoData! pos:%lld, spec_.position:%lld",
                                 GetContextId(), __func__, pos, spec_.position);
                return kResultBufferDataSourceReadNoData;
            } else {
                spec_.position += ret;
                bytes_to_skip -= ret;
            }
        }
    }
    return spec_.position + buf_read_offset_;
}

int64_t BufferedDataSource::ReOpen() {
    Close();
    return Open(spec_.WithPosition(spec_.position + buf_read_offset_));
}

}
} // namesapce kuaishou::cache
