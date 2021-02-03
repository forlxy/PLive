#pragma once
#include <memory>
#include "download/input_stream.h"
#include "download/connection_info.h"
#include "data_spec.h"

namespace kuaishou {
namespace cache {

static const int kResponseCodePositionOutOfRange = 416;

// deprecated
struct DownloadQos {
    int download_feed_input_cost_ms;
    int download_total_cost_ms;
    int drop_data_cnt;
    int drop_total_bytes;
    int curl_buffer_size_kb;

    int current_speed_kbps;
};

#define DownloadQos_Initilizer {-1, -1, 0, 0}

class DownloadTask: public HasConnectionInfo {
  public:
    virtual ~DownloadTask() {}

    /**
     * Get task id.
     */
    uint32_t id() { return id_; }

    /**
     * Blocking call. Won't return until we received the http header.
     */
    virtual ConnectionInfo MakeConnection(const DataSpec& spec) = 0;

    /**
     * Blocking call, will return after connection is closed.
     */
    virtual void Close() = 0;

    virtual void LimitCurlSpeed() {
        return;
    }

    /**
     * Get the input stream to read content from.
     */
    virtual std::shared_ptr<InputStream> GetInputStream() = 0;

    virtual  const ConnectionInfo& GetConnectionInfo() = 0;

    // deprecated
    virtual DownloadQos GetDownloadQos() {return (DownloadQos)DownloadQos_Initilizer;};

  protected:
    DownloadTask() : id_(GenerateId()) {}

  private:
    static uint32_t GenerateId() {
        static uint32_t id = 0;
        return id++;
    }

    uint32_t id_;
};

}
}
