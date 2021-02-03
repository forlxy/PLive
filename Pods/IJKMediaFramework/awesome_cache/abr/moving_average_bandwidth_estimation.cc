#include "moving_average_bandwidth_estimation.h"
#include "ac_log.h"

namespace kuaishou {
namespace abr {

MovingAverageBandwidthEstimation::MovingAverageBandwidthEstimation(const RateAdaptConfig& rate_adapt_config, const RateAdaptConfigA1& rate_adapt_config_a1)
    : short_total_bandwidth_(0)
    , long_total_bandwidth_(0)
    , short_bandwidth_kbps_(0)
    , long_bandwidth_kbps_(0)
    , rate_adapt_config_(rate_adapt_config)
    , rate_adapt_config_a1_(rate_adapt_config_a1)
    , real_bandwidth_kbps_(0)
    , SampleInfoQueue<BandwidthSample>(rate_adapt_config.short_keep_interval, rate_adapt_config.long_keep_interval, rate_adapt_config_a1.short_keep_interval, rate_adapt_config_a1.long_keep_interval) {}   //interval set from config

void MovingAverageBandwidthEstimation::UpdateDownloadInfo(DownloadSampleInfo& info) {
    if (info.begin_timestamp >= info.end_timestamp ||
        (info.end_timestamp - info.begin_timestamp) <= kMinTransmittedTime ||
        info.total_bytes <= kMinTransmittedSize) {
        return;
    }
    uint64_t duration = info.end_timestamp - info.begin_timestamp;
    real_bandwidth_kbps_ = static_cast<uint32_t>(info.total_bytes * 8 / duration);    //kbps
    LOG_DEBUG("RateAdaptation: begin_time: %llu, end_time: %llu, delta: %llu, bytes: %llu, bandwidth: %d \n", info.begin_timestamp, info.end_timestamp, duration, info.total_bytes, real_bandwidth_kbps_);
    SampleInfoQueue::ShortInfoQueuePush(Info{info.begin_timestamp, info.end_timestamp, BandwidthSample{real_bandwidth_kbps_, rate_adapt_config_.default_weight}});
    SampleInfoQueue::LongInfoQueuePush(Info{info.begin_timestamp, info.end_timestamp, BandwidthSample{real_bandwidth_kbps_, rate_adapt_config_.default_weight}});
    SampleInfoQueue::ShortInfoQueuePushA1(Info{info.begin_timestamp, info.end_timestamp, BandwidthSample{real_bandwidth_kbps_, rate_adapt_config_.default_weight}});
    SampleInfoQueue::LongInfoQueuePushA1(Info{info.begin_timestamp, info.end_timestamp, BandwidthSample{real_bandwidth_kbps_, rate_adapt_config_.default_weight}});
}

uint32_t MovingAverageBandwidthEstimation::ShortTermBandwidthEstimate(uint32_t algorithm_mode) {
    uint64_t now_ms = kpbase::SystemUtil::GetCPUTime();
    if (algorithm_mode == 0) {
        ShortInfoQueueRemoveTillKeep(now_ms);
        if (short_sample_info_queue_.size() == 0) {
            short_bandwidth_kbps_ = 0;
            return 0;
        }
        short_bandwidth_kbps_ = (uint32_t)(short_total_bandwidth_ / short_sample_info_queue_.size());
        return short_bandwidth_kbps_;
    } else {
        ShortInfoQueueRemoveTillKeepA1(now_ms);
        if (short_sample_info_queue_a1_.size() == 0) {
            short_bandwidth_kbps_a1_ = 0;
            return 0;
        }
        short_bandwidth_kbps_a1_ = (uint32_t)(short_total_bandwidth_a1_ / short_sample_info_queue_a1_.size());
        return short_bandwidth_kbps_a1_;
    }
}

uint32_t MovingAverageBandwidthEstimation::LongTermBandwidthEstimate(uint32_t algorithm_mode) {
    uint64_t now_ms = kpbase::SystemUtil::GetCPUTime();
    if (algorithm_mode == 0) {
        LongInfoQueueRemoveTillKeep(now_ms);
        if (long_sample_info_queue_.size() == 0) {
            long_bandwidth_kbps_ = 0;
            return 0;
        }
        long_bandwidth_kbps_ = (uint32_t)(long_total_bandwidth_ / long_sample_info_queue_.size());
        return long_bandwidth_kbps_;
    } else {
        LongInfoQueueRemoveTillKeepA1(now_ms);
        if (long_sample_info_queue_a1_.size() == 0) {
            long_bandwidth_kbps_a1_ = 0;
            return 0;
        }
        long_bandwidth_kbps_a1_ = (uint32_t)(long_total_bandwidth_a1_ / long_sample_info_queue_a1_.size());
        return long_bandwidth_kbps_a1_;
    }
}

void MovingAverageBandwidthEstimation::UpdateBlockInfo(std::vector<uint64_t>& block_info) {
    return;
}

BandwidthEstimationAlgoType MovingAverageBandwidthEstimation::GetBandwidthEstimationAlgoType() const {
    return BandwidthEstimationAlgoType::kMovingAverage;
}

void MovingAverageBandwidthEstimation::ShortInfoQueueOnPush(const BandwidthSample& bandwidth_sample) {
    short_total_bandwidth_ += bandwidth_sample.bandwidth * bandwidth_sample.weight;
}

void MovingAverageBandwidthEstimation::ShortInfoQueueOnRemove(const BandwidthSample& bandwidth_sample) {
    short_total_bandwidth_ -= bandwidth_sample.bandwidth * bandwidth_sample.weight;
}

void MovingAverageBandwidthEstimation::LongInfoQueueOnPush(const BandwidthSample& bandwidth_sample) {
    long_total_bandwidth_ += bandwidth_sample.bandwidth * bandwidth_sample.weight;
}

void MovingAverageBandwidthEstimation::LongInfoQueueOnRemove(const BandwidthSample& bandwidth_sample) {
    long_total_bandwidth_ -= bandwidth_sample.bandwidth * bandwidth_sample.weight;
}

void MovingAverageBandwidthEstimation::ShortInfoQueueOnPushA1(const BandwidthSample& bandwidth_sample) {
    short_total_bandwidth_a1_ += bandwidth_sample.bandwidth * bandwidth_sample.weight;
}

void MovingAverageBandwidthEstimation::ShortInfoQueueOnRemoveA1(const BandwidthSample& bandwidth_sample) {
    short_total_bandwidth_a1_ -= bandwidth_sample.bandwidth * bandwidth_sample.weight;
}

void MovingAverageBandwidthEstimation::LongInfoQueueOnPushA1(const BandwidthSample& bandwidth_sample) {
    long_total_bandwidth_a1_ += bandwidth_sample.bandwidth * bandwidth_sample.weight;
}

void MovingAverageBandwidthEstimation::LongInfoQueueOnRemoveA1(const BandwidthSample& bandwidth_sample) {
    long_total_bandwidth_a1_ -= bandwidth_sample.bandwidth * bandwidth_sample.weight;
}

uint32_t MovingAverageBandwidthEstimation::short_bandwidth_kbps(uint32_t algorithm_mode) {
    if (algorithm_mode == 0) {
        return short_bandwidth_kbps_;
    }
    return short_bandwidth_kbps_a1_;
}

uint32_t MovingAverageBandwidthEstimation::long_bandwidth_kbps(uint32_t algorithm_mode) {
    if (algorithm_mode == 0) {
        return long_bandwidth_kbps_;
    }
    return long_bandwidth_kbps_a1_;
}

uint32_t MovingAverageBandwidthEstimation:: real_time_throughput() {
    return real_bandwidth_kbps_;
}

void MovingAverageBandwidthEstimation::UpdateConfig(RateAdaptConfig& rate_config, RateAdaptConfigA1& rate_config_a1) {
    rate_adapt_config_ = rate_config;
    rate_adapt_config_a1_ = rate_config_a1;
    set_long_keep_interval(rate_adapt_config_.long_keep_interval);
    set_short_keep_interval(rate_adapt_config_.short_keep_interval);
    set_long_keep_interval_a1(rate_adapt_config_a1_.long_keep_interval);
    set_short_keep_interval_a1_(rate_adapt_config_a1_.short_keep_interval);
}

}
}
