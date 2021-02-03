//
// Created by wangtao03 on 2019/3/29.
//

#include "kwai_priv_aac_parser.h"

static uint16_t Fletcher16(const uint8_t* data, size_t len) {
    uint16_t sum1 = 0xff, sum2 = 0xff;

    while (len) {
        size_t tlen = len > 20 ? 20 : len;
        len -= tlen;
        do {
            sum2 += sum1 += *data++;
        } while (--tlen);
        sum1 = (sum1 & 0xff) + (sum1 >> 8);
        sum2 = (sum2 & 0xff) + (sum2 >> 8);
    }
    /* Second reduction step to reduce sums to 8 bits */
    sum1 = (sum1 & 0xff) + (sum1 >> 8);
    sum2 = (sum2 & 0xff) + (sum2 >> 8);
    return sum2 << 8 | sum1;
}

static void handlePassThroughMsg(FFPlayer* ffp, const uint8_t* data, int len, uint64_t pts) {
    LiveEvent event;
    memset(&event, 0, sizeof(event));
    event.content_len = len;
    event.time = pts;
    memcpy(event.content, data, event.content_len);
    ALOGI("[%u] handlePrivDataInAac: event.content_len=%d, event.time=%lld\n",
          ffp->session_id, event.content_len, event.time);
    if (len >= 2) {
        ALOGI("[%u] handlePrivDataInAac: content: (0x%02X, 0x%02X, ..., 0x%02X, 0x%02X)\n",
              ffp->session_id, event.content[0], event.content[1], event.content[len - 2], event.content[len - 1]);
    }
    live_event_queue_put(&ffp->is->event_queue, &event);
}

static void handleVideoMixType(FFPlayer* ffp, int mix_type) {
    if (mix_type != ffp->mix_type) {
        ALOGI("[%u] handlePrivDataInAac: VideoMixType change from %d to %d\n",
              ffp->session_id, ffp->mix_type, mix_type);
        ffp->mix_type = mix_type;
        ffp_notify_msg2(ffp, FFP_MSG_LIVE_TYPE_CHANGE, mix_type);
    }
}

static void handleAbsTime(FFPlayer* ffp, AVPacket* pkt, const uint8_t* data) {
    if (pkt->pts != AV_NOPTS_VALUE) {
        uint32_t abstime[2];
        abstime[0] = ntohl(*(uint32_t*)data);
        abstime[1] = ntohl(*(uint32_t*)(data + 4));
        int64_t abst = (((int64_t)abstime[0]) << 32) + abstime[1];
        int64_t pts_ms = av_rescale_q(pkt->pts, ffp->is->ic->streams[pkt->stream_index]->time_base, (AVRational) {1, 1000});
        ALOGI("[%u] handlePrivDataInAac: AbsTime %lld, pts_ms:%lld\n", ffp->session_id, abst, pts_ms);
        ffp->qos_pts_offset = abst - pts_ms;
        ffp->qos_pts_offset_got = true;
    }
}

