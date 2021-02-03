#pragma once
#include <memory>
#include <awesome_cache_runtime_info_c.h>
#include "data_source.h"
#include "data_sink.h"
#include "stats/json_stats.h"
#include "download/connection_info.h"


namespace kuaishou {
namespace cache {

class TeeDataSourceStats : public JsonStats {
  public:
    TeeDataSourceStats(DataSource* source, DataSink* sink);
  private:
    virtual void FillJson() override;
    DataSource* source_;
    DataSink* sink_;
};

class TeeDataSource : public DataSource, public HasConnectionInfo {
  public:
    TeeDataSource(std::shared_ptr<DataSource> source, std::shared_ptr<DataSink> sink,
                  AwesomeCacheRuntimeInfo* ac_rt_info);
    virtual int64_t Open(const DataSpec& spec) override;
    virtual int64_t Read(uint8_t* buf, int64_t offset, int64_t len) override;
    virtual AcResultType Close() override;
    virtual void LimitCurlSpeed() override;
    virtual Stats* GetStats() override;
    virtual const ConnectionInfo& GetConnectionInfo() override;
  private:
    bool write_failed_;
    std::shared_ptr<DataSource> source_;
    std::shared_ptr<DataSink> sink_;
    std::unique_ptr<TeeDataSourceStats> stats_;
    const bool USE_STATS = false;

    AwesomeCacheRuntimeInfo* ac_rt_info_;

    DataSpec spec_;
    int64_t read_bytes_total_ = 0;
};

} // cache
} // kuaishou
