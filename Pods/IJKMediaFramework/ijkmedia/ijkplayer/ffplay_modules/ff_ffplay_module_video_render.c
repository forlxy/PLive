//
// Created by MarshallShuai on 2019/4/19.
//

#include "ff_ffplay.h"
#include "ff_ffplay_debug.h"
#include "ff_ffplay_module_video_render.h"

#include "ff_ffplay_def.h"
#include "ff_ffplay_clock.h"
#include "ff_ffplay_internal.h"

/* polls for possible required screen refresh at least this often, should be less than 1/fps */
#define REFRESH_RATE 0.01

/* external clock speed adjustment constants for realtime sources based on buffer fullness */
#define EXTERNAL_CLOCK_SPEED_MIN  0.900
#define EXTERNAL_CLOCK_SPEED_MAX  1.010
#define EXTERNAL_CLOCK_SPEED_STEP 0.001

#define EXTERNAL_CLOCK_MIN_FRAMES 2
#define EXTERNAL_CLOCK_MAX_FRAMES 10

static double vp_duration(VideoState* is, Frame* vp, Frame* nextvp) {
    if (vp->serial == nextvp->serial) {
        double duration = nextvp->pts - vp->pts;
        if (isnan(duration) || duration <= 0 || duration > is->max_frame_duration) {
            return vp->duration;
        } else {
            return duration;
        }
    } else {
        return 0.0;
    }
}

static void update_video_pts(VideoState* is, double pts, int64_t pos, int serial) {
    /* update current video pts */
    set_clock(&is->vidclk, pts, serial);
    sync_clock_to_slave(&is->extclk, &is->vidclk);

    is->last_vp_pts = pts;
    is->last_vp_pos = pos;
    is->last_vp_serial = serial;

    if (FFP_SHOW_VIDEO_REFRESH_DISPLAY)
        ALOGI("[av_sync], after update_video_pts , vp->pts:%f, vidclk:%f , audclk:%f\n", pts, get_clock(&is->vidclk), get_clock(&is->audclk));
}

/**
 * 貌似会判断 audioq/videoq缓存的packet数量，来动态调节ext clock的speed
 */
static void check_external_clock_speed(VideoState* is) {
    if ((is->video_stream >= 0 && is->videoq.nb_packets <= EXTERNAL_CLOCK_MIN_FRAMES) ||
        (is->audio_stream >= 0 && is->audioq.nb_packets <= EXTERNAL_CLOCK_MIN_FRAMES)) {
        set_clock_speed(&is->extclk, FFMAX(EXTERNAL_CLOCK_SPEED_MIN,
                                           is->extclk.speed - EXTERNAL_CLOCK_SPEED_STEP));
    } else if ((is->video_stream < 0 || is->videoq.nb_packets > EXTERNAL_CLOCK_MAX_FRAMES) &&
               (is->audio_stream < 0 || is->audioq.nb_packets > EXTERNAL_CLOCK_MAX_FRAMES)) {
        set_clock_speed(&is->extclk, FFMIN(EXTERNAL_CLOCK_SPEED_MAX,
                                           is->extclk.speed + EXTERNAL_CLOCK_SPEED_STEP));
    } else {
        double speed = is->extclk.speed;
        if (speed != 1.0)
            set_clock_speed(&is->extclk,
                            speed + EXTERNAL_CLOCK_SPEED_STEP * (1.0 - speed) / fabs(1.0 - speed));
    }
}

