//
//  shared.h
//  KSYStreamer
//
//  Created by Minglei Zhang on 12/28/16.
//  Copyright © 2016 yiqian. All rights reserved.
//

#ifndef shared_h
#define shared_h

#ifdef __ANDROID__
#include <android/log.h>
#define LOG(format, ...) __android_log_print(ANDROID_LOG_DEBUG, "KSY_LIVE_SDK", format, ##__VA_ARGS__)
#else
#define LOG(format, ...) printf(format, __VA_ARGS__); printf("\n")
#endif

#endif /* shared_h */
