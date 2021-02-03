#include "io_stream.h"
#include "file.h"

namespace kuaishou {
namespace kpbase {

#pragma mark InputStream
InputStream::InputStream(const File& file) {
  stream_wrapper_ = std::make_shared<InStreamWrapper>();
  stream_wrapper_->stream.open(file.path(), std::fstream::in | std::fstream::binary);
}

InputStream::InputStream(const InputStream& another_stream) : stream_wrapper_(another_stream.stream_wrapper_) {
}

bool InputStream::Good() {
  return stream_wrapper_->stream.good();
}

int64_t InputStream::Read(uint8_t* buf, int64_t offset, int64_t length) {
  int64_t read_len = 0;
  if (stream_wrapper_->stream.good()) {
    stream_wrapper_->stream.read((char*)buf + offset, length);
    read_len = stream_wrapper_->stream.gcount();
    if (read_len > 0) {
      return read_len;
    }
  }
  if (stream_wrapper_->stream.eof()) {
    return kIoStreamEof;
  }
  if (stream_wrapper_->stream.fail()) {
    return kIoStreamIoException;
  }

  return read_len;
}

void InputStream::Reset() {
  if (stream_wrapper_->stream.good()) {
    stream_wrapper_->stream.seekg(0, std::ios_base::beg);
  }
}

void InputStream::Skip(int64_t length) {
  if (stream_wrapper_->stream.good()) {
    stream_wrapper_->stream.seekg(length, std::ios_base::cur);
  }
}

#pragma mark OutputStream

OutputStream::OutputStream(const File& file, bool append) : append_(append) {
  stream_wrapper_ = std::make_shared<OutStreamWrapper>();
  std::ios_base::openmode mode = std::fstream::out | std::fstream::binary;
  if (append) {
    mode |= std::fstream::app;
  }
  stream_wrapper_->stream.open(file.path(), mode);
}

OutputStream::OutputStream(const OutputStream& another_stream) : stream_wrapper_(another_stream.stream_wrapper_) {
}

bool OutputStream::Good() {
  return stream_wrapper_->stream.good();
}

int OutputStream::Write(uint8_t* buf, int64_t offset, int64_t length, bool flush) {
  assert(buf);
  if (stream_wrapper_->stream.good()) {
    stream_wrapper_->stream.write((char*)buf + offset, length);

    if (flush)
      stream_wrapper_->stream.flush();

    return kIoStreamOK;
  } else {
    return kIoStreamIoException;
  }
}

} // base
} // kuaishou
