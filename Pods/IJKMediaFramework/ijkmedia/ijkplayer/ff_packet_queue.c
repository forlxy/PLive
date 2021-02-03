//
//  ff_packet_queue.c
//  IJKMediaFramework
//
//  Created by 帅龙成 on 01/03/2018.
//  Copyright © 2018 kuaishou. All rights reserved.
//

#include "ff_packet_queue.h"
#include "ijksdl/ijksdl_error.h"
#include "ijksdl/ijksdl_log.h"

AVPacket flush_pkt;

int packet_queue_put_private(PacketQueue* q, AVPacket* pkt, AVPacketTime* p_pkttime) {
    MyAVPacketList* pkt1;

    if (q->abort_request)
        return -1;

#ifdef FFP_MERGE
    pkt1 = av_malloc(sizeof(MyAVPacketList));
#else
    pkt1 = q->recycle_pkt;
    if (pkt1) {
        q->recycle_pkt = pkt1->next;
        q->recycle_count++;
    } else {
        q->alloc_count++;
        pkt1 = av_malloc(sizeof(MyAVPacketList));
    }
#ifdef FFP_SHOW_PKT_RECYCLE
    int total_count = q->recycle_count + q->alloc_count;
    if (!(total_count % 50)) {
        av_log(ffp, AV_LOG_DEBUG, "pkt-recycle \t%d + \t%d = \t%d\n", q->recycle_count, q->alloc_count, total_count);
    }
#endif
#endif
    if (!pkt1)
        return -1;
    pkt1->pkt = *pkt;
    memset(&(pkt1->pkttime), 0, sizeof(AVPacketTime));
    if (p_pkttime != NULL)
        memcpy(&(pkt1->pkttime), p_pkttime, sizeof(AVPacketTime));

    pkt1->next = NULL;
    if (pkt == &flush_pkt)
        q->serial++;
    pkt1->serial = q->serial;

    if (!q->last_pkt)
        q->first_pkt = pkt1;
    else
        q->last_pkt->next = pkt1;
    q->last_pkt = pkt1;
    q->nb_packets++;
    if (pkt != &flush_pkt && pkt->data != NULL) {
        //ignore flush_pkt and nullpacket
        q->max_pts = pkt->pts;
    }
    q->size += pkt1->pkt.size + sizeof(*pkt1);
    q->history_total_size += pkt1->pkt.size;
    if (pkt1->pkt.duration > 0) {
        q->duration += pkt1->pkt.duration;
        q->history_total_duration += pkt1->pkt.duration;
    }
    /* XXX: should duplicate packet data in DV case */
    SDL_CondSignal(q->cond);
    return 0;
}

int packet_queue_put(PacketQueue* q, AVPacket* pkt, AVPacketTime* p_pkttime) {
    int ret;

    /* duplicate the packet */
    if (pkt != &flush_pkt && av_dup_packet(pkt) < 0)
        return -1;

    SDL_LockMutex(q->mutex);
    ret = packet_queue_put_private(q, pkt, p_pkttime);
    SDL_UnlockMutex(q->mutex);

    if (pkt != &flush_pkt && ret < 0)
        av_packet_unref(pkt);

    return ret;
}

int packet_queue_put_nullpacket(PacketQueue* q, int stream_index) {
    AVPacket pkt1, *pkt = &pkt1;
    av_init_packet(pkt);
    pkt->data = NULL;
    pkt->size = 0;
    pkt->stream_index = stream_index;
    return packet_queue_put(q, pkt, NULL);
}

/* packet queue handling */
int packet_queue_init(PacketQueue* q) {
    return packet_queue_init_with_name(q, NULL);
}

int packet_queue_init_with_name(PacketQueue* q, const char* name) {
    memset(q, 0, sizeof(PacketQueue));
    q->mutex = SDL_CreateMutex();
    if (!q->mutex) {
        av_log(NULL, AV_LOG_FATAL, "SDL_CreateMutex(): %s\n", SDL_GetError());
        return AVERROR(ENOMEM);
    }
    q->cond = SDL_CreateCond();
    if (!q->cond) {
        av_log(NULL, AV_LOG_FATAL, "SDL_CreateCond(): %s\n", SDL_GetError());
        return AVERROR(ENOMEM);
    }
    q->abort_request = 1;
    q->name = name;
    return 0;
}

void packet_queue_flush(PacketQueue* q) {
    MyAVPacketList* pkt, *pkt1;

    SDL_LockMutex(q->mutex);
    for (pkt = q->first_pkt; pkt; pkt = pkt1) {
        pkt1 = pkt->next;
        av_packet_unref(&pkt->pkt);
#ifdef FFP_MERGE
        av_freep(&pkt);
#else
        pkt->next = q->recycle_pkt;
        q->recycle_pkt = pkt;
#endif
    }
    q->last_pkt = NULL;
    q->first_pkt = NULL;
    q->nb_packets = 0;
    q->size = 0;
    q->duration = 0;
    q->max_pts = 0;
    SDL_UnlockMutex(q->mutex);
}

void packet_queue_destroy(PacketQueue* q) {
    packet_queue_flush(q);

    SDL_LockMutex(q->mutex);
    while (q->recycle_pkt) {
        MyAVPacketList* pkt = q->recycle_pkt;
        if (pkt)
            q->recycle_pkt = pkt->next;
        av_freep(&pkt);
    }
    SDL_UnlockMutex(q->mutex);

    SDL_DestroyMutex(q->mutex);
    SDL_DestroyCond(q->cond);
}

