//
// Created by MarshallShuai on 2018/11/14.
//
#pragma once

#include <string>
#include <cstdint>

namespace kuaishou {
namespace cache {

class AcUtils {
  public:
    static void SetThreadName(const std::string& name);
    static int64_t GetCurrentTime();
};

}
}
