#pragma once

#include <stdint.h>
#include <memory>
#include <fstream>

namespace kuaishou {
namespace kpbase {
class File;

static const int kIoStreamOK = 0;
static const int kIoStreamIoException = -1;
static const int kIoStreamEof = -2;

class InputStream {
 public:
  InputStream(const File& file);
  InputStream(const InputStream& another_stream);
  virtual ~InputStream() {}

  virtual bool Good();

  virtual int64_t Read(uint8_t* buf, int64_t offset, int64_t length);

  virtual void Reset();

  virtual void Skip(int64_t length);
 protected:
  // An object that can be shared in multiple streams.
  struct InStreamWrapper {
    virtual ~InStreamWrapper() {
      stream.close();
    }
    std::ifstream stream;
  };
  std::shared_ptr<InStreamWrapper> stream_wrapper_;
};

class OutputStream {
 public:
  OutputStream(const File& file, bool append = false);
  OutputStream(const OutputStream& another_stream);
  virtual ~OutputStream() {}

  virtual bool Good();

  virtual int Write(uint8_t* buf, int64_t offset, int64_t length, bool flush = true);

 protected:
  struct OutStreamWrapper {
    virtual ~OutStreamWrapper() {
      stream.close();
    }
    std::ofstream stream;
  };
  std::shared_ptr<OutStreamWrapper> stream_wrapper_;

 private:
  bool append_;
};

} // base
} // kuaishou
