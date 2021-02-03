//
// Created by MarshallShuai on 2019-07-01.
// 这个文件暂时定义
//

#pragma once

#include <stdint.h>

#ifdef __APPLE__
#include <TargetConditionals.h>
#endif

// 这个开关是CacheV2唯一开关，在决定上线前，这个变量一定保持是0的，开发分支改为1
#if (TARGET_OS_IPHONE)
// ios暂时单独作为一个开关
#define ENABLE_CACHE_V2 1
#else
// android & unit test(mac)
#define ENABLE_CACHE_V2 1
#endif

#define CACHE_V2_CACHE_FILE_NAME_MAX_LEN 128

#ifndef GB
#define GB (1024*1024*1024)
#define MB (1024*1024)
#define KB (1024)
#endif

static const int kDefaultScopeBytes = 1 * MB;
static const int kMinScopeBytes = 256 * KB;
static const int kMaxScopeBytes = 10 * MB;
static const int kScopeBytesUnit = 1 * KB;   // scope bytes必须是这个bytes数的整数倍


// 默认的总缓存上限
static const int64_t kDefaultCacheBytesLimit = 256 * MB;
static const int64_t kMinCacheBytesLimit = 10 * MB;
static const int64_t kMaxCacheBytesLimit = (int64_t)10 * GB;

static const int kMinAcCallbackProgressIntervalMs = 200;