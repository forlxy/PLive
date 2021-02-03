//#include <ac_log.h>
#include "file.h"

namespace kuaishou {
namespace kpbase {

bool File::CopyFile(const File& from, const File& to) {
  if (!from.IsRegularFile()) {
    return false;
  }
  fs::path to_path(to.path());
  fs::path from_path(from.path());
  if (fs::is_directory(to_path)) {
    to_path.append(from_path.filename().string());
  } else if (to_path.parent_path() != "" && !fs::is_directory(to_path.parent_path())) {
    return false;
  }
  boost::system::error_code error;
  fs::copy_file(from_path, to_path, fs::copy_option::overwrite_if_exists, error);
  if (error != boost::system::errc::success) {
    return false;
  }
  return true;
}

bool File::MakeDirectory(const File& directory) {
  boost::system::error_code error;
  bool ret = fs::create_directory(directory.path(), error);
  if (error != boost::system::errc::success) {
    return false;
  }
  return ret;
}

//函数：bool create_directories(const path& p, system::error_code& ec)
//参数：
//      p：要创建的新目录的路径
//      ec：不抛出重载中报告错误的输出参数
//返回值：
//      若创建了 p 所解析到的目录则为 true ，否则为 false

bool File::MakeDirectories(const File& directory) {
  boost::system::error_code error;
  bool ret = fs::create_directories(directory.path(), error);
  if (error != boost::system::errc::success) {
    // LOG_ERROR("[File::MakeDirectories] return false, error:%d, ret:%d", error.value(), ret);
    return false;
  }

  if (directory.Exists()){
    return true;
  }

  return ret;
}

bool File::Remove(const File& file) {
  boost::system::error_code error;
  bool ret = fs::remove(file.path(), error);
  if (error != boost::system::errc::success) {
    return false;
  }
  return ret;
}


int64_t File::RemoveAll(const File& file) {
  boost::system::error_code error;
  int64_t ret = fs::remove_all(file.path(), error);
  if (error != boost::system::errc::success) {
    return 0;
  }
  return ret;
}

bool File::Exists(const File& file) {
  boost::system::error_code error;
  bool exists = fs::exists(file.path(), error);
  if (error != boost::system::errc::success) {
    return false;
  }
  return exists;
}

bool File::Rename(const File& file_old, const File& file_new) {
  boost::system::error_code error;
  fs::rename(file_old.path(), file_new.path(), error);
  if (error != boost::system::errc::success) {
    return false;
  }
  return true;
}

void File::WalkDir(const File& file, bool recursive, std::function<void(File)> func) {
  if (!file.IsDirectory()) {
    return;
  }
  fs::path p(file.path());
  boost::system::error_code ec;
  fs::directory_iterator i(p, ec);
  if (ec != boost::system::errc::success) {
    return;
  }

  for (; i != fs::directory_iterator(); i.increment(ec)) {
    if (ec != boost::system::errc::success) {
      return;
    }
    File listed_file = File(i->path().string());
    if (recursive && listed_file.IsDirectory()) {
      File::WalkDir(listed_file, recursive, func);
    } else {
      func(listed_file);
    }
  }
}

std::vector<File> File::ListDirWithFilter(const File& file, std::function<bool(File)> filter) {
  std::vector<File> ret;
  WalkDir(file, false, [&](File listed_file) {
    if (filter(listed_file)) {
      ret.push_back(listed_file);
    }
  });
  return ret;
}

std::vector<File> File::ListRegularFiles(const kuaishou::kpbase::File& file) {
  return ListDirWithFilter(file, [ = ](File listed_file) {
    return listed_file.IsRegularFile();
  });
}

File File::CurrentDirectory() {
  return File(fs::current_path().string());
}

File::File() : path_("") {}

File::File(const std::string& path) : path_(path) {}

File::File(const File& parent, const std::string& child) {
  path_ = parent.path_;
  path_.append(child);
}

File::~File() {}

std::string File::path() const {
  return path_.string();
}

File File::parent() const {
  return File(path_.parent_path().string());
}

int64_t File::file_size() const {
  boost::system::error_code error;
  int64_t size = fs::file_size(path_, error);
  if (error != boost::system::errc::success) {
    return -1;
  }
  return size;
}

std::string File::file_name() const {
  return path_.filename().string();
}

std::string File::base_name() const {
  std::string filename = file_name();
  const auto iter = filename.find_last_of(".");
  if (iter == std::string::npos)
    return filename;
  else
    return filename.substr(0, iter);
}

bool File::Exists() const {
  boost::system::error_code error;
  bool exist = fs::exists(path_, error);
  return error == boost::system::errc::success ? exist : false;
}

bool File::IsDirectory() const {
  boost::system::error_code error;
  bool is = fs::is_directory(path_, error);
  return error == boost::system::errc::success ? is : false;
}

bool File::IsRegularFile() const {
  boost::system::error_code error;
  bool is = fs::is_regular_file(path_, error);
  return error == boost::system::errc::success ? is : false;
}

void File::Remove() const {
  File::Remove(*this);
}

bool File::RenameTo(const File& file) const {
  if (!IsRegularFile()) {
    return false;
  }
  boost::system::error_code error;
  fs::rename(path_, file.path(), error);
  if (error != boost::system::errc::success) {
    return false;
  }
  return true;
}

bool File::CopyTo(const File& file) const {
  if (!IsRegularFile()) {
    return false;
  }
  return File::CopyFile(*this, file);
}


} // base
} // kuaishou
