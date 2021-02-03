//
// Created by MarshallShuai on 2019/4/19.
//
#pragma once

#include <libavformat/avformat.h>
#include "ff_ffplay_def.h"


/**
 * io线程和 直播场景video_render线程用到了下面3个宏
 */
// TODO: params need to be adjust for audio playing speed change
#define KS_AUDIO_BUFFER_SPEED_UP_1_THR_MS (2000)
#define KS_AUDIO_BUFFER_SPEED_UP_2_THR_MS (3000)
#define KS_AUDIO_BUFFER_SPEED_DOWN_THR_MS (500)


#define LOCK(_mutex) if(_mutex) SDL_LockMutex(_mutex)
#define UNLOCK(_mutex) if(_mutex) SDL_UnlockMutex(_mutex)

/**
 *
 * 这个函数要处理的几种情况：
 * 1.url是本地file，则不会走 CacheDataSource流程，而是走原生的avformat_open_input
 * 2.url是http类的情况，如果打开失败，则falback到原生avformat_open_input，如果失败或者无法fallback，则直接返回错误表示最终错误
 */
int64_t ffp_setup_open_AwesomeCache_AVIOContext(FFPlayer* ffp, const char* url,
                                                AVDictionary** options);

void ffp_avformat_close_input(FFPlayer* ffp, AVFormatContext** s);
/**
 * 这函数两个可能调用的线程：
 * 1.read_thread 结束的时候
 * 2.stream_close (调用stop_wait_l）的线程
 */
void ffp_close_release_AwesomeCache_AVIOContext(FFPlayer* ffp);


int ffp_avformat_open_input(FFPlayer* ffp, AVFormatContext** ps,
                            const char* url, AVInputFormat* iformat, AVDictionary** fmt_options);


void ffp_AwesomeCache_AVIOContext_abort(FFPlayer* ffp);

// packet_queue_get_or_buffering原来是static的，因为decoder_internal需要用到，现在暂时暴露出来
int packet_queue_get_or_buffering(FFPlayer* ffp, PacketQueue* q, AVPacket* pkt, int* serial, AVPacketTime* p_pkttime, int* finished);


void ffp_show_av_sync_status(FFPlayer* ffp, VideoState* is);

int queue_picture(FFPlayer* ffp, AVFrame* src_frame, double pts, double duration,
                  int64_t pos, int serial, AVPacketTime* p_pkttime);

int stream_component_open(FFPlayer* ffp, int stream_index);

// 如果发现下发了mediacodec不能解码的视频，不打开mediacodec
#if defined(__ANDROID__)
void check_mediacodec_availability(FFPlayer* ffp, int stream_index);
#endif

bool use_video_hardware_decoder(FFPlayer* ffp, int stream_index);

/**
 * 只有直播用到的，挑战上下buffer阈值的一个函数，本重构阶段暂时不改名字（加live前缀）
 */
int sync_chasing_threshold(FFPlayer* ffp);

/* pause or resume the video */
void stream_toggle_pause_l(FFPlayer* ffp, int pause_on);

void stream_update_pause_l(FFPlayer* ffp);

/* seek in the stream */
void stream_seek(VideoState* is, int64_t pos, int64_t rel, int seek_by_bytes);

void toggle_pause_l(FFPlayer* ffp, int pause_on);

void toggle_pause(FFPlayer* ffp, int pause_on);

// FFP_MERGE: toggle_mute
// FFP_MERGE: update_volume

void step_to_next_frame_l(FFPlayer* ffp);

void step_to_next_frame(FFPlayer* ffp);

bool is_hls(AVFormatContext* ic);

/**
 * 获取FFPlayer总共往 a/v packet queue里积累过多少duration的数据
 * @return cached_duration_ms
 */
int ffp_get_total_history_cached_duration_ms(FFplayer* ffp);

bool ffp_is_pre_demux_enabled(FFPlayer* ffp);
bool ffp_is_pre_demux_ver1_enabled(FFPlayer* ffp);
bool ffp_is_pre_demux_ver2_enabled(FFPlayer* ffp);
