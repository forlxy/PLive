//
// Created by MarshallShuai on 2019-10-18.
//

#include "traffic_coordinator.h"
#include "ac_log.h"

HODOR_NAMESPACE_START

static const bool kVerbose = true;

TrafficCoordinator::TrafficCoordinator(NetworkMonitor* network_monitor,
                                       TrafficCoordinator::Listener* listener)
    : network_monitor_(network_monitor), listener_(listener) {
}

void TrafficCoordinator::OnPlayerDownloadStart(std::string key) {
    if (key.empty()) {
        LOG_ERROR("[TrafficCoordinator::OnPlayerDownloadStart] key is empty");
        return;
    }
    if (kVerbose) {
        LOG_DEBUG("[TrafficCoordinator::OnPlayerDownloadStart] key:%s", key.c_str());
    }
    std::lock_guard<std::mutex> lg(mutex_);
    auto insert_ret = key_set_.insert(key);
    if (insert_ret.second > 0 && listener_) {
        current_player_task_count_ = key_set_.size();
        CheckToNotifyListener();
    }
}

void TrafficCoordinator::OnPlayerDownloadFinish(std::string key, bool error_happened) {
    if (key.empty()) {
        LOG_ERROR("[TrafficCoordinator::OnPlayerDownloadStart] key is empty");
        return;
    }
    if (error_happened) {
        LOG_WARN("TrafficCoordinator::OnPlayerDownloadFinish] key:%s, error happened", key.c_str());
    } else if (kVerbose) {
        LOG_DEBUG("TrafficCoordinator::OnPlayerDownloadFinish] key:%s, no error happened", key.c_str());
    }

    std::lock_guard<std::mutex> lg(mutex_);
    auto erase_count = key_set_.erase(key);
    if (!error_happened && erase_count > 0 && listener_) {
        current_player_task_count_ = key_set_.size();
        CheckToNotifyListener();
    }
}

void TrafficCoordinator::OnNetWorkSpeedUpdated() {
    if (!network_monitor_) {
        LOG_WARN("[TrafficCoordinator::OnNetWorkSpeedUpdated] network_monitor_ is null");
        return;
    }

    CheckToNotifyListener();
}

void TrafficCoordinator::SetPreloadSpeedThresholdKbps(int64_t speed_kpbs) {
    preload_speed_th_kbps = speed_kpbs;
}


void TrafficCoordinator::CheckToNotifyListener() {
    if (!listener_ || !enable_) {
        return;
    }
    // 满足网速要求
    auto speed_kpbs = network_monitor_->GetNetSpeedKbps();
    if (//
        // 逻辑2：下发的阈值preload_speed_th_kbps如果<=0，网速阈值这个逻辑，直接进行预加载的
        (speed_kpbs <= 0 || preload_speed_th_kbps <= 0 || speed_kpbs <= preload_speed_th_kbps)
        && current_player_task_count_ <= 0) {
        LOG_VERBOSE("[TrafficCoordinator::CheckToNotifyListener]To ResumePreloadTask, 网速阈值:%.2fMb, speed_kpbs:%.2fMb, player_cnt:%d",
                    preload_speed_th_kbps * 1.f / 1024, speed_kpbs * 1.f / 1024, current_player_task_count_)
        listener_->ResumePreloadTask();
    } else {
        LOG_VERBOSE("[TrafficCoordinator::CheckToNotifyListener]To PausePreloadTask, 网速阈值:%.2fMb, speed_kpbs:%.2fMb, player_cnt:%d",
                    preload_speed_th_kbps * 1.f / 1024,
                    speed_kpbs * 1.f / 1024, current_player_task_count_)
        listener_->PausePreloadTask();
    }
}


HODOR_NAMESPACE_END
