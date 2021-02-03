//
// Created by MarshallShuai on 2019-10-18.
//
#pragma once

#include <string>
#include <mutex>
#include <set>
#include <v2/cache/cache_def_v2.h>
#include "utils/macro_util.h"
#include "multi_priority.h"
#include "network_monitor.h"

HODOR_NAMESPACE_START

class HodorDownloader;

/**
 * 这个类主要是用来协调管理Hodor的下载任何和播放器的高优播放下载任务的
 */
class TrafficCoordinator {
  public:
    class Listener {
      public:
        virtual void ResumePreloadTask() = 0;
        virtual void PausePreloadTask() = 0;
    };

    TrafficCoordinator(NetworkMonitor* monitor, Listener* listener);

    void OnPlayerDownloadStart(std::string key);

    void OnPlayerDownloadFinish(std::string key, bool error_happened);

    /**
     * 预加载需要低于这个阈值才会进行
     * @param speed_kpbs
     */
    void SetPreloadSpeedThresholdKbps(int64_t speed_kpbs);
    int64_t GetPreloadSpeedThresholdKbps() {
        return preload_speed_th_kbps;
    };

    void OnNetWorkSpeedUpdated();

    /**
     * 通过综合各方面条件判断是否要 恢复/暂停预加载任务
     */
    void CheckToNotifyListener();

    size_t CurrentPlayerTaskCount() {
        return current_player_task_count_;
    }

    /**
     *
     * @param enable 使能接口，默认是true的，如果设置为false，则Hodor不再受到Player下载状态影响
     */
    void SetEnable(bool enable) {
        enable_ = enable;
    }

  private:
    std::mutex mutex_;
    size_t current_player_task_count_{};
    std::set<std::string> key_set_;


    Listener* const listener_{};

    NetworkMonitor* network_monitor_;
    int64_t preload_speed_th_kbps = 20 * MB;

    bool enable_ = true;
};

HODOR_NAMESPACE_END
