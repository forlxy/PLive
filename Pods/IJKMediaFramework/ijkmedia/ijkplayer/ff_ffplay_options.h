/*
 * ff_ffplaye_options.h
 *
 * Copyright (c) 2015 Zhang Rui <bbcallen@gmail.com>
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

#ifndef FFPLAY__FF_FFPLAY_OPTIONS_H
#define FFPLAY__FF_FFPLAY_OPTIONS_H

#define OPTION_OFFSET(x) offsetof(FFPlayer, x)
#define OPTION_INT(default__, min__, max__) \
    .type = AV_OPT_TYPE_INT, \
            { .i64 = default__ }, \
            .min = min__, \
                   .max = max__, \
                          .flags = AV_OPT_FLAG_DECODING_PARAM
#define OPTION_INT64(default__, min__, max__) \
    .type = AV_OPT_TYPE_INT64, \
            { .i64 = default__ }, \
            .min = min__, \
                   .max = max__, \
                          .flags = AV_OPT_FLAG_DECODING_PARAM
#define OPTION_CONST(default__) \
    .type = AV_OPT_TYPE_CONST, \
            { .i64 = default__ }, \
            .min = INT_MIN, \
                   .max = INT_MAX, \
                          .flags = AV_OPT_FLAG_DECODING_PARAM

#define OPTION_STR(default__) \
    .type = AV_OPT_TYPE_STRING, \
            { .str = default__ }, \
            .min = 0, \
                   .max = 0, \
                          .flags = AV_OPT_FLAG_DECODING_PARAM

static const AVOption ffp_context_options[] = {
    // original options in ffplay.c
    // FFP_MERGE: x, y, s, fs
    {
        "cache-key",                      "cache key for awesome cache",
        OPTION_OFFSET(cache_key),      OPTION_STR(NULL)
    },
    {
        "cache-enabled",                      "cache enabled status for awesome cache",
        OPTION_OFFSET(expect_use_cache),   OPTION_INT(0, 0, 1)
    },
    {
        "enable-segment-cache",                      "segment cache enabled for awesome cache",
        OPTION_OFFSET(enable_segment_cache),   OPTION_INT(0, 0, 1)
    },
    {
        "an",                             "disable audio",
        OPTION_OFFSET(audio_disable),   OPTION_INT(0, 0, 1)
    },
    {
        "vn",                             "disable video",
        OPTION_OFFSET(video_disable),   OPTION_INT(0, 0, 1)
    },
    // FFP_MERGE: sn, ast, vst, sst
    // TODO: ss
    {
        "nodisp",                         "disable graphical display",
        OPTION_OFFSET(display_disable), OPTION_INT(0, 0, 1)
    },
    {
        "fast",                           "non spec compliant optimizations",
        OPTION_OFFSET(fast),            OPTION_INT(0, 0, 1)
    },
    // FFP_MERGE: genpts, drp, lowres, sync, autoexit, exitonkeydown, exitonmousedown
    {
        "loop",                           "set number of times the playback shall be looped",
        OPTION_OFFSET(loop),            OPTION_INT(1, INT_MIN, INT_MAX)
    },
    {
        "enable-loop-on-error",                           "enable loop for io error",
        OPTION_OFFSET(enable_loop_on_error),            OPTION_INT(0, 0, 1)
    },
    {
        "exit-on-dec-error",                           "exit on dec error",
        OPTION_OFFSET(exit_on_dec_error),            OPTION_INT(0, 0, 1)
    },
    {
        "infbuf",                         "don't limit the input buffer size (useful with realtime streams)",
        OPTION_OFFSET(infinite_buffer), OPTION_INT(0, 0, 1)
    },
    {
        "framedrop",                      "max continuous drop frame count when cpu is too slow",
        OPTION_OFFSET(framedrop),       OPTION_INT(8, -1, INT_MAX)
    },
    {
        "seek-at-start",                  "set offset of player should be seeked",
        OPTION_OFFSET(seek_at_start),       OPTION_INT64(0, 0, INT_MAX)
    },
    // FFP_MERGE: window_title
#if CONFIG_AVFILTER
    {
        "af",                             "audio filters",
        OPTION_OFFSET(afilters),        OPTION_STR(NULL)
    },
    {
        "vf0",                            "video filters 0",
        OPTION_OFFSET(vfilter0),        OPTION_STR(NULL)
    },
#endif
    {
        "rdftspeed",                      "rdft speed, in msecs",
        OPTION_OFFSET(rdftspeed),       OPTION_INT(0, 0, INT_MAX)
    },
    // FFP_MERGE: showmode, default, i, codec, acodec, scodec, vcodec
    // TODO: autorotate

    // extended options in ff_ffplay.c
    {
        "max-fps",                        "drop frames in video whose fps is greater than max-fps",
        OPTION_OFFSET(max_fps),         OPTION_INT(31, 0, 121)
    },

    {
        "overlay-format",                 "fourcc of overlay format",
        OPTION_OFFSET(overlay_format),  OPTION_INT(SDL_FCC_I420, INT_MIN, INT_MAX),
        .unit = "overlay-format"
    },
    { "fcc-_es2",                       "", 0, OPTION_CONST(SDL_FCC__GLES2), .unit = "overlay-format" },
    { "fcc-i420",                       "", 0, OPTION_CONST(SDL_FCC_I420), .unit = "overlay-format" },
    { "fcc-yv12",                       "", 0, OPTION_CONST(SDL_FCC_YV12), .unit = "overlay-format" },
    { "fcc-rv16",                       "", 0, OPTION_CONST(SDL_FCC_RV16), .unit = "overlay-format" },
    { "fcc-rv24",                       "", 0, OPTION_CONST(SDL_FCC_RV24), .unit = "overlay-format" },
    { "fcc-rv32",                       "", 0, OPTION_CONST(SDL_FCC_RV32), .unit = "overlay-format" },
    { "fcc-nv21",                       "", 0, OPTION_CONST(SDL_FCC_NV21), .unit = "overlay-format" },

    {
        "start-on-prepared",                  "automatically start playing on prepared",
        OPTION_OFFSET(start_on_prepared),   OPTION_INT(1, 0, 1)
    },

    {
        "video-pictq-size",                   "max picture queue frame count",
        OPTION_OFFSET(pictq_size),          OPTION_INT(VIDEO_PICTURE_QUEUE_SIZE_DEFAULT,
                                                       VIDEO_PICTURE_QUEUE_SIZE_MIN,
                                                       VIDEO_PICTURE_QUEUE_SIZE_MAX)
    },

    {
        "max-buffer-size",                    "max buffer size should be pre-read",
        OPTION_OFFSET(dcc.max_buffer_size), OPTION_INT(MAX_QUEUE_SIZE, 0, MAX_QUEUE_SIZE)
    },
    {
        "min-frames",                         "minimal frames to stop pre-reading",
        OPTION_OFFSET(dcc.min_frames),      OPTION_INT(DEFAULT_MIN_FRAMES, MIN_MIN_FRAMES, MAX_MIN_FRAMES)
    },
    {
        "max-buffer-dur-ms",                  "max buffer duration should be pre-read",
        OPTION_OFFSET(dcc.max_buffer_dur_ms),   OPTION_INT(DEFAULT_QUEUE_DUR_MS, 0, MAX_QUEUE_DUR_MS)
    },
    {
        "dcc.max-buffer-dur-bsp-ms",          "max buffer duration that should be pre-read before start play",
        OPTION_OFFSET(dcc.max_buffer_dur_bsp_ms), OPTION_INT(DEFAULT_QUEUE_DUR_MS, MIN_QUEUE_DUR_BSP_MS, MAX_QUEUE_DUR_MS)
    },
    {
        "dcc.max-buffer-strategy",            "max buffer duration strateg",
        OPTION_OFFSET(dcc.max_buf_dur_strategy),
        OPTION_INT(MaxBufStrategy_None,
                   MaxBufStrategy_None,
                   MaxBufStrategy_ProgressToMax)
    },
    {
        "first-high-water-mark-ms",           "first chance to wakeup read_thread",
        OPTION_OFFSET(dcc.first_high_water_mark_in_ms),
        OPTION_INT(DEFAULT_FIRST_HIGH_WATER_MARK_IN_MS,
                   DEFAULT_FIRST_HIGH_WATER_MARK_IN_MS,
                   DEFAULT_LAST_HIGH_WATER_MARK_IN_MS)
    },
    {
        "next-high-water-mark-ms",            "second chance to wakeup read_thread",
        OPTION_OFFSET(dcc.next_high_water_mark_in_ms),
        OPTION_INT(DEFAULT_NEXT_HIGH_WATER_MARK_IN_MS,
                   DEFAULT_FIRST_HIGH_WATER_MARK_IN_MS,
                   DEFAULT_LAST_HIGH_WATER_MARK_IN_MS)
    },
    {
        "last-high-water-mark-ms",            "last chance to wakeup read_thread",
        OPTION_OFFSET(dcc.last_high_water_mark_in_ms),
        OPTION_INT(DEFAULT_LAST_HIGH_WATER_MARK_IN_MS,
                   DEFAULT_FIRST_HIGH_WATER_MARK_IN_MS,
                   DEFAULT_LAST_HIGH_WATER_MARK_IN_MS)
    },
    {
        "buffer-strategy",                    "buffer strategy",
        OPTION_OFFSET(dcc.buffer_strategy),
        OPTION_INT(BUFFER_STRATEGY_OLD,
                   BUFFER_STRATEGY_OLD,
                   BUFFER_STRATEGY_NEW)
    },
    {
        "buffer-increment-step",              "buffer increment step",
        OPTION_OFFSET(dcc.buffer_increment_step),
        OPTION_INT(DEFAULT_BUFFER_INCREMENT_STEP,
                   MIN_BUFFER_INCREMENT_STEP,
                   MAX_BUFFER_INCREMENT_STEP)
    },
    {
        "buffer-smooth-time",                 "buffer smooth time",
        OPTION_OFFSET(dcc.buffer_smooth_time),
        OPTION_INT(DEFAULT_BUFFER_SMOOTH_TIME,
                   MIN_BUFFER_SMOOTH_TIME,
                   MAX_BUFFER_SMOOTH_TIME)
    },
    {
        "packet-buffering",                   "pause output until enough packets have been read after stalling",
        OPTION_OFFSET(packet_buffering),    OPTION_INT(1, 0, 1)
    },
    {
        "sync-av-start",                      "synchronise a/v start time",
        OPTION_OFFSET(sync_av_start),       OPTION_INT(1, 0, 1)
    },
    {
        "iformat",                            "force format",
        OPTION_OFFSET(iformat_name),        OPTION_STR(NULL)
    },
    {
        "no-time-adjust",                     "return player's real time from the media stream instead of the adjusted time",
        OPTION_OFFSET(no_time_adjust),      OPTION_INT(0, 0, 1)
    },

    {
        "enable-accurate-seek",                      "enable accurate seek",
        OPTION_OFFSET(enable_accurate_seek),       OPTION_INT(0, 0, 1)
    },
    {
        "enable-seek-forward-offset",                      "enable seek forward offset",
        OPTION_OFFSET(enable_seek_forward_offset),       OPTION_INT(1, 0, 1)
    },

    {
        "accurate-seek-timeout",                      "accurate seek timeout",
        OPTION_OFFSET(accurate_seek_timeout),       OPTION_INT(MAX_ACCURATE_SEEK_TIMEOUT, 0, MAX_ACCURATE_SEEK_TIMEOUT)
    },

    {
        "enable-cache-seek",                   "enable cache seek",
        OPTION_OFFSET(enable_cache_seek),    OPTION_INT(1, 0, 1)
    },
    {
        "enable-audio-spectrum",               "enable audio spectrum",
        OPTION_OFFSET(enable_audio_spectrum),    OPTION_INT(0, 0, 1)
    },
    {
        "audio-gain.enable",                    "audio gain enable",
        OPTION_OFFSET(audio_gain.enabled),    OPTION_INT(0, 0, 1)
    },
    {
        "audio-gain.audio_str",                 "audio str",
        OPTION_OFFSET(audio_gain.audio_str),    OPTION_STR("0")
    },
    {
        "enable-modify-block",                  "enable revise block read",
        OPTION_OFFSET(enable_modify_block),     OPTION_INT(0, 0, 1)
    },
    {
        "host",                      "host used for httpnds ,for qos purpose",
        OPTION_OFFSET(host),   OPTION_STR(NULL)
    },

    {
        "aac-libfdk",               "aac: libfdk in ffmpeg",
        OPTION_OFFSET(aac_libfdk), OPTION_INT(1, 0, 1)
    },
    {
        "use-aligned-pts",               "use aligned pts",
        OPTION_OFFSET(use_aligned_pts), OPTION_INT(0, 0, 1)
    },
    // iOS only options
    {
        "videotoolbox-max-frame-width",       "VideoToolbox: max width of output frame",
        OPTION_OFFSET(vtb_max_frame_width), OPTION_INT(0, 0, INT_MAX)
    },
    {
        "videotoolbox-async",                 "VideoToolbox: use kVTDecodeFrame_EnableAsynchronousDecompression()",
        OPTION_OFFSET(vtb_async),           OPTION_INT(0, 0, 1)
    },
    {
        "videotoolbox-wait-async",            "VideoToolbox: call VTDecompressionSessionWaitForAsynchronousFrames()",
        OPTION_OFFSET(vtb_wait_async),      OPTION_INT(1, 0, 1)
    },
    {
        "vtb-h264",                           "VideoToolbox: enable h264",
        OPTION_OFFSET(vtb_h264),            OPTION_INT(0, 0, 1)
    },
    {
        "vtb-h265",                           "VideoToolbox: enable h265",
        OPTION_OFFSET(vtb_h265),            OPTION_INT(0, 0, 1)
    },
    {
        "vtb-auto-rotate",                    "VideoToolbox: auto rotate frame depending on meta",
        OPTION_OFFSET(vtb_auto_rotate),     OPTION_INT(1, 0, 1)
    },

    // Android only options
    {
        "mediacodec",                             "MediaCodec: enable H264 (deprecated by 'mediacodec-avc')",
        OPTION_OFFSET(mediacodec_avc),          OPTION_INT(0, 0, 1)
    },
    {
        "mediacodec-auto-rotate",                 "MediaCodec: auto rotate frame depending on meta",
        OPTION_OFFSET(mediacodec_auto_rotate),  OPTION_INT(1, 0, 1)
    },
    {
        "mediacodec-avc-height-limit",                 "MediaCodec: supported avc height limitation",
        OPTION_OFFSET(mediacodec_avc_height_limit),  OPTION_INT(1920, 0, 4320)
    },
    {
        "mediacodec-hevc-height-limit",                 "MediaCodec: supported hevc height limitation",
        OPTION_OFFSET(mediacodec_hevc_height_limit),  OPTION_INT(1280, 0, 4320)
    },
    {
        "mediacodec-avc-width-limit",                   "MediaCodec: supported avc width limitation",
        OPTION_OFFSET(mediacodec_avc_width_limit),    OPTION_INT(1920, 0, 4096)
    },
    {
        "mediacodec-hevc-width-limit",                  "MediaCodec: supported hevc width limitation",
        OPTION_OFFSET(mediacodec_hevc_width_limit),   OPTION_INT(1920, 0, 4096)
    },
    {
        "mediacodec-all-videos",                  "MediaCodec: enable all videos",
        OPTION_OFFSET(mediacodec_all_videos),   OPTION_INT(0, 0, 1)
    },
    {
        "mediacodec-avc",                         "MediaCodec: enable H264",
        OPTION_OFFSET(mediacodec_avc),          OPTION_INT(0, 0, 1)
    },
    {
        "mediacodec-hevc",                        "MediaCodec: enable HEVC",
        OPTION_OFFSET(mediacodec_hevc),         OPTION_INT(0, 0, 1)
    },
    {
        "mediacodec-mpeg2",                       "MediaCodec: enable MPEG2VIDEO",
        OPTION_OFFSET(mediacodec_mpeg2),        OPTION_INT(0, 0, 1)
    },
    {
        "mediacodec-max-cnt",                     "MediaCodec: Max Deocoder Number",
        OPTION_OFFSET(mediacodec_max_cnt),      OPTION_INT(1, 1, 9)
    },
    {
        "use-mediacodec-bytebuffer",              "MediaCodec: whether open bytebuffer",
        OPTION_OFFSET(use_mediacodec_bytebuffer), OPTION_INT(0, 0, 1)
    },
    {
        "opensles",                               "OpenSL ES: enable",
        OPTION_OFFSET(opensles),                OPTION_INT(0, 0, 1)
    },
    {
        "islive",                                 "Whether live stream or not",
        OPTION_OFFSET(islive),                  OPTION_INT(0, 0, 1)
    },
    {
        "app-start-time",                                 "start time of app",
        OPTION_OFFSET(app_start_time),                  OPTION_INT64(0, 0, INT64_MAX)
    },
    {
        "enable-live-manifest",                   "Whether live Manifest or not",
        OPTION_OFFSET(is_live_manifest),          OPTION_INT(0, 0, 1)
    },
    {
        "enable-vod-manifest",                    "Enable vod Manifest",
        OPTION_OFFSET(enable_vod_manifest),          OPTION_INT(0, 0, 1)
    },
    {
        "input-data-type",                          "Input Data Type",
        OPTION_OFFSET(input_data_type),                 OPTION_INT(0, 0, INT_MAX)
    },
    {
        "prefer-bandwidth",                          "prefer bandwidth",
        OPTION_OFFSET(prefer_bandwidth),                 OPTION_INT(0, 0, INT_MAX)
    },
    {
        "index-content.pre_path",                                  "content pre path",
        OPTION_OFFSET(index_content.pre_path),                         OPTION_STR(NULL)
    },
    {
        "index-content.content",                                  "content",
        OPTION_OFFSET(index_content.content),                         OPTION_STR(NULL)
    },
    {
        "pre-demux-ver",                          "which version of pre demux to use",
        OPTION_OFFSET(use_pre_demux_ver),          OPTION_INT(1, 0, 1)
    },
    {
        "tag1",                                   "set from app for ab test tagging",
        OPTION_OFFSET(tag1),          OPTION_INT(0, 0, INT_MAX)
    },
    {
        "dcc-alg.config_enabled",                      "enable dcc algorithm",
        OPTION_OFFSET(dcc_algorithm.config_enabled),          OPTION_INT(0, 0, 1)
    },
    {
        "dcc-alg.config_mark_bitrate_th_10",                      "Mark vs bitrate threshold",
        OPTION_OFFSET(dcc_algorithm.config_mark_bitrate_th_10),   OPTION_INT(30, 10, 10000)
    },
    {
        "dcc-alg.config_dcc_pre_read_ms",                      "opt pre-read-dur-ms",
        OPTION_OFFSET(dcc_algorithm.config_dcc_pre_read_ms),   OPTION_INT(5000, 100, (2 * 60 * 1000))
    },
    {
        "async-stream-component-open",                          "async open stream component",
        OPTION_OFFSET(async_stream_component_open),             OPTION_INT(0, 0, 1)
    },

    {
        "fade-in-end-time-ms",               "fadein: end time",
        OPTION_OFFSET(fade_in_end_time_ms), OPTION_INT(0, 0, 10000)
    },
    { NULL }
};

#undef OPTION_STR
#undef OPTION_CONST
#undef OPTION_INT
#undef OPTION_OFFSET

#endif
