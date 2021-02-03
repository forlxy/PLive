//
// Created by MarshallShuai on 2019/4/19.
//
#pragma once

#include "config.h"

/**
 * 音频解码线程
 */
int audio_decode_thread(void* arg);

#if CONFIG_AVFILTER
int configure_audio_filters(FFPlayer* ffp, const char* afilters, int force_output_format);
#endif
