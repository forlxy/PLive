#include <cassert>
#include "video_adaptation_algorithm_interface.h"
#include "video_adaptation_v2.h"

namespace kuaishou {
namespace abr {

VideoAdaptationAlgorithmInterface* VideoAdaptationAlgorithmInterface::Create(const RateAdaptConfig& rate_adapt_config, const RateAdaptConfigA1& rate_adapt_config_a1) {
    switch (rate_adapt_config.rate_addapt_type) {
        case kBandwidthBased:
            return new VideoAdaptationV2(rate_adapt_config, rate_adapt_config_a1);
            break;
        default:
            return new VideoAdaptationV2(rate_adapt_config, rate_adapt_config_a1);
            break;
    }
}

}
}
