#include "data_io_stream.h"
#include "file.h"

namespace kuaishou {
namespace kpbase {
namespace impl {

template<typename T, size_t size = sizeof(T)>
union DataUnion {
  T val;
  char buf[size];
};
} // impl

DataInputStream::DataInputStream(const File& file) : InputStream(file) {
}

int DataInputStream::ReadInt(int& err) {
  impl::DataUnion<int> int_union;
  stream_wrapper_->stream.read(int_union.buf, sizeof(int));
  err = stream_wrapper_->stream.good() ? 0 : -1;
  return stream_wrapper_->stream.good() ? int_union.val : 0;
}

long DataInputStream::ReadLong(int& err) {
  impl::DataUnion<long> long_union;
  stream_wrapper_->stream.read(long_union.buf, sizeof(long));
  err = stream_wrapper_->stream.good() ? 0 : -1;
  return stream_wrapper_->stream.good() ? long_union.val : 0;
}

std::string DataInputStream::ReadString(int& err) {
  char c;
  std::string str = "";
  while (stream_wrapper_->stream.read(&c, 1), c != '\0') {
    if (stream_wrapper_->stream.good()) {
      str += c;
    } else {
      err = -1;
      return "";
    }
  }
  return str;
}

int DataInputStream::ReadInt() {
  int err;
  return ReadInt(err);
}

long DataInputStream::ReadLong() {
  int err;
  return ReadLong(err);
}

float DataInputStream::ReadFloat() {
  impl::DataUnion<float> float_union;
  stream_wrapper_->stream.read(float_union.buf, sizeof(float));
  return float_union.val;
}

double DataInputStream::ReadDouble() {
  impl::DataUnion<double> double_union;
  stream_wrapper_->stream.read(double_union.buf, sizeof(double));
  return double_union.val;
}

std::string DataInputStream::ReadString() {
  int err;
  return ReadString(err);
}


int64_t DataInputStream::ReadInt64(int& err) {
    impl::DataUnion<int64_t> int64_t_union;
    stream_wrapper_->stream.read(int64_t_union.buf, sizeof(int64_t));
    err = stream_wrapper_->stream.good() ? 0 : -1;
    return stream_wrapper_->stream.good() ? int64_t_union.val : 0;
}

int32_t DataOutputStream::WriteInt64(int64_t val) {
  impl::DataUnion<int64_t> int64_t_union;
  int64_t_union.val = val;
  return Write((uint8_t*)int64_t_union.buf, 0, sizeof(int64_t));
}


DataInputStream::DataInputStream(const InputStream& another_stream) : InputStream(another_stream) {
}

DataOutputStream::DataOutputStream(const File& file, bool append) : OutputStream(file, append) {
}

int32_t DataOutputStream::WriteInt(int val) {
  impl::DataUnion<int> int_union;
  int_union.val = val;
  return Write((uint8_t*)int_union.buf, 0, sizeof(int));
}

int32_t DataOutputStream::WriteLong(long val) {
  impl::DataUnion<long> long_union;
  long_union.val = val;
  return Write((uint8_t*)long_union.buf, 0, sizeof(long));
}

int32_t DataOutputStream::WriteFloat(float val) {
  impl::DataUnion<float> float_union;
  float_union.val = val;
  return Write((uint8_t*)float_union.buf, 0, sizeof(float));
}

int32_t DataOutputStream::WriteDouble(double val) {
  impl::DataUnion<double> double_union;
  double_union.val = val;
  return Write((uint8_t*)double_union.buf, 0, sizeof(double));
}

int32_t DataOutputStream::WriteString(std::string str) {
  return Write((uint8_t*)str.c_str(), 0, str.size() + 1);
}

DataOutputStream::DataOutputStream(const OutputStream& another_stream) : OutputStream(another_stream) {
}

} // base
} // kuaishou
