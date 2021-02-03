//
// Created by 帅龙成 on 15/09/2017.
//

#ifndef IJKPLAYER_KWAI_QOS_H
#define IJKPLAYER_KWAI_QOS_H
#include <pthread.h>
#include <stdbool.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <awesome_cache/include/awesome_cache_c.h>
#include <awesome_cache/include/dcc_algorithm_c.h>
#include "ijkkwai/kwai_player_qos.h"
#include "qos/qos_live_realtime.h"
#include "ijkkwai/kwai_time_recorder.h"
#include "ijksdl/ijksdl_mutex.h"
#include "kwai_packet_queue_strategy.h"
#include <libavkwai/cJSON.h>

#define TRANSCODE_TYPE_MAX_LEN 16

typedef struct FFPlayer FFPlayer;
typedef struct VideoState VideoState;
typedef struct VodPlayList VodPlayList;
typedef struct VodRateAdaptConfig VodRateAdaptConfig;
typedef struct VodRateAdaptConfigA1 VodRateAdaptConfigA1;

enum VIDEO_TOOL_BOX_MODE {
    VIDEO_TOOL_BOX_ASYNC,
    VIDEO_TOOL_BOX_SYNC
};

enum {
    MEDIACODEC_OUTPUT_ERROR_TRY_AGAIN_LATER = -1,
    MEDIACODEC_OUTPUT_ERROR_BUFFERS_CHANGED = -3,
    MEDIACODEC_OUTPUT_ERROR_UNKNOWN = -1000
};


// vod adaptive
typedef struct VodAdaptiveQos {
    uint32_t max_bitrate_kbps;      // max bitrate in kbps
    uint32_t avg_bitrate_kbps;      // average bitrate in kbps
    uint32_t width;
    uint32_t height;
    uint32_t device_width;
    uint32_t device_height;
    float quality;
    int32_t low_device;
    int32_t switch_code;
    int32_t algorithm_mode;

    uint64_t consumed_download_ms;
    uint64_t actual_video_size_byte;
    uint32_t average_download_rate_kbps;
    cJSON* representations;
    SDL_mutex* mutex;

    // get from abr
    uint64_t idle_last_request_ms;
    uint32_t short_throughput_kbps;
    uint32_t long_throughput_kbps;
    uint32_t real_time_throughput_kbps;
    char* switch_reason;
    char* detail_switch_reason;
    char* bandwidth_computer_process;
    char* representations_str;
    char* net_type;
    char* cur_url;
    char* cur_host;
    char* cur_key;
    char* cur_quality_show;
    char transcode_type[TRANSCODE_TYPE_MAX_LEN + 1];
    uint32_t is_vod_adaptive;
    int cached;

    // rate config
    uint32_t rate_addapt_type;
    uint32_t bandwidth_estimation_type;
    uint32_t absolute_low_res_low_device;
    uint32_t adapt_under_4G;
    uint32_t adapt_under_wifi;
    uint32_t adapt_under_other_net;
    uint32_t absolute_low_rate_4G;
    uint32_t absolute_low_rate_wifi;
    uint32_t absolute_low_res_4G;
    uint32_t absolute_low_res_wifi;
    uint32_t short_keep_interval;
    uint32_t long_keep_interval;
    uint32_t bitrate_init_level;
    float default_weight;
    uint32_t block_affected_interval;
    float wifi_amend;
    float fourG_amend;
    float resolution_amend;
    uint32_t device_width_threshold;
    uint32_t device_hight_threshold;
    uint32_t priority_policy;
    uint32_t max_resolution;
} VodAdaptiveQos;