#if 0
static double compute_target_delay(FFPlayer* ffp, double delay, VideoState* is) {
    double sync_threshold, diff = 0;

    /* update delay to follow master synchronisation source */
    if (get_master_sync_type(is) != AV_SYNC_VIDEO_MASTER) {
        /* if video is slave, we try to correct big delays by
           duplicating or deleting a frame */
        double master_clock = get_master_clock(is);
        if (isnan(master_clock)) {
            return NAN;
        }
        double video_clock = get_clock(&is->vidclk);
        diff = video_clock - master_clock;

        /* skip or repeat frame. We take into account the
           delay to compute the threshold. I still don't know
           if it is the best guess */
        sync_threshold = FFMAX(AV_SYNC_THRESHOLD_MIN, FFMIN(AV_SYNC_THRESHOLD_MAX, delay));
        /* -- by bbcallen: replace is->max_frame_duration with AV_NOSYNC_THRESHOLD */
        if (!isnan(diff) && fabs(diff) < AV_NOSYNC_THRESHOLD) {
            if (ffp->is->is_illegal_pts_checked) {
                ffp->is->illegal_audio_pts = 0;
                ffp->is->is_illegal_pts_checked = 0;
            }
            if (diff <= -sync_threshold) {
                delay = FFMAX(0, delay + diff);
            } else if (diff >= sync_threshold && delay > AV_SYNC_FRAMEDUP_THRESHOLD) {
                delay = delay + diff;
            } else if ((ffp->first_video_frame_rendered == 1)
                       && (diff >= sync_threshold + AV_SYNC_FRAMEDUP_THRESHOLD)) {
                delay = diff - AV_SYNC_FRAMEDUP_THRESHOLD;
            } else if (diff >= sync_threshold) {
                delay = 2 * delay;
            }
        } else if (ffp->is->illegal_audio_pts) {
            diff = ffp->stat.avdiff;
            ffp->is->is_illegal_pts_checked = 1;
            if (diff <= -sync_threshold) {
                delay = FFMAX(0, delay + diff);
            } else if (diff >= sync_threshold && delay > AV_SYNC_FRAMEDUP_THRESHOLD) {
                delay = delay + diff;
            } else if (diff >= sync_threshold) {
                delay = 2 * delay;
            }
        }
    }

    if (ffp) {
        ffp->stat.avdelay = (float) delay;
        ffp->stat.avdiff = -(float) diff;

        if (ffp->stat.avdiff < 0) {
            if (ffp->stat.avdiff < ffp->stat.minAvDiffRealTime)
                ffp->stat.minAvDiffRealTime = ffp->stat.avdiff;
            if (ffp->stat.avdiff < ffp->stat.minAvDiffTotalTime) {
                ffp->stat.minAvDiffTotalTime = ffp->stat.avdiff;
                KwaiQos_setMinAvDiff((&ffp->kwai_qos), (int)(1000 * ffp->stat.avdiff));
            }
        }
        if (ffp->stat.avdiff > 0) {
            if (ffp->stat.avdiff > ffp->stat.maxAvDiffRealTime)
                ffp->stat.maxAvDiffRealTime = ffp->stat.avdiff;
            if (ffp->stat.avdiff > ffp->stat.maxAvDiffTotalTime) {
                ffp->stat.maxAvDiffTotalTime = ffp->stat.avdiff;
                KwaiQos_setMaxAvDiff((&ffp->kwai_qos), (int)(1000 * ffp->stat.avdiff));
            }
        }
    }

#ifdef FFP_SHOW_AUDIO_DELAY
    ALOGD("[%u][av_sync][%s] video: delay=%0.3f A-V=%f\n",
          ffp->session_id, __func__, delay, -diff);
#endif

    return delay;
}
#endif

