//
//  sample_info_queue.h
//  IJKMediaFramework
//
//  Created by zhouchao on 2018/3/30.
//  Copyright © 2018年 kuaishou. All rights reserved.
//

#pragma once
#include<deque>
#include<vector>
#include <functional>
#include "abr_types.h"
namespace kuaishou {
namespace abr {

template<typename T>
class SampleInfoQueue {
  public:
    typedef struct Info {
        uint64_t begin_timestamp;
        uint64_t end_timestamp;
        T data;
    } Info;
    using InfoQueueItr = typename std::deque<Info>::iterator;

    SampleInfoQueue(uint64_t short_keep_interval, uint32_t long_keep_interval, uint64_t short_keep_interval_a1, uint32_t long_keep_interval_a1)
        : short_keep_interval_(short_keep_interval)
        , long_keep_interval_(long_keep_interval)
        , short_keep_interval_a1_(short_keep_interval_a1)
        , long_keep_interval_a1_(long_keep_interval_a1) {}

    void set_short_keep_interval(uint32_t keep_interval) {
        short_keep_interval_ = keep_interval;
        return;
    }

    void set_long_keep_interval(uint32_t keep_interval) {
        long_keep_interval_ = keep_interval;
        return;
    }

    uint32_t short_keep_interval() const {
        return short_keep_interval_;
    }

    uint32_t long_keep_interval() const {
        return long_keep_interval_;
    }

    void set_short_keep_interval_a1_(uint32_t keep_interval) {
        short_keep_interval_a1_ = keep_interval;
        return;
    }

    void set_long_keep_interval_a1(uint32_t keep_interval) {
        long_keep_interval_a1_ = keep_interval;
        return;
    }

    uint32_t short_keep_interval_a1() const {
        return short_keep_interval_a1_;
    }

    uint32_t long_keep_interval_a1() const {
        return long_keep_interval_a1_;
    }

    /*
        std::vector<Info&> AffectedItems(uint64_t timestamp, uint32_t interval) {
            std::vector<Info&> affected_item_vector;
            if (Empty()) {
                return affected_item_vector;
            }
            auto it = sample_info_queue_.begin();
            if (it->begin_timestamp > timestamp) {
                return affected_item_vector;
            } else if (it->begin_timestamp <= timestamp && it->end_timestamp >= timestamp) {
                affected_item_vector.push(*it);
            } else {
                auto pre = it;
                it++;
                for (; it != sample_info_queue_.end(); it++) {
                    if (it->begin_timestamp <= timestamp && it->end_timestamp >= timestamp) {
                        affected_item_vector.push(*it);
                    } else if (it->begin_timestamp > timestamp) {
                        if (pre->end_timestamp < timestamp) {
                            affected_item_vector.push(*pre);
                        }
                        break;
                    }
                    pre = it;
                }
                return affected_item_vector;
            }
        }
    */
    size_t ShortInfoQueueSize() const {
        return short_sample_info_queue_.size();
    }

    size_t LongInfoQueueSize() const {
        return long_sample_info_queue_.size();
    }

    bool ShortInfoQueueEmpty() const {
        return short_sample_info_queue_.empty();
    }

    bool LongInfoQueueEmpty() const {
        return long_sample_info_queue_.empty();
    }

    size_t ShortInfoQueueSizeA1() const {
        return short_sample_info_queue_a1_.size();
    }

    size_t LongInfoQueueSizeA1() const {
        return long_sample_info_queue_a1_.size();
    }

    bool ShortInfoQueueEmptyA1() const {
        return short_sample_info_queue_a1_.empty();
    }

    bool LongInfoQueueEmptyA1() const {
        return long_sample_info_queue_a1_.empty();
    }

    void ShortInfoQueuePush(Info&& info) {
        ShortInfoQueueOnPush(info.data);
        if (ShortInfoQueueEmpty()) {
            short_sample_info_queue_.emplace_back(info);
            return;
        }
        for (auto it = short_sample_info_queue_.rbegin(); it != short_sample_info_queue_.rend(); it++) {
            if (it->begin_timestamp <= info.begin_timestamp) {
                short_sample_info_queue_.insert(it.base(), info);
                return;
            }
        }
        short_sample_info_queue_.emplace_front(info);
    }

    void LongInfoQueuePush(Info&& info) {
        LongInfoQueueOnPush(info.data);
        if (LongInfoQueueEmpty()) {
            long_sample_info_queue_.emplace_back(info);
            return;
        }
        for (auto it = long_sample_info_queue_.rbegin(); it != long_sample_info_queue_.rend(); it++) {
            if (it->begin_timestamp <= info.begin_timestamp) {
                long_sample_info_queue_.insert(it.base(), info);
                return;
            }
        }
        long_sample_info_queue_.emplace_front(info);
    }

