//
//  speed_calculator.cpp
//  IJKMediaFramework
//
//  Created by MarshallShuai on 2018/11/3.
//  Copyright © 2018 kuaishou. All rights reserved.
//

#include "ac_log.h"
#include "speed_calculator.h"
#include "utility.h"

namespace kuaishou {
namespace cache {

namespace {
static const bool kVerbose = true;
}

SpeedCalculator::SpeedCalculator() : last_download_bytes_(0),
    last_ts_ms_(0),
    last_speed_kbps_(0),
    last_avg_speed_kbps_(0),
    last_avg_speed_kbps_before_stop_(0),
    last_avg_speed_on_stop_valid_(false),
    sample_cnt_(0),
    stopped_(false) {
    avg_sampler_kbps_ = std::make_shared<AvgSampler>();
}

int SpeedCalculator::Update(int64_t downloaded_bytes) {
    int64_t now = kpbase::SystemUtil::GetCPUTime();
    if (last_ts_ms_ == 0) {
        last_download_bytes_ = downloaded_bytes;
        last_ts_ms_ = now;
        return 0;
    } else if (now - last_ts_ms_ < kThrottleIntervalMs) {
        return last_speed_kbps_;
    } else {
        last_speed_kbps_ =
            static_cast<int>((downloaded_bytes - last_download_bytes_) /
                             (now - last_ts_ms_));
        last_speed_kbps_ *= 8; // bytes -> bits

//        LOG_DEBUG("SpeedCalculator::Update, downloaded_bytes:%lld diffBytes:%lld, diffMs:%lldms,last_speed_kbps_:%d",
//                  downloaded_bytes, downloaded_bytes - last_download_bytes_, now - last_ts_ms_, last_speed_kbps_);
        last_download_bytes_ = downloaded_bytes;
        last_ts_ms_ = now;

        last_avg_speed_kbps_ = avg_sampler_kbps_->AddSample(last_speed_kbps_);
        sample_cnt_++;
        if (!stopped_) {
            last_avg_speed_kbps_before_stop_ = last_avg_speed_kbps_;
        }

        return last_speed_kbps_;
    }

}

bool SpeedCalculator::IsStoped() {
    return stopped_;
}

void SpeedCalculator::Stop() {
    stopped_ = true;
    last_avg_speed_kbps_before_stop_ = last_avg_speed_kbps_;
    last_avg_speed_on_stop_valid_ = sample_cnt_ >= VALID_SAMPLE_MIN_CNT;

    if (kVerbose) {
        if (last_avg_speed_on_stop_valid_) {
            LOG_DEBUG("[dccAlg] IsMarkValid = true");
        } else {
            LOG_INFO("[dccAlg] IsMarkValid = false, sample_cnt on SpeedCalculator::Stop:%d", sample_cnt_);
        };
    }
}

bool SpeedCalculator::IsMarkValid() {
    bool valid;

    if (last_avg_speed_kbps_) {
        valid = true;
    } else {
        valid = sample_cnt_ >= VALID_SAMPLE_MIN_CNT;
    }

    return valid;
}

int SpeedCalculator::GetCurrentSpeedKbps() {
    return last_speed_kbps_;
}

int SpeedCalculator::GetAvgSpeedKbps() {
    return last_avg_speed_kbps_;
}

int SpeedCalculator::GetMarkSpeedKbps() {
    return last_avg_speed_kbps_before_stop_;
}

int SpeedCalculator::GetSampleCnt() {
    return sample_cnt_;
}

}
}

