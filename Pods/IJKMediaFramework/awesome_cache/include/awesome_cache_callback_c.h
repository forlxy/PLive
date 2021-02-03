//
// Created by MarshallShuai on 2019/1/31.
//

#pragma once

#include "hodor_config.h"

typedef void* AwesomeCacheCallback_Opaque;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 每个平台的创建方法不一样(from jni / from objective-c)，但是删除方法是一样的，是平台通用的
 */
HODOR_EXPORT void AwesomeCacheCallback_Opaque_delete(AwesomeCacheCallback_Opaque opaque);

#ifdef __cplusplus
}
#endif
