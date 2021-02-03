//
// Created by MarshallShuai on 2019/4/19.
//

#if defined(CONFIG_KS_AUDIOPROCESS) || defined(__APPLE__)
#include <ijkmedia/ijkkwai/c_audio_process.h>
#endif

#include "ijkplayer/ff_ffplay.h"
#include "ff_ffplay_module_audio_decode.h"

#include "ijkkwai/kwai_clock_tracker.h"
#include "ijkplayer/ff_ffplay_def.h"


#if CONFIG_AVFILTER
int configure_audio_filters(FFPlayer* ffp, const char* afilters, int force_output_format) {
    VideoState* is = ffp->is;
    static const enum AVSampleFormat sample_fmts[] = { AV_SAMPLE_FMT_S16, AV_SAMPLE_FMT_NONE };
    int sample_rates[2] = { 0, -1 };
    int64_t channel_layouts[2] = { 0, -1 };
    int channels[2] = { 0, -1 };
    AVFilterContext* filt_asrc = NULL, *filt_asink = NULL;
    char aresample_swr_opts[512] = "";
    AVDictionaryEntry* e = NULL;
    char asrc_args[256];
    int ret;
    char afilters_args[4096];

    avfilter_graph_free(&is->agraph);
    if (!(is->agraph = avfilter_graph_alloc()))
        return AVERROR(ENOMEM);

    while ((e = av_dict_get(ffp->swr_opts, "", e, AV_DICT_IGNORE_SUFFIX)))
        av_strlcatf(aresample_swr_opts, sizeof(aresample_swr_opts), "%s=%s:", e->key, e->value);
    if (strlen(aresample_swr_opts))
        aresample_swr_opts[strlen(aresample_swr_opts) - 1] = '\0';
    av_opt_set(is->agraph, "aresample_swr_opts", aresample_swr_opts, 0);

    ret = snprintf(asrc_args, sizeof(asrc_args),
                   "sample_rate=%d:sample_fmt=%s:channels=%d:time_base=%d/%d",
                   is->audio_filter_src.freq, av_get_sample_fmt_name(is->audio_filter_src.fmt),
                   is->audio_filter_src.channels,
                   1, is->audio_filter_src.freq);
    if (is->audio_filter_src.channel_layout)
        snprintf(asrc_args + ret, sizeof(asrc_args) - ret,
                 ":channel_layout=0x%"PRIx64, is->audio_filter_src.channel_layout);

    ret = avfilter_graph_create_filter(&filt_asrc,
                                       avfilter_get_by_name("abuffer"), "ffplay_abuffer",
                                       asrc_args, NULL, is->agraph);
    if (ret < 0)
        goto end;


    ret = avfilter_graph_create_filter(&filt_asink,
                                       avfilter_get_by_name("abuffersink"), "ffplay_abuffersink",
                                       NULL, NULL, is->agraph);
    if (ret < 0)
        goto end;

    if ((ret = av_opt_set_int_list(filt_asink, "sample_fmts", sample_fmts, AV_SAMPLE_FMT_NONE,
                                   AV_OPT_SEARCH_CHILDREN)) < 0)
        goto end;
    if ((ret = av_opt_set_int(filt_asink, "all_channel_counts", 1, AV_OPT_SEARCH_CHILDREN)) < 0)
        goto end;

    if (force_output_format) {
        channel_layouts[0] = is->audio_tgt.channel_layout;
        channels[0] = is->audio_tgt.channels;
        sample_rates[0] = is->audio_tgt.freq;
        if ((ret = av_opt_set_int(filt_asink, "all_channel_counts", 0, AV_OPT_SEARCH_CHILDREN)) < 0)
            goto end;
        if ((ret = av_opt_set_int_list(filt_asink, "channel_layouts", channel_layouts, -1,
                                       AV_OPT_SEARCH_CHILDREN)) < 0)
            goto end;
        if ((ret = av_opt_set_int_list(filt_asink, "channel_counts", channels,  -1, AV_OPT_SEARCH_CHILDREN)) < 0)
            goto end;
        if ((ret = av_opt_set_int_list(filt_asink, "sample_rates", sample_rates,  -1, AV_OPT_SEARCH_CHILDREN)) < 0)
            goto end;
    }

    afilters_args[0] = 0;
    if (afilters)
        snprintf(afilters_args, sizeof(afilters_args), "%s", afilters);

#ifdef FFP_AVFILTER_PLAYBACK_RATE
    if (fabsf(ffp->pf_playback_rate) > 0.00001 &&
        fabsf(ffp->pf_playback_rate - 1.0f) > 0.00001) {
        if (afilters_args[0])
            av_strlcatf(afilters_args, sizeof(afilters_args), ",");

        ALOGI("[%u] af_rate=%f\n", ffp->session_id, ffp->pf_playback_rate);
        av_strlcatf(afilters_args, sizeof(afilters_args), "atempo=%f", ffp->pf_playback_rate);
    }
#endif

    if ((ret = configure_filtergraph(is->agraph, afilters_args[0] ? afilters_args : NULL, filt_asrc,
                                     filt_asink)) < 0)
        goto end;

    is->in_audio_filter = filt_asrc;
    is->out_audio_filter = filt_asink;

end:
    if (ret < 0)
        avfilter_graph_free(&is->agraph);
    return ret;
}

