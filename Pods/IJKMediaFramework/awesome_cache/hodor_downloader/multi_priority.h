//
// Created by MarshallShuai on 2019/8/12.
//

#pragma once

#include "utils/macro_util.h"

HODOR_NAMESPACE_START

typedef enum _TaskPriority {
    Priority_UNLIMITED = 10000,
    Priority_HIGH = 3000,
    Priority_MEDIUM = 2000,
    Priority_LOW = 1000,
    Priority_UNKNOWN = -1,
} TaskPriority;


struct MultiPriority {
    MultiPriority() = delete;
    MultiPriority(int main, int sub): main_priority_(main), sub_priority_(sub) {};
//    MultiPriority &operator = (MultiPriority &other) {
//        main_priority_ = other.main_priority_;
//        sub_priority_ = other.sub_priority_;
//    }

    int main_priority_;
    int sub_priority_;
};

inline bool operator < (const MultiPriority& a, const MultiPriority& b) {
    if (a.main_priority_ != b.main_priority_) {
        return a.main_priority_ < b.main_priority_;
    }
    return a.sub_priority_ < b.sub_priority_;
}

inline bool operator > (const MultiPriority& a, const MultiPriority& b) {
    return b < a;
}

inline bool operator == (const MultiPriority& a, const MultiPriority& b) {
    return !(a > b || a < b);
}

inline bool operator >= (const MultiPriority& a, const MultiPriority& b) {
    return !(a < b);
}

inline bool operator <= (const MultiPriority& a, const MultiPriority& b) {
    return !(a > b);
}

inline bool operator != (const MultiPriority& a, const MultiPriority& b) {
    return !(a == b);
}

HODOR_NAMESPACE_END