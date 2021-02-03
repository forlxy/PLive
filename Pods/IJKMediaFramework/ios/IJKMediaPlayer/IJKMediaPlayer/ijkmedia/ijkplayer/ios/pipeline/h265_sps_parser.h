//
//  h265_sps_parser.h
//  IJKMediaPlayer
//
//  Created by liuyuxin on 2018/7/6.
//  Copyright © 2018年 kuaishou. All rights reserved.
//

#ifndef IJKMediaPlayer_h265_sps_parser_h
#define IJKMediaPlayer_h265_sps_parser_h

typedef enum {
    NAL_UNIT_CODED_SLICE_TRAIL_N = 0,  // 0
    NAL_UNIT_CODED_SLICE_TRAIL_R,      // 1

    NAL_UNIT_CODED_SLICE_TSA_N,  // 2
    NAL_UNIT_CODED_SLICE_TSA_R,  // 3

    NAL_UNIT_CODED_SLICE_STSA_N,  // 4
    NAL_UNIT_CODED_SLICE_STSA_R,  // 5

    NAL_UNIT_CODED_SLICE_RADL_N,  // 6
    NAL_UNIT_CODED_SLICE_RADL_R,  // 7

    NAL_UNIT_CODED_SLICE_RASL_N,  // 8
    NAL_UNIT_CODED_SLICE_RASL_R,  // 9

    NAL_UNIT_RESERVED_VCL_N10,
    NAL_UNIT_RESERVED_VCL_R11,
    NAL_UNIT_RESERVED_VCL_N12,
    NAL_UNIT_RESERVED_VCL_R13,
    NAL_UNIT_RESERVED_VCL_N14,
    NAL_UNIT_RESERVED_VCL_R15,

    NAL_UNIT_CODED_SLICE_BLA_W_LP,    // 16
    NAL_UNIT_CODED_SLICE_BLA_W_RADL,  // 17
    NAL_UNIT_CODED_SLICE_BLA_N_LP,    // 18
    NAL_UNIT_CODED_SLICE_IDR_W_RADL,  // 19
    NAL_UNIT_CODED_SLICE_IDR_N_LP,    // 20
    NAL_UNIT_CODED_SLICE_CRA,         // 21
    NAL_UNIT_RESERVED_IRAP_VCL22,
    NAL_UNIT_RESERVED_IRAP_VCL23,

    NAL_UNIT_RESERVED_VCL24,
    NAL_UNIT_RESERVED_VCL25,
    NAL_UNIT_RESERVED_VCL26,
    NAL_UNIT_RESERVED_VCL27,
    NAL_UNIT_RESERVED_VCL28,
    NAL_UNIT_RESERVED_VCL29,
    NAL_UNIT_RESERVED_VCL30,
    NAL_UNIT_RESERVED_VCL31,

    NAL_UNIT_VPS,                    // 32
    NAL_UNIT_SPS,                    // 33
    NAL_UNIT_PPS,                    // 34
    NAL_UNIT_ACCESS_UNIT_DELIMITER,  // 35
    NAL_UNIT_EOS,                    // 36
    NAL_UNIT_EOB,                    // 37
    NAL_UNIT_FILLER_DATA,            // 38
    NAL_UNIT_PREFIX_SEI,             // 39
    NAL_UNIT_SUFFIX_SEI,             // 40
    NAL_UNIT_RESERVED_NVCL41,
    NAL_UNIT_RESERVED_NVCL42,
    NAL_UNIT_RESERVED_NVCL43,
    NAL_UNIT_RESERVED_NVCL44,
    NAL_UNIT_RESERVED_NVCL45,
    NAL_UNIT_RESERVED_NVCL46,
    NAL_UNIT_RESERVED_NVCL47,
    NAL_UNIT_UNSPECIFIED_48,
    NAL_UNIT_UNSPECIFIED_49,
    NAL_UNIT_UNSPECIFIED_50,
    NAL_UNIT_UNSPECIFIED_51,
    NAL_UNIT_UNSPECIFIED_52,
    NAL_UNIT_UNSPECIFIED_53,
    NAL_UNIT_UNSPECIFIED_54,
    NAL_UNIT_UNSPECIFIED_55,
    NAL_UNIT_UNSPECIFIED_56,
    NAL_UNIT_UNSPECIFIED_57,
    NAL_UNIT_UNSPECIFIED_58,
    NAL_UNIT_UNSPECIFIED_59,
    NAL_UNIT_UNSPECIFIED_60,
    NAL_UNIT_UNSPECIFIED_61,
    NAL_UNIT_UNSPECIFIED_62,
    NAL_UNIT_UNSPECIFIED_63,
    NAL_UNIT_INVALID
} NaluType;

#define H265_MAX_TLAYER 7

#define PKT_KEY_FRAME_FLAG (0x0001)

