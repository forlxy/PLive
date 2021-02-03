//
// Created by MarshallShuai on 2019/1/16.
//
#pragma once

#include <stdint.h>
#include <jni.h>
#include "hodor_config.h"
#include "awesome_cache_callback_c.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 这个接口会生成一个 AwesomeCacheCallback的新对象指针
 * 约定：外部的类需要负责释放
 * @param env
 * @param j_obj
 * @return
 */
HODOR_EXPORT AwesomeCacheCallback_Opaque AwesomeCacheCallback_Opaque_new(JNIEnv* env, jobject j_obj);

#ifdef __cplusplus
}
#endif