void packet_queue_abort(PacketQueue* q) {
    SDL_LockMutex(q->mutex);

    q->abort_request = 1;

    SDL_CondSignal(q->cond);

    SDL_UnlockMutex(q->mutex);
}

void packet_queue_start(PacketQueue* q) {
    SDL_LockMutex(q->mutex);
    q->abort_request = 0;
    packet_queue_put_private(q, &flush_pkt, NULL);
    SDL_UnlockMutex(q->mutex);
}

/* return < 0 if aborted, 0 if no packet and > 0 if packet.  */
int packet_queue_get(PacketQueue* q, AVPacket* pkt, int block, int* serial, AVPacketTime* p_pkttime) {
    MyAVPacketList* pkt1;
    int ret;

    SDL_LockMutex(q->mutex);

    for (;;) {
        if (q->abort_request) {
            ret = -1;
            break;
        }

        pkt1 = q->first_pkt;
        if (pkt1) {
            q->first_pkt = pkt1->next;
            if (!q->first_pkt)
                q->last_pkt = NULL;
            q->nb_packets--;
            q->size -= pkt1->pkt.size + sizeof(*pkt1);
            if (pkt1->pkt.duration > 0)
                q->duration -= pkt1->pkt.duration;
            *pkt = pkt1->pkt;
            if (serial)
                *serial = pkt1->serial;
            if (p_pkttime)
                memcpy(p_pkttime, &(pkt1->pkttime), sizeof(AVPacketTime));
#ifdef FFP_MERGE
            av_free(pkt1);
#else
            pkt1->next = q->recycle_pkt;
            q->recycle_pkt = pkt1;
#endif
            ret = 1;
            break;
        } else if (!block) {
            ret = 0;
            break;
        } else {
            SDL_CondWait(q->cond, q->mutex);
        }
    }
    SDL_UnlockMutex(q->mutex);

    return ret;
}

int64_t packet_queue_get_first_packet_pts(PacketQueue* q) {
    int64_t pts;

    SDL_LockMutex(q->mutex);
    if (q->first_pkt) {
        pts = q->first_pkt->pkt.pts;
    } else {
        pts = -1;
    }
    SDL_UnlockMutex(q->mutex);

    return pts;
}


/* return delete num*/
void packet_queue_delete_elements_until_by_pts(PacketQueue* q, double base, double seek_point_pts_time) {
    MyAVPacketList* pkt1 = NULL;
    int delete_count = 0;
    double cur_pts_time = 0.0;

    SDL_LockMutex(q->mutex);
    for (;;) {
        if (q->abort_request) {
            break;
        }

        pkt1 = q->first_pkt;
        if (pkt1) {
            cur_pts_time = base * pkt1->pkt.pts;
            if (cur_pts_time >= seek_point_pts_time) {
                break;
            }

            //av_log(NULL, AV_LOG_ERROR, "lyx %s:%d delete one packet: %d, left count%d, pts: %lf, "
            //                    "cur_pts_time: %lf, packet_pts: %lld\n",
            //                    __FUNCTION__, __LINE__, delete_count, q->nb_packets, seek_point_pts_time,
            //                    cur_pts_time, pkt1->pkt.pts);

            q->first_pkt = pkt1->next;
            if (!q->first_pkt) {
                q->last_pkt = NULL;
            }
            q->nb_packets--;
            q->size -= pkt1->pkt.size + sizeof(*pkt1);
            if (pkt1->pkt.duration > 0)
                q->duration -= pkt1->pkt.duration;

            av_packet_unref(&pkt1->pkt);
#ifdef FFP_MERGE
            av_free(pkt1);
#else
            pkt1->next = q->recycle_pkt;
            q->recycle_pkt = pkt1;
#endif
            delete_count++;
        } else
            break;
    }

    pkt1 = q->first_pkt;
    for (;;) {
        if (q->abort_request) {
            break;
        }

        if (pkt1) {
            pkt1->serial++;
            pkt1 = pkt1->next;
        } else
            break;
    }

    SDL_UnlockMutex(q->mutex);

    //av_log(NULL, AV_LOG_DEBUG, "%s:%d delete  packte:%d\n",__FUNCTION__, __LINE__, delete_count);
    return;
}

/* if is_video_stream is 1,  it is video queue and need to seek to position according to key frame.
 *  if is_video_stream is 0, it is audio queue and seek to position according to pts.
 * return value:
 * if seek succeed, return pts.
 * if failed, return 0
 */
int64_t packet_queue_seek(PacketQueue* q, int64_t seek_pos, int is_video_stream) {
    MyAVPacketList* pkt1 = NULL;
    int64_t key_frame_pts = 0, current_pts = 0;

    if (q->nb_packets == 0)
        return 0;

    pkt1 = q->first_pkt;
    while (pkt1) {
        if (is_video_stream) {
            if (pkt1->pkt.flags & AV_PKT_FLAG_KEY)
                key_frame_pts = pkt1->pkt.pts;
        }
        if (pkt1->pkt.pts >= seek_pos) {
            current_pts = pkt1->pkt.pts;
            break;
        }
        pkt1 = pkt1->next;
    }
    if (is_video_stream)
        return key_frame_pts;
    else
        return current_pts;
}

int packet_queue_is_started(PacketQueue* q) {
    SDL_LockMutex(q->mutex);
    int ret = !q->abort_request;
    SDL_UnlockMutex(q->mutex);
    return ret;
}

