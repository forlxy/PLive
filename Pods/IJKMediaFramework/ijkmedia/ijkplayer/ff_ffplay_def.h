/*
 * Copyright (c) 2003 Fabrice Bellard
 * Copyright (c) 2013-2015 Zhang Rui <bbcallen@gmail.com>
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

#ifndef FFPLAY__FF_FFPLAY_DEF_H
#define FFPLAY__FF_FFPLAY_DEF_H

/**
 * @file
 * simple media player based on the FFmpeg libraries
 */

#include "config.h"
#include <inttypes.h>
#include <math.h>
#include <limits.h>
#include <signal.h>
#include <stdint.h>

#include "libavutil/avstring.h"
#include "libavutil/eval.h"
#include "libavutil/mathematics.h"
#include "libavutil/pixdesc.h"
#include "libavutil/imgutils.h"
#include "libavutil/dict.h"
#include "libavutil/parseutils.h"
#include "libavutil/samplefmt.h"
#include "libavutil/avassert.h"
#include "libavutil/time.h"
#include "libavformat/avformat.h"
// FFP_MERGE: #include "libavdevice/avdevice.h"
#include "libswscale/swscale.h"
#include "libavutil/opt.h"
#include "libavcodec/avfft.h"
#include "libswresample/swresample.h"

#if CONFIG_AVFILTER
# include "libavfilter/avfilter.h"
# include "libavfilter/buffersink.h"
# include "libavfilter/buffersrc.h"
#endif

#include <stdbool.h>
#include <ijkmedia/ijkplayer/ff_ffplay_def.h>
#include "ff_ffinc.h"
#include "ff_ffmsg_queue.h"
#include "ff_ffpipenode.h"
#include "ff_packet_queue.h"
#include "ff_frame_queue.h"
#include "kwai_live_voice_comment_queue.h"
#include "kwai_live_event_queue.h"
#include "kwai_live_type_queue.h"
#include "ijkmeta.h"

#include "ijksdl/ijksdl_aout.h"
#include "sound_touch_c.h"

#include "ijkkwai/kwai_qos.h"
#include "ijkkwai/kflv_statistic.h"
#include "ijkkwai/kwai_rotation.h"
#include "ijkkwai/kwai_audio_gain.h"
#include "ijkkwai/kwai_pre_demux.h"
#include "ijkkwai/kwai_live_abs_time_control.h"
#include "ijkkwai/kwai_ab_loop.h"
#include "ijkkwai/kwai_vod_manifest.h"
#include "ijkkwai/kwai_clock_tracker.h"
#include "ijkkwai/kwai_audio_volume_progress.h"
#include "ijkkwai/kwai_packet_queue_strategy.h"
#include "ijkkwai/cache/cache_statistic.h"
#include "ijkkwai/ff_buffer_strategy.h"
#include "ijkkwai/kwai_live_delay_stat.h"
#include "ijkkwai/kwai_io_queue_observer.h"

#include "awesome_cache/include/dcc_algorithm_c.h"
#include "awesome_cache/include/awesome_cache_c.h"
#include "awesome_cache/include/player_statistic_c.h"
#include "awesome_cache/include/awesome_cache_callback_c.h"

#include "ffplay_modules/ff_ffplay_video_state.h"
#include "ffplay_modules/ff_ffplay_decoder_internal.h"
#include "ffplay_modules/ff_ffplay_clock.h"
#include "ffplay_modules/ff_ffplay_statistic_def.h"


#define FLAG_KWAI_USE_MEDIACODEC_H264 0x0001  ///<flag to use h264 mediacodec
#define FLAG_KWAI_USE_MEDIACODEC_H265 0x0002  ///<flag to use h265 mediacodec
#define FLAG_KWAI_USE_MEDIACODEC_ALL  0x1000  ///<flag to all codec use mediacodec

// 检测是否需要buffer（ffp_check_buffering_l）的一些阈值
#define BUFFERING_CHECK_PER_BYTES               (512)
#define BUFFERING_CHECK_PER_MILLISECONDS        (100)


