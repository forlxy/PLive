#pragma once

#include "download/download_manager.h"

namespace kuaishou {
namespace cache {

class NoOpDownloadManager : public DownloadManager {
  public:
    NoOpDownloadManager() {};
    ~NoOpDownloadManager() {};
};


} // cache
} // kuaishou
