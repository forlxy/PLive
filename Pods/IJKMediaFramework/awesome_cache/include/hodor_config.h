//
// Created by MarshallShuai on 2019/10/25.
//

#pragma once

/**
 * 这个类主要是放一些hodor的一些编译相关的宏定义
 */

#define HODOR_EXPORT  __attribute__ ((visibility ("default")))

#ifdef CATCHYA_UNIT_TEST
#define HODOR_EXPORT_ON_UNIT_TEST  __attribute__ ((visibility ("default")))
#else
#define HODOR_EXPORT_ON_UNIT_TEST
#endif