#define MAX_ACCURATE_SEEK_TIMEOUT (5000)
#define DEFAULT_PLAYBACK_RATE (1.0f)
#ifdef FFP_MERGE
#define MIN_FRAMES 25
#endif

// 判断是否达到最少的一个缓存packet数，达到的话，则可以进入 不继续av_read_frame的while等待过程
#define MAX_QUEUE_FRAMES    15 * 5
#define MIN_MIN_FRAMES      5
#define MAX_MIN_FRAMES      50000
#define MIN_FRAMES (ffp->dcc.min_frames)

// 音视频同步用到的一些变量，会在 audio/video的decode/render线程里引用
/* no AV sync correction is done if below the minimum AV sync threshold */
#define AV_SYNC_THRESHOLD_MIN 0.04
/* AV sync correction is done if above the maximum AV sync threshold */
#define AV_SYNC_THRESHOLD_MAX 0.1
/* If a frame duration is longer than this, it will not be duplicated to compensate AV sync */
#define AV_SYNC_FRAMEDUP_THRESHOLD 0.1
/* no AV correction is done if too big error */
#define AV_NOSYNC_THRESHOLD 100.0

/* we use about AUDIO_DIFF_AVG_NB A-V differences to make the average */
#define AUDIO_DIFF_AVG_NB   20

// util macro
#define fftime_to_milliseconds(ts) (av_rescale(ts, 1000, AV_TIME_BASE))
#define milliseconds_to_fftime(ms) (av_rescale(ms, AV_TIME_BASE, 1000))

#define FFTRACE ALOGW

// 这两个变量暂时不用了，原来是传给ffp_set_video_codec_info的codec参数的
#define AVCODEC_MODULE_NAME    "avcodec"
#define MEDIACODEC_MODULE_NAME "MediaCodec"

#ifdef FFP_MERGE
#define CURSOR_HIDE_DELAY 1000000

static unsigned sws_flags = SWS_BICUBIC;
#endif

#define MAX_DEVIATION 1200000   // 1200ms
#define AV_DEVIATION -100*1000  // audio/video pts deviation, 100ms


enum INPUT_DATA_TYPE {
    INPUT_DATA_TYPE_SINGLE_URL, /* default choice */
    INPUT_DATA_TYPE_VOD_MANIFEST,
    INPUT_DATA_TYPE_INDEX_CONTENT, /* synchronize to an external clock */
    INPUT_DATA_TYPE_HLS_CUSTOME_MANIFEST, //自定义hls manifest
};

typedef struct IndexContent {
    char* pre_path;
    char* content;
} IndexContent;


struct CCacheSessionListener;

/* ffplayer */
struct IjkMediaMeta;
struct IJKFF_Pipeline;

/* options specified by the user */
#ifdef FFP_MERGE
static AVInputFormat* file_iformat;
static const char* input_filename;
static const char* window_title;
static int fs_screen_width;
static int fs_screen_height;
static int default_width  = 640;
static int default_height = 480;
static int screen_width  = 0;
static int screen_height = 0;
static int audio_disable;
static int video_disable;
static int subtitle_disable;
static const char* wanted_stream_spec[AVMEDIA_TYPE_NB] = {0};
static int seek_by_bytes = -1;
static int display_disable;
static int show_status = 1;
static int av_sync_type = AV_SYNC_AUDIO_MASTER;
static int64_t start_time = AV_NOPTS_VALUE;
static int64_t duration = AV_NOPTS_VALUE;
static int fast = 0;
static int genpts = 0;
static int lowres = 0;
static int decoder_reorder_pts = -1;
static int autoexit;
static int exit_on_keydown;
static int exit_on_mousedown;
static int loop = 1;
static int framedrop = -1;
static int infinite_buffer = -1;
static enum ShowMode show_mode = SHOW_MODE_NONE;
static const char* audio_codec_name;
static const char* subtitle_codec_name;
static const char* video_codec_name;
double rdftspeed = 0.02;
static int64_t cursor_last_shown;
static int cursor_hidden = 0;
#if CONFIG_AVFILTER
static const char** vfilters_list = NULL;
static int nb_vfilters = 0;
static char* afilters = NULL;
#endif
static int autorotate = 1;

