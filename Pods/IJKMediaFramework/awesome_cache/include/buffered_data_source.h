#pragma once
#include <functional>
#include <mutex>
#include <atomic>
#include "data_source_seekable.h"
#include "data_source.h"
#include "awesome_cache_runtime_info_c.h"

namespace kuaishou {
namespace cache {

class BufferedDataSource : public DataSourceSeekable {
  public:
    BufferedDataSource(int buffer_size_bytes, int seek_reopen_threshold,
                       std::unique_ptr<DataSource> data_source,
                       int context_id,
                       AwesomeCacheRuntimeInfo* ac_rt_info);

    virtual ~BufferedDataSource();

    virtual int64_t Open(const DataSpec& spec) override;

    virtual int64_t Read(uint8_t* buf, int64_t offset, int64_t to_read_total_len) override;

    virtual AcResultType Close() override;

    virtual void LimitCurlSpeed() override;

    virtual Stats* GetStats() override;

    int64_t Seek(int64_t pos) override;

    int64_t ReOpen() override;
  private:
    int64_t FillBuffer();
    DataSpec spec_;
    AcResultType error_; // eof or error
    /**
     *      | <--  spec_.position -->|
     * File |----------------------------------------------------------------------|
     *                                   buf_read_offset_   buf_write_offset_
     * Buf                           |---*------------------*----------|
     *                                buf_[buffer_size_]
     */
    int64_t buf_read_offset_;
    int64_t buf_write_offset_;
    uint8_t* buf_;
    int buffer_size_;

    uint8_t* skip_buf_;
    int seek_reopen_threshold_;

    std::unique_ptr<DataSource> data_source_;
    AwesomeCacheRuntimeInfo* ac_rt_info_;

    std::mutex read_lock_;
    std::atomic_bool stopping_;
};

}
} // namesapce kuaishou::cache

