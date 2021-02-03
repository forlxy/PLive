#pragma once

#include "abr_types.h"

namespace kuaishou {
namespace abr {

class  BandwidthEstimationAlgorithmInterface {
  public:
    static BandwidthEstimationAlgorithmInterface* Create(const RateAdaptConfig& rate_adapt_config, const RateAdaptConfigA1& rate_adapt_config_a1);
    virtual ~BandwidthEstimationAlgorithmInterface() {}

    virtual void UpdateDownloadInfo(DownloadSampleInfo& info) = 0;
    virtual void UpdateBlockInfo(std::vector<uint64_t>& block_info) = 0;
    virtual uint32_t ShortTermBandwidthEstimate(uint32_t algorithm_mode = 0) = 0;
    virtual uint32_t LongTermBandwidthEstimate(uint32_t algorithm_mode = 0) = 0;
    virtual BandwidthEstimationAlgoType GetBandwidthEstimationAlgoType() const = 0;
    virtual uint32_t short_bandwidth_kbps(uint32_t algorithm_mode = 0) = 0;
    virtual uint32_t long_bandwidth_kbps(uint32_t algorithm_mode = 0) = 0;
    virtual uint32_t real_time_throughput() = 0;
    virtual void UpdateConfig(RateAdaptConfig& rate_config, RateAdaptConfigA1& rate_config_a1) = 0;
};

}
}
