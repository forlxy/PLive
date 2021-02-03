#pragma once

#include "abr_types.h"
#include <iostream>
#include <vector>
#include <string>
#include <thread>
extern "C" {
#include <curl/curl.h>
}

namespace kuaishou {
namespace abr {

struct AbrStats {
    std::string ip = "";
    std::string type = "";
    uint32_t last_download_bytes = 0;
    uint32_t last_download_duration_ms = 0;
    uint32_t last_download_speed_kbps = 0;
    uint32_t estimate_bandwidth_kbps = 0;
    uint32_t current_profile_avg_bitrate_kbps = 0;
    uint32_t current_profile_max_bitrate_kbps = 0;
    uint32_t current_profile_rep_id = 0;
    float current_profile_psnr = 0;
    float current_profile_ssim = 0;

    friend std::ostream& operator<<(std::ostream& os, const AbrStats& s);
};

class QosLogger {
  public:
    QosLogger();
    virtual ~QosLogger();

    void UpdateDownloadSample(DownloadSampleInfo& sample);
    void UpdateCurrentProfile(VideoProfile& profile);
    void UpdateEstimateBandwidth(uint32_t bandwidth);
    void Report();

  private:
    void Run(std::string payload);
    std::string GetLocalIp();

    bool ip_inited_;
    AbrStats qos_info_;
    std::mutex mutex_;
    std::vector<std::pair<bool, std::thread>> thread_list_;
};

}
}