static double compute_target_delay_opt(FFPlayer* ffp, double delay, VideoState* is) {
    double sync_threshold, diff = 0;

    /* update delay to follow master synchronisation source */
    if (get_master_sync_type(is) != AV_SYNC_VIDEO_MASTER) {
        /* if video is slave, we try to correct big delays by
           duplicating or deleting a frame */
        double master_clock = get_master_clock(is);
        if (isnan(master_clock)) {
            return NAN;
        }
        double video_clock = get_clock(&is->vidclk);
        diff = video_clock - master_clock;

        /* skip or repeat frame. We take into account the
           delay to compute the threshold. I still don't know
           if it is the best guess */
        sync_threshold = FFMAX(AV_SYNC_THRESHOLD_MIN, FFMIN(AV_SYNC_THRESHOLD_MAX, delay));
        /* -- by bbcallen: replace is->max_frame_duration with AV_NOSYNC_THRESHOLD */
        if (!isnan(diff) && fabs(diff) < AV_NOSYNC_THRESHOLD) {
            if (ffp->is->is_illegal_pts_checked) {
                ffp->is->illegal_audio_pts = 0;
                ffp->is->is_illegal_pts_checked = 0;
            }
            if (diff <= -sync_threshold) {
                delay = FFMAX(0, delay + diff);
                if (FFP_SHOW_VIDEO_REFRESH_DISPLAY)
                    ALOGD("[%u][av_sync] 11 delay:%f \n", ffp->session_id, delay);
            } else if (diff >= sync_threshold && delay > AV_SYNC_FRAMEDUP_THRESHOLD) {
                delay = delay + diff;
                if (FFP_SHOW_VIDEO_REFRESH_DISPLAY)
                    ALOGD("[%u][av_sync] 12 delay:%f \n", ffp->session_id, delay);
            }
//            else if ((ffp->first_video_frame_rendered == 1)
//                     && (diff >= sync_threshold + AV_SYNC_FRAMEDUP_THRESHOLD)) {
//                delay = diff - AV_SYNC_FRAMEDUP_THRESHOLD;
//            }
            else if (diff >=  AV_SYNC_FRAMEDUP_THRESHOLD) {
                if (FFP_SHOW_VIDEO_REFRESH_DISPLAY)
                    ALOGD("[%u][av_sync] 13 delay:%f diff:%f video_clock:%f, master_clock%f AV_SYNC_FRAMEDUP_THRESHOLD:%f \n", ffp->session_id, delay, diff, video_clock, master_clock, AV_SYNC_FRAMEDUP_THRESHOLD);
                return NAN;
            } else if (diff >= sync_threshold) {
                delay = 2 * delay;
                if (FFP_SHOW_VIDEO_REFRESH_DISPLAY)
                    ALOGD("[%u][av_sync] 14 delay:%f diff:%f sync_threshold:%f \n", ffp->session_id, delay, diff, sync_threshold);
            } else {
                if (FFP_SHOW_VIDEO_REFRESH_DISPLAY)
                    ALOGD("[%u][av_sync] 15 delay:%f \n", ffp->session_id, delay);
            }
        } else if (ffp->is->illegal_audio_pts) {
            diff = ffp->stat.avdiff;
            ffp->is->is_illegal_pts_checked = 1;
            if (diff <= -sync_threshold) {
                delay = FFMAX(0, delay + diff);
                if (FFP_SHOW_VIDEO_REFRESH_DISPLAY)
                    ALOGD("[%u][av_sync] 21 delay:%f \n", ffp->session_id, delay);
            } else if (diff >= sync_threshold && delay > AV_SYNC_FRAMEDUP_THRESHOLD) {
                delay = delay + diff;
                if (FFP_SHOW_VIDEO_REFRESH_DISPLAY)
                    ALOGD("[%u][av_sync] 22 delay:%f \n", ffp->session_id, delay);
            } else if (diff >= sync_threshold) {
                delay = 2 * delay;
                if (FFP_SHOW_VIDEO_REFRESH_DISPLAY)
                    ALOGD("[%u][av_sync] 23 delay:%f \n", ffp->session_id, delay);
            }
        } else {
            if (FFP_SHOW_VIDEO_REFRESH_DISPLAY)
                ALOGD("[%u][av_sync] 31 fabs(diff):%f, ffp->is->illegal_audio_pts:%d, delay:%f \n",
                      ffp->session_id, fabs(diff), ffp->is->illegal_audio_pts, delay);
        }
    }

    if (ffp) {
        ffp->stat.avdelay = (float) delay;
        ffp->stat.avdiff = -(float) diff;

        if (!isnan(ffp->stat.avdiff) && ffp->stat.avdiff < 0) {
            if (ffp->stat.avdiff < ffp->stat.minAvDiffRealTime)
                ffp->stat.minAvDiffRealTime = ffp->stat.avdiff;
            if (ffp->stat.avdiff < ffp->stat.minAvDiffTotalTime) {
                ffp->stat.minAvDiffTotalTime = ffp->stat.avdiff;
                KwaiQos_setMinAvDiff((&ffp->kwai_qos), (int)(1000 * ffp->stat.avdiff));
            }
        }
        if (!isnan(ffp->stat.avdiff) && ffp->stat.avdiff > 0) {
            if (ffp->stat.avdiff > ffp->stat.maxAvDiffRealTime)
                ffp->stat.maxAvDiffRealTime = ffp->stat.avdiff;
            if (ffp->stat.avdiff > ffp->stat.maxAvDiffTotalTime) {
                ffp->stat.maxAvDiffTotalTime = ffp->stat.avdiff;
                KwaiQos_setMaxAvDiff((&ffp->kwai_qos), (int)(1000 * ffp->stat.avdiff));
            }
        }
    }

#ifdef FFP_SHOW_AUDIO_DELAY
    ALOGD("[%u][av_sync][%s] video: delay=%0.3f A-V=%f\n",
          ffp->session_id, __func__, delay, -diff);
#endif

    return delay;
}

