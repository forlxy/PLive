//
// Created by MarshallShuai on 2018/10/30.
//
#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct AwesomeCacheInterruptCB {
    int (*callback)(void*);

    void* opaque;

    // 这个interrupted放在这里不是最合理的，应该放在各个具体实现的类中作成员变量
    bool interrupted;
} AwesomeCacheInterruptCB;

bool AwesomeCacheInterruptCB_is_interrupted(AwesomeCacheInterruptCB* self);

#ifdef __cplusplus
}
#endif


