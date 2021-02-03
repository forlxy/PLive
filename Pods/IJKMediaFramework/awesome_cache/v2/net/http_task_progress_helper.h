#pragma once

#include "cache_opts.h"
#include "connection_info_v2.h"
#include "scope_task.h"
#include "download/speed_calculator.h"
#include "data_source.h"


namespace kuaishou {
namespace cache {

// Handle speed_cal, dcc, abr, connection info etc.
// Construct this class before transfer start
class HttpTaskProgressHelper {

    int id_;
    const DownloadOpts options_;
    ConnectionInfoV2* connection_info_;
    AwesomeCacheRuntimeInfo* ac_rt_info_;
    std::shared_ptr<SpeedCalculator> speed_cal_;

    uint64_t start_ts_ms_ = 0;
    uint64_t end_ts_ms_ = 0;

  public:
    HttpTaskProgressHelper(int id, const DownloadOpts& options, ConnectionInfoV2* connection_info, AwesomeCacheRuntimeInfo* ac_rt_info);

    void OnStart();
    void OnProgress(int64_t received_bytes);
    void OnFinish();

};

}
}
