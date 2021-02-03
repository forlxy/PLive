#pragma once
#include "data_spec.h"
#include "constant.h"
#include "stats.h"

namespace kuaishou {
namespace cache {

class DataSink {
  public:
    virtual ~DataSink() {}

    virtual int64_t Open(const DataSpec& spec) = 0;

    virtual int64_t Write(uint8_t* buf, int64_t offset, int64_t len) = 0;

    virtual AcResultType Close() = 0;

    virtual Stats* GetStats() = 0;
};

} // namespace cache
} // namespace kuaishou
