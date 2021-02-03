//
// Created by MarshallShuai on 2019/1/17.
//

#include "include/awesome_cache_callback.h"

#pragma once

namespace kuaishou {
namespace cache {

class MacAcCallbackInfo : public AcCallbackInfo {
  public:
    MacAcCallbackInfo() = default;
    ~MacAcCallbackInfo() override = default;


    // 结果数据
    string cdnStatJson;

    void SetCdnStatJson(string cdnStatJson) override;
};


}
}
