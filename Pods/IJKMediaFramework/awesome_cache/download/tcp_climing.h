#pragma once

namespace kuaishou {
namespace cache {
class TcpCliming final {
  public:
    TcpCliming();
    void Init(int64_t max_threshold_bytes, int64_t now_ts_ms);
    void Update(int64_t downloaded_bytes, int64_t time_ms);

    std::string GetTcpClimbingInfoString();

  private:
    std::vector<std::string> tcp_climbing_info_;//开始下载后，每100ms窗口统计一次下载数据量，目前只统计前800KB的下载信息
    int64_t last_ts_ms_;
    int64_t last_download_bytes_;
    int64_t max_threshold_bytes_;
    bool stopped_;
};
}
}