// awesome cache
typedef struct AwesomeCacheQos {
    // config
#define CACHE_KEY_MAX_LEN 128
    char cfg_cache_key[CACHE_KEY_MAX_LEN + 1];
    int data_source_type; // 值对应 cache_defs.h里的DataSourceType
    int upstream_type;
    int buffered_type;

    // for general, player runtime info
    int is_fully_cached_on_open;
    int is_fully_cached_cnt_on_loop;
    int is_not_fully_cached_cnt_on_loop;


    // for ffmpeg_adapter
    int adapter_error;
    int read_cost_ms;
    // for ffmpeg_adapter debug, can be removed later
    int seek_size_cnt;
    int seek_set_cnt;
    int seek_cur_cnt;
    int seek_end_cnt;

    char* stats_json_str;   // AwesomeCache的模块malloc出来的内存，需要用freep释放
    char http_version[HTTP_VERSION_MAX_LEN + 1];

    // ---- from AwesomeCacheRuntimeInfo start ----
    bool ignore_cache_on_error;
    int reopen_cnt;
    int total_bytes;
    int cached_bytes;

    // for buffered data source
    int buffered_datasource_size_kb;
    int buffered_datasource_seek_threshold_kb;

    //for curl content-range wrong
    int drop_data_cnt;
    int drop_data_bytes;

    int curl_buffer_size_kb;
    int curl_buffer_max_used_kb;
    int http_retried_cnt;
    int curl_byte_range_error;

    int sock_orig_size_kb;
    int sock_cfg_size_kb;
    int sock_act_size_kb;

    // for AsyncV2
    bool need_report_header;
    char invalid_header[INVALID_RESPONSE_HEADER]; // 当返回kCurlHttpResponseHeaderInvalid的时候，上报此内容
    int http_response_code;
    int curl_ret;
    int64_t downloaded_bytes;
    int64_t recv_valid_bytes;
    int resume_file_fail_cnt;
    int flush_file_fail_cnt;
    int64_t cached_bytes_on_play_start;

    // for SyncCacheDataSource
    int stop_reason;
    int cache_read_source_cnt;
    int cache_write_source_cnt;
    int cache_upstream_source_cnt;

    // for AsyncCacheDataSource
    int byte_range_size;
    int first_byte_range_length;
    int download_exit_reason;
    int read_from_upstream;
    int64_t read_position;
    int64_t bytes_remaining;
    int pre_download_cnt;

    // for TeeDataSource
    int sink_write_cost_ms;
    // for DownloadTask
    int con_timeout_ms;
    int read_timeout_ms;
    int download_feed_input_cost_ms;
    int download_total_cost_ms;

    // for vod p2sp
    bool p2sp_enabled;
    int64_t p2sp_cdn_bytes;
    int64_t p2sp_bytes_used;
    int64_t p2sp_bytes_repeated;
    int64_t p2sp_bytes_received;
    int64_t p2sp_bytes_requested;
    int64_t p2sp_start;
    int p2sp_error_code;
    int64_t p2sp_first_byte_duration;
    int64_t p2sp_first_byte_offset;
    char* p2sp_sdk_details;

    // ---- from AwesomeCacheRuntimeInfo end ----

    int fs_error_code;
    long os_errno;
} AwesomeCacheQos;

