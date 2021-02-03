#pragma once

#include <mutex>
#include <vector>
#include <string>
#include "abr_types.h"
#include "qos_logger.h"
#include "video_adaptation_algorithm_interface.h"
#include "bandwidth_estimation_algorithm_interface.h"
#include "utility.h"
// #define ABR_DEBUG_QOS

namespace kuaishou {
namespace abr {

class AbrEngine {
  public:
    static AbrEngine* GetInstance();

    // call this function before using the AbrEngine
    // init the AbrEngine with specific bandwidth estimation algorithm and video adaptation algorithm
    void Init();
    void Init(const RateAdaptConfig& rate_adapt_config);
    void Init(const RateAdaptConfig& rate_adapt_config, const RateAdaptConfigA1& rate_adapt_config_a1);
    bool IsInit();

    void UpdateConfig(RateAdaptConfig& rate_config);
    void UpdateConfig(RateAdaptConfig& rate_config, RateAdaptConfigA1& rate_config_a1);

    // feed new download info to AbrEngine, so the algorithm will udpate
    // bandwidth estimation based on the new download sample
    void UpdateDownloadInfo(DownloadSampleInfo& info);

    // giving the meta info of all profiles, adapt to a proper profile to be downloaded
    // return the <success, representation_id> pair
    std::pair<bool, uint32_t> AdaptPendingNextProfileRepId(uint32_t duration_ms, AdaptionProfile profile);

    void UpdateBlockInfo(std::vector<uint64_t>& block_info);

    uint32_t get_real_time_throughput();
    uint32_t get_short_term_throughput(uint32_t algorithm_mode = 0);
    uint32_t get_long_term_throughput(uint32_t algorithm_mode = 0);
    uint64_t get_idle_time_from_last_request();
    std::string get_switch_reason();
    const char* get_detail_switch_reason();
    std::string get_rate_adaption_algo();
    std::string get_bandwidth_estimation_algo();
    void SetHistoryData(const std::string& history_data);
    std::string GetHistoryData();
    //return 0: auto; 1: manual & success; 2: manual & failure, fallback to auto
    uint32_t get_manual_auto_state();

  private:
    AbrEngine();
    virtual ~AbrEngine() {}
    void InitInternale(const RateAdaptConfig& rate_adapt_config, const RateAdaptConfigA1& rate_adapt_config_a1);
    int SetDefaultRateAdaptConfig(RateAdaptConfig& rate_config);
    int SetDefaultRateAdaptConfigA1(RateAdaptConfigA1& rate_config_a1);

    // delete copy and move constructors and aissgn operators
    AbrEngine(AbrEngine const&) = delete;
    AbrEngine(AbrEngine&&) = delete;
    AbrEngine& operator=(AbrEngine const&) = delete;
    AbrEngine& operator=(AbrEngine&&) = delete;

    bool is_inited_;
    std::mutex mutex_;
    std::unique_ptr<VideoAdaptationAlgorithmInterface> video_adaptation_algorithm_;
    std::unique_ptr<BandwidthEstimationAlgorithmInterface> bandwidth_estimation_algorithm_;
    uint64_t last_request_time_;
    uint64_t idle_time_from_last_request_;
    std::string rate_adaption_algo_;
    std::string bandwidth_estimation_algo_;
    std::string history_data_;
    uint32_t manual_auto_state_;
#ifdef ABR_DEBUG_QOS
    QosLogger qos_logger_;
#endif
};

}
}
