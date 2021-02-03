//
// Created by MarshallShuai on 2019/4/24.
//
#pragma once

#include "ff_ffinc.h"
#include "ijksdl_thread.h"
#include "ijksdl_mutex.h"
#include "ff_ffplay_clock.h"
#include "ff_ffplay_decoder_internal.h"
#include "sound_touch_c.h"
#include "ff_frame_queue.h"
#include "kwai_live_voice_comment_queue.h"
#include "kwai_live_event_queue.h"
#include "kwai_live_type_queue.h"

/* Minimum SDL audio buffer size, in samples. */
#define SDL_AUDIO_MIN_BUFFER_SIZE 512
/* Calculate actual buffer size keeping in mind not cause too frequent audio callbacks */
#define SDL_AUDIO_MAX_CALLBACKS_PER_SEC 30

/* Step size for volume control */
#define SDL_VOLUME_STEP (SDL_MIX_MAXVOLUME / 50)


/* NOTE: the size must be big enough to compensate the hardware audio buffersize size */
/* TODO: We assume that a decoded and resampled frame fits into this buffer */
#define SAMPLE_ARRAY_SIZE (8 * 65536)

typedef enum ShowMode {
    SHOW_MODE_NONE = -1,
    SHOW_MODE_VIDEO = 0,
    SHOW_MODE_WAVES,
    SHOW_MODE_RDFT,
    SHOW_MODE_NB
} ShowMode;

typedef struct AudioParams {
    int freq;
    int channels;
    int64_t channel_layout;
    enum AVSampleFormat fmt;
    int frame_size;
    int bytes_per_sec;
} AudioParams;

enum AV_SYNC_TYPE {
    AV_SYNC_AUDIO_MASTER, /* default choice */
    AV_SYNC_VIDEO_MASTER,
    AV_SYNC_EXTERNAL_CLOCK, /* synchronize to an external clock */
};


// 音频变速，目前只有直播用到了，包括 直播绝对时钟控制(LiveAbsTimeControl) 和 live_pkt_in_play_range的逻辑
#define KS_AUDIO_PLAY_SPEED_NORMAL (100)
#define KS_AUDIO_PLAY_SPEED_UP_1 (125)
#define KS_AUDIO_PLAY_SPEED_UP_2 (150)
#define KS_AUDIO_PLAY_SPEED_DOWN (75)