/* current context */
static int is_full_screen;
static int64_t audio_callback_time;


#define FF_ALLOC_EVENT   (SDL_USEREVENT)
#define FF_QUIT_EVENT    (SDL_USEREVENT + 2)

static SDL_Surface* screen;
#endif

static volatile unsigned global_session_id;
typedef struct FFPlayer {
    const AVClass* av_class;

    /* ffplay context */
    VideoState* is;

    /* format/codec options */
    AVDictionary* format_opts;
    AVDictionary* codec_opts;
    AVDictionary* sws_dict;
    AVDictionary* player_opts;
    AVDictionary* swr_opts;

    /* ffplay options specified by the user */
#ifdef FFP_MERGE
    AVInputFormat* file_iformat;
#endif
    char* input_filename;

#ifdef FFP_MERGE
    const char* window_title;
    int fs_screen_width;
    int fs_screen_height;
    int default_width;
    int default_height;
    int screen_width;
    int screen_height;
#endif
    int audio_disable;
    int video_disable;
#ifdef FFP_MERGE
    int subtitle_disable;
#endif
    const char* wanted_stream_spec[AVMEDIA_TYPE_NB];
    int seek_by_bytes;
    int display_disable;
    int show_status;
    int av_sync_type;
    int64_t start_time;
    int64_t duration;
    int fast;
    int genpts;
    int lowres;
    int decoder_reorder_pts;
    int autoexit;
#ifdef FFP_MERGE
    int exit_on_keydown;
    int exit_on_mousedown;
#endif
    int loop;
    int enable_loop_on_error;
    int exit_on_dec_error;
    int framedrop;
    int64_t seek_at_start;
    int infinite_buffer;
    enum ShowMode show_mode; // 最终会传入到VideoState
    char* audio_codec_name;
#ifdef FFP_MERGE
    char* subtitle_codec_name;
#endif
    char* video_codec_name;
    double rdftspeed;

    // kwai logic
    char* preferred_hevc_codec_name;

#ifdef FFP_MERGE
    int64_t cursor_last_shown;
    int cursor_hidden;
#endif
#if CONFIG_AVFILTER
    const char** vfilters_list;
    int nb_vfilters;
    char* afilters;
    char* vfilter0;
#endif
    int autorotate;

    unsigned sws_flags;

    /* current context */
#ifdef FFP_MERGE
    int is_full_screen;
#endif
    int64_t audio_callback_time;
#ifdef FFP_MERGE
    SDL_Surface* screen;
#endif

    /* extra fields */
    SDL_Aout* aout;
    SDL_Vout* vout;
    struct IJKFF_Pipeline* pipeline;
    struct IJKFF_Pipenode* node_vdec;
    int sar_num;
    int sar_den;

    char* video_codec_info;
    char* audio_codec_info;
    Uint32 overlay_format;

    int prepared;
    int auto_resume;
    int error;
    int error_count;
    int v_dec_err;
    int start_on_prepared;
    int sync_av_start;

    // kwai logic
    int first_video_frame_rendered;
    int first_audio_frame_rendered;
    volatile int video_rendered_after_seek_need_notify;
    volatile int audio_render_after_seek_need_notify;

    MessageQueue msg_queue;

    int64_t playable_duration_ms;

    // kwai logic pkts with pts < 0 won't be rendered
    int64_t audio_invalid_duration;
    int64_t video_invalid_duration;
    bool audio_pts_invalid;
    bool video_pts_invalid;

    // 这个字段表示是否会进行缓冲，VideoState里的buffer_on表示是否正在进行缓冲
    int packet_buffering;
    int pictq_size;
    int max_fps;

    int aac_libfdk;

    int use_aligned_pts;

    int vtb_max_frame_width;
    int vtb_async;
    int vtb_wait_async;
    int vtb_h264;
    int vtb_h265;
    int vtb_auto_rotate;

    int mediacodec_all_videos;
    int mediacodec_avc;
    int mediacodec_hevc;
    int mediacodec_mpeg2;
    int mediacodec_auto_rotate;
    int mediacodec_avc_height_limit;   // supported avc height limitation
    int mediacodec_hevc_height_limit;   // supported hevc height limitation
    int mediacodec_avc_width_limit;   // supported avc width limitation
    int mediacodec_hevc_width_limit;   // supported hevc width limitation
    int mediacodec_max_cnt;  // Max configurable MediaCodec Num: 9, default value: 1
    int use_mediacodec_bytebuffer; // whether enable bytebuffer-mode, disable in default

    int opensles;

    char* iformat_name;

    int no_time_adjust;

    struct IjkMediaMeta* meta;

    // used to count frame rate of reading packets
    SDL_SpeedSampler vfps_sampler;
    // used to count frame rate of decoding packets
    SDL_SpeedSampler vdps_sampler;
    // used to count frame rate of displaying frames
    SDL_SpeedSampler vrps_sampler;

    /* filters */
    SDL_mutex*  vf_mutex;
    SDL_mutex*  af_mutex;
    int         vf_changed;
    int         af_changed;
    float       pf_playback_rate;
    int         pf_playback_rate_changed;

    FFStatistic         stat;
    FFDemuxCacheControl dcc;

    int         pf_playback_tone;

    // kwai ,鲜学需求，用soundtouch变速
    bool pf_playback_rate_is_sound_touch;
    bool pf_playback_tone_is_sound_touch;

    int enable_accurate_seek;
    int accurate_seek_timeout;
    int enable_seek_forward_offset;

    // kwai code
    int cached_enable_accurate_seek;

    // ======================== Kwai start from here  ========================
    volatile unsigned session_id;


    // 原金山逻辑，目前仍在用 QyVideo read data size
    int64_t i_video_decoded_size;
    int64_t i_audio_decoded_size;

    //  金山代码，短视频的追帧逻辑
    int         i_buffer_time_max;
    // live only Add audio_speed up/down logic for live adaptive @wangtao
    int         i_buffer_time_max_live_manifest;

    // configs
    char* host;      // options
    int islive;     // options
    int tag1;        // options
    // decode_interrupt_cb的timeout，后续可以改名字
    int64_t timeout;
    int64_t app_start_time; // app start play time

    int muted;

    int kwai_error_code;

    int enable_cache_seek;

    // avoid report buffering end many times. fixme 这块会影响上层的卡顿时长漏统计么？ @刘玉鑫
    int block_start_cnt;

    // 上报FFP_MSG_BUFFERING_UPDATE的相关逻辑
    struct {
        bool eof_reported;  // eof的情况只上报最后一次即可

        int64_t last_report_buf_time_position; // 1~100,重复的percent不上报
        int last_report_buf_hwm_percent; // 1~100,重复的percent不上报

        int64_t last_check_buffering_ts_ms;
    } buffer_update_control;

#define DECODE_ERROR_WARN_THRESHOLD 5
    int continue_audio_dec_err_cnt;

    // Android only , for pip
    void* weak_thiz;
    bool should_export_video_raw;
    int64_t exported_pcm_ts_ms;
    bool should_export_process_audio_pcm;
    void* java_process_pcm_byte_buffer;
    SDL_mutex* pcm_process_mutex;

    //iOS only audio callback
    void* extra;

    // 记录是否用了硬解
    bool hardware_vdec;

    SDL_mutex* volude_mutex;
    float volumes[2];
    int last_packet_drop;


    // live event callback handle, only for iOS
    void* live_event_callback;

    // live only ，Qos statistic，用来对齐推流端到播放端延迟的
    bool wall_clock_updated;
    int64_t wall_clock_offset; // LocalClock - WallClock, in ms
    int64_t qos_pts_offset;
    bool qos_pts_offset_got;
    int64_t qos_dts_duration;

    // live only,
    DelayStat qos_delay_audio_render;
    DelayStat qos_delay_video_recv;
    DelayStat qos_delay_video_before_dec;
    DelayStat qos_delay_video_after_dec;
    DelayStat qos_delay_video_render;

    // live only ，private nal里的 QOS_VENC_DYN_PARAM_LEN，只做qos用途
#define QOS_VENC_DYN_PARAM_LEN 512
    char qos_venc_dyn_param[QOS_VENC_DYN_PARAM_LEN];

    // live only
    LiveAbsTimeControl live_abs_time_control;

    // live only, pk场景, fromprivate nal，，信息里的mixType，如果有变化要通知上层 FFP_MSG_LIVE_TYPE_CHANGE 消息
    int mix_type;
    // live only, pk场景, private nal，
    int source_device_type;

    // live only,直播语音评论
    int64_t live_voice_comment_time;


    // live only,直播多码率
    int is_live_manifest;      // for ffp_context_options
    int live_manifest_last_decoding_flv_index;
    int live_manifest_switch_mode;
    KFlvPlayerStatistic kflv_player_statistic;


    // live onlydepreated，直播中自动转屏功能
    KwaiRotateControl kwai_rotate_control;

    KwaiQos kwai_qos;

    int enable_modify_block;

    // 音频组给的 音频增益模块，增加音频响度，c++源码形式集成的
    AudioGain audio_gain;

    // 音频频谱回调
    int enable_audio_spectrum;
    void* audio_spectrum_processor;

    // 开播的音频渐入
    AudioVolumeProgress audio_vol_progress;

    // 使用SoundTouch库实现音频的变速变调播放
    int audio_speed_change_enable;
    // 音频渲染中变速的统计模块，只做qos用途
    SpeedChangeStat qos_speed_change;

    // live low delay
    int live_low_delay_buffer_time_max;

    // live reload_audio
    char* reload_audio_filename;
    bool is_audio_reloaded;
    int64_t last_audio_pts;
    double last_vp_pts_before_audio_only;

    // live reload_video
    bool is_video_reloaded;
    int64_t last_only_audio_pts;
    bool last_only_audio_pts_updated;
    int64_t reloaded_video_first_pts;
    int first_reloaded_v_frame_rendered;
    bool last_audio_pts_updated;  // only update in video_read_thread

    // QY265's auth obj, opaque
    void* qy265_auth_opaque;

    // 预加载 有1和2两个版本，老的是1，新的是2
    int use_pre_demux_ver; // for ffp_context_options
    PreDemux* pre_demux;

    // abloop
    AbLoop ab_loop;

    BufferLoop buffer_loop;

    // 新的无锁的时钟轴
    ClockTracker clock_tracker;


    // todo OOP refactor this cache related members
    // for awesome cache
    char* cache_key;
    int expect_use_cache;       // for ffp_context_options
    int enable_segment_cache;    //for hls
    bool cache_actually_used;
    // cache_avio_overrall_mutex 负责 cache_avio_context的create/open/close/destroy的互斥，其他的地方勿使用
    SDL_mutex* cache_avio_overrall_mutex;
    SDL_mutex* cache_avio_exist_mutex;
    AVIOContext* cache_avio_context;
    struct CCacheSessionListener* cache_session_listener;
    // 新的callback生命周期需要ffplay来负责delete
    AwesomeCacheCallback_Opaque cache_callback;

    C_DataSourceOptions data_source_opt;
    CacheStatistic cache_stat;
    char session_uuid[SESSION_UUID_BUFFER_LEN];

    // 短视频多码率 vod multirate
    VodPlayList vodplaylist;
    int enable_vod_manifest;
    uint32_t vod_adaptive_rep_id;

    // IndexContent，hls等格式
    int input_data_type;
    IndexContent index_content;

    int prefer_bandwidth;  //多码率默认码率，暂时只有hls用到

    // 表示此次seek是否是loop导致的seek
    bool is_loop_seek;

#define CDN_KWAI_SIGN_MAX_LEN   128
    char live_kwai_sign[CDN_KWAI_SIGN_MAX_LEN];

    // kwai http redirect json str
    char* http_redirect_info;

#define CDN_X_KS_CACHE_MAX_LEN   2048
    char live_x_ks_cache[CDN_X_KS_CACHE_MAX_LEN];

    // buffer控制模块，包括启播buffer策略 和 播放中的开始缓冲/结束缓冲策略
    KwaiPacketQueueBufferChecker kwai_packet_buffer_checker;

    // 短视频，带宽控制策略
    DccAlgorithm dcc_algorithm;

    KwaiIoQueueObserver kwai_io_queue_observer;

    // p2sp需要用到的播放器统计信息
    ac_player_statistic_t player_statistic;

    // 异步打开解码器优化，目前只在ios上开启，在android上无收益，
    // stream index for async stream_component_open
    int st_index[AVMEDIA_TYPE_NB];
    int async_stream_component_open;

    // fadein
    int fade_in_end_time_ms;

    // hdr
    bool is_hdr;

    // 转码类型，目前用户上报，并且春节活动是强依赖的
    char trancode_type[TRANSCODE_TYPE_MAX_LEN + 1]; // URL里的转码格式，比如h11,h8
} FFPlayer;