typedef struct _KwaiQos {
    // basic data ,useful for data analysis when group by;
    struct {
        const char* sdk_version;
    } basic;

    struct {
        char* filename;
        char trancode_type[TRANSCODE_TYPE_MAX_LEN + 1]; // URL里的转码格式，比如h11,h8
        char* server_ip;
        char* host;     //应用设置headers内的host(httpdns)
        char* domain;   //域名 无论是否走httpdns
        char* stream_id;    //直播流id
        char* product_context; //APP设置 上下文信息，如重试次数，业务场景

        int64_t seek_at_start_ms;       //启播seek到某个位置
        int max_buffer_dur_ms;       //播放器缓冲大小Duration
        int max_buffer_strategy;     //dcc的策略
        int max_buffer_dur_bsp_ms;   //开播前(before start play)的播放器缓冲大小Duration
        int max_buffer_size;        //播放器缓冲大小Byte
        int last_high_water_mark_in_ms;   // 播放器缓冲大小动态增加最大水位值ms
        int start_on_prepared;      //是否prepare后直接start，不等应用调用start
        int enable_accurate_seek;   //是否使用精准seek
        int enable_seek_forward_offset;    //是否支持seek前向偏差
        bool islive;                 //是否直播业务
        int is_last_try;             //是否最后一次重试

        int use_pre_load;
        int pre_load_duraion_ms;
        int pre_load_finish;

        int use_awesome_cache;
        int enable_segment_cache;
        int input_data_type;
        int tag1;
        int prefer_bandwidth;

        uint32_t overlay_format;   // overlay output format
        int64_t  app_start_time;

    } player_config;

    struct {
        bool is_used;
        int pre_read_ms_used;
        int cfg_mbth_10;
        int cfg_pre_read_ms;
        int cmp_mark_kbps;
        float actual_mb_ratio;
    } exp_dcc;

    // 目前VideoStatEvent里的类型，先放在这留之后use_httpdns实现参考
//    enum IpSource {   // ip地址的获取来源模块
//        UNKNOWN3 = 0;
//        LOCAL = 1;      // 本地解析dns（非httpdns功能）获取
//        HTTP_DNS = 2;   // 从httpdns功能获取
//        AUTO = 3;     // 播放器从url获取
//    }

    // 多媒体流的metadata解析出来的静态指标
    struct {
        float fps;
        double duration;
        double audio_duration;
        char* comment;
        int channels;
        int sample_rate;

        char* video_codec_info;
        int64_t video_bit_rate;

        char* audio_codec_info;
        int64_t audio_bit_rate;
        char* audio_profile;

        int width;
        int height;
        int64_t bitrate;

        // AVsync指标
        int64_t a_first_pkg_dts;
        int64_t v_first_pkg_dts;

        // Transcoder version
        char* transcoder_ver;
        // used for live transcoding type
        // 包括：CDN转码、源站转码、明眸转码
        char* transcoder_group;

        char* stream_info;
        char* input_fomat;

        // color-space: BT601/709
        enum AVColorSpace color_space;
        //
    } media_metadata;

    // ts 是timestamp简称，表示打点时间戳
    int64_t ts_start_prepare_async;
    int64_t ts_app_start;
    int64_t ts_av_input_opened;
    int64_t ts_av_find_stream_info;
    // --> ts_first_video_pkt_received 当用预加载方式的时候，ts_first_video_pkt_received会发生在这个位置
    int64_t ts_predmux_finish;    // find info后有一个pre demux的耗时，这个时长包括了纯preDemux的耗时，和preDemux完成后等待客户端调用startPlay的耗时
    int64_t ts_open_decoder;
    int64_t ts_all_prepared;
    int64_t ts_first_video_pkt_received;    // 这个值不能被过度依赖，因为在预加载的情况下，会发生在ts_open_decoder之前
    int64_t ts_first_audio_pkt_received;
    int64_t ts_before_decode_first_frame;
    int64_t ts_after_decode_first_frame;
    int64_t ts_before_decode_first_audio_frame;
    int64_t ts_after_decode_first_audio_frame;
    int64_t ts_first_render_ready;    // 启播buffer满足条件可以开播
    int64_t ts_frist_frame_rendered;
    int64_t ts_frist_sample_rendered;
    int64_t ts_first_frame_filled;    // 首帧已经填充到显示buffer

    int64_t ts_playing_begin; // open decoder后可能因为 start-on-prepare为0，还会等等待app真正调用start，这里会有一段耗时

    bool read_thread_for_loop_started;                // 表示read_thread已经进入play流程（for循环阶段）

    // 一些耗时相关指标
    struct {
        // 建连相关指标,建连后从ffmpeg的format_opts获取
        int64_t cost_dns_analyze;
        int64_t cost_http_connect;
        int64_t cost_http_first_data;

        struct QosConnectInfo {
            int64_t cost_dns_analyze;
            int64_t cost_http_connect;
            int64_t cost_http_first_data;
            int64_t first_data_interval;
        } connect_infos[CONNECT_INFO_COUNT];

        // 首屏相关的一些指标
        int64_t cost_prepare_ms;            // ts_start_prepare_asnc -> ts_all_prepared （需要减掉step_pre_demux的cost）
        int64_t cost_first_screen;          // ts_start_prepare_asnc -> ts_frist_frame_rendered（需要减掉step_pre_demux和step_wait_for_playing_begin的cost）
        int64_t cost_first_sample;
        int64_t cost_start_play_block;      // 从prepared 到 start_play_block的条件满足的时间间隔
        int64_t cost_total_first_screen;    // 从preare_aync 到 first_screen总间隔
        int64_t cost_total_first_sample;    // 从preare_aync 到 first_sample总间隔
        int64_t cost_pause_at_first_screen; // 从start 到 first_screen之间video pause总时间间隔
        int64_t cost_app_start_play;        // 从app 调用 prepareAync 到 app 调用 start的时间间隔
        int64_t cost_first_render_ready;    // 在app 调用 prepareAync 到 达到启播buffer条件的时间间隔
        // 这个是单独的一个step了，不再是上面这些顺序执行的step
        int64_t cost_pure_pre_demux;        //  纯preDemux的耗时，不包括preDemux完成后等待app调用start的耗时
        int64_t cost_wait_for_playing;        // 语义：播放器没在实质性工作的时候，纯等待app的时间，比如preDemux finish后，或者 preDecode的情况下 满足启播buffer了

        // 单步之间的耗时指标
        int64_t step_av_input_open;                 // ts_start_prepare_async        -> ts_av_input_opened
        int64_t step_av_find_stream_info;           // ts_av_input_opened           -> ts_av_find_stream_info
        int64_t step_pre_demux_including_waiting;   // ts_av_find_stream_info       -> ts_predmux_finish
        int64_t step_open_decoder;                  // ts_predmux_finish            -> ts_open_decoder
        int64_t step_all_prepared;                  // ts_open_decoder              -> ts_all_prepared
        int64_t step_first_video_pkt_received;      // ts_av_find_stream_info       -> ts_first_video_pkt_received
        int64_t step_pre_decode_first_video_pkt;    // ts_open_decoder              -> ts_before_decode_first_frame
        int64_t step_decode_first_frame;            // ts_before_decode_first_frame -> ts_after_decode_first_frame
        int64_t step_first_framed_rendered;         // ts_after_decode_first_frame  -> ts_frist_frame_rendered

        int64_t step_first_audio_pkt_received;      // ts_av_find_stream_info       -> ts_first_audio_pkt_received
        int64_t step_pre_decode_first_audio_pkt;    // ts_open_decoder              -> ts_before_decode_first_audio_frame
        int64_t step_decode_first_audio_frame;      // ts_before_decode_first_audio_frame    -> ts_after_decode_first_audio_frame
        int64_t step_first_audio_framed_rendered;   // ts_after_decode_first_audio_frame     -> ts_frist_audio_frame_rendered

    } runtime_cost;

    struct {
        int64_t data_after_open_input;             // bytes of data read after open_input
        int64_t data_after_stream_info;            // bytes of data read after find_stream_info
        int64_t data_fst_video_pkt;                // bytes of data read after first video packet
        int64_t data_fst_audio_pkt;                // bytes of data read after first audio packet
    } data_read;

    struct {
        char session_uuid[SESSION_UUID_BUFFER_LEN];
        int last_error;
        int setup_cache_error;
        int open_input_error;
        int cache_global_enabled;
        bool cache_used;

        int url_in_cache_whitelist;
        int loop_cnt;

        bool start_play_block_used;
        int start_play_block_type;
        int start_play_block_th; // threshold
        int64_t start_play_max_cost_ms;

        // 卡顿
        int block_cnt;
        TimeRecoder block_duration;
        bool is_blocking;

        // 首屏之前pause时间
        TimeRecoder pause_at_first_screen_duration;
        bool is_pause_at_first_screen;

        //统计前两秒发生卡顿的次数和时长
        int block_cnt_start_period;
        int64_t block_duration_start_period;

        // 播放器生命周期
        TimeRecoder alive_duration;

        // 播放时长
        TimeRecoder app_played_duration;  // 利用客户端pause start调用产生的播放时长(包含buffering seek)
        double v_played_duration;
        double a_played_duration;
        TimeRecoder actual_played_duration;    // 利用播放器内部pause start调用产生的播放时长(不包含buffering seek)

        int v_read_frame_count;
        int v_decode_frame_count;
        int render_frame_count;

        int a_read_frame_dur_ms;
        int a_decode_frame_dur_ms;
        int a_decode_err_dur_ms;
        //渲染audio frame的个数
        int render_sample_count;
        //渲染无声的audio frame的个数
        int silence_sample_count;
        // 丢帧
        int64_t begining_dropped_duration;   // 开播前几秒丢的时长
        int64_t total_dropped_duration;      // 丢packet
        int v_decoded_dropped_frame;         // video early丢帧数
        int v_render_dropped_frame;          // video late丢帧数

        // live audioonly时间戳跳变
        uint32_t audio_pts_jump_forward_cnt;         // 切换a/v后新的audio pts比切换前最后一次大，
        int64_t audio_pts_jump_forward_duration;     // 发生跳跃，错误，会导致audio播放不连续
        uint32_t audio_pts_jump_backward_index;       // 切换a/v后新的audio pts比切换前最后一次小，
        int64_t audio_pts_jump_backward_time_gap;     // 可以做到无缝，仅统计是第几次切换以及小的幅度

        // live video时间戳回退
        uint32_t video_ts_rollback_cnt;               // video时间戳回退的次数
        int64_t video_ts_rollback_duration;           // video时间戳回退的累计总时长

        // 视频软件解码出错计数
        int v_sw_dec_err_cnt;
        int v_hevc_paramete_set_change_cnt;            // video sps/pps/vps length动态变化，例如直播PK
        int v_hevc_paramete_set_update_fail_cnt;       // video sps/pps/vps length变化内存分配失败
#if defined(__APPLE__)
        // 视频硬件解码出错计数
        int v_tool_box_err_cnt;
#elif defined(__ANDROID__)
        // mediacodec解码出错
        int v_mediacodec_input_err_cnt;
        int v_mediacodec_input_err_code;
        int v_mediacodec_output_try_again_err_cnt;       // error code: -1
        int v_mediacodec_output_buffer_changed_err_cnt;  // error code: -3
        int v_mediacodec_output_unknown_err_cnt;         // error code: -1000
        int v_mediacodec_output_err_cnt;  // other output error cnt
        int v_mediacodec_output_err_code; // other output error's code
#define MEDIACODEC_CONFIG_TYPE_MAX_LEN 64
        char v_mediacodec_config_type[MEDIACODEC_CONFIG_TYPE_MAX_LEN];
        int v_mediacodec_codec_max_cnt;
#endif
        // 是否使用硬件解码
        bool v_hw_dec;

        // 解码器输出的pixel format
        enum AVPixelFormat pix_format;

        //音频设备延时
        int audio_device_latency;
        int audio_device_applied_latency;

        //视频显示错误
        int v_error_native_windows_lock;
        int v_error_unknown;
        //音频显示错误
        // To do

        //该值为正数，audio超前video的diff最大值
        int max_av_diff;
        //该值为负数，video超前audio的diff最大值
        int min_av_diff;

        int64_t a_cahce_bytes;
        int64_t a_cache_duration;
        int64_t a_cache_pakets;
        int64_t v_cache_bytes;
        int64_t v_cache_duration;
        int64_t v_cache_packets;

        int64_t max_video_dts_diff_ms;
        int64_t max_audio_dts_diff_ms;
        int32_t speed_changed_cnt;  //倍速切换次数
    } runtime_stat;

    struct {
        // seek次数与时长
        int seek_cnt;
        int seek_first_frame_cnt;
        int seek_first_packet_cnt;
        TimeRecoder seek_duration;
        TimeRecoder first_frame_after_seek;
        TimeRecoder first_packet_after_seek;
    } seek_stat;

    // awesome cache
    AwesomeCacheQos ac_cache;
    bool ac_cache_collected;

    bool is_block_start_period_set;

    struct {
        int reset_session_cnt;//硬解重启的次数
        int err_code;                   //硬解出错时的错误码
        //Android hardware decoder
        //struct {
        //} media_codec;
        // ios hardware decoder
        struct {
            bool enable;
            int mode;
            int pkt_cnt_on_err;
            int queue_is_full_cnt;
            int resolution_change;          //分辨率改变的次数
        } video_tool_box;
    } hw_decode;

    // 系统性能监控参数
    struct {
        int sample_cnt;
        int64_t last_sample_ts_ms;
        int64_t total_prof_cost_ms; // 总的用来打点消耗的时间

        // 性能监控
        uint32_t  process_cpu_pct;
        uint32_t  process_memory_size_kb;
        uint32_t  process_cpu_cnt;  // cpu cnt alive
        uint32_t  device_cpu_cnt_total;  // 这暂时只有android有


        uint32_t  last_process_cpu;
        uint32_t  last_process_memory_size_kb;

        uint32_t  last_system_cpu;
//        todo，添加prof_cost, alive_cpu_cnt, total_cpu_cnt

    } system_performance;

    AVDictionary* ic_metadata;
    AVDictionary* video_st_metadata;
    VodAdaptiveQos vod_adaptive;
    //对ic_metadata和video_st_metadata加锁，防止av_dict_set和av_dict_get多线程同时执行造成crash
    SDL_mutex* dict_mutex;

    SDL_mutex* real_time_block_info_mutex;
    cJSON* real_time_block_info; //实时卡顿相关信息，实时上报使用，每次获取后清空
    SDL_mutex* sum_block_info_mutex;
    cJSON* sum_block_info; //汇总上报卡顿相关信息

    char* audio_str;
    int enable_audio_gain;
    int enable_modify_block;
    int64_t audio_process_cost;
    int audio_track_write_error_count;

    // live realtime Qos
    QosLiveRealtime qos_live_realtime;
    QosLiveAdaptive live_adaptive;
} KwaiQos;