typedef struct VideoState {
    SDL_Thread* read_tid;
    SDL_Thread _read_tid;

    // kwai logic, live only, 直播无缝切后台，reload audio
    SDL_Thread* audio_read_tid;
    SDL_Thread _audio_read_tid;
    SDL_cond* continue_audio_read_thread;
    // kwai logic,live only, 直播无缝切后台，reload video from audioonly
    SDL_Thread* video_read_tid;
    SDL_Thread _video_read_tid;
    SDL_cond* continue_video_read_thread;

    AVInputFormat* iformat;
    int abort_request;
    int read_abort_request; //疑似金山逻辑
    int force_refresh;
    int paused;
    int last_paused;
    int queue_attachments_req;
    int seek_req;
    int seek_flags;
    int64_t seek_pos;
    int64_t seek_rel;
    // kwai loic
    int seek_cached_flags;
    int64_t seek_cached_pos;
    int64_t seek_cached_rel;
#ifdef FFP_MERGE
    int read_pause_return;
#endif
    AVFormatContext* ic;
    int realtime;

    Clock audclk;
    Clock vidclk;
    Clock extclk;

    FrameQueue pictq;
#ifdef FFP_MERGE
    FrameQueue subpq;
#endif
    FrameQueue sampq;

    Decoder auddec;
    Decoder viddec;
#ifdef FFP_MERGE
    Decoder subdec;

    int viddec_width;
    int viddec_height;
#endif

    int audio_stream;

    enum AV_SYNC_TYPE av_sync_type;

    double audio_clock;
    int audio_clock_serial;
    double audio_diff_cum; /* used for AV difference average computation */
    double audio_diff_avg_coef;
    double audio_diff_threshold;
    int audio_diff_avg_count;
    AVStream* audio_st;
    PacketQueue audioq;
    int audio_hw_buf_size;
    uint8_t silence_buf[SDL_AUDIO_MIN_BUFFER_SIZE];
    uint8_t* audio_buf;
    uint8_t* audio_buf1;
    unsigned int audio_buf_size; /* in bytes */
    unsigned int audio_buf1_size;
    int audio_buf_index; /* in bytes */
    int audio_write_buf_size;
    int audio_volume;
    int muted;
    struct AudioParams audio_src;
#if CONFIG_AVFILTER
    struct AudioParams audio_filter_src;
#endif
    struct AudioParams audio_tgt;
    struct SwrContext* swr_ctx;
    int frame_drops_early;  // 解码后直接丢的帧数（主要是通过判断当前是否已经落后音频了）
    int frame_drops_late;   // 渲染的时候丢的帧数
    int continuous_frame_drops_early;

    enum ShowMode show_mode;
    int16_t sample_array[SAMPLE_ARRAY_SIZE];
    int sample_array_index;
    int last_i_start;
#ifdef FFP_MERGE
    RDFTContext* rdft;
    int rdft_bits;
    FFTSample* rdft_data;
    int xpos;
#endif
    double last_vis_time;

#ifdef FFP_MERGE
    int subtitle_stream;
    AVStream* subtitle_st;
    PacketQueue subtitleq;
#endif

    double frame_timer;
    double frame_last_returned_time;
    double frame_last_filter_delay;
    int video_stream;
    AVStream* video_st;
    PacketQueue videoq;
    double max_frame_duration;      // maximum duration of a frame - above this, we consider the jump a timestamp discontinuity
#if !CONFIG_AVFILTER
    struct SwsContext* img_convert_ctx;
#endif
#ifdef FFP_SUB
    struct SwsContext* sub_convert_ctx;
    SDL_Rect last_display_rect;
#endif
    int eof;

    char* filename;
    int width, height, xleft, ytop;
    int step;

    // kwai logic
    int rotation;

#if CONFIG_AVFILTER
    int vfilter_idx;
    AVFilterContext* in_video_filter;   // the first filter in the video chain
    AVFilterContext* out_video_filter;  // the last filter in the video chain
    AVFilterContext* in_audio_filter;   // the first filter in the audio chain
    AVFilterContext* out_audio_filter;  // the last filter in the audio chain
    AVFilterGraph* agraph;              // audio filter graph
#endif

    int last_video_stream, last_audio_stream, last_subtitle_stream;

    SDL_cond* continue_read_thread;

    /* extra fields */
    SDL_mutex*  play_mutex; // only guard state, do not block any long operation
    SDL_Thread* video_refresh_tid;
    SDL_Thread _video_refresh_tid;
    int video_refresh_abort_request;  // exit video render thread request

    /* kwai logic, live only, live manifest statistic thread */
    SDL_Thread* live_manifest_tid;
    SDL_Thread _live_manifest_tid;
    SDL_cond* continue_kflv_thread;

    /* kwai logic, audio/video component open thread */
    SDL_Thread* a_component_open_tid;
    SDL_Thread _a_component_open_tid;
    SDL_Thread* v_component_open_tid;
    SDL_Thread _v_component_open_tid;

    int buffering_on;
    int pause_req;

    int dropping_frame;
    int is_video_high_fps; // above 30fps
    int is_video_high_res; // above 1080p

    PacketQueue* buffer_indicator_queue;

    volatile int latest_seek_load_serial;
    volatile int64_t latest_seek_load_start_at;

    int drop_aframe_count;
    int drop_vframe_count;
    int64_t accurate_seek_start_time;
    volatile int64_t accurate_seek_vframe_pts;
    int audio_accurate_seek_req;
    int video_accurate_seek_req;
    int accurate_seek_notify;
    SDL_mutex* accurate_seek_mutex;
    SDL_cond*  video_accurate_seek_cond;
    SDL_cond*  audio_accurate_seek_cond;


    //  ================ kwai start  ================
    int64_t read_start_time;
    int i_buffer_time_max;

    int chasing_enabled;
    int chasing_status;

    int cache_seeked;


    double last_vp_pts;
    int64_t last_vp_pos;
    int last_vp_serial;
    int64_t debug_last_frame_render_ts;

    int show_frame_count;
    int v_frame_enqueue_count; // 视频帧解析出来之后，没丢弃的帧，被enqueue 到 frame queue的帧数

    int interrupt_exit;
    // compatible for illegal audio pts
    int64_t prev_audio_pts;
    int illegal_audio_pts;
    int is_illegal_pts_checked;

    int64_t dts_of_last_frame;  //added for metrics  // 另外貌似是为了解决直播中dts不严格递增的相关bug --kikyo
    int64_t prev_keyframe_dts;
    int64_t bytes_read; // bytes read

    // indicating whether the video and audio is aligned.
    int av_aligned;
    int64_t video_duration;
    int64_t audio_duration;

    // audio speed up/down
    int audio_speed_percent;
    SoundTouchC* sound_touch;

    // httpdns
    char* http_headers; // http请求的headers，主要是包含了httpdns的host
    // for qos
    char* server_ip;
    int audio_bit_rate;
    char* audio_profile;

    // live voice comment
    VoiceCommentQueue vc_queue;

    // live event for pass-throung
    LiveEventQueue event_queue;

    // live type
    LiveTypeQueue live_type_queue;

    //fps
    float probe_fps;

    SDL_mutex*  cached_seek_mutex;

    bool is_video_first_dts_got;
    bool is_audio_first_dts_got;
    int64_t last_audio_dts_ms;
    int64_t last_video_dts_ms;
    int64_t audio_first_dts;
    int64_t video_first_dts;
    int64_t first_dts;
    // ================ kwai end ================

    int64_t video_first_pts_ms;
    bool is_audio_pts_aligned;//直播时，音频的开始部分packet pts可能会小于视频的第一个packet pts，起播时会丢弃这部分音频数据，直到pts与视频对齐
} VideoState;


void video_state_set_av_sync_type(VideoState* is, enum AV_SYNC_TYPE av_sync_type);
