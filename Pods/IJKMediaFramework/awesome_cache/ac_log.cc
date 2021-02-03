#include "ac_log.h"
//此处的tag只会在客户端没有给播放器设置callback时才会打印
#define AWESOME_CACHE_LOG_TAG "AwesomeCache"

KwaiPlayerLogCallback log_cb_ = nullptr;
void set_native_cache_log_callback(KwaiPlayerLogCallback cb) {
    log_cb_ = cb;
}


// 暂时用这种实现，先解决A站问题再回来完善这块
#ifdef __ANDROID__
#define VLOG_SYS(level, TAG, ...)    ((void)__android_log_vprint(level, TAG, __VA_ARGS__))
#endif

void ac_log(int level, const char* fmt, ...) {
    va_list vl;
    va_start(vl, fmt);

    if (log_cb_) {
        log_cb_(level, AWESOME_CACHE_LOG_TAG, fmt, vl);
    } else {
#ifdef __ANDROID__
        VLOG_SYS(level, AWESOME_CACHE_LOG_TAG, fmt, vl);
#endif
    }

    va_end(vl);
}


#include <iostream>
void std_log(int level, const char* fmt, ...) {
    va_list vl;
    va_start(vl, fmt);

    char buf[1024];
    snprintf(buf, 1024, fmt, vl);
    std::cout << buf << std::endl;

    va_end(vl);
}