static void ffp_live_type_queue_get_or_notify(FFPlayer* ffp, int64_t ptsMs) {
    if (!ffp || !ffp->is)
        return;

    VideoState* is = ffp->is;

    if (ffp->islive && is->live_type_queue.nb_livetype > 0) {
        int live_type = 0;
        int ret = live_type_queue_get(&is->live_type_queue, ptsMs, &live_type);
        if (ret == 0) {
            ffp_notify_msg2(ffp, FFP_MSG_LIVE_TYPE_CHANGE, live_type);
        }
    }
}

// called by video_display2
static void video_image_display2(FFPlayer* ffp) {
    VideoState* is = ffp->is;
    Frame* vp;

    vp = frame_queue_peek_last(&is->pictq);

    int latest_seek_load_serial = __atomic_exchange_n(&(is->latest_seek_load_serial), -1,
                                                      memory_order_seq_cst);
    if (latest_seek_load_serial == vp->serial)
        ffp->stat.latest_seek_load_duration = (av_gettime() - is->latest_seek_load_start_at) / 1000;

    if (vp->bmp) {
        int64_t t0 = av_gettime_relative() / 1000;
        int ret = SDL_VoutDisplayYUVOverlay(ffp->vout, vp->bmp);
        int64_t t1 = av_gettime_relative() / 1000;
#define RENDER_VIDEO_OVERHEAD_THRESHOLD_MS 30
        if (t1 - t0 > RENDER_VIDEO_OVERHEAD_THRESHOLD_MS) {
            ALOGW("[%d][video_image_display2] SDL_VoutDisplayYUVOverlay(pts:%3.2f) overhead cost %lld ms!", ffp->session_id, vp->pts, t1 - t0);
        }

        if (ret < 0) {
            KwaiQos_onDisplayError(&ffp->kwai_qos, ret);
        }
        ffp->stat.vfps = SDL_SpeedSamplerAdd(&ffp->vfps_sampler, FFP_SHOW_VFPS_FFPLAY,
                                             "vfps[ffplay]");

        ffp_live_type_queue_get_or_notify(ffp, fftime_to_milliseconds(vp->pts * 1000 * 1000));

        if (is->width != vp->width || is->height != vp->height ||
            is->rotation != vp->rotation) {
            if (vp->rotation == 90 || vp->rotation == 270) {
                ffp_notify_msg3(ffp, FFP_MSG_VIDEO_SIZE_CHANGED, vp->height,
                                vp->width);
            } else {
                ffp_notify_msg3(ffp, FFP_MSG_VIDEO_SIZE_CHANGED, vp->width,
                                vp->height);
            }
        }

        is->width = vp->width;
        is->height = vp->height;
        is->rotation = vp->rotation;
        KwaiQos_onFrameRendered(&ffp->kwai_qos, vp->duration, ffp->start_on_prepared);

        KwaiQos_setBlockInfoStartPeriod(&ffp->kwai_qos);

        // QosInfo
        if (ffp->qos_pts_offset_got && ffp->wall_clock_updated) {
            DelayStat_calc_pts_delay(&ffp->qos_delay_video_render, ffp->wall_clock_offset,
                                     ffp->qos_pts_offset, (int64_t)(is->vidclk.pts * 1000));
        }

        if (ffp->is_video_reloaded && ffp->islive && !ffp->first_reloaded_v_frame_rendered) {
            ffp->first_reloaded_v_frame_rendered = 1;
            ffp_notify_msg1(ffp, FFP_MSG_RELOADED_VIDEO_RENDERING_START);
        }

        if (!ffp->first_video_frame_rendered) {
            ffp->first_video_frame_rendered = 1;
            ffp_notify_msg1(ffp, FFP_MSG_VIDEO_RENDERING_START);
            SDL_CondSignal(is->continue_audio_read_thread);
            if (is->audio_stream < 0) {
                KwaiQos_onStartPlayer(&ffp->kwai_qos);
            }
        }
        if (ffp->video_rendered_after_seek_need_notify && is->seek_pos != -1) {
            if (ffp->enable_accurate_seek) {
                if (!is->video_accurate_seek_req) {
                    ALOGD("[%u] video_image_display2, FFP_MSG_VIDEO_RENDERING_START_AFTER_SEEK\n", ffp->session_id);
                    ffp->video_rendered_after_seek_need_notify = 0;
                    KwaiQos_onFirstFrameAfterSeekEnd(&ffp->kwai_qos);
                    ffp_notify_msg1(ffp, FFP_MSG_VIDEO_RENDERING_START_AFTER_SEEK);
                }
            } else {
                ffp->video_rendered_after_seek_need_notify = 0;
                KwaiQos_onFirstFrameAfterSeekEnd(&ffp->kwai_qos);
                ffp_notify_msg1(ffp, FFP_MSG_VIDEO_RENDERING_START_AFTER_SEEK);
            }
        }
    }
}

