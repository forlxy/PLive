#pragma once

#include "bandwidth_estimation_algorithm_interface.h"
#include "sample_info_queue.h"
#include "utility.h"

namespace kuaishou {
namespace abr {

typedef struct BandwidthSample {
    uint32_t bandwidth;
    float weight;
    bool is_weight_adjusted;
} BandwidthSample;

class MovingAverageBandwidthEstimation : public BandwidthEstimationAlgorithmInterface, public SampleInfoQueue<BandwidthSample> {
  public:
    MovingAverageBandwidthEstimation(const RateAdaptConfig& rate_adapt_config, const RateAdaptConfigA1& rate_adapt_config_a1);
    virtual ~MovingAverageBandwidthEstimation() {}

    void UpdateDownloadInfo(DownloadSampleInfo& info) override;
    void UpdateBlockInfo(std::vector<uint64_t>& block_info) override;
    uint32_t ShortTermBandwidthEstimate(uint32_t algorithm_mode = 0) override;
    uint32_t LongTermBandwidthEstimate(uint32_t algorithm_mode = 0) override;
    BandwidthEstimationAlgoType GetBandwidthEstimationAlgoType() const override;

    void ShortInfoQueueOnPush(const BandwidthSample& bandwidth_sample) override;
    void ShortInfoQueueOnRemove(const BandwidthSample& bandwidth_sample) override;
    void LongInfoQueueOnPush(const BandwidthSample& bandwidth_sample) override;
    void LongInfoQueueOnRemove(const BandwidthSample& bandwidth_sample) override;
    void ShortInfoQueueOnPushA1(const BandwidthSample& bandwidth_sample) override;
    void ShortInfoQueueOnRemoveA1(const BandwidthSample& bandwidth_sample) override;
    void LongInfoQueueOnPushA1(const BandwidthSample& bandwidth_sample) override;
    void LongInfoQueueOnRemoveA1(const BandwidthSample& bandwidth_sample) override;
    uint32_t short_bandwidth_kbps(uint32_t algorithm_mode = 0) override;
    uint32_t long_bandwidth_kbps(uint32_t algorithm_mode = 0) override;
    uint32_t real_time_throughput() override;
    void UpdateConfig(RateAdaptConfig& rate_config, RateAdaptConfigA1& rate_config_a1) override;

  private:
    uint32_t short_bandwidth_kbps_;
    uint32_t long_bandwidth_kbps_;
    uint32_t real_bandwidth_kbps_;
    float short_total_bandwidth_ = 0;
    float long_total_bandwidth_ = 0;
    RateAdaptConfig rate_adapt_config_;

    uint32_t short_bandwidth_kbps_a1_;
    uint32_t long_bandwidth_kbps_a1_;
    float short_total_bandwidth_a1_ = 0;
    float long_total_bandwidth_a1_ = 0;
    RateAdaptConfigA1 rate_adapt_config_a1_;
};

}
}
