#include "./player_statistic.h"
#include "./ac_datasource.h"
#include "utility.h"

#include <string.h>

ac_player_statistic_t ac_player_statistic_create() {
    return new PlayerStatistic();
}

void ac_player_statistic_destroy(ac_player_statistic_t* s) {
    if (s && *s) {
        delete *s;
        *s = nullptr;
    }
}

void ac_player_statistic_update(ac_player_statistic_t s) {
    if (!s)
        return;

    // 下面的代码是线程不安全的，但是我并不希望在这里加锁引入开销
    // 不加锁最坏情况只是偶尔多调用一次UpdatePlayerStatistic（况且很有可能updatePlayerStatistic什么都没做）
    // datasource如果需要的话应该自己做线程安全保护
    uint64_t now = kuaishou::kpbase::SystemUtil::GetEpochTime();
    if (now - s->last_updated_time < 100  // max update frequency: 10Hz
        // Only limit the update frequency if cache duration/bytes is not zero
        // So that we would not miss any updates when the player blocks
        && s->video_cache_bytes > 0 && s->video_cache_duration_ms > 0
        && s->audio_cache_bytes > 0 && s->audio_cache_duration_ms > 0)
        return;

    s->last_updated_time = now;

    std::lock_guard<std::mutex> lock(s->listeners_mtx);
    for (auto& listener : s->listeners) {
        listener(s);
    }
}

void ac_player_statistic_set_audio_buffer(ac_player_statistic_t s, int64_t duration_ms, int64_t bytes) {
    if (!s)
        return;

    s->audio_cache_duration_ms = duration_ms;
    s->audio_cache_bytes = bytes;
}

void ac_player_statistic_set_video_buffer(ac_player_statistic_t s, int64_t duration_ms, int64_t bytes) {
    if (!s)
        return;

    s->video_cache_duration_ms = duration_ms;
    s->video_cache_bytes = bytes;
}

void ac_player_statistic_set_bitrate(ac_player_statistic_t s, int bitrate) {
    if (!s)
        return;

    s->bitrate = bitrate;
}

void ac_player_statistic_set_pre_read(ac_player_statistic_t s, int64_t pre_read_ms) {
    if (!s)
        return;

    s->pre_read_ms = pre_read_ms;
}

void ac_player_statistic_set_read_position(ac_player_statistic_t s, int64_t pos_bytes) {
    if (!s)
        return;

    s->read_position_bytes = pos_bytes;
}
