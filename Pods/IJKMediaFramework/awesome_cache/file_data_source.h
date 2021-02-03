#pragma once
#include <memory>
#include "data_source.h"
#include "transfer_listener.h"
#include "file.h"
#include "io_stream.h"
#include "stats/json_stats.h"
#include "file_data_source_stats.h"

namespace kuaishou {
namespace cache {

class FileDataSource final : public DataSource {
  public:
    FileDataSource();

    FileDataSource(std::shared_ptr<TransferListener<FileDataSource>> listener);

    virtual ~FileDataSource() {}

    virtual int64_t Open(const DataSpec& spec) override;

    virtual int64_t Read(uint8_t* buf, int64_t offset, int64_t len) override;

    virtual AcResultType Close() override;

    virtual Stats* GetStats() override;

  private:
    DataSpec spec_;
    int64_t file_length_ = 0;
    int64_t read_bytes_total_ = 0;
    int64_t bytes_remaining_;
    std::shared_ptr<TransferListener<FileDataSource>> listener_;
    kpbase::File file_;
    std::unique_ptr<kpbase::InputStream> input_stream_;
    bool opened_;
    std::unique_ptr<FileDataSourceStats> stats_;
};

}
}
