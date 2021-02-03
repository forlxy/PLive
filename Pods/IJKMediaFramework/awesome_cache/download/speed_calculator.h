//
//  speed_calculator.hpp
//  IJKMediaFramework
//
//  Created by MarshallShuai on 2018/11/3.
//  Copyright © 2018 kuaishou. All rights reserved.
//

#pragma once

#include "utility.h"
#include <stdint.h>
#include <memory>
#include "avg_sampler.h"
#include "speed_marker.h"

namespace kuaishou {
namespace cache {
class SpeedCalculator final {
  public:
    SpeedCalculator();

    int Update(int64_t downloaded_bytes);

    // 对于Sync模式，如果上层读取block超过了那么长时间，那么停止测速
    constexpr static int FEED_COST_MS_THRESHOLD_TO_STOP = 50;
    // 对于Async模式，如果两次Open之间超过了这么长时间，那么停止（重启）测速
    constexpr static int REOPEN_COST_MS_THRESHOLD_TO_STOP = 500;

    void Stop();

    bool IsStoped();

    /**
     * 最后一次的瞬时速度
     */
    int GetCurrentSpeedKbps();

    /**
     * 最后一次平均速度（不一定代表带宽，因为可能被限速了）
     */
    int GetAvgSpeedKbps();

    /**
     * 对带宽的估算结果
     */
    int GetMarkSpeedKbps();

    bool IsMarkValid();

    int GetSampleCnt();

  private:
    static const int kThrottleIntervalMs = 200;

    std::shared_ptr<AvgSampler> avg_sampler_kbps_;

    int64_t last_ts_ms_;
    int64_t last_download_bytes_;

    int last_speed_kbps_;
    int last_avg_speed_kbps_;

    int last_avg_speed_kbps_before_stop_;
    bool last_avg_speed_on_stop_valid_;

    // 统计收到的sample个数
    int sample_cnt_;
    /**
     * sample数少于这个个数，测测速不可信
     */
    static const int VALID_SAMPLE_MIN_CNT = 3;

    bool stopped_;
};
}
}

