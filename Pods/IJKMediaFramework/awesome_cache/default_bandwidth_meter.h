//
//  default_bandwidth_meter.h
//  IJKMediaPlayer
//
//  Created by wangtao03 on 2017/12/12.
//  Copyright © 2017年 kuaishou. All rights reserved.
//
#pragma once

#include <stdint.h>
#include <mutex>
#include <thread>
#include <chrono>
#include "assert.h"
#include <cmath>
#include "bandwidth_meter.h"
#include "transfer_listener.h"
#include "sliding_percentile.h"
#include "ac_log.h"
#include "abr/abr_engine.h"
#include "abr/abr_types.h"


namespace kuaishou {
namespace cache {

static const int32_t kDefaultMaxWeight = 2000;
static const int32_t kElapsedMillisForEstimate = 2000;
static const int32_t kBytesTransferredForEstimate = 512 * 1024;
static const int32_t kVideoPacketSize = 512 * 1024;

template<typename S>
class DefaultBandwidthMeter : public BandwidthMeter, public TransferListener<S> {
  public:
    DefaultBandwidthMeter(std::shared_ptr<EventListener> event_listener) :
        DefaultBandwidthMeter(event_listener, kDefaultMaxWeight) {

    }
    DefaultBandwidthMeter(std::shared_ptr<EventListener> event_listener, int32_t max_weight) :
        event_listener_(event_listener),
#ifdef SLIDING_PERCENTILE
        sliding_percentile_(std::make_shared<kuaishou::cache::SlidingPercentile>(max_weight)),
#endif
        bitrate_estimate_(-1),
        sample_bytes_transferred_(0),
        total_elapsed_time_ms_(0),
        total_bytes_transferred_(0) {
    }
  public:
    virtual int32_t GetBitrateEstimate() override {
        std::lock_guard<std::mutex> lg(cache_mutex_);
        return bitrate_estimate_;
    }

    virtual void OnTransferStart(S* source, const DataSpec& spec) override {
        std::lock_guard<std::mutex> lg(cache_mutex_);
        sample_start_time_ms_ = kpbase::SystemUtil::GetCPUTime();
        LOG_ERROR_DETAIL("[DefaultBandwidthMeter]:OnTransferStart, sample_start_time_ms_=%llu", sample_start_time_ms_);
        sample_bytes_transferred_ = 0;
    }

    virtual void OnBytesTransfered(S* source, int64_t byte_transfered) override {
        std::lock_guard<std::mutex> lg(cache_mutex_);

        if (sample_bytes_transferred_ == 0) {
            LOG_DEBUG("[DefaultBandwidthMeter]:OnBytesTransfered, transfer_start_time_ms_=%llu", kpbase::SystemUtil::GetCPUTime());
        }

        sample_bytes_transferred_ += byte_transfered;

        if (sample_bytes_transferred_ >= kVideoPacketSize) {
            uint64_t now_ms = kpbase::SystemUtil::GetCPUTime();
            uint64_t sample_elapsed_time_ms = now_ms - sample_start_time_ms_;

            total_bytes_transferred_ += sample_bytes_transferred_;
            total_elapsed_time_ms_ += sample_elapsed_time_ms;

            if (sample_elapsed_time_ms > 0) {
#ifdef SLIDING_PERCENTILE
                float bits_per_second = (sample_bytes_transferred_ * 8000) / sample_elapsed_time_ms;
                sliding_percentile_->AddSample((int32_t)sqrt(sample_bytes_transferred_), bits_per_second);
                if (total_elapsed_time_ms_ >= kElapsedMillisForEstimate || total_bytes_transferred_ >= kBytesTransferredForEstimate) {
                    float bitrate_estimate_float = sliding_percentile_->GetPercentile(0.5f);
                    bitrate_estimate_ = isnan(bitrate_estimate_float) ? -1 : (int32_t)bitrate_estimate_float;
                }
#endif
            } else {
                LOG_ERROR("[DefaultBandwidthMeter]: OnBytesTransfered Error, sample_elapsed_time_ms=%d\n", sample_elapsed_time_ms);
                return;
            }

#ifdef SLIDING_PERCENTILE
            NotifyBandwidthSample(sample_elapsed_time_ms, sample_bytes_transferred_, bitrate_estimate_);
#endif
            sample_start_time_ms_ = now_ms;
            sample_bytes_transferred_ = 0;
        }
    }

    virtual void OnTransferEnd(S* source) override {
        // fix me: nothing needed here for long-time http-connection
        sample_bytes_transferred_ = 0;
    }

  private:
    void NotifyBandwidthSample(int32_t elapsed_ms, int32_t bytes, int32_t bitrate) {
        if (event_listener_ != nullptr) {
            event_listener_->OnBandwidthSample(elapsed_ms, bytes, bitrate);
        }
    }


  private:
    const std::shared_ptr<EventListener> event_listener_;
#ifdef SLIDING_PERCENTILE
    const std::shared_ptr<kuaishou::cache::SlidingPercentile> sliding_percentile_;
#endif
    int32_t bitrate_estimate_;
    std::mutex cache_mutex_;
    uint64_t sample_start_time_ms_;
    uint64_t total_elapsed_time_ms_;
    int64_t sample_bytes_transferred_;
    int64_t total_bytes_transferred_;
};

} //cache
} //kuaishou