#endif  /* CONFIG_AVFILTER */

/**
 * audio解码线程入口
 * @param arg
 * @return
 */
int audio_decode_thread(void* arg) {
    FFPlayer* ffp = arg;
    VideoState* is = ffp->is;
    AVFrame* frame = av_frame_alloc();
    Frame* af;
    AVPacketTime pkttime;
#if CONFIG_AVFILTER
    int last_serial = -1;
    int64_t dec_channel_layout;
    int reconfigure;
#endif
    int got_frame = 0;
    AVRational tb;
    int ret = 0;
    int audio_accurate_seek_fail = 0;
    int64_t audio_seek_pos = 0;
    double frame_pts = 0;
    double audio_clock = 0;
    int64_t now = 0;
    double samples_duration = 0;
    bool audio_eos = false;
    int64_t deviation = 0;
    int64_t deviation2 = 0;
    int64_t deviation3 = 0;

    if (!frame) {
        return AVERROR(ENOMEM);
    }


    while (!is->abort_request &&
           !ffp->kwai_packet_buffer_checker.func_check_can_start_play(&ffp->kwai_packet_buffer_checker, ffp)) {
        SDL_Delay(10);
    }

    do {
        ffp_audio_statistic_l(ffp);
        memset(&pkttime, 0, sizeof(AVPacketTime));

        if ((got_frame = decoder_decode_frame(ffp, &is->auddec, frame, NULL, &pkttime)) < 0)
            goto the_end;

        if (got_frame) {
            tb = (AVRational) {1, frame->sample_rate};
            if (ffp->enable_accurate_seek && is->audio_accurate_seek_req && !is->seek_req) {
                frame_pts = (frame->pts == AV_NOPTS_VALUE) ? NAN : frame->pts * av_q2d(tb);
                now = av_gettime_relative() / 1000;
                double latency = SDL_AoutGetLatencySeconds(ffp->aout);
                if (!isnan(frame_pts)) {
                    samples_duration = (double) frame->nb_samples / frame->sample_rate;
                    audio_clock = frame_pts + samples_duration;
                    audio_seek_pos = is->seek_pos + latency * 1000 * 1000;
                    deviation = llabs((int64_t)(audio_clock * 1000 * 1000) - audio_seek_pos);
                    audio_eos =
                        fftime_to_milliseconds((audio_clock + samples_duration) * 1000 * 1000) >
                        ffp_get_duration_l(ffp) ||
                        (is->audio_duration != AV_NOPTS_VALUE &&
                         fftime_to_milliseconds(
                             (audio_clock + samples_duration) * 1000 * 1000) >
                         fftime_to_milliseconds(is->audio_duration));
                }
                if ((!audio_eos && (audio_clock * 1000 * 1000 < audio_seek_pos)) ||
                    isnan(frame_pts)) {
                    if (is->drop_aframe_count == 0) {
                        SDL_LockMutex(is->accurate_seek_mutex);
                        if (is->accurate_seek_start_time <= 0 &&
                            (is->video_stream < 0 || is->video_accurate_seek_req)) {
                            is->accurate_seek_start_time = now;
                        }
                        SDL_UnlockMutex(is->accurate_seek_mutex);
                        av_log(NULL, AV_LOG_INFO,
                               "audio accurate_seek start, is->seek_pos=%lld, audio_clock=%lf, is->accurate_seek_start_time = %lld\n",
                               is->seek_pos, audio_clock, is->accurate_seek_start_time);
                    }
                    is->drop_aframe_count++;
                    while (!isnan(frame_pts) && is->video_accurate_seek_req && !is->abort_request) {
                        deviation2 = is->accurate_seek_vframe_pts - audio_clock * 1000 * 1000;
                        deviation3 = is->accurate_seek_vframe_pts - is->seek_pos;
                        if (deviation2 > AV_DEVIATION && deviation3 < 0) {
                            break;
                        } else {
                            av_usleep(20 * 1000);
                        }
                        now = av_gettime_relative() / 1000;
                        if ((now - is->accurate_seek_start_time) > ffp->accurate_seek_timeout) {
                            break;
                        }
                    }

                    if (!isnan(frame_pts) && !is->video_accurate_seek_req &&
                        is->video_stream >= 0 &&
                        audio_clock * 1000 * 1000 > is->accurate_seek_vframe_pts) {
                        // only for pause->seek case, both video_accurate_seek_req and accurate_seek_vframe_pts already reset in queue_picture,
                        if (frame_pts * 1000 * 1000 < audio_seek_pos ||
                            deviation > MAX_DEVIATION) {
                            av_frame_unref(frame);
                            continue;
                        } else {
                            audio_accurate_seek_fail = 1;
                        }
                    } else {
                        now = av_gettime_relative() / 1000;
                        if ((now - is->accurate_seek_start_time) <=
                            ffp->accurate_seek_timeout) {
                            av_frame_unref(frame);
                            continue;  // drop some old frame when do accurate seek
                        } else {
                            audio_accurate_seek_fail = 1;
                        }
                    }
                } else {
                    int64_t latency_pos = (int64_t)(latency * 1000 * 1000);
                    if (audio_seek_pos == is->seek_pos + latency_pos) {
                        ALOGI("[%u] audio accurate_seek is ok, is->drop_aframe_count=%d, audio_clock = %lf\n",
                              ffp->session_id, is->drop_aframe_count, audio_clock);
                        is->drop_aframe_count = 0;
                        SDL_LockMutex(is->accurate_seek_mutex);
                        is->audio_accurate_seek_req = 0;
                        SDL_CondSignal(is->video_accurate_seek_cond);
                        if (audio_seek_pos == (is->seek_pos + latency_pos) &&
                            is->video_accurate_seek_req && !is->abort_request && !audio_eos) {
                            SDL_CondWaitTimeout(is->audio_accurate_seek_cond,
                                                is->accurate_seek_mutex,
                                                ffp->accurate_seek_timeout);
                        } else {
                            if (!is->accurate_seek_notify) {
                                ffp_notify_msg2(ffp, FFP_MSG_ACCURATE_SEEK_COMPLETE,
                                                (int)(audio_clock * 1000));
                                is->accurate_seek_notify = 1;
                            }
                        }

                        if (audio_seek_pos != (is->seek_pos + latency_pos) && !is->abort_request) {
                            is->audio_accurate_seek_req = 1;
                            SDL_UnlockMutex(is->accurate_seek_mutex);
                            av_frame_unref(frame);
                            continue;
                        }

                        SDL_UnlockMutex(is->accurate_seek_mutex);
                    }
                }

                if (audio_accurate_seek_fail) {
                    ALOGI("[%u] audio accurate_seek is error, is->drop_aframe_count=%d, now = %lld, audio_clock = %lf\n",
                          ffp->session_id, is->drop_aframe_count, now, audio_clock);
                    is->drop_aframe_count = 0;
                    SDL_LockMutex(is->accurate_seek_mutex);
                    is->audio_accurate_seek_req = 0;
                    SDL_CondSignal(is->video_accurate_seek_cond);
                    if (is->video_accurate_seek_req && !is->abort_request) {
                        SDL_CondWaitTimeout(is->audio_accurate_seek_cond, is->accurate_seek_mutex,
                                            ffp->accurate_seek_timeout);
                    } else {
                        if (!is->accurate_seek_notify) {
                            ffp_notify_msg2(ffp, FFP_MSG_ACCURATE_SEEK_COMPLETE,
                                            (int)(audio_clock * 1000));
                            is->accurate_seek_notify = 1;
                        }
                    }
                    SDL_UnlockMutex(is->accurate_seek_mutex);
                }
                is->accurate_seek_start_time = 0;
                audio_accurate_seek_fail = 0;
            }

#if CONFIG_AVFILTER
            dec_channel_layout = get_valid_channel_layout(frame->channel_layout,
                                                          av_frame_get_channels(frame));

            reconfigure =
                cmp_audio_fmts(is->audio_filter_src.fmt, is->audio_filter_src.channels,
                               frame->format, av_frame_get_channels(frame)) ||
                is->audio_filter_src.channel_layout != dec_channel_layout ||
                is->audio_filter_src.freq != frame->sample_rate ||
                is->auddec.pkt_serial != last_serial ||
                ffp->af_changed;

            if (reconfigure) {
                SDL_LockMutex(ffp->af_mutex);
                ffp->af_changed = 0;
                char buf1[1024], buf2[1024];
                av_get_channel_layout_string(buf1, sizeof(buf1), -1, is->audio_filter_src.channel_layout);
                av_get_channel_layout_string(buf2, sizeof(buf2), -1, dec_channel_layout);
                ALOGD("[%u][%s] Audio frame changed from rate:%d ch:%d fmt:%s layout:%s serial:%d to rate:%d ch:%d fmt:%s layout:%s serial:%d\n",
                      ffp->session_id, __func__,
                      is->audio_filter_src.freq, is->audio_filter_src.channels, av_get_sample_fmt_name(is->audio_filter_src.fmt), buf1, last_serial,
                      frame->sample_rate, av_frame_get_channels(frame), av_get_sample_fmt_name(frame->format), buf2, is->auddec.pkt_serial);

                is->audio_filter_src.fmt            = frame->format;
                is->audio_filter_src.channels       = av_frame_get_channels(frame);
                is->audio_filter_src.channel_layout = dec_channel_layout;
                is->audio_filter_src.freq           = frame->sample_rate;
                last_serial                         = is->auddec.pkt_serial;

                if ((ret = configure_audio_filters(ffp, ffp->afilters, 1)) < 0) {
                    SDL_UnlockMutex(ffp->af_mutex);
                    goto the_end;
                }
                SDL_UnlockMutex(ffp->af_mutex);
            }

            if ((ret = av_buffersrc_add_frame(is->in_audio_filter, frame)) < 0)
                goto the_end;
            while ((ret = av_buffersink_get_frame_flags(is->out_audio_filter, frame, 0)) >= 0) {
                tb = is->out_audio_filter->inputs[0]->time_base;
#endif
                if (!(af = frame_queue_peek_writable(&is->sampq)))
                    goto the_end;

                af->pts = (frame->pts == AV_NOPTS_VALUE) ? NAN : frame->pts * av_q2d(tb);
                af->pos = av_frame_get_pkt_pos(frame);
                af->serial = is->auddec.pkt_serial;
                af->duration = av_q2d((AVRational) {frame->nb_samples, frame->sample_rate});

                memset(&(af->pkttime), 0, sizeof(AVPacketTime));
                memcpy(&(af->pkttime), &pkttime, sizeof(AVPacketTime));
                av_frame_move_ref(af->frame, frame);

                char* audio_buf = af->frame->data[0];

#if defined(CONFIG_KS_AUDIOPROCESS) || defined(__APPLE__)
                int64_t process_begin_time = av_gettime_relative() / 1000;

                if (ffp->audio_gain.audio_compress_processor) {
                    AudioCompressProcessor_process(&ffp->audio_gain.audio_compress_processor, audio_buf, av_frame_get_channels(af->frame), af->frame->nb_samples);
                }

                if (ffp->audio_gain.audio_processor) {
                    AudioProcessor_process(&ffp->audio_gain.audio_processor, audio_buf, av_frame_get_channels(af->frame), af->frame->nb_samples);
                }
                int64_t process_end_time = av_gettime_relative() / 1000;
                KwaiQos_setAudioProcessCost(&ffp->kwai_qos, process_end_time - process_begin_time);
#endif

                frame_queue_push(&is->sampq);
#if CONFIG_AVFILTER
                if (is->audioq.serial != is->auddec.pkt_serial)
                    break;
            }
            if (ret == AVERROR_EOF)
                is->auddec.finished = is->auddec.pkt_serial;
#endif
        }

    } while (ret >= 0 || ret == AVERROR(EAGAIN) || ret == AVERROR_EOF);
the_end:
#if CONFIG_AVFILTER
    avfilter_graph_free(&is->agraph);
#endif
    av_frame_free(&frame);
    return ret;
}