    void ShortInfoQueuePushA1(Info&& info) {
        ShortInfoQueueOnPushA1(info.data);
        if (ShortInfoQueueEmptyA1()) {
            short_sample_info_queue_a1_.emplace_back(info);
            return;
        }
        for (auto it = short_sample_info_queue_a1_.rbegin(); it != short_sample_info_queue_a1_.rend(); it++) {
            if (it->begin_timestamp <= info.begin_timestamp) {
                short_sample_info_queue_a1_.insert(it.base(), info);
                return;
            }
        }
        short_sample_info_queue_a1_.emplace_front(info);
    }

    void LongInfoQueuePushA1(Info&& info) {
        LongInfoQueueOnPushA1(info.data);
        if (LongInfoQueueEmptyA1()) {
            long_sample_info_queue_a1_.emplace_back(info);
            return;
        }
        for (auto it = long_sample_info_queue_a1_.rbegin(); it != long_sample_info_queue_a1_.rend(); it++) {
            if (it->begin_timestamp <= info.begin_timestamp) {
                long_sample_info_queue_a1_.insert(it.base(), info);
                return;
            }
        }
        long_sample_info_queue_a1_.emplace_front(info);
    }

    void ShortInfoQueueRemoveTillKeep(uint64_t now) {
        if (ShortInfoQueueEmpty()) {
            return;
        }
        for (auto it = short_sample_info_queue_.begin(); it != short_sample_info_queue_.end();) {
            if (now - it->begin_timestamp <= short_keep_interval_) {
                break;
            }
            ShortInfoQueueOnRemove(it->data);
            it = short_sample_info_queue_.erase(it);
        }
        return;
    }

    void LongInfoQueueRemoveTillKeep(uint64_t now) {
        if (LongInfoQueueEmpty()) {
            return;
        }
        for (auto it = long_sample_info_queue_.begin(); it != long_sample_info_queue_.end();) {
            if (now - it->begin_timestamp <= long_keep_interval_) {
                break;
            }
            LongInfoQueueOnRemove(it->data);
            it = long_sample_info_queue_.erase(it);
        }
        auto it = long_sample_info_queue_.begin();
        while (long_sample_info_queue_.size() > 10) {
            LongInfoQueueOnRemove(it->data);
            it = long_sample_info_queue_.erase(it);
        }
        return;
    }

    void ShortInfoQueueRemoveTillKeepA1(uint64_t now) {
        if (ShortInfoQueueEmptyA1()) {
            return;
        }
        for (auto it = short_sample_info_queue_a1_.begin(); it != short_sample_info_queue_a1_.end();) {
            if (now - it->begin_timestamp <= short_keep_interval_a1_) {
                break;
            }
            ShortInfoQueueOnRemoveA1(it->data);
            it = short_sample_info_queue_a1_.erase(it);
        }
        return;
    }

    void LongInfoQueueRemoveTillKeepA1(uint64_t now) {
        if (LongInfoQueueEmptyA1()) {
            return;
        }
        for (auto it = long_sample_info_queue_a1_.begin(); it != long_sample_info_queue_a1_.end();) {
            if (now - it->begin_timestamp <= long_keep_interval_a1_) {
                break;
            }
            LongInfoQueueOnRemoveA1(it->data);
            it = long_sample_info_queue_a1_.erase(it);
        }
        auto it = long_sample_info_queue_a1_.begin();
        while (long_sample_info_queue_a1_.size() > 8) {
            LongInfoQueueOnRemoveA1(it->data);
            it = long_sample_info_queue_a1_.erase(it);
        }
        return;
    }

    virtual void ShortInfoQueueOnPush(const T&) {}
    virtual void ShortInfoQueueOnRemove(const T&) {}
    virtual void LongInfoQueueOnPush(const T&) {}
    virtual void LongInfoQueueOnRemove(const T&) {}
    virtual void ShortInfoQueueOnPushA1(const T&) {}
    virtual void ShortInfoQueueOnRemoveA1(const T&) {}
    virtual void LongInfoQueueOnPushA1(const T&) {}
    virtual void LongInfoQueueOnRemoveA1(const T&) {}
  protected:
    std::deque<Info> short_sample_info_queue_;
    std::deque<Info> long_sample_info_queue_;
    uint64_t short_keep_interval_ = 0;
    uint64_t long_keep_interval_ = 0;
    std::deque<Info> short_sample_info_queue_a1_;
    std::deque<Info> long_sample_info_queue_a1_;
    uint64_t short_keep_interval_a1_ = 0;
    uint64_t long_keep_interval_a1_ = 0;
};

}
}
