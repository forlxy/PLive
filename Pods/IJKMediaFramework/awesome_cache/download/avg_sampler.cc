//
//  avg_sampler.cpp
//  IJKMediaFramework
//
//  Created by MarshallShuai on 2018/11/3.
//  Copyright © 2018 kuaishou. All rights reserved.
//

#include <algorithm>
#include <utility.h>
#include "ac_log.h"
#include "avg_sampler.h"

namespace kuaishou {
namespace cache {

AvgSampler::AvgSampler() :
    first_index_(0),
    count_(0),
    last_avg_val_(0) {}

int AvgSampler::AddSample(int val) {
    int64_t now = kpbase::SystemUtil::GetCPUTime();

    int next_index = 0;
    if (count_ == 0) {
        samples_[count_].timestamp_ms = now;
        samples_[count_].val = val;

        count_++;
        last_avg_val_ = val;
        return last_avg_val_;
    } else {
        next_index = (first_index_ + count_) % kCapacity;
    }

    samples_[next_index].timestamp_ms = now;
    samples_[next_index].val = val;

    if (count_ < kCapacity) {
        count_++;
    } else {
        first_index_ = (first_index_ + 1) % kCapacity;
    }

    int sum_kbps = 0;
    for (int i = 0; i < count_; i++) {
        sum_kbps += samples_[i].val;
    }
    last_avg_val_ = sum_kbps / count_;

    return last_avg_val_;
}


}
}

