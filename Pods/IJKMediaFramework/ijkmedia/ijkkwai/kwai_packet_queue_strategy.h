//
//  kwai_packet_queue_strategy.h
//  IJKMediaFramework
//
//  Created by MarshallShuai on 2018/9/29.
//  Copyright © 2018 kuaishou. All rights reserved.
//

#ifndef kwai_packet_queue_strategy_h
#define kwai_packet_queue_strategy_h
/**
 * 这个类里有一个原有的开始buffer的逻辑：check_pkt_q_need_buffering。它和ff_buffer_strategy的关系是：
 * * check_pkt_q_need_buffering作用在消费线程，是在消费av frame的时候，发现frame不够消费的时候，决定不够到什么程度的时候开启buffer，并toggle_pause
 * * ff_buffer_strategy里的high water mark 作用在生产线程(read_thread)，它来决定卡顿之后，缓冲多少能重新恢复播放
 *
 * 启播总体策略：
 * 1.对非nativeCache路径的播放不设置起播条件（一般是外部传入的已经是本地文件了），无条件开播
 * 2.起播buffer对直播不起效，只对点播生效
 */
#include "ff_packet_queue.h"

typedef struct FFPlayer FFplayer;

typedef enum KwaiPacketQueueBufferCheckerStrategy {
    kStrategyStartPlayBlockByNone = 0,
    kStrategyStartPlayBlockByTimeMs = 1,
} KwaiPacketQueueBufferCheckerStrategy;

typedef enum KwaiPacketQueueBufferCheckerDisableReason {
    kStartPlayCheckeDisableNone = 0,
    kStartPlayCheckDisableCacheNotUsed = 1,
    kStartPlayCheckDisableIsLive = 2,
} KwaiPacketQueueBufferCheckerDisableReason;

typedef struct KwaiPacketQueueBufferChecker KwaiPacketQueueBufferChecker;
struct KwaiPacketQueueBufferChecker {
    bool play_started;

    // 播放器起播的时间戳
    bool self_life_cycle_started;
    int64_t self_life_cycle_start_ts_ms;
#define START_PLAY_MAX_COST_MS_DEFAULT (500)
#define START_PLAY_MAX_COST_MS_MAX (60000)
    int64_t self_max_life_cycle_ms;  // <0: 表示无穷大，不会超时
    int64_t self_life_cycle_cost_ms;

    int current_buffer_ms;
#define MIN_START_PLAY_BUFFER_MS_DEFAULT (500)
#define MIN_START_PLAY_BUFFER_MS_MAX (50*1000)
    int buffer_threshold_ms;    // for kStrategyStartPlayBlockByTimeMs

#define MIN_START_PLAY_BUFFER_BYTES_DEFAULT (1*1024)
#define MIN_START_PLAY_BUFFER_BYTES_MAX (10*1024*1024)
    int buffer_threshold_bytes; // for kStrategyStartPlayBlockByBytes

    bool enabled;
    int disable_reason;
    bool ready; // 当ready为false的时候，表示还没决策好是否用啥策略，所以任何video_thread/audio_thread的询问都应该返回不可开始播放

    int used_strategy;


    void(*on_read_frame_error)(KwaiPacketQueueBufferChecker* ck, FFplayer* ffp);
    bool(*func_check_can_start_play)(KwaiPacketQueueBufferChecker* ck, FFplayer* ffp);

    // 需要开始buffering的时候，返回true，否则返回false
    bool(*func_check_pkt_q_need_buffering)(KwaiPacketQueueBufferChecker* ck, FFplayer* ffp, PacketQueue* q);
};

void KwaiPacketQueueBufferChecker_init(KwaiPacketQueueBufferChecker* ck);
void KwaiPacketQueueBufferChecker_use_strategy(KwaiPacketQueueBufferChecker* ck, KwaiPacketQueueBufferCheckerStrategy strategy);

/**
 * 记录启播buffer的生命周期开始，在read_thread的for循环开始记录
 */
void KwaiPacketQueueBufferChecker_on_lifecycle_start(KwaiPacketQueueBufferChecker* ck);
void KwaiPacketQueueBufferChecker_set_start_play_max_buffer_cost_ms(KwaiPacketQueueBufferChecker* ck, int max_buffer_cost_ms);

void KwaiPacketQueueBufferChecker_set_start_play_buffer_ms(KwaiPacketQueueBufferChecker* ck, int block_buffer_ms);

void KwaiPacketQueueBufferChecker_set_enable(KwaiPacketQueueBufferChecker* ck, bool enable, KwaiPacketQueueBufferCheckerDisableReason reason);

#endif /* kwai_packet_queue_strategy_h */
