//
// Created by MarshallShuai on 2018/11/3.
//
#pragma once

#include <stdint.h>

namespace kuaishou {
namespace cache {
class SpeedMarker {
  public:
    SpeedMarker();

    int AddSample(int kbps);

    int GetMarkKbps();

  private:

    static const int kCapacity = 10;
    static const int kVerbose = false;

    int kbps_samples_[kCapacity];
    int count_;

    int last_avg_kbps_;
};
}
}
