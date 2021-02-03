/*****************************************************************************
 * IJKVideoToolBox.m
 *****************************************************************************
 *
 * copyright (c) 2014 Zhou Quan <zhouqicy@gmail.com>
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

#include "IJKVideoToolBoxAsync.h"
#import <CoreFoundation/CoreFoundation.h>
#import <CoreMedia/CoreMedia.h>
#import <CoreVideo/CVHostTime.h>
#import <Foundation/Foundation.h>
#import <VideoToolbox/VideoToolbox.h>
#include <mach/mach_time.h>
#include <stdatomic.h>
#import "IJKDeviceModel.h"
#include "ff_fferror.h"
#include "ff_ffinc.h"
#include "ff_ffmsg.h"
#include "ff_ffplay_module_video_decode.h"
#include "ff_sps_parser.h"
#include "ffpipeline_ios.h"
#include "ijkkwai/ios/KwaiVideoToolBoxColor.h"
#include "ijkkwai/kwai_qos.h"
#include "ijkplayer/ff_ffplay_debug.h"
#include "ijksdl/ios/ijksdl_vout_overlay_videotoolbox.h"
#include "ijksdl_vout_ios_gles2.h"
#include "ijksdl_vout_overlay_videotoolbox.h"
#include "libavformat/avc.h"

#define IJK_VTB_FCC_AVCC SDL_FOURCC('C', 'c', 'v', 'a')

#define MAX_PKT_QUEUE_DEEP 350
#define VTB_MAX_DECODING_SAMPLES 3

typedef struct sample_info {
    int sample_id;

    double sort;
    double dts;
    double pts;
    int serial;

    int sar_num;
    int sar_den;
    int64_t abs_time;

    enum AVColorSpace color_space;
    enum AVColorRange color_range;

    volatile int is_decoding;
} sample_info;

typedef struct sort_queue {
    AVFrame pic;
    int serial;
    int64_t sort;
    volatile struct sort_queue* nextframe;
} sort_queue;

typedef struct VTBFormatDesc {
    CMFormatDescriptionRef fmt_desc;
    int32_t max_ref_frames;
    bool convert_bytestream;
    bool convert_3byteTo4byteNALSize;
    int codec_id;
} VTBFormatDesc;

struct Ijk_VideoToolBox_Opaque {
    FFPlayer* ffp;
    volatile bool refresh_request;
    volatile bool new_seg_flag;
    volatile bool idr_based_identified;
    volatile bool refresh_session;
    volatile bool recovery_drop_packet;

    VTBFormatDesc fmt_desc;

    VTDecompressionSessionRef vt_session;
    pthread_mutex_t m_queue_mutex;
    volatile sort_queue* m_sort_queue;
    volatile int32_t m_queue_depth;
    int serial;
    bool dealloced;
    int m_buffer_deep;
    AVPacket m_buffer_packet[MAX_PKT_QUEUE_DEEP];

    SDL_mutex* sample_info_mutex;
    SDL_cond* sample_info_cond;
    sample_info sample_info_array[VTB_MAX_DECODING_SAMPLES];
    volatile int sample_info_index;
    volatile int sample_info_id_generator;
    volatile int sample_infos_in_decoding;

    SDL_SpeedSampler sampler;

    int width;
    int height;
    int total_sequence_len;  // vps+pps+sps
};

static void vtbformat_destroy(VTBFormatDesc* fmt_desc);
static int vtbformat_init(VTBFormatDesc* fmt_desc, AVCodecContext* ic);

static const char* vtb_get_error_string(OSStatus status) {
    switch (status) {
        case kVTInvalidSessionErr:
            return "kVTInvalidSessionErr";
        case kVTVideoDecoderBadDataErr:
            return "kVTVideoDecoderBadDataErr";
        case kVTVideoDecoderUnsupportedDataFormatErr:
            return "kVTVideoDecoderUnsupportedDataFormatErr";
        case kVTVideoDecoderMalfunctionErr:
            return "kVTVideoDecoderMalfunctionErr";
        default:
            return "UNKNOWN";
    }
}

static void SortQueuePop(Ijk_VideoToolBox_Opaque* context) {
    if (!context->m_sort_queue || context->m_queue_depth == 0) {
        return;
    }
    pthread_mutex_lock(&context->m_queue_mutex);
    volatile sort_queue* top_frame = context->m_sort_queue;
    context->m_sort_queue = context->m_sort_queue->nextframe;
    context->m_queue_depth--;
    pthread_mutex_unlock(&context->m_queue_mutex);
    CVBufferRelease(top_frame->pic.opaque);
    free((void*)top_frame);
}

static void CFDictionarySetSInt32(CFMutableDictionaryRef dictionary, CFStringRef key,
                                  SInt32 numberSInt32) {
    CFNumberRef number;
    number = CFNumberCreate(NULL, kCFNumberSInt32Type, &numberSInt32);
    CFDictionarySetValue(dictionary, key, number);
    CFRelease(number);
}

static void CFDictionarySetBoolean(CFMutableDictionaryRef dictionary, CFStringRef key, BOOL value) {
    CFDictionarySetValue(dictionary, key, value ? kCFBooleanTrue : kCFBooleanFalse);
}

inline static void sample_info_flush(Ijk_VideoToolBox_Opaque* context, int wait_ms) {
    int total_wait = 0;
    SDL_LockMutex(context->sample_info_mutex);

    while (wait_ms < 0 || total_wait < wait_ms) {
        if (context->sample_infos_in_decoding <= 0) break;

        int wait_step = 10;
        SDL_CondWaitTimeout(context->sample_info_cond, context->sample_info_mutex, wait_step);
        total_wait += wait_step;
    }

    SDL_UnlockMutex(context->sample_info_mutex);
}

inline static sample_info* sample_info_peek(Ijk_VideoToolBox_Opaque* context) {
    FFPlayer* ffp = context->ffp;
    VideoState* is = ffp->is;

    SDL_LockMutex(context->sample_info_mutex);

    sample_info* sample_info = &context->sample_info_array[context->sample_info_index];
    while (sample_info->is_decoding) {
        if (is->videoq.abort_request) {
            sample_info = NULL;
            goto abort;
        }

        SDL_CondWaitTimeout(context->sample_info_cond, context->sample_info_mutex, 10);
    }

abort:
    SDL_UnlockMutex(context->sample_info_mutex);
    return sample_info;
}

inline static void sample_info_push(Ijk_VideoToolBox_Opaque* context) {
    FFPlayer* ffp = context->ffp;
    VideoState* is = ffp->is;

    SDL_LockMutex(context->sample_info_mutex);

    sample_info* sample_info = &context->sample_info_array[context->sample_info_index];
    while (sample_info->is_decoding) {
        if (is->videoq.abort_request) goto abort;

        SDL_CondWaitTimeout(context->sample_info_cond, context->sample_info_mutex, 10);
    }

    if (sample_info->is_decoding) {
        ALOGW("%s, reallocate sample in decoding %d -> %d /%d\n", __FUNCTION__,
              sample_info->sample_id, context->sample_info_id_generator,
              context->sample_infos_in_decoding);
    } else {
        sample_info->is_decoding = 1;
        context->sample_infos_in_decoding++;
    }

    sample_info->sample_id = context->sample_info_id_generator++;
    context->sample_info_index++;
    context->sample_info_index %= VTB_MAX_DECODING_SAMPLES;

abort:
    SDL_UnlockMutex(context->sample_info_mutex);
}

inline static void sample_info_drop_last_push(Ijk_VideoToolBox_Opaque* context) {
    SDL_LockMutex(context->sample_info_mutex);

    int last_index = context->sample_info_index + VTB_MAX_DECODING_SAMPLES - 1;
    last_index %= VTB_MAX_DECODING_SAMPLES;

    sample_info* sample_info = &context->sample_info_array[last_index];
    if (sample_info->is_decoding) {
        sample_info->is_decoding = 0;
        context->sample_infos_in_decoding--;
    }

    SDL_UnlockMutex(context->sample_info_mutex);
}

inline static void sample_info_recycle(Ijk_VideoToolBox_Opaque* context, sample_info* sample_info) {
    SDL_LockMutex(context->sample_info_mutex);

    if (sample_info->is_decoding) {
        sample_info->is_decoding = 0;
        if (context->sample_infos_in_decoding > 0) context->sample_infos_in_decoding--;
    } else {
        ALOGW("%s, multiple frames in same sample %d / %d\n", __FUNCTION__, sample_info->sample_id,
              context->sample_info_id_generator);
    }

    SDL_CondSignal(context->sample_info_cond);
    SDL_UnlockMutex(context->sample_info_mutex);
}

static CMSampleBufferRef CreateSampleBufferFrom(CMFormatDescriptionRef fmt_desc, void* demux_buff,
                                                size_t demux_size) {
    OSStatus status;
    CMBlockBufferRef newBBufOut = NULL;
    CMSampleBufferRef sBufOut = NULL;

    status = CMBlockBufferCreateWithMemoryBlock(NULL, demux_buff, demux_size, kCFAllocatorNull,
                                                NULL, 0, demux_size, FALSE, &newBBufOut);

    if (!status) {
        status = CMSampleBufferCreate(NULL, newBBufOut, TRUE, 0, 0, fmt_desc, 1, 0, NULL, 0, NULL,
                                      &sBufOut);
    }

    if (newBBufOut) CFRelease(newBBufOut);
    if (status == 0) {
        return sBufOut;
    } else {
        return NULL;
    }
}

static bool GetVTBPicture(Ijk_VideoToolBox_Opaque* context, AVFrame* pVTBPicture) {
    if (context->m_sort_queue == NULL) {
        return false;
    }
    pthread_mutex_lock(&context->m_queue_mutex);

    volatile sort_queue* sort_queue = context->m_sort_queue;
    *pVTBPicture = sort_queue->pic;
    pVTBPicture->opaque = CVBufferRetain(sort_queue->pic.opaque);

    pthread_mutex_unlock(&context->m_queue_mutex);

    return true;
}

static void QueuePicture(Ijk_VideoToolBox_Opaque* ctx, enum AVColorSpace color_space,
                         enum AVColorRange color_range, int64_t abs_time) {
    AVFrame picture = {0};
    if (true == GetVTBPicture(ctx, &picture)) {
        AVRational tb = ctx->ffp->is->video_st->time_base;
        AVRational frame_rate = av_guess_frame_rate(ctx->ffp->is->ic, ctx->ffp->is->video_st, NULL);
        double duration =
            (frame_rate.num && frame_rate.den ? av_q2d((AVRational){frame_rate.den, frame_rate.num})
                                              : 0);
        double pts = (picture.pts == AV_NOPTS_VALUE) ? NAN : picture.pts * av_q2d(tb);

        picture.format = IJK_AV_PIX_FMT__VIDEO_TOOLBOX;
        picture.colorspace = color_space;
        picture.color_range = color_range;

        ffp_queue_picture_with_abs_time(ctx->ffp, &picture, pts, duration, 0,
                                        ctx->ffp->is->viddec.pkt_serial, abs_time);

        CVBufferRelease(picture.opaque);

        SortQueuePop(ctx);
    } else {
        ALOGI("Get Picture failure!!!\n");
    }
}

static void VTDecoderCallback(void* decompressionOutputRefCon, void* sourceFrameRefCon,
                              OSStatus status, VTDecodeInfoFlags infoFlags,
                              CVImageBufferRef imageBuffer, CMTime presentationTimeStamp,
                              CMTime presentationDuration) {
    @autoreleasepool {
        Ijk_VideoToolBox_Opaque* ctx = (Ijk_VideoToolBox_Opaque*)decompressionOutputRefCon;
        if (!ctx) return;

        FFPlayer* ffp = ctx->ffp;
        VideoState* is = ffp->is;
        sort_queue* newFrame = NULL;

        sample_info* sample_info = sourceFrameRefCon;
        if (!sample_info->is_decoding) {
            ALOGD("VTB: frame out of date: id=%d\n", sample_info->sample_id);
            goto failed;
        }

        newFrame = (sort_queue*)mallocz(sizeof(sort_queue));
        if (!newFrame) {
            ALOGE("VTB: create new frame fail: out of memory\n");
            goto failed;
        }

        newFrame->pic.pts = sample_info->pts;
        newFrame->pic.pkt_dts = sample_info->dts;
        newFrame->pic.sample_aspect_ratio.num = sample_info->sar_num;
        newFrame->pic.sample_aspect_ratio.den = sample_info->sar_den;
        newFrame->serial = sample_info->serial;
        newFrame->nextframe = NULL;

        if (newFrame->pic.pts != AV_NOPTS_VALUE) {
            newFrame->sort = newFrame->pic.pts;
        } else {
            newFrame->sort = newFrame->pic.pkt_dts;
            newFrame->pic.pts = newFrame->pic.pkt_dts;
        }

        CVBufferSetAttachment(imageBuffer, kCVImageBufferYCbCrMatrixKey,
                              getColorSpace(sample_info->color_space),
                              kCVAttachmentMode_ShouldPropagate);

        if (ctx->dealloced || is->abort_request || is->viddec.queue->abort_request) goto failed;

        if (status != 0) {
            ALOGE("decode callback %d %s\n", (int)status, vtb_get_error_string(status));
            ffp->v_dec_err = status;
            KwaiQos_onToolBoxDecodeErr(&ffp->kwai_qos);
            goto failed;
        }

        if (ctx->refresh_session) {
            goto failed;
        }

        if (newFrame->serial != ctx->serial) {
            goto failed;
        }

        if (imageBuffer == NULL) {
            ALOGI("imageBuffer null\n");
            goto failed;
        }

        ffp->stat.vdps =
            SDL_SpeedSamplerAdd(&ctx->sampler, FFP_SHOW_VDPS_VIDEOTOOLBOX, "vdps[VideoToolbox]");
        KwaiQos_onVideoFrameDecoded(&ffp->kwai_qos);
#ifdef FFP_VTB_DISABLE_OUTPUT
        goto failed;
#endif

        OSType format_type = CVPixelBufferGetPixelFormatType(imageBuffer);
        if (format_type != getPixelBufferFormat(sample_info->color_range)) {
            ALOGI("format_type error \n");
            goto failed;
        }
        if (kVTDecodeInfo_FrameDropped & infoFlags) {
            ALOGI("droped\n");
            goto failed;
        }

        if (ctx->new_seg_flag) {
            ALOGI("new seg process!!!!");
            while (ctx->m_queue_depth > 0) {
                QueuePicture(ctx, sample_info->color_space, sample_info->color_range,
                             sample_info->abs_time);
            }
            ctx->new_seg_flag = false;
        }

        // Do not need monotonically increasing pts
        /*
        if (ctx->m_sort_queue && newFrame->pic.pts < ctx->m_sort_queue->pic.pts)
        { goto failed;
        }
        */

        if (should_drop_decoded_video_frame(ffp, &newFrame->pic,
                                            "IJKVideoToolBoxSync:VTDecoderCallback")) {
            goto failed;
        }

        if (CVPixelBufferIsPlanar(imageBuffer)) {
            newFrame->pic.width = (int)CVPixelBufferGetWidthOfPlane(imageBuffer, 0);
            newFrame->pic.height = (int)CVPixelBufferGetHeightOfPlane(imageBuffer, 0);
        } else {
            newFrame->pic.width = (int)CVPixelBufferGetWidth(imageBuffer);
            newFrame->pic.height = (int)CVPixelBufferGetHeight(imageBuffer);
        }
        if (ffp->vtb_auto_rotate) {
            int64_t nature_degree = ffp_get_video_rotate_degrees(ffp);
            if (nature_degree == 90) {
                nature_degree = 270;
            } else if (nature_degree == 270) {
                nature_degree = 90;
            }
            newFrame->pic.angle = nature_degree;
        }

        newFrame->pic.opaque = CVBufferRetain(imageBuffer);
        pthread_mutex_lock(&ctx->m_queue_mutex);
        volatile sort_queue* queueWalker = ctx->m_sort_queue;
        if (!queueWalker || (newFrame->sort < queueWalker->sort)) {
            newFrame->nextframe = queueWalker;
            ctx->m_sort_queue = newFrame;
        } else {
            bool frameInserted = false;
            volatile sort_queue* nextFrame = NULL;
            while (!frameInserted) {
                nextFrame = queueWalker->nextframe;
                if (!nextFrame || (newFrame->sort < nextFrame->sort)) {
                    newFrame->nextframe = nextFrame;
                    queueWalker->nextframe = newFrame;
                    frameInserted = true;
                }
                queueWalker = nextFrame;
            }
        }
        ctx->m_queue_depth++;
        pthread_mutex_unlock(&ctx->m_queue_mutex);

        // ALOGI("%lf %lf %lf \n", newFrame->sort,newFrame->pts, newFrame->dts);
        // ALOGI("display queue deep %d\n", ctx->m_queue_depth);

        if (ctx->ffp->is == NULL || ctx->ffp->is->abort_request ||
            ctx->ffp->is->viddec.queue->abort_request) {
            while (ctx->m_queue_depth > 0) {
                SortQueuePop(ctx);
            }
            goto successed;
        }
        // ALOGI("depth %d  %d\n", ctx->m_queue_depth, ctx->m_max_ref_frames);
        if (ctx->m_queue_depth > ctx->fmt_desc.max_ref_frames) {
            QueuePicture(ctx, sample_info->color_space, sample_info->color_range,
                         sample_info->abs_time);
        }
    successed:
        sample_info_recycle(ctx, sample_info);
        return;
    failed:
        sample_info_recycle(ctx, sample_info);
        if (newFrame) {
            free(newFrame);
        }
        return;
    }
}

