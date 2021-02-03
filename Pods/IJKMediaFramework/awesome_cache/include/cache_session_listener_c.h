#pragma once

#include <stdbool.h>
#include "cache_defs.h"

#ifdef __cplusplus
extern "C" {
#endif


typedef struct CCacheSessionListener {
    const void* context;

    void (*on_session_started)(const struct CCacheSessionListener* context, const char* key, uint64_t start_pos, int64_t cached_bytes, uint64_t total_bytes);

    void (*on_download_started)(const struct CCacheSessionListener* context, uint64_t position, const char* url, const char* host,
                                const char* ip, int response_code, uint64_t connect_time_ms);

    void (*on_download_progress)(const struct CCacheSessionListener* context, uint64_t download_position, uint64_t total_bytes);

    void (*on_download_paused)(const struct CCacheSessionListener* context);

    void (*on_download_resumed)(const struct CCacheSessionListener* context);

    void (*on_download_stopped)(const struct CCacheSessionListener* context, DownloadStopReason reason,
                                uint64_t downloaded_bytes, uint64_t transfer_consume_ms,
                                const char* sign, int error_code, const char* x_ks_cache,
                                const char* session_uuid, const char* download_uuid, const char* extra);

    void (*on_session_closed)(const struct CCacheSessionListener* context, int32_t error_code, uint64_t network_cost,
                              uint64_t total_cost, uint64_t downloaded_bytes, const char* detail_stat, bool has_opened);

} CCacheSessionListener;

#ifdef __cplusplus
}
#endif