void KwaiQos_init(KwaiQos* qos);
void KwaiQos_close(KwaiQos* qos);
void KwaiQos_onFFPlayerReleased(KwaiQos* qos);

void KwaiQos_copyAvformatContextMetadata(KwaiQos* qos, AVFormatContext* ic);
void KwaiQos_copyVideoStreamMetadata(KwaiQos* qos, AVStream* video_stream);
// 系统性能
void KwaiQos_onSystemPerformance(KwaiQos* qos);

void KwaiQos_onError(KwaiQos* qos, int error);
// 记录视频解码出错次数
void KwaiQos_onSoftDecodeErr(KwaiQos* qos, int value);
void KwaiQos_onHevcParameterSetLenChange(KwaiQos* qos, uint8_t* ps);
#if defined(__APPLE__)
void KwaiQos_onToolBoxDecodeErr(KwaiQos* qos);
#elif defined(__ANDROID__)
void KwaiQos_onMediaCodecDequeueInputBufferErr(KwaiQos* qos, int err);
void KwaiQos_onMediaCodecDequeueOutputBufferErr(KwaiQos* qos, int err);
void KwaiQos_onMediacodecType(KwaiQos* qos, FFPlayer* ffp);
#endif
void KwaiQos_onDisplayError(KwaiQos* qos, int32_t error);

