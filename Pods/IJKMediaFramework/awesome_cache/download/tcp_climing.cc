#include "utility.h"
#include <json/json.h>
#include "ac_log.h"
#include "tcp_climing.h"

using json = nlohmann::json;

namespace kuaishou {
namespace cache {

TcpCliming::TcpCliming() : last_download_bytes_(0),
    last_ts_ms_(0),
    stopped_(false) {
}

void TcpCliming::Init(int64_t max_threshold_bytes, int64_t now_ts_ms) {
    max_threshold_bytes_ = max_threshold_bytes;
    last_ts_ms_ = now_ts_ms;
}

void TcpCliming::Update(int64_t downloaded_bytes, int64_t now_time_ms) {
    int64_t time_diff_tcp_climbing = now_time_ms - last_ts_ms_;
    if (stopped_) {
        return;
    }
    if (last_download_bytes_ >= max_threshold_bytes_ || tcp_climbing_info_.size() >= 70) {
        stopped_ = true;
    } else {
        if (time_diff_tcp_climbing >= 100) {
            std::string tcp_info = kpbase::StringUtil::Int2Str(time_diff_tcp_climbing) + ": " +
                                   kpbase::StringUtil::Int2Str(downloaded_bytes - last_download_bytes_);
            tcp_climbing_info_.push_back(tcp_info);
            last_ts_ms_ = now_time_ms;
            last_download_bytes_ = downloaded_bytes;
        }
    }
    return;
}

std::string TcpCliming::GetTcpClimbingInfoString() {
    int size = tcp_climbing_info_.size();

    if (size == 0) {
        return "{}";
    }

    json j;

    for (int i = 0; i < size; i++) {
        j.push_back(tcp_climbing_info_[i]);
    }
    return j.dump();
}

}
}


