//
//  AppQosLiveAdaptiveRealtime.m
//  IJKMediaFramework
//
//  Created by wangtao03 on 2018/3/6.
//  Copyright © 2018年 kuaishou. All rights reserved.
//

#import "AppQosLiveAdaptiveRealtime.h"
#import <Foundation/Foundation.h>
#include <libavkwai/cJSON.h>
#import "AppQosUtil.h"
#import "cpu_resource.h"
#import "qos_live_adaptive_realtime.h"

#define JSON_NAME_MAX_LEN 64

#define DEFAULT_LIVE_ADAPTIVE_REALTIME_UPLOAD_INTERVAL_MS 2000

@interface AppQosLiveAdaptiveRealtime ()
@property(weak, nonatomic) KwaiFFPlayerController* kwaiPlayer;
@end

@implementation AppQosLiveAdaptiveRealtime {
    int64_t _playStartTime;

    int64_t _tickStartTimestamp;

    int64_t _reportIntervalMs;
    KSYPlyQosStatBlock _qosUploadBlock;
    NSTimer* _timer;

    int32_t _gop_collect_cnt;
    int64_t _last_rep_read_start_time;
    int64_t _last_rep_first_data_time;
    uint32_t _last_rep_switch_cnt;
    int64_t _index;

    BOOL _enableAdditional;
}

- (instancetype)initWith:(KwaiFFPlayerController*)kwaiPlayer {
    self = [super init];
    if (self) {
        _kwaiPlayer = kwaiPlayer;
    }
    return self;
}

/**
 * NOTE:如果启用实时上报，必须在startPlay的时候startReport。因为_playStartTime依赖于此逻辑
 */
- (void)startReport:(KSYPlyQosStatBlock)qosStatBlock
    reportIntervalMs:(int64_t)intervalMs
    enableAdditional:(BOOL)enable {
    @synchronized(self) {
        if (_timer != nil) {
            return;
        }

        _reportIntervalMs =
            intervalMs >= 0 ? intervalMs : DEFAULT_LIVE_ADAPTIVE_REALTIME_UPLOAD_INTERVAL_MS;
        _qosUploadBlock = qosStatBlock;
        _enableAdditional = enable;

        _playStartTime = _tickStartTimestamp = [AppQosUtil getTimestamp];
        _index = 0;
        _gop_collect_cnt = 0;
        _last_rep_read_start_time = 0;
        _last_rep_first_data_time = 0;
        _last_rep_switch_cnt = 0;

        _timer = [NSTimer scheduledTimerWithTimeInterval:_reportIntervalMs / 1000
                                                  target:self
                                                selector:@selector(uploadReport:)
                                                userInfo:nil
                                                 repeats:YES];
    }
}

- (void)stopReport {
    @synchronized(self) {
        if (_timer == nil) {
            return;
        }

        // TODO upload last report and stop timer
        [_timer invalidate];
        _timer = nil;

        // upload tail report
        [self uploadReport:nil];

        _qosUploadBlock = nil;
    }
}

- (void)uploadReport:(NSTimer*)t {
    if (!_kwaiPlayer) {
        return;
    }
    if (_qosUploadBlock) {
        _qosUploadBlock([self getQosJson:_kwaiPlayer.mediaPlayer]);
    }
    _tickStartTimestamp = [AppQosUtil getTimestamp];
}

