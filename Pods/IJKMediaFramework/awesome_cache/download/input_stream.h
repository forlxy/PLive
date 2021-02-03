#pragma once
#include <stdint.h>

namespace kuaishou {
namespace cache {

class InputStream {
  public:
    virtual ~InputStream() {}

    virtual int Read(uint8_t* buf, int32_t offset, int32_t len) = 0;

    virtual bool HasMoreData() = 0;

    virtual int error_code() = 0;

};

} // cache
} // kuaishou
