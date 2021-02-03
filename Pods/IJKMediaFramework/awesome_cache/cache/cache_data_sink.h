#pragma once
#include <stdint.h>
#include <vector>
#include <memory>
#include "data_sink.h"
#include "cache/cache.h"
#include "file.h"
#include "buffered_output_stream.h"
#include "stats/default_data_stats.h"

namespace kuaishou {
namespace cache {

namespace internal {
class CacheDataSinkStats : public DefaultDataStats {
  public:
    struct StatSpan {
        std::string uri;
        int64_t pos;
        int64_t length;
        json ToJson();
    };
    CacheDataSinkStats();
    void StartSpan(std::string uri, int64_t position);
    void AppendSpan(int64_t len);

  private:
    virtual void FillJson() override;
    std::vector<StatSpan> spans_;
};
} // internal

class CacheDataSink : public DataSink {
  public:
    CacheDataSink(Cache* cache, int64_t max_cache_size, int buffer_size = kDefaultBufferOutputStreamSize);
    virtual ~CacheDataSink();

    virtual int64_t Open(const DataSpec& spec) override;

    virtual int64_t Write(uint8_t* buf, int64_t offset, int64_t len) override;

    virtual AcResultType Close() override;

    virtual Stats* GetStats() override;

  private:
    AcResultType OpenNextOutputStream();
    void CloseCurrentOutputStream();

    Cache* cache_;
    const int64_t max_cache_file_size_;
    const int buffer_size_;
    DataSpec spec_;
    kpbase::File file_;
    int64_t data_spec_bytes_written_;
    int64_t output_stream_bytes_written_;
    std::shared_ptr<kpbase::BufferedOutputStream> output_stream_;
    std::shared_ptr<kpbase::BufferedOutputStream> current_output_stream_;
    std::unique_ptr<internal::CacheDataSinkStats> stats_;
};
} // cache
} // kuaishou
