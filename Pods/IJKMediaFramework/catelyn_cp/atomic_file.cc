#include "atomic_file.h"
#include <stdio.h>
#include <ac_log.h>

namespace kuaishou {
namespace kpbase {

AtomicFile::AtomicFile(File file) {
  base_name_ = file;
  backup_name_ = File(file.path() + ".bk");
}

AtomicFile::~AtomicFile() {
}

void AtomicFile::Remove() {
  base_name_.Remove();
  backup_name_.Remove();
}

std::unique_ptr<OutputStream> AtomicFile::StartWrite() {
  if (base_name_.Exists()) {
    if (!backup_name_.Exists()) {
      bool ret = base_name_.RenameTo(backup_name_);
      if (!ret) {
        LOG_ERROR("[AtomicFile]Rename base name %s to back up name %s failed",
                  base_name_.path().c_str(), backup_name_.path().c_str());
      }
    } else {
      base_name_.Remove();
    }
  }

  OutputStream* stream = new OutputStream(base_name_);
  if (!stream->Good()) {
    LOG_INFO("[AtomicFile::StartWrite], !stream->Good(), to MakeDirectories");
    delete stream;
    File parent = base_name_.parent();
    File::MakeDirectories(parent);
    stream = new OutputStream(base_name_);
    if (!stream->Good()) {
      LOG_ERROR("[AtomicFile] Failed to create output stream, base_name_:%s",
                base_name_.path().c_str());
      delete stream;
      stream = nullptr;
    }
  }

  return std::unique_ptr<OutputStream>(stream);
}

void AtomicFile::EndWrite(std::unique_ptr<OutputStream> stream) {
  stream.reset();
  backup_name_.Remove();
}

std::unique_ptr<InputStream> AtomicFile::StartRead() {
  RestoreBackup();
  return std::unique_ptr<InputStream>(new InputStream(base_name_));
}

void AtomicFile::RestoreBackup() {
  if (backup_name_.Exists()) {
    base_name_.Remove();
    backup_name_.RenameTo(base_name_);
  }
}

} // base
} // kuaishou
