#pragma once
#include <string>
#include <map>
#include "data_source.h"
#include "stats/json_stats.h"
#include "stats/default_data_stats.h"
#include "download/download_task_listener.h"

namespace kuaishou {
namespace cache {

class HttpDataSourceStats : public DefaultDataStats {
  public:
    HttpDataSourceStats();
    int connect_time_ms = 0;
    int response_code = 0;
  private:
    virtual void FillJson() override;
};

static const std::string kRequestPropertyTimeout = "request-time-out";

class HttpDataSource : public DataSource {
  public:
    HttpDataSource() : download_listener_(nullptr) {}
    virtual ~HttpDataSource() {
        download_listener_ = nullptr;
    }

    void SetDownloadListener(DownloadTaskListener* listener) {
        download_listener_ = listener;
    }

    /**
     * methods to handle http request properties, such as headers, timeout and so on.
     */
    virtual void SetRequestProperties(const std::string& name, const std::string& value);

    virtual void ClearRequestProperty(const std::string& name);

    virtual void ClearAllRequestProperties();

    virtual bool HasRequestProperty(const std::string& name);

    virtual std::string GetRequestProperty(const std::string& name);

    virtual Stats* GetStats() = 0;

  protected:
    DownloadTaskListener* download_listener_;
  private:
    std::map<std::string, std::string> request_property_map_;
};

}
}
