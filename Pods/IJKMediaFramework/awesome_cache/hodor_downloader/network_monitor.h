/**
 * Created by MarshallShuai on 2019-10-23.
 *
 */
#pragma once

#include <string>
#include <mutex>
#include <list>
#include "utils/macro_util.h"
#include "multi_priority.h"

HODOR_NAMESPACE_START

/**
 *
 * 这个类主要是负责HodorDownloader的网络测速。
 * 目前主要的用途是给预加载提供网速参考
 * 长期来说，希望以后的Hodor输出的网速都能从这个类中输出
 *
 * 目前策略：
 * 1.一定时间内（5分钟）的网速会有效，只取最后3次的采样值的平均值
 * 2.如果切网(onNetworkChanged)了，之前统计的网速都无效
 */
class NetworkMonitor {
  public:
    struct SpeedSample {
        SpeedSample(int64_t bytes, int64_t cost_ms, int64_t timestamp_ms): bytes_(bytes), cost_ms_(cost_ms), timestamp_ms_(timestamp_ms) {
            if (cost_ms <= 0) {
                speed_kbps_ = 0;
            } else {
                speed_kbps_ = bytes * 8 * 1000 / cost_ms;
            }
        };
        /**
         * @return kilo bit per second
         */
        const int64_t GetSpeedKpbs() {
            return speed_kbps_;
        }
        const int64_t bytes_{};
        const int64_t cost_ms_{};
        const int64_t timestamp_ms_{};

        int64_t speed_kbps_;
    };
    /**
     * 获取当前一个比较可信的网速
     * @param current_ts_ms 当前的时间戳，这里没封装到内部，是为了更好的单元测试，外部调用的时候使用默认参数值即可
     * @return 如果当前没有测出稳定的带宽，则返回-1，否则返回测出来的网速
     */
    int64_t GetNetSpeedKbps(int64_t current_ts_ms = GetTimestampMs());

    /**
     * 网络状态变化后需要调用此函数
     */
    void OnNetworkChanged();

    /**
     * PS：内部小于512K的下载都会被过滤掉，因为太小的字节数下载因为tcp爬坡的因素，会导致测速偏小
     * @param downloaded_bytes 下载的字节数
     * @param transfer_cost_ms 下载消耗的时长
     * @param timestampe_ms 下载场景发生的时间戳
     */
    void AddSpeedSample(int64_t downloaded_bytes, int32_t transfer_cost_ms,
                        int64_t timestampe_ms = NetworkMonitor::GetTimestampMs());

    /**
     * NetSpeed内部的时间戳都应该用这个函数来生成，保持一致性
     * @return
     */
    static int64_t GetTimestampMs();

    /**
     * 这个接口是单元测试使用，所以也不加锁，即使正式逻辑中使用，加锁意义也不大
     * @return 当前sample count
     */
    size_t GetSampleCount() {
        return sample_list_.size();
    }

    // 为了单元测试需求，下面几个常量暴露出来

    // 至少要有 count 次计数才会产生比较稳定的网速，默认是3次
    static const size_t MAX_SAMPLE_COUNT = 3;
    // 内部小于512K的下载都会被过滤掉，因为太小的字节数下载因为tcp爬坡的因素，会导致测速偏小
    static const int64_t MIN_VALID_DOWNLOAD_BYTES = 512 * 1024;
    // 网速sample超过以下时长后自动过时，目前暂定5分钟
    static const int SPEED_SAMPLE_TIMEOUT_INTERVAL_MS = 5 * 60 * 1000;
  private:
    /**
     * 清空状态
     */
    void Reset();

  private:

    std::mutex mutex_;
    std::list<SpeedSample> sample_list_;
};


HODOR_NAMESPACE_END