// 丢帧
void KwaiQos_onDecodedDroppedFrame(KwaiQos* qos, int value);
void KwaiQos_onRenderDroppedFrame(KwaiQos* qos, int value);

// audioonly时间戳跳变
void KwaiQos_onAudioPtsJumpForward(KwaiQos* qos, int64_t time_gap);
void KwaiQos_onAudioPtsJumpBackward(KwaiQos* qos, int64_t time_gap);
void KwaiQos_onVideoTimestampRollback(KwaiQos* qos, int64_t time_gap);

// 生命周期
void KwaiQos_onPrepareAsync(KwaiQos* qos);
void KwaiQos_onAppStart(KwaiQos* qos);
void KwaiQos_onInputOpened(KwaiQos* qos);
void KwaiQos_onStreamInfoFound(KwaiQos* qos);
void KwaiQos_onPreDemuxFinish(KwaiQos* qos, FFPlayer* ffp);
void KwaiQos_onDecoderOpened(KwaiQos* qos);
void KwaiQos_onAllPrepared(KwaiQos* qos);
void KwaiQos_onVideoPacketReceived(KwaiQos* qos);
void KwaiQos_onAudioPacketReceived(KwaiQos* qos);
void KwaiQos_onVideoFrameBeforeDecode(KwaiQos* qos);
void KwaiQos_onVideoFrameDecoded(KwaiQos* qos);
void KwaiQos_onVideoRenderFirstFrameFilled(KwaiQos* qos);
void KwaiQos_onDropPacket(KwaiQos* qos, AVPacket* pkt, AVFormatContext* ic,
                          int audio_stream, int video_stream, unsigned session_id);
