//
// Created by MarshallShuai on 2019-08-02.
//

#pragma once

#include "v2/cache/cache_def_v2.h"

const static int kHodorThreadWorkCountMax = 10;
const static int kHodorThreadWorkCountMin = 1;
const static int kHodorThreadWorkCountDefault = 1;
// hodor下载器初始并发数
const static int kHodorThreadWorkCountInit = 1;

// 打印整个下载状态，需要多个类来协同打印，所以用宏开关来 屏蔽/打开 日志代码
#define LOG_OVERALL_DOWNLOAD_STATUS 1

#define HODOR_THREAD_NAME_MAX_LEN 64