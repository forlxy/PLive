#ifndef AWESOME_CACHE_C_PLAYER_STATISTIC_H
#define AWESOME_CACHE_C_PLAYER_STATISTIC_H

#include <stdint.h>
#include "hodor_config.h"

#ifdef __cplusplus
extern "C" {
#endif


/**
 * Some statistics provided by player for awesome cache, to help schedule downloading progress
 */
typedef struct PlayerStatistic* ac_player_statistic_t;

HODOR_EXPORT ac_player_statistic_t ac_player_statistic_create();
HODOR_EXPORT void ac_player_statistic_destroy(ac_player_statistic_t* s);

HODOR_EXPORT void ac_player_statistic_set_audio_buffer(ac_player_statistic_t s, int64_t duration_ms, int64_t bytes);
HODOR_EXPORT void ac_player_statistic_set_video_buffer(ac_player_statistic_t s, int64_t duration_ms, int64_t bytes);
HODOR_EXPORT void ac_player_statistic_set_bitrate(ac_player_statistic_t s, int bitrate);
HODOR_EXPORT void ac_player_statistic_set_pre_read(ac_player_statistic_t s, int64_t pre_read_ms);
HODOR_EXPORT void ac_player_statistic_set_read_position(ac_player_statistic_t s, int64_t pos_bytes);
HODOR_EXPORT void ac_player_statistic_update(ac_player_statistic_t s);


#ifdef __cplusplus
}
#endif

#endif