void KwaiQos_onFrameRendered(KwaiQos* qos, double duration, int start_on_prepared);// video
void KwaiQos_onAudioFrameBeforeDecode(KwaiQos* qos, int64_t value);
void KwaiQos_onAudioFrameDecoded(KwaiQos* qos, int64_t value);
void KwaiQos_onAudioDecodeErr(KwaiQos* qos, int64_t value);
void KwaiQos_onSamplePlayed(KwaiQos* qos, double duration, int start_on_prepared); // audio
void KwaiQos_onSilenceSamplePlayed(KwaiQos* qos);

void KwaiQos_onReadThreadForLoopStart(KwaiQos* qos);

/**
 * 满足开播条件
 */
void KwaiQos_onReadyToRender(KwaiQos* qos);

// 卡顿时间
void KwaiQos_onBufferingStart(FFPlayer* ffp, int is_block);
void KwaiQos_onBufferingEnd(FFPlayer* ffp, int is_block);
int64_t KwaiQos_getBufferTotalDurationMs(KwaiQos* qos);

// seek时间
void KwaiQos_onSeekStart(KwaiQos* qos);
void KwaiQos_onSeekEnd(KwaiQos* qos);
void KwaiQos_onFirstFrameAfterSeekStart(KwaiQos* qos);
void KwaiQos_onFirstFrameAfterSeekEnd(KwaiQos* qos);
void KwaiQos_onFirstPacketAfterSeekStart(KwaiQos* qos);
void KwaiQos_onFirstPacketAfterSeekEnd(KwaiQos* qos);

