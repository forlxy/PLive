#include <sstream>
#include "qos_logger.h"
#include "utility.h"
#include <json/json.h>

using json = nlohmann::json;

namespace kuaishou {
namespace abr {

std::ostream& operator<<(std::ostream& os, const AbrStats& s) {
    os << "{ ";
    os << "\"photo_abr_ip\": \"" << s.ip << "\"";
    os << ", \"photo_abr_type\": \"" << s.type << "\"";
    os << ", \"photo_abr_last_download_bytes\": " << s.last_download_bytes;
    os << ", \"photo_abr_last_download_duration\": " << s.last_download_duration_ms;
    os << ", \"photo_abr_last_download_speed\": " << s.last_download_speed_kbps;
    os << ", \"photo_abr_estimate_bandwidth\": " << s.estimate_bandwidth_kbps;
    os << ", \"photo_abr_current_profile_avg_bitrate\": " << s.current_profile_avg_bitrate_kbps;
    os << ", \"photo_abr_current_profile_max_bitrate\": " << s.current_profile_max_bitrate_kbps;
    os << ", \"photo_abr_current_profile_rep_id\": " << s.current_profile_rep_id;
    os << ", \"photo_abr_current_profile_psnr\": " << s.current_profile_psnr;
    os << ", \"photo_abr_current_profile_ssim\": " << s.current_profile_ssim;
    os << " }";
    return os;
}

QosLogger::QosLogger() : ip_inited_(false) {}
QosLogger::~QosLogger() {}

void QosLogger::UpdateDownloadSample(DownloadSampleInfo& sample) {
    uint32_t duration = static_cast<uint32_t>(sample.end_timestamp - sample.begin_timestamp);
    qos_info_.last_download_bytes = static_cast<uint32_t>(sample.total_bytes);
    qos_info_.last_download_duration_ms = duration;
    qos_info_.last_download_speed_kbps = qos_info_.last_download_bytes * 8 / duration;
    qos_info_.type = "bandwidth_update";
}

void QosLogger::UpdateCurrentProfile(VideoProfile& profile) {
    qos_info_.current_profile_rep_id = profile.representation_id;
    qos_info_.current_profile_avg_bitrate_kbps = profile.avg_bitrate_kbps;
    qos_info_.current_profile_max_bitrate_kbps = profile.max_bitrate_kbps;
    qos_info_.type = "profile_update";
}

void QosLogger::UpdateEstimateBandwidth(uint32_t bandwidth) {
    qos_info_.estimate_bandwidth_kbps = bandwidth;
}

void QosLogger::Report() {
    std::lock_guard<std::mutex> lg(mutex_);
    // clear finished threads
    for (auto it = thread_list_.begin(); it != thread_list_.end();) {
        if (it->first) {
            if (it->second.joinable()) {
                it->second.join();
            }
            it = thread_list_.erase(it);
        } else {
            it++;
        }
    }
    // init ip addrs
    if (!ip_inited_) {
        qos_info_.ip = GetLocalIp();
        ip_inited_ = true;
    }
    // get payload string
    std::stringstream ss;
    ss << qos_info_;
    json stats_json = json::parse(ss);
    json payload_json;
    payload_json["path"] = "client.log";
    payload_json["@timestamp"] = kpbase::SystemUtil::GetTimeString(kpbase::SystemUtil::GetEpochTime(), 0);
    payload_json["@version"] = "photo-adaptive";
    payload_json["host"] = "0.0.0.0";
    payload_json["message"] = stats_json.dump();
    std::string payload = payload_json.dump();
    thread_list_.emplace_back(false, std::thread(&QosLogger::Run, this, payload));
}

void QosLogger::Run(std::string payload) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        std::lock_guard<std::mutex> lg(mutex_);
        auto id = std::this_thread::get_id();
        auto it = std::find_if(thread_list_.begin(), thread_list_.end(),
        [&id](const std::pair<bool, std::thread>& th_info) {
            return th_info.second.get_id() == id;
        });
        assert(it != thread_list_.end());
        it->first = true;
        return;
    }
    curl_easy_setopt(curl, CURLOPT_URL, "http://123.59.169.36/rest/n/videogroup/livesourcestation");
    curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, 1L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload.c_str());
    // add http headers.
    struct curl_slist* header_list = curl_slist_append(NULL, "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header_list);
    curl_easy_perform(curl);
    int response_code;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
    // always clean up curl.
    curl_slist_free_all(header_list);
    curl_easy_cleanup(curl);
    std::lock_guard<std::mutex> lg(mutex_);
    auto id = std::this_thread::get_id();
    auto it = std::find_if(thread_list_.begin(), thread_list_.end(),
    [&id](const std::pair<bool, std::thread>& th_info) {
        return th_info.second.get_id() == id;
    });
    assert(it != thread_list_.end());
    it->first = true;
}

std::string QosLogger::GetLocalIp() {
    // TODO doesn't work for android currently
    /*
    struct ifaddrs* ifaddr;
    if (getifaddrs(&ifaddr) == -1) {
      return "";
    }
    std::string ip = "";
    for (struct ifaddrs* ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
      if (ifa->ifa_addr && ifa->ifa_addr->sa_family == AF_INET &&
          // en0 is the wifi interface name for ios
          (!strcmp("en0", ifa->ifa_name) ||
           // wlan0 is the wifi interface name for android
           !strcmp("wlan0", ifa->ifa_name))) {
        ip = base::SocketUtil::GetAddress(ifa->ifa_addr).first;
        break;
      }
    }
    freeifaddrs(ifaddr);
    */
    return "127.0.0.1";
}

}
}
