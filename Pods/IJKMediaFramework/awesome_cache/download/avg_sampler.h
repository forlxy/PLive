//
//  avg_sampler.hpp
//  IJKMediaFramework
//
//  Created by MarshallShuai on 2018/11/3.
//  Copyright © 2018 kuaishou. All rights reserved.
//
#pragma once

#include <stdio.h>

namespace kuaishou {
namespace cache {
class AvgSampler {
  public:
    AvgSampler();

    int AddSample(int val);
  private:

    static const int kCapacity = 30;

    typedef struct {
        int64_t timestamp_ms;
        int val;
    } Sample;

    Sample samples_[kCapacity];
    int first_index_;
    int count_;

    int last_avg_val_;
};
}
}