- (NSString*)getQosJson:(IjkMediaPlayer*)mp {
    if (!mp) {
        return NULL;
    }

    QosLiveAdaptiveRealtime ffplayQos;
    ijkmp_get_qos_live_adaptive_realtime(mp, &ffplayQos);

    cJSON* stat_json = cJSON_CreateObject();

    cJSON_AddNumberToObject(stat_json, "tick_start", _tickStartTimestamp);

    // invariants
    cJSON_AddStringToObject(stat_json, "play_url", ffplayQos.url);
    cJSON_AddStringToObject(stat_json, "stream_id", [[self getStreamId] UTF8String]);
    cJSON_AddStringToObject(stat_json, "server_ip", [[_kwaiPlayer serverAddress] UTF8String]);
    cJSON_AddNumberToObject(stat_json, "play_start_time", _playStartTime);

    // variants
    cJSON_AddNumberToObject(stat_json, "v_buf_time",
                            ffplayQos.video_buffer_time);  // ms
    cJSON_AddNumberToObject(stat_json, "a_buf_time",
                            ffplayQos.audio_buffer_time);  // ms
    cJSON_AddNumberToObject(stat_json, "index", ++_index);

    // additional Qos for debug
    if (_enableAdditional) {
        cJSON_AddNumberToObject(stat_json, "bandwidth_current", ffplayQos.bandwidth_current);
        cJSON_AddNumberToObject(stat_json, "bandwidth_fragment", ffplayQos.bandwidth_fragment);
        cJSON_AddNumberToObject(stat_json, "bitrate_downloading", ffplayQos.bitrate_downloading);
        cJSON_AddNumberToObject(stat_json, "bitrate_playing", ffplayQos.bitrate_playing);
        cJSON_AddNumberToObject(stat_json, "current_buffer", ffplayQos.current_buffer_ms);
        cJSON_AddNumberToObject(stat_json, "estimated_buffer", ffplayQos.estimate_buffer_ms);
        cJSON_AddNumberToObject(stat_json, "predicted_buffer", ffplayQos.predicted_buffer_ms);
        cJSON_AddNumberToObject(stat_json, "cached_tag_duration", ffplayQos.cached_tag_dur_ms);
        cJSON_AddNumberToObject(stat_json, "cached_total_duration", ffplayQos.cached_total_dur_ms);

        cJSON_AddNumberToObject(stat_json, "switch_time_gap", ffplayQos.rep_switch_gap_time);
        cJSON_AddNumberToObject(stat_json, "switch_cnt",
                                ffplayQos.rep_switch_cnt - _last_rep_switch_cnt);
        _last_rep_switch_cnt = ffplayQos.rep_switch_cnt;

        cJSON_AddNumberToObject(stat_json, "switch_point_v_buf_time",
                                ffplayQos.rep_switch_point_v_buffer_ms);

        int64_t current_rep_read_start_time = ffplayQos.cur_rep_read_start_time;
        if (_last_rep_read_start_time != current_rep_read_start_time) {
            if (current_rep_read_start_time == 0) _gop_collect_cnt = 0;

            int64_t current_rep_first_data_time = ffplayQos.cur_rep_first_data_time;
            if (current_rep_first_data_time == 0) {
                cJSON_AddNumberToObject(stat_json, "cur_rep_first_data_time",
                                        _gop_collect_cnt * _reportIntervalMs);
                _gop_collect_cnt++;
            } else {
                _last_rep_first_data_time =
                    current_rep_first_data_time - current_rep_read_start_time;
                cJSON_AddNumberToObject(stat_json, "cur_rep_first_data_time",
                                        _last_rep_first_data_time);
                _gop_collect_cnt = 0;
                _last_rep_read_start_time = current_rep_read_start_time;
            }
        } else {
            cJSON_AddNumberToObject(stat_json, "cur_rep_first_data_time",
                                    _last_rep_first_data_time);
        }
    }

    char* ret = cJSON_Print(stat_json);
    NSString* jsonStr = [NSString stringWithUTF8String:ret];
    free(ret);
    cJSON_Delete(stat_json);
    return jsonStr;
}

- (NSString*)getStreamId {
    if (_kwaiPlayer.mediaPlayer) {
        char* streamid =
            ijkmp_get_property_string(_kwaiPlayer.mediaPlayer, FFP_PROP_STRING_STREAM_ID);
        if (!streamid) {
            return nil;
        }
        NSString* ret = [NSString stringWithUTF8String:streamid];
        return ret;
    }
    return nil;
}

@end
