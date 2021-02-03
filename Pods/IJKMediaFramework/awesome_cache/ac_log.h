#pragma once

#include "hodor_config.h"

#ifdef __APPLE__
#include <TargetConditionals.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#include <stdarg.h>

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

#endif

typedef void(* KwaiPlayerLogCallback)(int, const char*, const char*, va_list);

HODOR_EXPORT void ac_log(int level, const char* fmt, ...);
HODOR_EXPORT void set_native_cache_log_callback(KwaiPlayerLogCallback cb);


#define LOG_VERBOSE(...)          ac_log(IJK_LOG_VERBOSE, __VA_ARGS__);

#if TARGET_OS_MAC
#define LOG_DEBUG(...)            ac_log(IJK_LOG_DEBUG, __VA_ARGS__);   // 单元测试可以用DEBUG
#else
#define LOG_DEBUG(...)            ac_log(IJK_LOG_INFO, __VA_ARGS__);    // 暂时都用INFO，cache所有的日志都要存debug.log文件
#endif

#define LOG_INFO(...)             ac_log(IJK_LOG_INFO, __VA_ARGS__);
#define LOG_WARN(...)             ac_log(IJK_LOG_WARN, __VA_ARGS__);
#define LOG_ERROR(...)            ac_log(IJK_LOG_ERROR, __VA_ARGS__);
#define LOG_ERROR_DETAIL(...)     ac_log(IJK_LOG_ERROR, __VA_ARGS__);
#define LOG_FATAL(...)            ac_log(IJK_LOG_FATAL, __VA_ARGS__);


#ifdef __cplusplus
}
#endif
