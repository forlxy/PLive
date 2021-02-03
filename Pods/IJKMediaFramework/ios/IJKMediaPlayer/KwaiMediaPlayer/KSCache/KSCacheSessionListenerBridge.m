//
//  KSCacheSessionDelegate.m
//  IJKMediaPlayer
//
//  Created by 帅龙成 on 21/12/2017.
//  Copyright © 2017 kuaishou. All rights reserved.
//

#import <Foundation/Foundation.h>

#import "KSCacheSessionDelegate.h"
#include "KSCacheSessionListenerBridge.h"
#import "KwaiFFPlayerController.h"
#import "cache_session_listener_c.h"

static KwaiFFPlayerController* CCacheSessionListener_get_controller(
    const CCacheSessionListener* listener);

static void cache_session_on_session_started(const CCacheSessionListener* listener, const char* key,
                                             uint64_t start_pos, int64_t cached_byes,
                                             uint64_t total_bytes) {
    [CCacheSessionListener_get_controller(listener).cacheSessionDelegate
        onSessionStartWithKey:key
                          pos:start_pos
                  cachedBytes:cached_byes
                   totalBytes:total_bytes];
}

static void cache_session_on_download_started(const CCacheSessionListener* listener,
                                              uint64_t position, const char* url, const char* host,
                                              const char* ip, int response_code,
                                              uint64_t connect_time_ms) {
    [CCacheSessionListener_get_controller(listener).cacheSessionDelegate
        onDownloadStarted:position
                      url:url
                     host:host
                       ip:ip
             responseCode:response_code
              connectTime:connect_time_ms];
}

static void cache_session_on_download_progress(const CCacheSessionListener* listener,
                                               uint64_t download_position, uint64_t total_bytes) {
    [CCacheSessionListener_get_controller(listener).cacheSessionDelegate
        onDownloadProgress:download_position
                totalBytes:total_bytes];
}

static void cache_session_resumed(const CCacheSessionListener* listener) {
    [CCacheSessionListener_get_controller(listener).cacheSessionDelegate onDownloadResumed];
}

static void cache_session_paused(const CCacheSessionListener* listener) {
    [CCacheSessionListener_get_controller(listener).cacheSessionDelegate onDownloadPaused];
}

static void cache_session_on_download_stopped(const CCacheSessionListener* listener,
                                              DownloadStopReason reason, uint64_t downloaded_bytes,
                                              uint64_t transfer_consume_ms, const char* sign,
                                              int error_code, const char* x_ks_cache,
                                              const char* session_uuid, const char* download_uuid,
                                              const char* extra_msg) {
    KwaiFFPlayerController* player = CCacheSessionListener_get_controller(listener);
    [player.cacheSessionDelegate onDownloadStopped:(KSDownloadStopReason)reason
                                     downloadBytes:downloaded_bytes
                                   transferConsume:transfer_consume_ms
                                          kwaiSign:sign
                                         errorCode:error_code
                                          xKsCache:x_ks_cache
                                       sessionUUID:session_uuid
                                      downloadUUID:download_uuid
                                             extra:extra_msg];
}

static void cache_session_on_session_closed(const CCacheSessionListener* listener,
                                            int32_t error_code, uint64_t network_cost_ms,
                                            uint64_t total_cost_ms, uint64_t downloaded_bytes,
                                            const char* detail_stat, bool has_opened) {
    KwaiFFPlayerController* player = CCacheSessionListener_get_controller(listener);
    if (player == NULL) {
        return;
    }
    [player.cacheSessionDelegate onSessionClosed:error_code
                                     networkCost:network_cost_ms
                                       totalCost:total_cost_ms
                                 downloadedBytes:downloaded_bytes
                                           stats:detail_stat
                                          opened:has_opened];
}

#include <pthread.h>
CCacheSessionListener* CCacheSessionListener_create(IJKFFMoviePlayerController* controller) {
    assert(controller);

    CCacheSessionListener* c_listener =
        (CCacheSessionListener*)malloc(sizeof(CCacheSessionListener));

    NSLog(@"CacheDataSource  CCacheSessionListener_create over, c_listener:%p", c_listener);
    if (c_listener) {
        c_listener->context = CFBridgingRetain(controller);
        c_listener->on_session_started = &cache_session_on_session_started;
        c_listener->on_session_closed = &cache_session_on_session_closed;
        c_listener->on_download_started = &cache_session_on_download_started;
        c_listener->on_download_paused = &cache_session_paused;
        c_listener->on_download_resumed = &cache_session_resumed;
        c_listener->on_download_progress = &cache_session_on_download_progress;
        c_listener->on_download_stopped = &cache_session_on_download_stopped;
    }

    return c_listener;
}

KwaiFFPlayerController* CCacheSessionListener_get_controller(
    const CCacheSessionListener* listener) {
    return (__bridge KwaiFFPlayerController*)(listener->context);
}

void CCacheSessionListener_freep(CCacheSessionListener** listener) {
    if (!listener || !*listener) {
        return;
    }

    CCacheSessionListener* l = *listener;
    CFBridgingRelease(l->context);
    l->context = NULL;

    free(l);
    *listener = NULL;
}
