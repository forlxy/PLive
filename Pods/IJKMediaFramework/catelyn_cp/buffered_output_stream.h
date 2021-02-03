#pragma once

#include <stdint.h>
#include "io_stream.h"

namespace kuaishou {
namespace kpbase {

class BufferedOutputStream {
 public:
  BufferedOutputStream(int64_t buffer_size, const File& file, bool append = false);
  BufferedOutputStream(int64_t buffer_size, const OutputStream& another_stream);
  virtual ~BufferedOutputStream();

  void Reset(const File& file, bool append = false);

  void Reset(const OutputStream& another_stream);

  int Write(uint8_t* buf, int64_t offset, int64_t length);

  bool Good();

  void Flush();

 private:
  std::unique_ptr<OutputStream> output_stream_;
  uint8_t* buf_;
  int64_t buf_size_;
  int64_t len_;
};

} // base
} // kuaishou