/**
 * display the current picture, if any
 * called by video_refresh
 */
static void video_display2(FFPlayer* ffp) {
    VideoState* is = ffp->is;
    if (is->video_st) {
        video_image_display2(ffp);
    }
}

inline static bool need_check_live_video_reload(FFPlayer* ffp) {
    if (ffp && ffp->islive && ffp->is_video_reloaded && !ffp->first_reloaded_v_frame_rendered) {
        return true;
    } else {
        return false;
    }
}

inline static bool need_drop_video_frame_after_reload_video(FFPlayer* ffp, Frame* vp) {
    if (!ffp || ffp->first_reloaded_v_frame_rendered || !ffp->is
        || !ffp->is_video_reloaded || !ffp->islive || !vp || isnan(ffp->is->audio_clock)) {
        return false;
    }

    // 1. 视频reload后，还未下载到第一个视频帧(reloaded_video_first_pts=AV_NOPTS_VALUE),直接丢弃
    // 2. 视频reload后，如果发现frame_queue里吐出来的frame pts比新video的首帧pts小，直接丢弃
    //    原因是video decoder有时候有缓存的解码完的帧没有吐出来
    // 3. 视频reload后，如果当前送给render的video pts比audio pts小，也丢弃，否则会快速显示(闪烁)
    int64_t cur_v_pts = fftime_to_milliseconds(vp->pts * 1000 * 1000);
    int64_t cur_a_pts = fftime_to_milliseconds(ffp->is->audio_clock * 1000 * 1000);
    if ((ffp->reloaded_video_first_pts == AV_NOPTS_VALUE) ||
        (cur_v_pts < ffp->reloaded_video_first_pts) || (cur_v_pts < cur_a_pts)) {
        ALOGI("[%u] cur_v_pts=%lld cur_a_pts=%lld, reloaded_video_first_pts=%lld, frame will drop\n",
              ffp->session_id, cur_v_pts, cur_a_pts, ffp->reloaded_video_first_pts);
        return true;
    } else {
        return false;
    }
}

inline static bool need_delay_reloaded_video_frame_display(FFPlayer* ffp, Frame* vp) {
    if (!ffp || ffp->first_reloaded_v_frame_rendered || !ffp->is
        || !ffp->is_video_reloaded || !ffp->islive || !vp || isnan(ffp->is->audio_clock)) {
        return false;
    }

    double audio_clock = ffp->is->audio_clock;
    int64_t cur_v_pts = fftime_to_milliseconds(vp->pts * 1000 * 1000);
    int64_t cur_a_pts = fftime_to_milliseconds(audio_clock * 1000 * 1000);

    // audioonly返回video播放时，首帧video等到audio播放到首帧对应的pts时再输出，否则可能会看到先输出
    // 一帧，然后卡住了，过会才开始流畅播放；按40ms sleep，原因是每个audioframe长度为46ms（2048/44100）
    if ((cur_v_pts > cur_a_pts) && (cur_v_pts - (int64_t)(AV_SYNC_FRAMEDUP_THRESHOLD * 1000) > cur_a_pts)) {
        ALOGI("[%u] delay_reloaded_video_frame: cur_v_pts=%lld cur_a_pts=%lld, wait 30ms\n", ffp->session_id,
              cur_v_pts, cur_a_pts);
        av_usleep(30000); // 30ms + video_refresh_delay(10ms) = 40ms
        return true;
    }

    return false;
}

