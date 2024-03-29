/*****************************************************************************
 * ijksdl_log.h
 *****************************************************************************
 *
 * copyright (c) 2015 Zhang Rui <bbcallen@gmail.com>
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

#ifndef IJKSDL__IJKSDL_LOG_H
#define IJKSDL__IJKSDL_LOG_H

#include <stdio.h>

#ifdef __ANDROID__

#include <android/log.h>

#define IJK_LOG_UNKNOWN     ANDROID_LOG_UNKNOWN
#define IJK_LOG_DEFAULT     ANDROID_LOG_DEFAULT

#define IJK_LOG_VERBOSE     ANDROID_LOG_VERBOSE
#define IJK_LOG_DEBUG       ANDROID_LOG_DEBUG
#define IJK_LOG_INFO        ANDROID_LOG_INFO
#define IJK_LOG_WARN        ANDROID_LOG_WARN
#define IJK_LOG_ERROR       ANDROID_LOG_ERROR
#define IJK_LOG_FATAL       ANDROID_LOG_FATAL
#define IJK_LOG_SILENT      ANDROID_LOG_SILENT

#define VLOG_SYS(level, TAG, ...)    ((void)__android_log_vprint(level, TAG, __VA_ARGS__))

#else

#define IJK_LOG_UNKNOWN     0
#define IJK_LOG_DEFAULT     1

#define IJK_LOG_VERBOSE     2
#define IJK_LOG_DEBUG       3
#define IJK_LOG_INFO        4
#define IJK_LOG_WARN        5
#define IJK_LOG_ERROR       6
#define IJK_LOG_FATAL       7
#define IJK_LOG_SILENT      8

#define VLOG_SYS(level, TAG, ...)    ((void)vprintf(__VA_ARGS__))

#endif

#define IJK_LOG_TAG "IjkMediaPlayer"

typedef void(* KwaiCallback)(int, const char*);
typedef void(* KwaiKlogCallback)(int, const char*, va_list vl);

void inject_log_callback(KwaiCallback cb);
void inject_klog_callback(KwaiKlogCallback cb);

void ALOG_KWAI(int level, const char* tag, const char* fmt, ...);
void VLOG_KWAI(int level, const char* tag, const char* fmt, va_list vl);
void kwai_set_log_level(int level);

#define VLOG(level, TAG, ...) ((void)VLOG_KWAI(level, TAG, __VA_ARGS__))
#define ALOG(level, TAG, ...)    ((void)ALOG_KWAI(level, TAG, __VA_ARGS__))

#define VLOGV(...)  VLOG(IJK_LOG_VERBOSE,   IJK_LOG_TAG, __VA_ARGS__)
#define VLOGD(...)  VLOG(IJK_LOG_DEBUG,     IJK_LOG_TAG, __VA_ARGS__)
#define VLOGI(...)  VLOG(IJK_LOG_INFO,      IJK_LOG_TAG, __VA_ARGS__)
#define VLOGW(...)  VLOG(IJK_LOG_WARN,      IJK_LOG_TAG, __VA_ARGS__)
#define VLOGE(...)  VLOG(IJK_LOG_ERROR,     IJK_LOG_TAG, __VA_ARGS__)

#define ALOGV(...)  ALOG(IJK_LOG_VERBOSE,   IJK_LOG_TAG, __VA_ARGS__)
#define ALOGD(...)  ALOG(IJK_LOG_DEBUG,     IJK_LOG_TAG, __VA_ARGS__)
#define ALOGI(...)  ALOG(IJK_LOG_INFO,      IJK_LOG_TAG, __VA_ARGS__)
#define ALOGW(...)  ALOG(IJK_LOG_WARN,      IJK_LOG_TAG, __VA_ARGS__)
#define ALOGE(...)  ALOG(IJK_LOG_ERROR,     IJK_LOG_TAG, __VA_ARGS__)
#define LOG_ALWAYS_FATAL(...)   do { ALOGE(__VA_ARGS__); exit(1); } while (0)

static inline char* get_error_code_fourcc_string(char* buf, int err_tag) {
    err_tag = -err_tag;
    for (int i = 0; i < 4; i++) {
        buf[i] = err_tag >> (i * 8) & 0xff;
    }
    return buf;
}

#define get_error_code_fourcc_string_macro(err_tag)\
    get_error_code_fourcc_string((char[5]){0}, err_tag)
#endif
