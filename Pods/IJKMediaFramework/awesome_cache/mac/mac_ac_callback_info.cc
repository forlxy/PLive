//
// Created by MarshallShuai on 2019/1/17.
//

#include <ac_log.h>
#include "mac_ac_callback_info.h"
//#include "catch.hpp"


namespace kuaishou {
namespace cache {


void MacAcCallbackInfo::SetCdnStatJson(string cdnStatJson) {
    this->cdnStatJson = cdnStatJson;
}

kuaishou::cache::AcCallbackInfo* AcCallbackInfoFactory::CreateCallbackInfo() {
    return new MacAcCallbackInfo();
}

}
}