/* called to display each frame */
static void video_refresh(FFPlayer* opaque, double* remaining_time) {
    FFPlayer* ffp = opaque;
    VideoState* is = ffp->is;
    double time;

#ifdef FFP_MERGE
    Frame* sp, *sp2;
#endif

    if (!is->paused && get_master_sync_type(is) == AV_SYNC_EXTERNAL_CLOCK && is->realtime) {
        check_external_clock_speed(is);
    }

    if (!ffp->display_disable && is->show_mode != SHOW_MODE_VIDEO && is->audio_st) {
        time = av_gettime_relative() / 1000000.0;
        if (is->force_refresh || is->last_vis_time + ffp->rdftspeed < time) {
            video_display2(ffp);
            is->last_vis_time = time;
        }
        *remaining_time = FFMIN(*remaining_time, is->last_vis_time + ffp->rdftspeed - time);
    }

    int remain_pic_cnt;
    if (is->video_st) {
retry:
        remain_pic_cnt = frame_queue_nb_remaining(&is->pictq);
        if (remain_pic_cnt == 0) {
            // nothing to do, no picture to display in the queue
            // ALOGD("[av_sync] remain_pic_cnt = 0  nothing to do, no picture to display in the queue \n");
        } else {
            double last_duration, duration, delay;
            Frame* vp, *lastvp;

            /* dequeue the picture */
            lastvp = frame_queue_peek_last(&is->pictq);
            vp = frame_queue_peek(&is->pictq);

            if (need_check_live_video_reload(ffp)) {
                if (need_drop_video_frame_after_reload_video(ffp, vp)) {
                    frame_queue_next(&is->pictq);
                    goto retry;
                }

                if (need_delay_reloaded_video_frame_display(ffp, vp)) {
                    return;
                }
            }

            if (vp->serial != is->videoq.serial) {
                frame_queue_next(&is->pictq);
                if (FFP_SHOW_VIDEO_REFRESH_DISPLAY)
                    ALOGI("[%u][av_sync][no_display] vp->serial != is->videoq.serial goto retry !!!", ffp->session_id);
                goto retry;
            }

            if (lastvp->serial != vp->serial)
                is->frame_timer = av_gettime_relative() / 1000000.0;

            if (is->paused) {
                if (FFP_SHOW_VIDEO_REFRESH_DISPLAY)
                    ALOGI("[%u][av_sync][no_display] is->paused=true,  goto display!!!", ffp->session_id);
                goto display;
            }

            /* compute nominal last_duration */
            last_duration = vp_duration(is, lastvp, vp);
            delay = compute_target_delay_opt(ffp, last_duration, is);
//            ALOGD("[av_sync][compute_target_delay_strict] delay:%f \n", delay);
            if (isnan(delay) && !is->step) {
                if (FFP_SHOW_VIDEO_REFRESH_DISPLAY)
                    ALOGE("[%u][av_sync][no_display] remain_pic_cnt = %d, delay = nan return \n",
                          ffp->session_id, remain_pic_cnt, delay);
                if (is->last_vp_serial >= 0) {
                    update_video_pts(is, is->last_vp_pts, is->last_vp_pos, is->last_vp_serial);
                }
                return;
            }

            time = av_gettime_relative() / 1000000.0;
            if (isnan(is->frame_timer) || time < is->frame_timer)
                is->frame_timer = time;
            if (time < is->frame_timer + delay) {
                *remaining_time = FFMIN(is->frame_timer + delay - time, *remaining_time);
                if (FFP_SHOW_VIDEO_REFRESH_DISPLAY)
                    ALOGI("[%u][av_sync][no_display] goto display without display!!!, current time:%f, is->frame_timer:%f, delay:%f, cur_vs_timer_diff:%f, remaining_time:%f",
                          ffp->session_id, time, is->frame_timer, delay, time - is->frame_timer, *remaining_time);
                goto display;
            }

            is->frame_timer += delay;
            if (delay > 0 && time - is->frame_timer > AV_SYNC_THRESHOLD_MAX)
                is->frame_timer = time;

            SDL_LockMutex(is->pictq.mutex);
            if (!isnan(vp->pts)) {
                update_video_pts(is, vp->pts, vp->pos, vp->serial);
                ffp_on_clock_changed(ffp, &is->vidclk);
                ffp_on_clock_changed(ffp, &is->extclk);
            }
            SDL_UnlockMutex(is->pictq.mutex);

            if (frame_queue_nb_remaining(&is->pictq) > 1) {
                Frame* nextvp = frame_queue_peek_next(&is->pictq);
                duration = vp_duration(is, vp, nextvp);
                if (!is->step
                    && (ffp->framedrop > 0 || (ffp->framedrop && get_master_sync_type(is) != AV_SYNC_VIDEO_MASTER))
                    && time > is->frame_timer + duration) {
                    frame_queue_next(&is->pictq);
                    if (is_playback_rate_normal(ffp->pf_playback_rate)) {
                        is->frame_drops_late++;
                        KwaiQos_onRenderDroppedFrame(&ffp->kwai_qos, is->frame_drops_late);
                    }
                    if (FFP_SHOW_VIDEO_REFRESH_DISPLAY)
                        ALOGI("[%u][av_sync][no_display] drop frame ,goto retry !!!, ffp->framedrop:%d", ffp->session_id, ffp->framedrop);
                    goto retry;
                }
            }

            //get absTime
            int64_t abs_pts = vp->pkttime.abs_pts;
            LiveAbsTimeControl_update_abs_time(&ffp->live_abs_time_control, abs_pts);

            if (abs_pts > 0 && ffp->audio_speed_change_enable && ffp->wall_clock_updated) {
                LiveAbsTimeControl_control(ffp, abs_pts, KS_AUDIO_BUFFER_SPEED_DOWN_THR_MS);
            }

            // FFP_MERGE: if (is->subtitle_st) { {...}
            frame_queue_next(&is->pictq);
            is->force_refresh = 1;

            SDL_LockMutex(ffp->is->play_mutex);
            if (is->step) {
                is->step = 0;
                if (!is->paused)
                    stream_update_pause_l(ffp);
            }
            SDL_UnlockMutex(ffp->is->play_mutex);
        }
display:
        /* display picture */
        if (!ffp->display_disable && is->force_refresh && is->show_mode == SHOW_MODE_VIDEO &&
            is->pictq.rindex_shown) {
            FFDemuxCacheControl_on_av_rendered(&ffp->dcc);
            video_display2(ffp);
            is->show_frame_count++;

            if (FFP_SHOW_VIDEO_REFRESH_DISPLAY) {
                int64_t debug_interval = 0;
                if (is->debug_last_frame_render_ts <= 0) {
                    is->debug_last_frame_render_ts = av_gettime_relative() / 1000;
                } else {
                    int64_t now = av_gettime_relative() / 1000;
                    debug_interval = now - is->debug_last_frame_render_ts;
                    is->debug_last_frame_render_ts = now;
                }
                ALOGD("[%u][do video_display2] video_display2 interval:%lldms", ffp->session_id, debug_interval);
            }
        }
    }
    is->force_refresh = 0;

    ffp_show_av_sync_status(ffp, is);
}