static void vtbsession_destroy(Ijk_VideoToolBox_Opaque* context) {
    if (!context) return;

    vtbformat_destroy(&context->fmt_desc);

    if (context->vt_session) {
        VTDecompressionSessionWaitForAsynchronousFrames(context->vt_session);
        VTDecompressionSessionInvalidate(context->vt_session);
        CFRelease(context->vt_session);
        context->vt_session = NULL;
    }
}

static VTDecompressionSessionRef vtbsession_create(Ijk_VideoToolBox_Opaque* context, int width,
                                                   int height, enum AVColorRange color_range) {
    FFPlayer* ffp = context->ffp;
    int ret = 0;

    VTDecompressionSessionRef vt_session = NULL;
    CFMutableDictionaryRef destinationPixelBufferAttributes;
    VTDecompressionOutputCallbackRecord outputCallback;
    OSStatus status;

    ret = vtbformat_init(&context->fmt_desc, ffp->is->viddec.avctx);

    if (ret < 0) {
        ALOGE("[%d][IJKVideoToolBoxAsync:vtbsession_create] vtbformat_init fail:%d \n",
              context->ffp->session_id, ret);
        return NULL;
    }

    if (ffp->vtb_max_frame_width > 0 && width > ffp->vtb_max_frame_width) {
        double w_scaler = (float)ffp->vtb_max_frame_width / width;
        width = ffp->vtb_max_frame_width;
        height = height * w_scaler;
    }

    ALOGD("after scale width %d height %d \n", width, height);

    destinationPixelBufferAttributes = CFDictionaryCreateMutable(
        NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetSInt32(destinationPixelBufferAttributes, kCVPixelBufferPixelFormatTypeKey,
                          getPixelBufferFormat(color_range));
    CFDictionarySetSInt32(destinationPixelBufferAttributes, kCVPixelBufferWidthKey, width);
    CFDictionarySetSInt32(destinationPixelBufferAttributes, kCVPixelBufferHeightKey, height);
    CFDictionarySetBoolean(destinationPixelBufferAttributes, kCVPixelBufferOpenGLESCompatibilityKey,
                           YES);
    outputCallback.decompressionOutputCallback = VTDecoderCallback;
    outputCallback.decompressionOutputRefCon = context;
    status = VTDecompressionSessionCreate(kCFAllocatorDefault, context->fmt_desc.fmt_desc, NULL,
                                          destinationPixelBufferAttributes, &outputCallback,
                                          &vt_session);

    if (status != noErr) {
        NSError* error = [NSError errorWithDomain:NSOSStatusErrorDomain code:status userInfo:nil];
        NSLog(@"Error %@", [error description]);
        ALOGI("%s - failed with status = (%d)", __FUNCTION__, (int)status);
    }
    CFRelease(destinationPixelBufferAttributes);

    memset(context->sample_info_array, 0, sizeof(context->sample_info_array));
    context->sample_infos_in_decoding = 0;
    return vt_session;
}

static int decode_video_internal(Ijk_VideoToolBox_Opaque* context, AVCodecContext* avctx,
                                 const AVPacket* avpkt, int* got_picture_ptr, int64_t abs_time,
                                 BOOL is_output_frame) {
    FFPlayer* ffp = context->ffp;
    OSStatus status = 0;
    uint32_t decoder_flags = 0;
    sample_info* sample_info = NULL;
    CMSampleBufferRef sample_buff = NULL;
    AVIOContext* pb = NULL;
    int demux_size = 0;
    uint8_t* demux_buff = NULL;
    uint8_t* pData = avpkt->data;
    int iSize = avpkt->size;
    double pts = avpkt->pts;
    double dts = avpkt->dts;

    if (!context) {
        goto failed;
    }

    if (ffp->vtb_async) {
        decoder_flags |= kVTDecodeFrame_EnableAsynchronousDecompression;
    }

    if (context->refresh_session || !is_output_frame) {
        decoder_flags |= kVTDecodeFrame_DoNotOutputFrame;
        // ALOGI("flag :%d flag %d \n", decoderFlags,avpkt->flags);
    }

    if (context->refresh_request) {
        while (context->m_queue_depth > 0) {
            SortQueuePop(context);
        }

        sample_info_flush(context, 1000);
        vtbsession_destroy(context);
        memset(context->sample_info_array, 0, sizeof(context->sample_info_array));
        context->sample_infos_in_decoding = 0;

        context->vt_session =
            vtbsession_create(context, context->ffp->is->viddec.avctx->width,
                              context->ffp->is->viddec.avctx->height, avctx->color_range);
        if (!context->vt_session) goto failed;
        context->refresh_request = false;
    }

    if (pts == AV_NOPTS_VALUE) {
        pts = dts;
    }

    if (context->fmt_desc.convert_bytestream) {
        // ALOGI("the buffer should m_convert_byte\n");
        if (avio_open_dyn_buf(&pb) < 0) {
            goto failed;
        }
        ff_avc_parse_nal_units(pb, pData, iSize);
        demux_size = avio_close_dyn_buf(pb, &demux_buff);
        // ALOGI("demux_size:%d\n", demux_size);
        if (demux_size == 0) {
            goto failed;
        }
        sample_buff = CreateSampleBufferFrom(context->fmt_desc.fmt_desc, demux_buff, demux_size);
    } else if (context->fmt_desc.convert_3byteTo4byteNALSize) {
        // ALOGI("3byteto4byte\n");
        if (avio_open_dyn_buf(&pb) < 0) {
            goto failed;
        }

        uint32_t nal_size;
        uint8_t* end = avpkt->data + avpkt->size;
        uint8_t* nal_start = pData;
        while (nal_start < end) {
            nal_size = AV_RB24(nal_start);
            avio_wb32(pb, nal_size);
            nal_start += 3;
            avio_write(pb, nal_start, nal_size);
            nal_start += nal_size;
        }
        demux_size = avio_close_dyn_buf(pb, &demux_buff);
        sample_buff = CreateSampleBufferFrom(context->fmt_desc.fmt_desc, demux_buff, demux_size);
    } else {
        sample_buff = CreateSampleBufferFrom(context->fmt_desc.fmt_desc, pData, iSize);
    }
    if (!sample_buff) {
        if (demux_size) {
            av_free(demux_buff);
        }
        ALOGI("%s - CreateSampleBufferFrom failed", __FUNCTION__);
        goto failed;
    }

    //    if (avpkt->flags & AV_PKT_FLAG_NEW_SEG) {
    //        context->new_seg_flag = true;
    //    }

    sample_info = sample_info_peek(context);
    if (!sample_info) {
        ALOGE("%s, failed to peek frame_info\n", __FUNCTION__);
        goto failed;
    }

    sample_info->pts = pts;
    sample_info->dts = dts;
    sample_info->serial = context->serial;
    sample_info->sar_num = avctx->sample_aspect_ratio.num;
    sample_info->sar_den = avctx->sample_aspect_ratio.den;
    sample_info->color_space = avctx->colorspace;
    sample_info->color_range = avctx->color_range;
    sample_info->abs_time = abs_time;
    sample_info_push(context);

    status = VTDecompressionSessionDecodeFrame(context->vt_session, sample_buff, decoder_flags,
                                               (void*)sample_info, 0);
    if (status == noErr) {
        if (context->ffp->is->videoq.abort_request) goto failed;

        // Wait for delayed frames even if kVTDecodeInfo_Asynchronous is not
        // set.
        if (ffp->vtb_wait_async) {
            status = VTDecompressionSessionWaitForAsynchronousFrames(context->vt_session);
        }
    }

    if (status != 0) {
        sample_info_drop_last_push(context);

        ALOGE("decodeFrame %d %s\n", (int)status, vtb_get_error_string(status));

        if (status == kVTInvalidSessionErr) {
            context->refresh_session = true;
        }
        if (status == kVTVideoDecoderMalfunctionErr) {
            context->recovery_drop_packet = true;
            context->refresh_session = true;
        }
        goto failed;
    }

    if (sample_buff) {
        CFRelease(sample_buff);
    }
    if (demux_size) {
        av_free(demux_buff);
    }

    *got_picture_ptr = 1;
    return 0;
failed:
    if (sample_buff) {
        CFRelease(sample_buff);
    }
    if (demux_size) {
        av_free(demux_buff);
    }
    *got_picture_ptr = 0;
    return -1;
}

static inline void ResetPktBuffer(Ijk_VideoToolBox_Opaque* context) {
    for (int i = 0; i < context->m_buffer_deep; i++) {
        av_packet_unref(&context->m_buffer_packet[i]);
    }
    context->m_buffer_deep = 0;
    memset(context->m_buffer_packet, 0, sizeof(context->m_buffer_packet));
}

static inline void DuplicatePkt(Ijk_VideoToolBox_Opaque* context, const AVPacket* pkt) {
    if (context->m_buffer_deep >= MAX_PKT_QUEUE_DEEP) {
        context->idr_based_identified = false;
        ResetPktBuffer(context);
    }
    AVPacket* avpkt = &context->m_buffer_packet[context->m_buffer_deep];
    av_copy_packet(avpkt, pkt);
    context->m_buffer_deep++;
}

// FIXME: install libavformat/internal.h
int ff_alloc_extradata(AVCodecContext* avctx, int size);

static void ExpandExtraDataIfNeed(AVCodecContext* avctx, int new_extradata_size) {
    if (avctx->extradata_size < new_extradata_size) {
        av_freep(&avctx->extradata);
        ff_alloc_extradata(avctx, new_extradata_size);
    }
    avctx->extradata_size = new_extradata_size;
}

static void UpdateExtraData(AVCodecContext* avctx, int new_extradata_size) {
    av_freep(&avctx->extradata);
    ff_alloc_extradata(avctx, new_extradata_size);
    avctx->extradata_size = new_extradata_size;
}

static void CheckResolutionChange(Ijk_VideoToolBox_Opaque* context, AVPacket* pkt, int codec_id) {
    int width = 0, height = 0;
    uint8_t *vps = NULL, *sps = NULL, *pps = NULL;
    int vps_len = 0, sps_len = 0, pps_len = 0;
    AVCodecContext* avctx = context->ffp->is->viddec.avctx;

    switch (codec_id) {
        case AV_CODEC_ID_HEVC:
            if (h265_avpacket_read_vps_sps_pps(pkt, &vps, &vps_len, &sps, &sps_len, &pps,
                                               &pps_len)) {
                parseh265_sps(sps, sps_len, &width, &height);
                // update extra_data
                if (avctx->width != width || avctx->height != height) {
                    avctx->width = width;
                    avctx->height = height;

                    uint8_t* new_extradata = NULL;
                    int extradata_size_diff =
                        vps_len + sps_len + pps_len - context->total_sequence_len;
                    int new_extradata_size = avctx->extradata_size + extradata_size_diff;

                    if (extradata_size_diff != 0 && new_extradata_size > 0) {
                        new_extradata = av_malloc(new_extradata_size);
                        KwaiQos_onHevcParameterSetLenChange(&context->ffp->kwai_qos, new_extradata);
                    }

                    write_hevc_sequence_header(avctx->extradata, avctx->extradata_size,
                                               new_extradata, vps, vps_len, sps, sps_len, pps,
                                               pps_len);
                    if (new_extradata) {
                        UpdateExtraData(avctx, new_extradata_size);
                        memcpy(avctx->extradata, new_extradata, new_extradata_size);
                        av_freep(&new_extradata);
                    }

                    context->refresh_session = true;
                    ALOGI("[videotoolbox] H265 resolution changed width:%d, height:%d\n",
                          avctx->width, avctx->height);
                }
                context->total_sequence_len = vps_len + sps_len + pps_len;
            }
            break;
        case AV_CODEC_ID_H264:
        default:
            if (pkt->side_data) {  // AVC sequence header in FLV
                uint8_t* new_extradata = pkt->side_data->data;
                int new_extradata_size = pkt->side_data->size;
                if (validate_avcC_spc(new_extradata, new_extradata_size, NULL, NULL, NULL, &width,
                                      &height)) {
                    // update extra_data
                    avctx->width = width;
                    avctx->height = height;
                    ExpandExtraDataIfNeed(avctx, new_extradata_size);
                    memcpy(avctx->extradata, new_extradata, new_extradata_size);
                    context->refresh_session = true;
                    ALOGI("[videotoolbox] H264 avvc resolution changed width:%d, height:%d\n",
                          avctx->width, avctx->height);
                }
            }
            if (h264_avpacket_read_sps_pps(pkt, &sps, &sps_len, &pps,
                                           &pps_len)) {  // check SPS/PPS in NALU
                parseh264_sps(sps + 1, sps_len - 1, NULL, NULL, NULL, NULL, &width, &height);
                int new_extradata_size = 16 + sps_len + pps_len;
                // update extra_data
                if (avctx->width != width || avctx->height != height) {
                    avctx->width = width;
                    avctx->height = height;
                    ExpandExtraDataIfNeed(avctx, new_extradata_size);
                    write_avc_sequence_header(avctx->extradata, new_extradata_size, sps, sps_len,
                                              pps, pps_len);
                    context->refresh_session = true;
                    ALOGI("[videotoolbox] H264 sps resolution changed width:%d, height:%d\n",
                          avctx->width, avctx->height);
                }
            }
            break;
    }
}

static int decode_video(Ijk_VideoToolBox_Opaque* context, AVCodecContext* avctx, AVPacket* avpkt,
                        int* got_picture_ptr, int64_t abs_time) {
    int ret = 0;
    int codec_id = context->fmt_desc.codec_id;

    if (!avpkt || !avpkt->data) {
        return 0;
    }

    if (ff_avpacket_is_idr(avpkt, codec_id) == true) {
        context->idr_based_identified = true;
    }
    if (ff_avpacket_i_or_idr(avpkt, context->idr_based_identified, codec_id) == true) {
        ResetPktBuffer(context);
        context->recovery_drop_packet = false;
    }
    if (context->recovery_drop_packet == true) {
        return -1;
    }

    CheckResolutionChange(context, avpkt, codec_id);

    DuplicatePkt(context, avpkt);

    if (context->refresh_session) {
        ret = 0;

        sample_info_flush(context, 1000);
        vtbsession_destroy(context);
        memset(context->sample_info_array, 0, sizeof(context->sample_info_array));
        context->sample_infos_in_decoding = 0;

        context->vt_session =
            vtbsession_create(context, context->ffp->is->viddec.avctx->width,
                              context->ffp->is->viddec.avctx->height, avctx->color_range);
        if (!context->vt_session) return -1;
        context->refresh_session = false;

        if ((context->m_buffer_deep > 0) &&
            ff_avpacket_i_or_idr(&context->m_buffer_packet[0], context->idr_based_identified,
                                 codec_id) == true) {
            for (int i = 0; i < context->m_buffer_deep; i++) {
                AVPacket* pkt = &context->m_buffer_packet[i];
                if (i == context->m_buffer_deep - 1) {
                    ret =
                        decode_video_internal(context, avctx, pkt, got_picture_ptr, abs_time, true);
                } else {
                    ret = decode_video_internal(context, avctx, pkt, got_picture_ptr, abs_time,
                                                false);
                }
            }
        } else {
            context->recovery_drop_packet = true;
            ret = -1;
            ALOGE("recovery error!!!!\n");
        }
        return ret;
    }
    return decode_video_internal(context, avctx, avpkt, got_picture_ptr, abs_time, true);
}

static void dict_set_string(CFMutableDictionaryRef dict, CFStringRef key, const char* value) {
    CFStringRef string;
    string = CFStringCreateWithCString(NULL, value, kCFStringEncodingASCII);
    CFDictionarySetValue(dict, key, string);
    CFRelease(string);
}

static void dict_set_boolean(CFMutableDictionaryRef dict, CFStringRef key, BOOL value) {
    CFDictionarySetValue(dict, key, value ? kCFBooleanTrue : kCFBooleanFalse);
}

static void dict_set_object(CFMutableDictionaryRef dict, CFStringRef key, CFTypeRef* value) {
    CFDictionarySetValue(dict, key, value);
}

static void dict_set_data(CFMutableDictionaryRef dict, CFStringRef key, uint8_t* value,
                          uint64_t length) {
    CFDataRef data;
    data = CFDataCreate(NULL, value, (CFIndex)length);
    CFDictionarySetValue(dict, key, data);
    CFRelease(data);
}

static void dict_set_i32(CFMutableDictionaryRef dict, CFStringRef key, int32_t value) {
    CFNumberRef number;
    number = CFNumberCreate(NULL, kCFNumberSInt32Type, &value);
    CFDictionarySetValue(dict, key, number);
    CFRelease(number);
}

static CMFormatDescriptionRef CreateFormatDescriptionFromCodecData(Uint32 format_id, int width,
                                                                   int height,
                                                                   const uint8_t* extradata,
                                                                   int extradata_size,
                                                                   uint32_t atom) {
    CMFormatDescriptionRef fmt_desc = NULL;
    OSStatus status;

    CFMutableDictionaryRef par = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks,
                                                           &kCFTypeDictionaryValueCallBacks);
    CFMutableDictionaryRef atoms = CFDictionaryCreateMutable(
        NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFMutableDictionaryRef extensions = CFDictionaryCreateMutable(
        NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

    /* CVPixelAspectRatio dict */
    dict_set_i32(par, CFSTR("HorizontalSpacing"), 0);
    dict_set_i32(par, CFSTR("VerticalSpacing"), 0);
    /* SampleDescriptionExtensionAtoms dict */
    switch (format_id) {
        case kCMVideoCodecType_H264:
            dict_set_data(atoms, CFSTR("avcC"), (uint8_t*)extradata, extradata_size);
            break;
        case kCMVideoCodecType_HEVC:
            dict_set_data(atoms, CFSTR("hvcC"), (uint8_t*)extradata, extradata_size);
            break;
        default:
            break;
    }

    /* Extensions dict */
    dict_set_string(extensions, CFSTR("CVImageBufferChromaLocationBottomField"), "left");
    dict_set_string(extensions, CFSTR("CVImageBufferChromaLocationTopField"), "left");
    dict_set_boolean(extensions, CFSTR("FullRangeVideo"), FALSE);
    dict_set_object(extensions, CFSTR("CVPixelAspectRatio"), (CFTypeRef*)par);
    dict_set_object(extensions, CFSTR("SampleDescriptionExtensionAtoms"), (CFTypeRef*)atoms);
    status = CMVideoFormatDescriptionCreate(NULL, format_id, width, height, extensions, &fmt_desc);

    CFRelease(extensions);
    CFRelease(atoms);
    CFRelease(par);

    if (status == 0)
        return fmt_desc;
    else
        return NULL;
}

void videotoolbox_async_free(Ijk_VideoToolBox_Opaque* context) {
    context->dealloced = true;

    while (context && context->m_queue_depth > 0) {
        SortQueuePop(context);
    }

    sample_info_flush(context, 3000);
    vtbsession_destroy(context);

    if (context) {
        ResetPktBuffer(context);
        SDL_DestroyCondP(&context->sample_info_cond);
        SDL_DestroyMutexP(&context->sample_info_mutex);
    }

    vtbformat_destroy(&context->fmt_desc);

    freep((void**)&context);
}

int videotoolbox_async_decode_frame(Ijk_VideoToolBox_Opaque* context) {
    FFPlayer* ffp = context->ffp;
    VideoState* is = ffp->is;
    Decoder* d = &is->viddec;
    int got_frame = 0;
    int64_t abs_time = 0;
    do {
        int ret = -1;
        if (is->abort_request || d->queue->abort_request) {
            return -1;
        }

        if (!d->packet_pending || d->queue->serial != d->pkt_serial) {
            AVPacket pkt;
            do {
                if (d->queue->nb_packets == 0) SDL_CondSignal(d->empty_queue_cond);
                ffp_video_statistic_l(ffp);
                if (ffp_packet_queue_get_with_abs_time(ffp, d->queue, &pkt, &d->pkt_serial,
                                                       &d->finished, &abs_time) < 0)
                    return -1;
                if (ffp_is_flush_packet(&pkt)) {
                    // avcodec_flush_buffers(d->avctx);
                    // //不知道为什么要在这里加上flush ffmpeg decoder的逻辑
                    context->refresh_request = true;
                    context->serial += 1;
                    d->finished = 0;
                    ALOGI("flushed last keyframe pts %lld \n", d->pkt.pts);
                    d->next_pts = d->start_pts;
                    d->next_pts_tb = d->start_pts_tb;
                }
            } while (ffp_is_flush_packet(&pkt) || d->queue->serial != d->pkt_serial);

            av_packet_split_side_data(&pkt);

            av_packet_unref(&d->pkt);
            d->pkt_temp = d->pkt = pkt;
            d->packet_pending = 1;
        }
        ffp->stat.vrps =
            SDL_SpeedSamplerAdd(&ffp->vrps_sampler, FFP_SHOW_VRPS_AVCODEC, "vrps[avcodec]");
        KwaiQos_onVideoFrameBeforeDecode(&ffp->kwai_qos);
        ret = decode_video(context, d->avctx, &d->pkt_temp, &got_frame, abs_time);
        if (ret < 0) {
            d->packet_pending = 0;
        } else {
            d->pkt_temp.dts = d->pkt_temp.pts = AV_NOPTS_VALUE;
            if (d->pkt_temp.data) {
                if (d->avctx->codec_type != AVMEDIA_TYPE_AUDIO) ret = d->pkt_temp.size;
                d->pkt_temp.data += ret;
                d->pkt_temp.size -= ret;
                if (d->pkt_temp.size <= 0) d->packet_pending = 0;
            } else {
                if (!got_frame) {
                    while (context->m_queue_depth > 0) {
                        QueuePicture(context, d->avctx->colorspace, d->avctx->color_range, 0);
                    }
                    d->packet_pending = 0;
                    d->finished = d->pkt_serial;
                }
            }
        }
    } while (!got_frame && !d->finished);
    return got_frame;
}

static void vtbformat_destroy(VTBFormatDesc* fmt_desc) {
    if (!fmt_desc || !fmt_desc->fmt_desc) return;

    CFRelease(fmt_desc->fmt_desc);
    fmt_desc->fmt_desc = NULL;
}

static int vtbformat_init(VTBFormatDesc* fmt_desc, AVCodecContext* ic) {
    int width = ic->width;
    int height = ic->height;
    int level = ic->level;
    int profile = ic->profile;
    int sps_level = 0;
    int sps_profile = 0;
    int extrasize = ic->extradata_size;
    int codec = ic->codec_id;
    uint8_t* extradata = ic->extradata;

    bool isHevcSupported = false;
#if 0
    switch (profile) {
        case FF_PROFILE_H264_HIGH_10:
            if ([IJKDeviceModel currentModel].rank >= kIJKDeviceRank_AppleA7Class) {
                // Apple A7 SoC
                // Hi10p can be decoded into NV12 ('420v')
                break;
            }
        case FF_PROFILE_H264_HIGH_10_INTRA:
        case FF_PROFILE_H264_HIGH_422:
        case FF_PROFILE_H264_HIGH_422_INTRA:
        case FF_PROFILE_H264_HIGH_444_PREDICTIVE:
        case FF_PROFILE_H264_HIGH_444_INTRA:
        case FF_PROFILE_H264_CAVLC_444:
            goto failed;
    }
#endif
    if (width < 0 || height < 0) {
        goto fail;
    }

    switch (codec) {
        case AV_CODEC_ID_HEVC:
            fmt_desc->codec_id = AV_CODEC_ID_HEVC;
            if (@available(iOS 11.0, *)) {
                isHevcSupported = VTIsHardwareDecodeSupported(kCMVideoCodecType_HEVC);
            } else {
                // Fallback on earlier versions
                isHevcSupported = false;
            }
            if (!isHevcSupported) {
                goto fail;
            }
            fmt_desc->fmt_desc = CreateFormatDescriptionFromCodecData(
                kCMVideoCodecType_HEVC, width, height, extradata, extrasize, IJK_VTB_FCC_AVCC);
            if (fmt_desc->fmt_desc == NULL) {
                goto fail;
            }

            ALOGI("%s - using hvcC atom of size(%d), ref_frames(%d)", __FUNCTION__, extrasize,
                  fmt_desc->max_ref_frames);
            break;
        case AV_CODEC_ID_H264:
            fmt_desc->codec_id = AV_CODEC_ID_H264;
            if (extrasize < 7 || extradata == NULL) {
                ALOGI("%s - avcC atom too data small or missing", __FUNCTION__);
                goto fail;
            }

            if (extradata[0] == 1) {
                if (!validate_avcC_spc(extradata, extrasize, &fmt_desc->max_ref_frames, &sps_level,
                                       &sps_profile, &width, &height)) {
                    ALOGE("VTD not support interlaced video, goto fail, %s\n", __FUNCTION__);
                    goto fail;
                }
                if (level == 0 && sps_level > 0) level = sps_level;

                if (profile == 0 && sps_profile > 0) profile = sps_profile;
                if (profile == FF_PROFILE_H264_MAIN && level == 32 &&
                    fmt_desc->max_ref_frames > 4) {
                    ALOGE("%s - Main@L3.2 detected, VTB cannot decode with %d ref "
                          "frames",
                          __FUNCTION__, fmt_desc->max_ref_frames);
                    goto fail;
                }

                if (extradata[4] == 0xFE) {
                    extradata[4] = 0xFF;
                    fmt_desc->convert_3byteTo4byteNALSize = true;
                }

                fmt_desc->fmt_desc = CreateFormatDescriptionFromCodecData(
                    kCMVideoCodecType_H264, width, height, extradata, extrasize, IJK_VTB_FCC_AVCC);
                if (fmt_desc->fmt_desc == NULL) {
                    goto fail;
                }

                ALOGI("%s - using avcC atom of size(%d), ref_frames(%d)", __FUNCTION__, extrasize,
                      fmt_desc->max_ref_frames);
            } else {
                if ((extradata[0] == 0 && extradata[1] == 0 && extradata[2] == 0 &&
                     extradata[3] == 1) ||
                    (extradata[0] == 0 && extradata[1] == 0 && extradata[2] == 1)) {
                    AVIOContext* pb;
                    if (avio_open_dyn_buf(&pb) < 0) {
                        goto fail;
                    }

                    fmt_desc->convert_bytestream = true;
                    ff_isom_write_avcc(pb, extradata, extrasize);
                    extradata = NULL;

                    extrasize = avio_close_dyn_buf(pb, &extradata);

                    if (!validate_avcC_spc(extradata, extrasize, &fmt_desc->max_ref_frames,
                                           &sps_level, &sps_profile, &width, &height)) {
                        av_free(extradata);
                        goto fail;
                    }

                    fmt_desc->fmt_desc = CreateFormatDescriptionFromCodecData(
                        kCMVideoCodecType_H264, width, height, extradata, extrasize,
                        IJK_VTB_FCC_AVCC);
                    if (fmt_desc->fmt_desc == NULL) {
                        goto fail;
                    }

                    av_free(extradata);
                } else {
                    ALOGI("%s - invalid avcC atom data", __FUNCTION__);
                    goto fail;
                }
            }
            break;
        default:
            goto fail;
    }

    if (fmt_desc->max_ref_frames == 0) {
        fmt_desc->max_ref_frames = 3;
    } else {
        fmt_desc->max_ref_frames = FFMAX(fmt_desc->max_ref_frames, 2);
        fmt_desc->max_ref_frames = FFMIN(fmt_desc->max_ref_frames, 5);
    }

    ALOGI("m_max_ref_frames %d \n", fmt_desc->max_ref_frames);

    return 0;
fail:
    vtbformat_destroy(fmt_desc);
    return -1;
}

Ijk_VideoToolBox_Opaque* videotoolbox_async_create(FFPlayer* ffp, AVCodecContext* avctx) {
    int ret = 0;

    if (ret) {
        ALOGW("%s - videotoolbox can not exists twice at the same time", __FUNCTION__);
        return NULL;
    }

    Ijk_VideoToolBox_Opaque* context_vtb =
        (Ijk_VideoToolBox_Opaque*)mallocz(sizeof(Ijk_VideoToolBox_Opaque));

    context_vtb->sample_info_mutex = SDL_CreateMutex();
    context_vtb->sample_info_cond = SDL_CreateCond();

    if (!context_vtb) {
        goto fail;
    }

    context_vtb->ffp = ffp;
    context_vtb->idr_based_identified = false;

    ret = vtbformat_init(&context_vtb->fmt_desc, avctx);
    if (ret) goto fail;
    assert(context_vtb->fmt_desc.fmt_desc);
    vtbformat_destroy(&context_vtb->fmt_desc);

    context_vtb->vt_session =
        vtbsession_create(context_vtb, context_vtb->ffp->is->viddec.avctx->width,
                          context_vtb->ffp->is->viddec.avctx->height, avctx->color_range);
    if (context_vtb->vt_session == NULL) goto fail;

    context_vtb->m_sort_queue = 0;
    context_vtb->m_queue_depth = 0;

    SDL_SpeedSamplerReset(&context_vtb->sampler);
    return context_vtb;

fail:
    videotoolbox_async_free(context_vtb);
    return NULL;
}
