//
// Created by MarshallShuai on 2019-10-23.
//

#include "network_monitor.h"
#include "ac_log.h"
#include <utility.h>

HODOR_NAMESPACE_START

static constexpr bool kVerbose = true;

const size_t NetworkMonitor::MAX_SAMPLE_COUNT;
const int64_t NetworkMonitor::MIN_VALID_DOWNLOAD_BYTES;
const int NetworkMonitor::SPEED_SAMPLE_TIMEOUT_INTERVAL_MS;

int64_t NetworkMonitor::GetNetSpeedKbps(int64_t current_ts_ms) {
    std::lock_guard<std::mutex> lg(mutex_);
    if (sample_list_.size() < MAX_SAMPLE_COUNT) {
        return -1;
    }

    int valid_sample = 0;
    int64_t sum_speed = 0;
    for (auto iter = sample_list_.begin(); iter != sample_list_.end();) {
        if (current_ts_ms - (*iter).timestamp_ms_ >= SPEED_SAMPLE_TIMEOUT_INTERVAL_MS) {
            iter = sample_list_.erase(iter);
            if (kVerbose) {
                LOG_DEBUG("[ NetworkMonitor::GetNetSpeedKbps] drop timeout sample");
            }
        } else {
            sum_speed += (*iter).GetSpeedKpbs();
            valid_sample++;
            iter++;
        }
    }
    if (valid_sample == MAX_SAMPLE_COUNT) {
        return sum_speed / valid_sample;
    } else {
        return -1;
    }
}

void NetworkMonitor::OnNetworkChanged() {
    // 目前的实现只需要reset状态即可
    Reset();
}

void NetworkMonitor::AddSpeedSample(int64_t downloaded_bytes, int32_t transfer_cost_ms, int64_t timestampe_ms) {
    if (downloaded_bytes < MIN_VALID_DOWNLOAD_BYTES) {
        LOG_DEBUG(
            "[NetworkMonitor::AddSpeedSample] downloaded_bytes(%lld) < MIN_VALID_DOWNLOAD_BYTES(%lld), drop the sample",
            downloaded_bytes, MIN_VALID_DOWNLOAD_BYTES);
        return;
    }
    if (transfer_cost_ms <= 0) {
        LOG_ERROR("[NetworkMonitor::AddSpeedSample] transfer_cost_ms invalid:%lld (with downloaded_bytes:%lld)",
                  transfer_cost_ms, downloaded_bytes);
        return;
    }

    std::lock_guard<std::mutex> lg(mutex_);
    sample_list_.emplace_back(downloaded_bytes, transfer_cost_ms, timestampe_ms);
    // 保持最后3个记录即可
    if (sample_list_.size() > MAX_SAMPLE_COUNT) {
        sample_list_.pop_front();
    }
}

int64_t NetworkMonitor::GetTimestampMs() {
    return kpbase::SystemUtil::GetCPUTime();
}

void NetworkMonitor::Reset() {
    sample_list_.clear();
}

HODOR_NAMESPACE_END