static uint32_t h265_bytes_to_int(uint8_t* src) {
    uint32_t value;
    value = (uint32_t)((src[0] & 0xFF) << 24 | (src[1] & 0xFF) << 16 | (src[2] & 0xFF) << 8 |
                       (src[3] & 0xFF));
    return value;
}

NaluType h265_parse_nalu_type(const uint8_t* const data) { return ((*data & 0x7E) >> 1); }

bool h265_avpacket_is_idr(uint8_t* data, int32_t size) {
    NaluType type = NAL_UNIT_INVALID;

    if (data && size >= 5) {
        int offset = 0;
        while (offset >= 0 && offset + 5 <= size) {
            uint8_t* nal_start = data + offset;
            type = h265_parse_nalu_type(nal_start + 4);
            if (NAL_UNIT_CODED_SLICE_IDR_W_RADL == type || NAL_UNIT_CODED_SLICE_IDR_N_LP == type ||
                NAL_UNIT_CODED_SLICE_CRA == type) {
                return true;
            }
            offset += (h265_bytes_to_int(nal_start) + 4);
        }
    }

    return false;
}

bool h265_avpacket_is_ref_frame(uint8_t* data, int32_t size) {
    NaluType type = NAL_UNIT_INVALID;

    if (data && size >= 5) {
        int offset = 0;
        while (offset >= 0 && offset + 5 <= size) {
            uint8_t* nal_start = data + offset;
            type = h265_parse_nalu_type(nal_start + 4);
            if (NAL_UNIT_CODED_SLICE_TRAIL_N == type || NAL_UNIT_CODED_SLICE_TSA_N == type ||
                NAL_UNIT_CODED_SLICE_STSA_N == type || NAL_UNIT_CODED_SLICE_RADL_N == type ||
                NAL_UNIT_CODED_SLICE_RASL_N == type) {
                return false;
            }
            offset += (h265_bytes_to_int(nal_start) + 4);
        }
    }

    return true;
}

void convert_payload_to_RBSP(uint8_t* src, int src_len, uint8_t* dst, int* dst_len) {
    (*dst_len) = 0;
    for (int i = 2; i < src_len; i++) {
        if ((i + 2 < src_len) && (src[i] == 0x00 && src[i + 1] == 0x00 && src[i + 2] == 0x03)) {
            dst[(*dst_len)++] = src[i];
            dst[(*dst_len)++] = src[i + 1];
            i += 2;
        } else {
            dst[(*dst_len)++] = src[i];
        }
    }
}

// 指数哥伦布码
static int u(uint8_t* data, unsigned bits, int start_bit) {
    int r = 0;
    int offset = start_bit;

    for (unsigned i = 0; i < bits; i++) {
        r <<= 1;
        if (data[offset / 8] & (0x80 >> offset % 8)) {
            r++;
        }
        offset++;
    }

    return r;
}

// 无符号的指数哥伦布码
static int ue(uint8_t* data, int* start_bit) {
    int zero_bits = 0;
    int start = *start_bit;

    while (u(data, 1, start) == 0) {
        zero_bits++;
        start++;
    }

    start++;
    int r = pow(2, zero_bits) - 1 + u(data, zero_bits, start);
    *start_bit = start + zero_bits;

    return r;
}

static void parseProfileTier(uint8_t* ptr, int* start_bit) {
    uint32_t uiCode;
    uiCode = u(ptr, 2, *start_bit);
    *start_bit += 2;
    uiCode = u(ptr, 1, *start_bit);
    *start_bit += 1;
    uiCode = u(ptr, 5, *start_bit);
    *start_bit += 5;
    for (int j = 0; j < 32; j++) {
        uiCode = u(ptr, 1, *start_bit);
        *start_bit += 1;
    }
    uiCode = u(ptr, 1, *start_bit);
    *start_bit += 1;

    uiCode = u(ptr, 1, *start_bit);
    *start_bit += 1;

    uiCode = u(ptr, 1, *start_bit);
    *start_bit += 1;

    uiCode = u(ptr, 1, *start_bit);
    *start_bit += 1;

    uiCode = u(ptr, 16, *start_bit);
    *start_bit += 16;

    uiCode = u(ptr, 16, *start_bit);
    *start_bit += 16;

    uiCode = u(ptr, 12, *start_bit);
    *start_bit += 12;
}

