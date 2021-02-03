/*
 * ff_ffmsg.h
 *      based on PacketQueue in ffplay.c
 *
 * Copyright (c) 2013 Zhang Rui <bbcallen@gmail.com>
 *
 * This file is part of ijkPlayer.
 *
 * ijkPlayer is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * ijkPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with ijkPlayer; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef FFPLAY__FF_FFMSG_H
#define FFPLAY__FF_FFMSG_H

#define FOREACH_FFP_MSG_REQ(X) \
    X(FFP_MSG_FLUSH, 0, "", "") \
    X(FFP_MSG_ERROR, 100, "error", "") \
    X(FFP_MSG_PREPARED, 200, "", "") \
    X(FFP_MSG_COMPLETED, 300, "", "") \
    X(FFP_MSG_VIDEO_SIZE_CHANGED, 400, "width", "height") \
    X(FFP_MSG_SAR_CHANGED, 401, "sar.num", "sar.den") \
    X(FFP_MSG_VIDEO_RENDERING_START, 402, "", "") \
    X(FFP_MSG_AUDIO_RENDERING_START, 403, "", "") \
    X(FFP_MSG_VIDEO_ROTATION_CHANGED, 404, "degree", "") \
    X(FFP_MSG_BUFFERING_START, 500, "", "") \
    X(FFP_MSG_BUFFERING_END, 501, "", "") \
    X(FFP_MSG_BUFFERING_UPDATE, 502, "buffering head position in time", "minimum percent in time or bytes") \
    X(FFP_MSG_BUFFERING_BYTES_UPDATE, 503, "cached data in bytes", "high water mark") \
    X(FFP_MSG_BUFFERING_TIME_UPDATE, 504, "cached duration in milliseconds", "high water mark") \
    X(FFP_MSG_SEEK_COMPLETE, 600, "", "") \
    X(FFP_MSG_PLAYBACK_STATE_CHANGED, 700, "new state", "") \
    X(FFP_MSG_RELOADED_VIDEO_RENDERING_START, 701, "", "") \
    X(FFP_MSG_PRE_LOAD_FINISH, 702, "prelaod cost_ms", "") \
    \
    X(FFP_MSG_SUGGEST_RELOAD, 810, "", "") \
    X(FFP_MSG_ACCURATE_SEEK_COMPLETE, 900, "current position", "") \
    X(FFP_MSG_VIDEO_RENDERING_START_AFTER_SEEK, 901, "current position", "") \
    X(FFP_MSG_AUDIO_RENDERING_START_AFTER_SEEK, 902, "", "") \
    \
    X(FFP_MSG_VIDEO_DECODER_OPEN, 10001, "", "") \
    X(FFP_MSG_PLAY_TO_END, 11003, "", "") \
    X(FFP_MSG_DECODE_ERROR, 11004, "error", "") \
    X(FFP_MSG_LIVE_TYPE_CHANGE, 11005, "live type", "") \
    X(FFP_MSG_LIVE_VOICE_COMMENT_CHANGE, 11006, "vc_time_high32", "vc_time_low32") \
    \
    X(FFP_REQ_START, 20001, "", "") \
    X(FFP_REQ_PAUSE, 20002, "", "") \
    X(FFP_REQ_SEEK, 20003, "seek to position msec", "") \
    X(FFP_REQ_STEP, 20004, "", "") \
    X(FFP_REQ_LIVE_RELOAD_AUDIO, 20005, "", "") \
    X(FFP_REQ_LIVE_RELOAD_VIDEO, 20006, "", "") \


#define GENERATE_ENUM(NAME, VAL, arg1, arg2) NAME = VAL,
enum {
    FOREACH_FFP_MSG_REQ(GENERATE_ENUM)
};

#define GENERATE_MSG_STR(NAME, VAL, arg1, arg2) case VAL: return #NAME;
static inline const char* ffp_msg_to_str(int msg_what) {
    switch (msg_what) {
            FOREACH_FFP_MSG_REQ(GENERATE_MSG_STR)
        default: return "UNKNOWN";
    }
}

#define GENERATE_MSG_ARG1_STR(NAME, VAL, arg1, arg2) case VAL: return 0 == strcmp(arg1, "") ? "arg1" : arg1;
static inline const char* ffp_msg_arg1_to_str(int msg_what) {
    switch (msg_what) {
            FOREACH_FFP_MSG_REQ(GENERATE_MSG_ARG1_STR)
        default: return "UNKNOWN msg";
    }
}

#define GENERATE_MSG_ARG2_STR(NAME, VAL, arg1, arg2) case VAL: return 0 == strcmp(arg2, "") ? "arg2" : arg2;
static inline const char* ffp_msg_arg2_to_str(int msg_what) {
    switch (msg_what) {
            FOREACH_FFP_MSG_REQ(GENERATE_MSG_ARG2_STR)
        default: return "UNKNOWN msg";
    }
}


#pragma mark ffp props defines
#define FFP_PROP_FLOAT_VIDEO_DECODE_FRAMES_PER_SECOND   10001
#define FFP_PROP_FLOAT_VIDEO_OUTPUT_FRAMES_PER_SECOND   10002
#define FFP_PROP_FLOAT_PLAYBACK_RATE                    10003
#define FFP_PROP_FLOAT_AVDELAY                          10004
#define FFP_PROP_FLOAT_AVDIFF                           10005

#define FFP_PROP_INT64_SELECTED_VIDEO_STREAM            20001
#define FFP_PROP_INT64_SELECTED_AUDIO_STREAM            20002
#define FFP_PROP_INT64_VIDEO_DECODER                    20003
#define FFP_PROP_INT64_AUDIO_DECODER                    20004
#define     FFP_PROPV_DECODER_UNKNOWN                   0
#define     FFP_PROPV_DECODER_AVCODEC                   1
#define     FFP_PROPV_DECODER_MEDIACODEC                2
#define     FFP_PROPV_DECODER_VIDEOTOOLBOX              3
#define FFP_PROP_INT64_VIDEO_CACHED_DURATION            20005
#define FFP_PROP_INT64_AUDIO_CACHED_DURATION            20006
#define FFP_PROP_INT64_VIDEO_CACHED_BYTES               20007
#define FFP_PROP_INT64_AUDIO_CACHED_BYTES               20008
#define FFP_PROP_INT64_VIDEO_CACHED_PACKETS             20009
#define FFP_PROP_INT64_AUDIO_CACHED_PACKETS             20010
#define FFP_PROP_INT64_BIT_RATE                         20100
#define FFP_PROP_INT64_TRAFFIC_STATISTIC_BYTE_COUNT     20204

#define FFP_PROP_INT64_LATEST_SEEK_LOAD_DURATION        20300

// ------------------ kwai properties start ------------------
#define FFP_PROP_FLOAT_MAX_AVDIFF_REALTIME              30000
#define FFP_PROP_FLOAT_MIN_AVDIFF_REALTIME              30001

#define FFP_PROP_INT64_CPU                              30002
#define FFP_PROP_INT64_MEMORY                           30003
#define FFP_PROP_INT64_BUFFERTIME                       30004
#define FFP_PROP_INT64_BLOCKCNT                         30005

#define FFP_PROP_FLOAT_VIDEO_AVG_FPS                    30006
#define FFP_PROP_INT64_VIDEO_WIDTH                      30007
#define FFP_PROP_INT64_VIDEO_HEIGHT                     30008
#define FFP_PROP_INT64_AUDIO_BUF_SIZE                   30009
#define FFP_PROP_INT64_CURRENT_ABSOLUTE_TIME            30010

#define FFP_PROP_LONG_READ_DATA                         30011
#define FFP_PROP_LONG_DOWNLOAD_SIZE                     30012   /* kBytes */
#define FFP_PROP_FLOAT_BUFFERSIZE_MAX                   30013   /* milliseconds */
#define FFP_PROP_INT64_DTS_DURATION                     30015   /* milliseconds */
#define FFP_PROP_INT64_VIDEO_DEC_ERROR_COUNT            30016
#define FFP_PROP_INT64_DROPPED_DURATION                 30017
#define FFP_PROP_INT64_DECODED_VIDEO_FRAME_COUNT        30018
#define FFP_PROP_INT64_DISPLAYED_FRAME_COUNT            30019
#define FFP_PROP_FLOAT_AVERAGE_DISPLAYED_FPS            30020
#define FFP_PROP_INT64_READ_VIDEO_FRAME_COUNT           30021
#define FFP_PROP_INT64_SOURCE_DEVICE_TYPE               30022

#define FFP_PROP_STRING_SERVER_IP                       30100
#define FFP_PROP_STRING_LOG_FILE_PATH                   30101
#define FFP_PROP_STRING_STREAM_ID                       30102
#define FFP_PROP_STRING_DOMAIN                          30103
#define FFP_PROP_STRING_HTTP_REDIRECT_INFO              30104
#define FFP_PROP_STRING_PLAYING_URL                     30105

#define FFP_PROP_INT64_VOD_ADAPTIVE_REP_ID              30200


#endif