// AVsync
void KwaiQos_setAudioFirstDts(KwaiQos* qos, int64_t value);
void KwaiQos_setVideoFirstDts(KwaiQos* qos, int64_t value);

// 统计播放器存活时长
void KwaiQos_onStartAlivePlayer(KwaiQos* qos);
void KwaiQos_onStopAlivePlayer(KwaiQos* qos);
int64_t KwaiQos_getAlivePlayerTotalDurationMs(KwaiQos* qos);

// 统计app引起的start/pause行为
void KwaiQos_onAppStartPlayer(FFPlayer* ffp);
void KwaiQos_onAppPausePlayer(FFPlayer* ffp);
/**
 * 视频实际播放时长(包含了卡顿时长).
 * 利用客户端pause start调用产生的播放时长(包含buffering seek)
 */
int64_t KwaiQos_getAppPlayTotalDurationMs(KwaiQos* qos);
// 统计播放器内部引起的start/pause行为
void KwaiQos_onStartPlayer(KwaiQos* qos);
void KwaiQos_onPausePlayer(KwaiQos* qos);
/**
 * 视频实际播放时长(不包含卡顿时长).
 * 利用客户端pause start调用产生的播放时长(不包含buffering seek)
 */
int64_t KwaiQos_getActualPlayedTotalDurationMs(KwaiQos* qos);
// 统计app调用start/pause行为
void KwaiQos_onAppCallStart(KwaiQos* qos, FFPlayer* ffp);
void KwaiQos_onAppCallPause(KwaiQos* qos);
int64_t KwaiQos_getPauseDurationAtFirstScreen(KwaiQos* qos);

void KwaiQos_onPlayToEnd(KwaiQos* qos);
void KwaiQos_setTranscodeType(KwaiQos* qos, const char* transcode_type);
void KwaiQos_collectPlayerStaticConfig(FFPlayer* ffp, const char* filename);

// 首屏时长
int64_t KwaiQos_getFirstScreenCostMs(KwaiQos* qos);

// 启播buffer
void KwaiQos_collectStartPlayBlock(KwaiQos* qos, KwaiPacketQueueBufferChecker* checker);
// DccAlg
void KwaiQos_collectDccAlg(KwaiQos* qos, DccAlgorithm* alg);
// from meta
void KwaiQos_setDomain(KwaiQos* qos, const char* domain);
char* KwaiQos_getDomain(KwaiQos* qos);
void KwaiQos_setStreamId(KwaiQos* qos, const char* stream_id);
char* KwaiQos_getStreamId(KwaiQos* qos);

void KwaiQos_setLastTryFlag(KwaiQos* qos, int is_last_try);

void KwaiQos_setDnsAnalyzeCostMs(KwaiQos* qos, int value);
void KwaiQos_setHttpConnectCostMs(KwaiQos* qos, int value);
void KwaiQos_setHttpFirstDataCostMs(KwaiQos* qos, int value);
void KwaiQos_updateConnectInfo(KwaiQos* qos, AwesomeCacheRuntimeInfo* info);
void KwaiQos_setSessionUUID(KwaiQos* qos, const char* uuid);

void KwaiQos_collectPlayerMetaInfo(FFPlayer* ffp);
void KwaiQos_collectPlayerNetInfo(FFPlayer* ffp);

void KwaiQos_setResolution(KwaiQos* qos, int width, int height);
void KwaiQos_onBitrate(KwaiQos* qos, int value);
void KwaiQos_setAudioProfile(KwaiQos* qos, const char* profile);

void KwaiQos_setVideoFramePixelInfo(KwaiQos* qos, enum AVColorSpace color_space, enum AVPixelFormat pixel_fmt);
void KwaiQos_setOverlayOutputFormat(KwaiQos* qos, uint32_t format);

void KwaiQos_setMaxAvDiff(KwaiQos* qos, int max_av_diff);
void KwaiQos_setMinAvDiff(KwaiQos* qos, int min_av_diff);

