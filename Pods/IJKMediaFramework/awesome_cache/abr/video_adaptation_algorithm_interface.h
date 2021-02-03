#pragma once

#include <vector>
#include <string>
#include "abr_types.h"
#include "bandwidth_estimation_algorithm_interface.h"

namespace kuaishou {
namespace abr {

class  VideoAdaptationAlgorithmInterface {
  public:
    static VideoAdaptationAlgorithmInterface* Create(const RateAdaptConfig& rate_adapt_config, const RateAdaptConfigA1& rate_adapt_config_a1);
    virtual ~VideoAdaptationAlgorithmInterface() {}

    virtual uint32_t AdaptPendingNextProfileRepId(BandwidthEstimationAlgorithmInterface* bandwidth_estimation,
                                                  uint32_t duration_ms,
                                                  AdaptionProfile profile) = 0;
    virtual VideoAdaptationAlgoType GetVideoAdaptationAlgoType() const = 0;
    virtual std::string GetSwitchReason() = 0;
    virtual const char* GetDetailSwitchReason() = 0;
    virtual void UpdateConfig(RateAdaptConfig& rate_config, RateAdaptConfigA1& rate_config_a1) = 0;
    virtual void SetHistoryData(const std::string& history_data) = 0;
    virtual std::string GetHistoryData() = 0;
    virtual uint32_t GetManualAutoState() = 0;
};

}
}
