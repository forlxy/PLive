//
// Created by MarshallShuai on 2018/11/3.
//

#include <algorithm>
#include <ostream>
#include <sstream>
#include "ac_log.h"
#include "speed_marker.h"

namespace kuaishou {
namespace cache {
SpeedMarker::SpeedMarker(): last_avg_kbps_(0),
    count_(0) {

}

int SpeedMarker::AddSample(int kbps) {
    if (count_ < kCapacity) {
        kbps_samples_[count_] = kbps;
        count_++;
        if (count_ == kCapacity) {
            // do the first sort
            std::sort(kbps_samples_, kbps_samples_ + kCapacity);
        }
    } else {
        if (kbps > kbps_samples_[0]) {
            kbps_samples_[0] = kbps;
            std::sort(kbps_samples_, kbps_samples_ + kCapacity);
        }
    }


    std::stringstream debugArray;

    int sum_kbps = 0;
    for (int i = 0; i < count_; i++) {
        sum_kbps += kbps_samples_[i];
        if (kVerbose) {
            debugArray << kbps_samples_[i] << ",";
        }
    }
    last_avg_kbps_ = sum_kbps / count_;

    if (kVerbose) {
        LOG_DEBUG("SpeedMarker, count:%d, array:%s", count_, debugArray.str().c_str());
    }

    return last_avg_kbps_;
}

int SpeedMarker::GetMarkKbps() {
    return last_avg_kbps_;
}
}
}