static void handleVoiceComment(FFPlayer* ffp, AVPacket* pkt, const uint8_t* data, int len) {
    int64_t vc_time = av_rescale_q(pkt->pts, ffp->is->ic->streams[pkt->stream_index]->time_base, (AVRational) {1, 1000});
    ALOGI("[%u] handlePrivDataInAac: VoiceComment time:%lld,data(%d):%s\n",
          ffp->session_id, vc_time, len, data);
    if (ffp->live_voice_comment_time != vc_time) {
        ffp->live_voice_comment_time = vc_time;
        VoiceComment vc;
        memset(&vc, 0, sizeof(VoiceComment));
        vc.time = vc_time;
        vc.com_len = (len < KWAI_LIVE_VOICE_COMMENT_LEN) ?
                     len : KWAI_LIVE_VOICE_COMMENT_LEN - 1;
        strncpy(vc.comment, (const char*)data, vc.com_len);
        live_voice_comment_queue_put(&ffp->is->vc_queue, &vc);
    }
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * bit padding consideration:
 * AAAA: FillElCount;  BBBBBBBB: FillElEscCount;  CCCC: FillExtType;  DDDD: FillExtDataElVer
 * EEE: ID_END;  FFF: ID_FIL;  XXXXXXXX: padding bits for byte-alignment
 * N+5: N + Length(1) + MagicNum(2) + Checksum(2)
 * 0: -----FFF | AAAABBBB | BBBBCCCC | DDDDXXXX | (N+5 bytes) | XXXXEEE0
 * 1: ----FFFA | AAABBBBB | BBBCCCCD | DDDXXXXX | (N+5 bytes) | XXXEEE00
 * 2: ---FFFAA | AABBBBBB | BBCCCCDD | DDXXXXXX | (N+5 bytes) | XXEEE000
 * 3: --FFFAAA | ABBBBBBB | BCCCCDDD | DXXXXXXX | (N+5 bytes) | XEEE0000
 * 4: -FFFAAAA | BBBBBBBB | CCCCDDDD | (N+5 bytes) | EEE00000
 * 5: FFFAAAAB | BBBBBBBC | CCCDDDDX | (N+5 bytes) | XXXXXXXE | EE000000
 * 6: FFAAAABB | BBBBBBCC | CCDDDDXX | (N+5 bytes) | XXXXXXEE | E0000000
 * 7: FAAAABBB | BBBBBCCC | CDDDDXXX | (N+5 bytes) | XXXXXEEE
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
void handlePrivDataInAac(FFPlayer* ffp, AVPacket* pkt) {
    if (!ffp || !ffp->is) {
        return;
    }

    // N+5: N + Length(1) + MagicNum(2) + Checksum(2)
    if (pkt->size <= 5) {
        ALOGE("[%u] handlePrivDataInAac, corrupt. pkt->size:%d\n", ffp->session_id, pkt->size);
        return;
    }

    int index = pkt->size - 1;
    uint8_t byte = pkt->data[index--];
    if (byte == 0x80 || byte == 0xC0) {
        --index;
    }
    uint16_t checksum = pkt->data[index--];
    checksum += ((uint16_t)pkt->data[index--] << 8);
    uint8_t magic1 = pkt->data[index--];
    uint8_t magic0 = pkt->data[index--];
    uint8_t total_size = pkt->data[index]; // max: 255
    int data_end_index = index;
    index -= total_size;
    if (magic0 == 'K' && magic1 == 'S'
        && index >= 0 && index + total_size + 5 < pkt->size
        && checksum == Fletcher16(pkt->data + index, total_size + 3)) { // +3 for length(1) && magic(2)
        ALOGD("checksum:0x%04X length:%u\n", checksum, total_size);

        while (index < data_end_index) {
            uint8_t data_id = pkt->data[index++];
            int data_len = pkt->data[index++];
            if (data_len <= 0 || index + data_len > data_end_index) {
                ALOGE("[%u] handlePrivDataInAac, corrupt. pkt->size:%d, index:%d, data_len:%d, data_end_index:%d\n",
                      ffp->session_id, pkt->size, index, data_len, data_end_index);
                return;
            }
            switch (data_id) {
                case KWAI_LIVE_EVENT_TYPE_PASSTHROUGH_LEGACY:
                case KWAI_LIVE_EVENT_TYPE_PASSTHROUGH:
                    handlePassThroughMsg(ffp, &pkt->data[index], data_len, pkt->pts);
                    break;
                case KWAI_LIVE_EVENT_TYPE_VIDEO_MIX_TYPE:
                    if (data_len != 1) {
                        ALOGE("[%u] handlePrivDataInAac, corrupt. pkt->size:%d, VideoMixType data_len:%d\n",
                              ffp->session_id, pkt->size, data_len);
                        return;
                    }
                    handleVideoMixType(ffp, pkt->data[index]);
                    break;
                case KWAI_LIVE_EVENT_TYPE_ABS_TIME:
                    if (data_len != 8) {
                        ALOGE("[%u] handlePrivDataInAac, corrupt. pkt->size:%d, AbsTime data_len:%d\n",
                              ffp->session_id, pkt->size, data_len);
                        return;
                    }
                    handleAbsTime(ffp, pkt, &pkt->data[index]);
                    break;
                case KWAI_LIVE_EVENT_TYPE_VOICE_COMMENT:
                    handleVoiceComment(ffp, pkt, &pkt->data[index], data_len);
                    break;
                default:
                    break;
            }
            index += data_len;
        }
    }
}
