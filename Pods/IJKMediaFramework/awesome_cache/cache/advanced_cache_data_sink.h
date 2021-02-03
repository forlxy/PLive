#pragma once
#include "data_sink.h"
#include "cache/cache.h"
#include "file.h"
#include "buffered_output_stream.h"
#include "advanced_cache_data_sink_stats.h"
#include "awesome_cache_runtime_info_c.h"

namespace kuaishou {
namespace cache {

class AdvancedCacheDataSink : public DataSink {
  public:
    AdvancedCacheDataSink(Cache* cache, AwesomeCacheRuntimeInfo* ac_rt_nfo,
                          int64_t max_cache_size, int buffer_size = kDefaultBufferOutputStreamSize);
    virtual ~AdvancedCacheDataSink();

    virtual int64_t Open(const DataSpec& spec) override;

    virtual int64_t Write(uint8_t* buf, int64_t offset, int64_t len) override;

    virtual AcResultType Close() override;

    virtual Stats* GetStats() override;
  private:
    inline AcResultType ReportError(AcResultType error);
  private:
    AcResultType OpenNextOutputStream();
    AcResultType CloseCurrentOutputStream();
    Cache* cache_;
    const int64_t max_cache_file_size_;
    const int buffer_size_;
    DataSpec spec_;
    kpbase::File file_;
    int64_t data_spec_bytes_written_;
    int64_t output_stream_bytes_written_;
    int64_t current_span_bytes_remaining_;
    std::shared_ptr<kpbase::BufferedOutputStream> output_stream_;
    std::shared_ptr<kpbase::BufferedOutputStream> current_output_stream_;
    std::shared_ptr<CacheSpan> locked_span_;
    // indicating that the next span got from cache is cached, should not write to cache any more.
    bool following_already_cached_;
    bool current_output_stream_write_error_;
    //indicating that the current output stream has write error or not
    AwesomeCacheRuntimeInfo* ac_rt_info_;
    std::unique_ptr<internal::AdvancedCacheDataSinkStats> stats_;
};

} // namespace cache
} // namespace kuaishou