static void parsePTL(uint8_t* ptr, int* start_bit, bool profilePresentFlag,
                     int maxNumSubLayersMinus1) {
    uint32_t uiCode;

    if (profilePresentFlag) {
        parseProfileTier(ptr, start_bit);
    }
    uiCode = u(ptr, 8, *start_bit);
    *start_bit += 8;

    int subLayerProfilePresentFlag[H265_MAX_TLAYER - 1];
    int subLayerLevelPresentFlag[H265_MAX_TLAYER - 1];

    for (int i = 0; i < maxNumSubLayersMinus1; i++) {
        if (profilePresentFlag) {
            uiCode = u(ptr, 1, *start_bit);
            subLayerProfilePresentFlag[i] = uiCode;
            *start_bit += 1;
        }
        uiCode = u(ptr, 1, *start_bit);
        subLayerLevelPresentFlag[i] = uiCode;
        *start_bit += 1;
    }

    if (maxNumSubLayersMinus1 > 0) {
        for (int i = maxNumSubLayersMinus1; i < 8; i++) {
            uiCode = u(ptr, 2, *start_bit);
            *start_bit += 2;
        }
    }

    for (int i = 0; i < maxNumSubLayersMinus1; i++) {
        if (profilePresentFlag && subLayerProfilePresentFlag[i]) {
            parseProfileTier(ptr, start_bit);
        }
        if (subLayerLevelPresentFlag[i]) {
            uiCode = u(ptr, 8, *start_bit);
            *start_bit += 8;
        }
    }
}

static bool parseh265_sps(uint8_t* sps_nal, int len, int* w, int* h) {
    if (!sps_nal) {
        return false;
    }

    if (h265_parse_nalu_type(sps_nal) != NAL_UNIT_SPS) {
        return false;
    }

    uint8_t* ptr = (uint8_t*)malloc(len * sizeof(uint8_t));
    if (!ptr) {
        return false;
    }
    int dst_len = 0;
    convert_payload_to_RBSP(sps_nal, len, ptr, &dst_len);
    uint32_t uiCode;
    int start_bit = 0;

    uiCode = u(ptr, 4, start_bit);
    start_bit += 4;
    uiCode = u(ptr, 3, start_bit);

    int uiMaxTLayers = uiCode + 1;
    start_bit += 3;
    uiCode = u(ptr, 1, start_bit);
    start_bit += 1;

    parsePTL(ptr, &start_bit, 1, uiMaxTLayers - 1);

    uiCode = ue(ptr, &start_bit);

    int chroma_format_idc = ue(ptr, &start_bit);

    if (chroma_format_idc == 3) {
        uiCode = u(ptr, 1, start_bit);
        start_bit += 1;
    }

    *w = ue(ptr, &start_bit);
    *h = ue(ptr, &start_bit);

    uiCode = u(ptr, 1, start_bit);
    start_bit += 1;

    if (uiCode != 0) {
        const int winUnitX[] = {1, 2, 2, 1};
        const int winUnitY[] = {1, 2, 1, 1};
        int WindowLeftOffset = ue(ptr, &start_bit);
        int WindowRightOffset = ue(ptr, &start_bit);
        int WindowTopOffset = ue(ptr, &start_bit);
        int WindowBottomOffset = ue(ptr, &start_bit);
        *w -= WindowLeftOffset * winUnitX[chroma_format_idc];
        *w -= WindowRightOffset * winUnitX[chroma_format_idc];
        *h -= WindowTopOffset * winUnitY[chroma_format_idc];
        *h -= WindowBottomOffset * winUnitY[chroma_format_idc];
    }

    if (ptr) {
        free(ptr);
        ptr = NULL;
    }

    return true;
}

static bool h265_avpacket_read_vps_sps_pps(const AVPacket* pkt, uint8_t** vps, int* vps_len,
                                           uint8_t** sps, int* sps_len, uint8_t** pps,
                                           int* pps_len) {
    if (pkt && (pkt->flags & PKT_KEY_FRAME_FLAG) && pkt->data && (pkt->size >= 5) && vps &&
        vps_len && sps && sps_len && pps && pps_len) {
        int offset = 0;
        *vps_len = 0;
        *sps_len = 0;
        *pps_len = 0;
        while (offset >= 0 && offset + 5 <= pkt->size) {
            uint8_t* nal_start = pkt->data + offset;
            int nal_size = h265_bytes_to_int(nal_start);
            int nal_type = h265_parse_nalu_type(nal_start + 4);
            if (nal_type == NAL_UNIT_VPS) {
                *vps = nal_start + 4;
                *vps_len = nal_size;
            } else if (nal_type == NAL_UNIT_SPS) {
                *sps = nal_start + 4;
                *sps_len = nal_size;
            } else if (nal_type == NAL_UNIT_PPS) {
                *pps = nal_start + 4;
                *pps_len = nal_size;
            }

            if (*vps_len > 0 && *sps_len > 0 && *pps_len > 0) {
                return true;
            }

            offset += nal_size + 4;
        }
    }
    return false;
}

static void reset_nal_length(uint8_t* extra, int len) {
    *extra = (len >> 8) & 0xFF;
    *(extra + 1) = len & 0xFF;
}

