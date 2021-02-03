#include "buffered_output_stream.h"
#include <assert.h>
#include "file.h"

namespace kuaishou {
namespace kpbase {

BufferedOutputStream::BufferedOutputStream(int64_t buffer_size, const File& file, bool append) :
  buf_size_(buffer_size),
  len_(0),
  buf_(new uint8_t[buffer_size]),
  output_stream_(new OutputStream(file, append)) {
}

BufferedOutputStream::BufferedOutputStream(int64_t buffer_size, const OutputStream& another_stream) :
  buf_size_(buffer_size),
  len_(0),
  buf_(new uint8_t[buffer_size]),
  output_stream_(new OutputStream(another_stream)) {
}

BufferedOutputStream::~BufferedOutputStream() {
  if (len_ > 0) {
    Flush();
  }
  delete [] buf_;
  buf_ = nullptr;
}

void BufferedOutputStream::Reset(const File& file, bool append) {
  output_stream_.reset(new OutputStream(file, append));
}

void BufferedOutputStream::Reset(const OutputStream& another_stream) {
  output_stream_.reset(new OutputStream(another_stream));
}

void BufferedOutputStream::Flush() {
  assert(len_ <= buf_size_);
  assert(output_stream_ != nullptr);
  if (len_ > 0) {
    output_stream_->Write(buf_, 0, len_);
    len_ = 0;
  }
}

int BufferedOutputStream::Write(uint8_t* buf, int64_t offset, int64_t length) {
  assert(output_stream_ != nullptr);
  if (!output_stream_->Good()) {
    return kIoStreamIoException;
  }
  if (len_ + length > buf_size_) {
    Flush();
  }
  if (length > buf_size_) {
    output_stream_->Write(buf, offset, length);
  } else {
    memcpy(buf_ + len_, buf + offset, length);
    len_ += length;
  }
  return kIoStreamOK;
}

bool BufferedOutputStream::Good() {
  return output_stream_->Good();
}

} // base
} // kuaishou