float KwaiQos_getAppAverageFps(KwaiQos* qos);   // 即时计算属性
void KwaiQos_onHardwareDec(KwaiQos* qos, bool value);
void KwaiQos_setVideoCodecInfo(KwaiQos* qos, const char* info);
void KwaiQos_setAudioCodecInfo(KwaiQos* qos, const char* info);
void KwaiQos_setStreamInfo(KwaiQos* qos, int audio_stream_count, int video_stream_count);
void KwaiQos_setInputFormat(KwaiQos* qos, const char* input_format);

void KwaiQos_setAudioDeviceLatencyMs(KwaiQos* qos, int value);
void KwaiQos_setAudioDeviceAppliedLatencyMs(KwaiQos* qos, int value);

void KwaiQos_setOpenInputReadBytes(KwaiQos* qos, int64_t bytes);
void KwaiQos_setFindStreamInfoReadBytes(KwaiQos* qos, int64_t bytes);
void KwaiQos_setAudioPktReadBytes(KwaiQos* qos, int64_t bytes);
void KwaiQos_setVideoPktReadBytes(KwaiQos* qos, int64_t bytes);

void KwaiQos_onFFPlayerOpenInputOver(KwaiQos* qos, int setup_err,
                                     bool cache_global_enabled,
                                     bool cache_used,
                                     bool url_is_cache_whitelist);
void KwaiQos_setAwesomeCacheIsFullyCachedOnOpen(KwaiQos* qos, bool cached);
void KwaiQos_setAwesomeCacheIsFullyCachedOnLoop(KwaiQos* qos, bool cached);
void KwaiQos_collectAwesomeCacheInfoOnce(KwaiQos* qos, FFPlayer* ffp);  // 非耗时操作
void KwaiQos_collectRealTimeStatInfoIfNeeded(FFPlayer* ffp);

void KwaiQos_getTranscoderVersionFromComment(KwaiQos* qos, const char* comment);

// TODO 这两个函数整理到单独模块
char* KwaiQos_getVideoStatJson(KwaiQos* qos);
char* KwaiQos_getBriefVideoStatJson(KwaiQos* qos);
void KwaiQos_getQosInfo(FFPlayer* ffp, KsyQosInfo* info); // deprecated, use QosDebugInfo instead

// live manifest
void KwaiQos_onSetLiveManifestSwitchMode(KwaiQos* qos, int mode);
int KwaiQos_getLiveManifestSwitchFlag(KwaiQos* qos);

//hw decode
void KwaiQos_onHwDecodeErrCode(KwaiQos* qos, int err_code);
void KwaiQos_onHwDecodeResetSession(KwaiQos* qos);

#if defined(__APPLE__)
// video_tool_box
void KwaiQos_onVideoToolBoxMode(KwaiQos* qos, int mode);
void KwaiQos_onVideoToolBoxPktCntOnErr(KwaiQos* qos, int count);
void KwaiQos_onVideoToolBoxQueueIsFull(KwaiQos* qos);
void KwaiQos_onVideoToolBoxResolutionChange(KwaiQos* qos);
void KwaiQos_onFfplayVideoToolBoxBuffer(KwaiQos* qos, bool is_hw, int type, int value);
#endif

void KwaiQos_setBlockInfoStartPeriod(KwaiQos* qos);
void KwaiQos_setBlockInfoStartPeriodIfNeed(KwaiQos* qos);

// vod adaptive
void KwaiQos_onVodAdaptive(KwaiQos* qos, VodPlayList* playlist, int index,
                           char* vod_resolution, VodRateAdaptConfig* rate_config,
                           VodRateAdaptConfigA1* rate_config_a1);
void KwaiQos_setBlockInfo(FFPlayer* ffp);
char* KwaiQos_getRealTimeBlockInfo(KwaiQos* qos);
char* KwaiQos_getSumBlockInfo(KwaiQos* qos);
void KwaiQos_setAudioStr(KwaiQos* qos, char* audio_str);
void KwaiQos_setEnableAudioGain(KwaiQos* qos, int enable_audio_gain);
void KwaiQos_setEnableModifyBlock(KwaiQos* qos, int enable_modify_block);
void KwaiQos_setAudioProcessCost(KwaiQos* qos, int64_t cost);
void KwaiQos_collectAudioTrackInfo(KwaiQos* qos, FFPlayer* ffp);

// overlay output format
const char* KwaiQos_getOverlayOutputFormatString(uint32_t format);
#endif //IJKPLAYER_KWAI_QOS_H