static void update_parameter_set(uint8_t* new_extra, int old_ps_len, uint8_t* ps, int ps_len) {
    if (old_ps_len != ps_len) {
        reset_nal_length(new_extra, ps_len);
    }
    new_extra += 2;
    if (new_extra) {
        memcpy(new_extra, ps, ps_len);
    }
}

static bool write_hevc_sequence_header(uint8_t* buf, int buf_len, uint8_t* new_extra, uint8_t* vps,
                                       int vps_len, uint8_t* sps, int sps_len, uint8_t* pps,
                                       int pps_len) {
    int num_arrays = 0;
    int num_nals = 0;
    if (buf && vps && sps && pps && buf_len >= (sps_len + pps_len + vps_len)) {
        // HEVCDecoderConfigurationRecord

        if (new_extra) {
            // first 23 bytes before NALUnitArray
            memcpy(new_extra, buf, 23);
            new_extra += 23;
        }

        // buf[0] configurationVersion
        buf++;
        // buf[1] general profile_space: 2, tier_flag: 1, profile_idc: 5
        buf++;
        // buf[2~5] 4 bytes, general profile_compatibility_flags
        buf += 4;
        // buf[6~11] 6 bytes, general constraint_indicator_flags
        buf += 6;
        // buf[12], general level_idc
        buf++;
        // buf[13~14], 2 bytes, reseverd: 4 '1111', min_spatial_segmentation_idc: 12
        buf += 2;
        // buf[15], reserverd: 6 '111111', parallelismType: 2
        buf++;
        // buf[16], reserverd: 6 '111111', chroma_format_idc: 2
        buf++;
        // buf[17], reserverd: 5 '11111', bit_depth_luma_minus8: 3
        buf++;
        // buf[18], reserverd: 5 '11111', bit_depth_chroma_minus8: 3
        buf++;
        // buf[19~20], 2 bytes, avg FrameRate
        buf += 2;
        // buf[21], constantFrameRate: 2, numTemporalLayers: 3, temporalIdnested: 1,
        // lengthSizeMinusOne: 2
        buf++;
        // buf[22], numOfArrays
        num_arrays = *buf++;

        // NALUnitArray from buf[23]
        for (int i = 0; i < num_arrays; i++) {
            if (new_extra) {
                // 3 bytes before NALUnit
                memcpy(new_extra, buf, 3);
                new_extra += 3;
            }
            // buf++, array completeness: 1, reserved: 1, Nal_type: 6
            int nal_type = (*buf) & 0x3F;
            buf++;
            // buf+=2, 2 bytes, numNals
            num_nals = ((*buf) & 0xFF << 8) + *(buf + 1);
            buf += 2;
            for (int j = 0; j < num_nals; j++) {
                // buf+=2, 2 bytes, nalUnitLength
                if (new_extra) {
                    // 2 bytes nalUnitLength, maybe reset_nal_length later
                    memcpy(new_extra, buf, 2);
                }
                int nal_size = ((*buf) & 0xFF << 8) + *(buf + 1);
                buf += 2;
                if (nal_type == NAL_UNIT_VPS) {
                    if (new_extra) {
                        update_parameter_set(new_extra, nal_size, vps, vps_len);
                        new_extra += (2 + vps_len);
                    } else {
                        memcpy(buf, vps, vps_len);
                    }
                    ALOGI(
                        "[write_hevc_sequence_header] vps, new_extra(%p) nal_size(%d) (vps_len)%d",
                        new_extra, nal_size, vps_len);
                } else if (nal_type == NAL_UNIT_SPS) {
                    if (new_extra) {
                        update_parameter_set(new_extra, nal_size, sps, sps_len);
                        new_extra += (2 + sps_len);
                    } else {
                        memcpy(buf, sps, sps_len);
                    }
                    ALOGI(
                        "[write_hevc_sequence_header] sps, new_extra(%p) nal_size(%d) (sps_len)%d",
                        new_extra, nal_size, sps_len);
                } else if ((nal_size == pps_len) && (nal_type == NAL_UNIT_PPS)) {
                    if (new_extra) {
                        update_parameter_set(new_extra, nal_size, pps, pps_len);
                        new_extra += (2 + pps_len);
                    } else {
                        memcpy(buf, pps, pps_len);
                    }
                    ALOGI(
                        "[write_hevc_sequence_header] pps, new_extra(%p) nal_size(%d) (pps_len)%d",
                        new_extra, nal_size, pps_len);
                } else {
                    ALOGE(
                        "[Error write_hevc_sequence_header] sequence parametes, nal_type=%d, "
                        "nal_size=%d, "
                        "vps(%d) sps(%d) pps(%d)",
                        nal_type, nal_size, vps_len, sps_len, pps_len);
                }
                buf += nal_size;
            }
        }
        return true;
    }
    return false;
}

#endif /* IJKMediaPlayer_h265_sps_parser_h */
