#ifndef AWESOME_CACHE_PLAYER_STATISTIC_H
#define AWESOME_CACHE_PLAYER_STATISTIC_H value

#include "player_statistic_c.h"

#include <mutex>
#include <functional>
#include <list>

struct PlayerStatistic {
    int64_t video_cache_duration_ms = 0;
    int64_t video_cache_bytes = 0;

    int64_t audio_cache_duration_ms = 0;
    int64_t audio_cache_bytes = 0;

    int64_t read_position_bytes = 0;

    uint64_t last_updated_time = 0;

    // Used by vod p2sp datasource
    int64_t pre_read_ms = 0;
    int bitrate = 0;

    // Use list so that each listener can easily be removed
    std::mutex listeners_mtx;
    std::list<std::function<void(PlayerStatistic const*)>> listeners;
    using listeners_iterator_t = typename decltype(listeners)::iterator;

    // Remember to call remove_listener!
    listeners_iterator_t add_listener(std::function<void(PlayerStatistic const*)> listener) {
        std::lock_guard<std::mutex> lock(listeners_mtx);
        this->listeners.push_front(listener);
        return this->listeners.begin();
    }

    void remove_listener(listeners_iterator_t it) {
        std::lock_guard<std::mutex> lock(listeners_mtx);
        this->listeners.erase(it);
    }
};

#endif /* ifndef AWESOME_CACHE_PLAYER_STATISTIC_H */
