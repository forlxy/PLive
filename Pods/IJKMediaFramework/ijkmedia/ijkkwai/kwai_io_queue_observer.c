//
// Created by MarshallShuai on 2019-08-15.
//

#include <memory.h>
#include "kwai_io_queue_observer.h"
#include "ijksdl_log.h"
#include "ff_ffplay_def.h"
#include "ijkplayer/ffplay_modules/ff_ffplay_internal.h"
#include "kwai_qos.h"


void KwaiIoQueueObserver_init(KwaiIoQueueObserver* self) {
    if (!self) {
        return;
    }
    memset(self, 0, sizeof(KwaiIoQueueObserver));
}

void KwaiIoQueueObserver_on_open_input(KwaiIoQueueObserver* self, FFPlayer* ffp) {
    if (!self || !ffp) {
        return;
    }

    self->read_bytes_on_open_input = ffp->cache_stat.ffmpeg_adapter_qos.total_read_bytes;
    KwaiQos_setOpenInputReadBytes(&ffp->kwai_qos, ffp->cache_stat.ffmpeg_adapter_qos.total_read_bytes);
}

void KwaiIoQueueObserver_on_find_stream_info(KwaiIoQueueObserver* self, FFPlayer* ffp) {
    if (!self || !ffp) {
        return;
    }
    self->read_bytes_on_find_stream_info = ffp->cache_stat.ffmpeg_adapter_qos.total_read_bytes;
    KwaiQos_setFindStreamInfoReadBytes(&ffp->kwai_qos, self->read_bytes_on_find_stream_info);
}

//static inline void KwaiIoQueueObserver_debug_print(KwaiIoQueueObserver* self, int cur_cached_dur_ms) {
//    ALOGD("[KwaiIoQueueObserver_on_av_read_frame] cache_dur_ms:%d, byterate:%lld, bytes_on_open_input:%lld, self->read_bytes_on_find_stream_info:%lld, "
//          "self->read_bytes_on_fst_audio_pkt:%lld, self->read_bytes_on_fst_video_pkt:%lld, self->read_bytes_on_1s:%lld, "
//          "self->read_bytes_on_2s:%lld, self->read_bytes_on_3s:%lld, self->finish_collect:%d",
//          cur_cached_dur_ms, self->byterate, self->read_bytes_on_open_input, self->read_bytes_on_find_stream_info, self->read_bytes_on_fst_audio_pkt,
//          self->read_bytes_on_fst_video_pkt, self->read_bytes_on_1s, self->read_bytes_on_2s, self->read_bytes_on_3s, self->finish_collect);
//}

#define PRINT_DEBUG_INFO
//#define PRINT_DEBUG_INFO KwaiIoQueueObserver_debug_print(self, cache_dur_ms)

static void KwaiIoQueueObserver_on_av_read_frame(KwaiIoQueueObserver* self, FFPlayer* ffp) {

    // 小于4秒不统计
#define OB_DURATION_THRESHOLD_SECOND 4

    if (ffp->kwai_qos.media_metadata.duration < OB_DURATION_THRESHOLD_SECOND
        || !ffp->cache_actually_used // 不用cache的这套行不通
       ) {
        self->finish_collect = true;
        return;
    }

    if (self->byterate <= 0) {
        self->byterate = ffp->kwai_qos.media_metadata.bitrate / 8;
    }

    int cache_dur_ms = ffp_get_total_history_cached_duration_ms(ffp);

    if (self->read_bytes_on_1s <= 0 && cache_dur_ms >= 1000) {
        self->read_bytes_on_1s = ffp->cache_stat.ffmpeg_adapter_qos.total_read_bytes;

        PRINT_DEBUG_INFO;
    } else if (self->read_bytes_on_2s <= 0 && cache_dur_ms >= 2000) {
        self->read_bytes_on_2s = ffp->cache_stat.ffmpeg_adapter_qos.total_read_bytes;

        PRINT_DEBUG_INFO;
    } else if (self->read_bytes_on_3s <= 0 && cache_dur_ms >= 3000) {
        self->read_bytes_on_3s = ffp->cache_stat.ffmpeg_adapter_qos.total_read_bytes;
        self->finish_collect = true;

        PRINT_DEBUG_INFO;
    }

}

void KwaiIoQueueObserver_on_read_audio_frame(KwaiIoQueueObserver* self, FFPlayer* ffp) {
    if (!self || self->finish_collect || !ffp) {
        return;
    }
    KwaiIoQueueObserver_on_av_read_frame(self, ffp);
    if (self->read_bytes_on_fst_audio_pkt <= 0) {
        self->read_bytes_on_fst_audio_pkt = ffp->cache_stat.ffmpeg_adapter_qos.total_read_bytes;
    }
    KwaiQos_setAudioPktReadBytes(&ffp->kwai_qos, ffp->cache_stat.ffmpeg_adapter_qos.total_read_bytes);
}

void KwaiIoQueueObserver_on_read_video_frame(KwaiIoQueueObserver* self, FFPlayer* ffp) {
    if (!self || self->finish_collect || !ffp) {
        return;
    }
    KwaiIoQueueObserver_on_av_read_frame(self, ffp);
    if (self->read_bytes_on_fst_video_pkt <= 0) {
        self->read_bytes_on_fst_video_pkt = ffp->cache_stat.ffmpeg_adapter_qos.total_read_bytes;
    }
    KwaiQos_setVideoPktReadBytes(&ffp->kwai_qos, ffp->cache_stat.ffmpeg_adapter_qos.total_read_bytes);
}