inline static int is_playback_rate_normal(float rate) {
    return (rate < DEFAULT_PLAYBACK_RATE + 0.01) && (rate > DEFAULT_PLAYBACK_RATE - 0.01);
}

inline static void ffp_notify_msg1(FFPlayer* ffp, int what) {
    msg_queue_put_simple3(&ffp->msg_queue, what, 0, 0);
}

inline static void ffp_notify_msg2(FFPlayer* ffp, int what, int arg1) {
    msg_queue_put_simple3(&ffp->msg_queue, what, arg1, 0);
}

inline static void ffp_notify_msg3(FFPlayer* ffp, int what, int arg1, int arg2) {
    msg_queue_put_simple3(&ffp->msg_queue, what, arg1, arg2);
}

inline static void ffp_remove_msg(FFPlayer* ffp, int what) {
    msg_queue_remove(&ffp->msg_queue, what);
}


inline static void* ffp_set_weak_thiz(FFPlayer* ffp, void* weak_thiz) {
    ALOGD("[%u] [ffp_set_weak_thiz], weak_thiz:%p", ffp->session_id, weak_thiz);
    void* prev_weak_thiz = ffp->weak_thiz;
    ffp->weak_thiz = weak_thiz;
    return prev_weak_thiz;
}

inline static void* ffp_set_process_pcm_buffer(FFPlayer* ffp, void* byteBuffer) {
    void* tmp_buffer = NULL;
    SDL_LockMutex(ffp->pcm_process_mutex);
    if (byteBuffer == ffp->java_process_pcm_byte_buffer) {
        ALOGW("[%u] [ffp_set_process_pcm_buffer] set same buf", ffp->session_id);
    } else {
        tmp_buffer = ffp->java_process_pcm_byte_buffer;
    }
    ffp->java_process_pcm_byte_buffer = byteBuffer;

    if (byteBuffer != NULL) {
        ffp->should_export_process_audio_pcm = true;
    } else {
        ALOGW("[%u] [ffp_set_process_pcm_buffer] byteBuffer is null", ffp->session_id);
        ffp->should_export_process_audio_pcm = false;
    }
    SDL_UnlockMutex(ffp->pcm_process_mutex);
    return tmp_buffer;
}

inline static const char* ffp_get_error_string(int error) {
    switch (error) {
        case AVERROR(ENOMEM):       return "AVERROR(ENOMEM)";       // 12
        case AVERROR(EINVAL):       return "AVERROR(EINVAL)";       // 22
        case AVERROR(EAGAIN):       return "AVERROR(EAGAIN)";       // 35
        case AVERROR(ETIMEDOUT):    return "AVERROR(ETIMEDOUT)";    // 60
        case AVERROR_EOF:           return "AVERROR_EOF";
        case AVERROR_EXIT:          return "AVERROR_EXIT";
    }
    return "unknown";
}


#endif
