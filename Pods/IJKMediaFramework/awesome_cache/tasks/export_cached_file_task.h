#pragma once
#include "data_source.h"
#include "buffered_data_source.h"
#include "task.h"
#include "task_listener.h"
#include "file.h"

namespace kuaishou {
namespace cache {

class ExportCachedFileTask : public Task {
  public:
    ExportCachedFileTask(std::unique_ptr<BufferedDataSource> data_source,
                         const DataSpec& spec, const std::string& file_name,
                         TaskListener* listener);
    virtual ~ExportCachedFileTask();
  private:
    virtual void RunInternal() override;
    DataSpec spec_;
    TaskListener* listener_;
    std::unique_ptr<BufferedDataSource> data_source_;
    kpbase::File file_;
    uint8_t* buf_;
};

}
} // namespace kuaishou::cache
