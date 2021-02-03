//
//  ff_sps_parser.h
//  IJKMediaPlayer
//
//  Created by liuyuxin on 2018/7/7.
//  Copyright © 2018年 kuaishou. All rights reserved.
//

#ifndef ff_sps_parser_h
#define ff_sps_parser_h

#include "h264_sps_parser.h"
#include "h265_sps_parser.h"

static bool ff_avpacket_is_idr(const AVPacket* pkt, int codec_id) {
    bool ret = false;

    switch (codec_id) {
        case AV_CODEC_ID_HEVC:
            ret = h265_avpacket_is_idr(pkt->data, pkt->size);
            break;
        case AV_CODEC_ID_H264:
        default:
            ret = h264_avpacket_is_idr(pkt);
            break;
    }

    return ret;
}

static bool ff_avpacket_is_ref_frame(const AVPacket* pkt, int codec_id) {
    bool ret = true;
    switch (codec_id) {
        case AV_CODEC_ID_HEVC:
            ret = h265_avpacket_is_ref_frame(pkt->data, pkt->size);
            break;
        case AV_CODEC_ID_H264:
        default:
            break;
    }
    return ret;
}

static bool ff_avpacket_is_key(const AVPacket* pkt) {
    if (pkt->flags & AV_PKT_FLAG_KEY) {
        return true;
    } else {
        return false;
    }
}

static bool ff_avpacket_i_or_idr(const AVPacket* pkt, bool isIdr, int codec_id) {
    if (isIdr == true) {
        return ff_avpacket_is_idr(pkt, codec_id);
    } else {
        return ff_avpacket_is_key(pkt);
    }
}

#endif /* ff_sps_parser_h */
