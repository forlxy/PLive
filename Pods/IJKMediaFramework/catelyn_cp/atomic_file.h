#pragma once
#include <memory>
#include "file.h"
#include "io_stream.h"

namespace kuaishou {
namespace kpbase {

class AtomicFile {
 public:
  AtomicFile(File file);
  virtual ~AtomicFile();

  void Remove();

  std::unique_ptr<OutputStream> StartWrite();

  void EndWrite(std::unique_ptr<OutputStream> stream);

  std::unique_ptr<InputStream> StartRead();

 private:
  void RestoreBackup();

  File base_name_;
  File backup_name_;
};

} // base
} // kuaishou
