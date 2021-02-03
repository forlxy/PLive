#pragma once
#include <string>
#include "io_stream.h"

namespace kuaishou {
namespace kpbase {

class DataInputStream : public InputStream {
 public:
  DataInputStream(const File& file);
  DataInputStream(const InputStream& another_stream);
  virtual ~DataInputStream() {}

  int ReadInt();
  int ReadInt(int& err);

  long ReadLong();
  long ReadLong(int& err);

  float ReadFloat();

  double ReadDouble();

  int64_t ReadInt64(int& err);

  std::string ReadString();
  std::string ReadString(int& err);


};

class DataOutputStream : public OutputStream {
 public:
  DataOutputStream(const File& file, bool append = false);
  DataOutputStream(const OutputStream& another_stream);
  virtual ~DataOutputStream() {}

  int32_t WriteInt(int val);

  int32_t WriteLong(long val);

  int32_t WriteFloat(float val);

  int32_t WriteDouble(double val);

  int32_t WriteString(std::string str);

  int32_t WriteInt64(int64_t val);
};

} // base
} // kuaishou
