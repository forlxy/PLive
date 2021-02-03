//
//  ijksdl_log.c
//  KSYPlayerCore
//
//  Created by 帅龙成 on 25/10/2017.
//  Copyright © 2017 kuaishou. All rights reserved.
//
#include "ijksdl_log.h"

static KwaiCallback kwai_callback = NULL;
static KwaiKlogCallback kwai_klog_callback = NULL;

static int call_injected_callback_ifneeded(int level, const char* fmt, va_list vl) {
    KwaiCallback callback = kwai_callback;
    if (!callback) {
        return 0;
    }
    char buf[512] = {0};
    vsnprintf(buf, 512, fmt, vl);

    callback(level, buf);
    return 1;
}

static int call_injected_klog_callback_ifneeded(int level, const char* fmt, va_list vl) {
    KwaiKlogCallback callback = kwai_klog_callback;
    if (!callback) {
        return 0;
    }

    callback(level - 3, fmt, vl);
    return 1;
}

void inject_log_callback(KwaiCallback cb) {
    kwai_callback = cb;
}

void inject_klog_callback(KwaiKlogCallback cb) {
    kwai_klog_callback = cb;
}

#include <stdarg.h>

static int log_level = IJK_LOG_DEFAULT;
void ALOG_KWAI(int level, const char* tag, const char* fmt, ...) {
    if (level < log_level) {
        return;
    }
    va_list vl;
    va_start(vl, fmt);

    if (call_injected_callback_ifneeded(level, fmt, vl) == 0 && call_injected_klog_callback_ifneeded(level, fmt, vl) == 0) {
        VLOG_SYS(level, tag, fmt, vl);
    }

    va_end(vl);
}

void VLOG_KWAI(int level, const char* tag, const char* fmt, va_list vl) {
    if (level < log_level) {
        return;
    }
    if (call_injected_callback_ifneeded(level, fmt, vl) == 0 && call_injected_klog_callback_ifneeded(level, fmt, vl) == 0) {
        VLOG_SYS(level, tag, fmt, vl);
    }
}


void kwai_set_log_level(int level) {
    log_level = level;
}