// FFP_MERGE: opt_sync
// FFP_MERGE: opt_seek
// FFP_MERGE: opt_duration
// FFP_MERGE: opt_show_mode
// FFP_MERGE: opt_input_file
// FFP_MERGE: opt_codec
// FFP_MERGE: dummy
// FFP_MERGE: options
// FFP_MERGE: show_usage
// FFP_MERGE: show_help_default
int video_refresh_thread(void* arg) {
    FFPlayer* ffp = arg;
    VideoState* is = ffp->is;
    double remaining_time = 0.0;
    bool can_start_play = false;

    while (!is->abort_request && !is->video_refresh_abort_request) {
        if (remaining_time > 0.0)
            av_usleep((int)(int64_t)(remaining_time * 1000000.0));
        remaining_time = REFRESH_RATE;
        if (!can_start_play) {
            // update value from start play strategy
            can_start_play = ffp->kwai_packet_buffer_checker.func_check_can_start_play(&ffp->kwai_packet_buffer_checker, ffp);
        }
        if (is->show_mode != SHOW_MODE_NONE
            && (!is->paused || is->force_refresh)
            && can_start_play
           ) {
            video_refresh(ffp, &remaining_time);
            AbLoop_on_frame_rendered(&ffp->ab_loop, ffp);
            BufferLoop_on_frame_rendered(&ffp->buffer_loop, ffp);
        }
    }

    return 0;
}

