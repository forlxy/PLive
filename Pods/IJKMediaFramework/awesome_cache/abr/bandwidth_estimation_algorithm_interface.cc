#include <cassert>
#include "bandwidth_estimation_algorithm_interface.h"
#include "moving_average_bandwidth_estimation.h"

namespace kuaishou {
namespace abr {

BandwidthEstimationAlgorithmInterface*
BandwidthEstimationAlgorithmInterface::Create(const RateAdaptConfig& rate_adapt_config, const RateAdaptConfigA1& rate_adapt_config_a1) {
    switch (rate_adapt_config.bandwidth_estimation_type) {
        case kMovingAverage:
            return new MovingAverageBandwidthEstimation(rate_adapt_config, rate_adapt_config_a1);
            break;
        default:
            return new MovingAverageBandwidthEstimation(rate_adapt_config, rate_adapt_config_a1);
            break;
    }
}

}
}
