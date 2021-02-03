#pragma once
#include <boost/filesystem.hpp>
#include <string>
#include <stdint.h>
#include <functional>
#include <vector>

namespace kuaishou {
namespace kpbase {
namespace fs = boost::filesystem;

class File {
 public:
  static bool CopyFile(const File& from, const File& to);
  static bool MakeDirectory(const File& directory);
  static bool MakeDirectories(const File& directory);
  static bool Remove(const File& file);
  static int64_t RemoveAll(const File& file);
  static bool Rename(const File& file_old, const File& file_new);
  static bool Exists(const File& file);
  static void WalkDir(const File& file, bool recursive, std::function<void(File)> func);
  static std::vector<File> ListDirWithFilter(const File& file, std::function<bool(File)> filter);
  static std::vector<File> ListRegularFiles(const File& file);
  static File CurrentDirectory();

  File();
  File(const std::string& path);
  File(const File& parent, const std::string& child);

  virtual ~File();

  // query
  std::string path() const;
  File parent() const;
  int64_t file_size() const;
  std::string file_name() const;
  std::string base_name() const;

  bool Exists() const;
  bool IsDirectory() const;
  bool IsRegularFile() const;

  // modify
  void Remove() const;
  bool RenameTo(const File& file) const;
  bool CopyTo(const File& file) const;

 private:
  fs::path path_;
};

} // base
} // kuaishou

